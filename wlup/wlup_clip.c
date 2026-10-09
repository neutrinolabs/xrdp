/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * wlup: text clipboard over the RDP clipboard channel ([MS-RDPECLIP])
 *
 * The channel handling follows the VNC module's (vnc/vnc_clip.c). The
 * difference is the session side: UTF-8 text, which is converted to and
 * from the RDP client's CF_UNICODETEXT (UTF-16LE, with CR LF line ends).
 *
 * Client to session: the client announces a format list; we ask for
 * CF_UNICODETEXT and hand the text to the backend, which makes the
 * session's clipboard offer it.
 * Session to client: the backend reads the session's clipboard when it
 * changes and calls wlup_clip_session_text(); we announce CF_UNICODETEXT
 * and answer the client's data request from that copy.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arch.h"
#include "wlup.h"
#include "wlup_clip.h"
#include "log.h"
#include "parse.h"
#include "string_calls.h"
#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"
#include "xrdp_constants.h"

/* Time allowed for the selection owner to write its data */
#define READ_TIMEOUT_MS 2000

/* Largest clipboard text accepted from the session */
#define MAX_TEXT (16 * 1024 * 1024)

struct wlup_clip
{
    struct stream *text_s;    /* session clipboard, UTF-8 */
    int active_data_requests; /* outstanding CB_FORMAT_DATA_REQUESTs */
    struct stream *dechunker_s;
    int capability_version;
    int capability_flags;
    int startup_complete;
};

/******************************************************************************/
/* Adds a CLIPRDR_HEADER ([MS-RDPECLIP] 2.2.1), the length is filled in by
 * send_clip_pdu() */
static void
out_cliprdr_header(struct stream *s, int msg_type, int msg_flags)
{
    out_uint16_le(s, msg_type);
    out_uint16_le(s, msg_flags);
    s_push_layer(s, channel_hdr, 4);
}

/******************************************************************************/
/* Sends a CLIPRDR PDU, in channel chunks if needed. return error */
static int
send_clip_pdu(struct wlup *v, struct stream *s)
{
    int rv = 0;
    int pos;
    int pdu_len;
    int total = (int)(s->end - s->data);
    int flags;

    s_pop_layer(s, channel_hdr);
    out_uint32_le(s, (int)(s->end - s->p) - 4);

    for (pos = 0; rv == 0 && pos < total; pos += pdu_len)
    {
        pdu_len = MIN(CHANNEL_CHUNK_LENGTH, total - pos);
        flags = 0;
        if (pos == 0)
        {
            flags |= XR_CHANNEL_FLAG_FIRST;
        }
        if (pos + pdu_len == total)
        {
            flags |= XR_CHANNEL_FLAG_LAST;
        }
        if (flags != (XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST))
        {
            flags |= XR_CHANNEL_FLAG_SHOW_PROTOCOL;
        }
        rv = v->server_send_to_channel(v, v->clip_chanid, s->data + pos,
                                       pdu_len, total, flags);
    }
    return rv;
}

/******************************************************************************/
/* Tells the client we have text ([MS-RDPECLIP] 2.2.3.1) */
static void
send_format_list(struct wlup *v)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    int long_names = c->capability_flags & CB_USE_LONG_FORMAT_NAMES;
    struct stream *s;

    make_stream(s);
    init_stream(s, 64);
    out_cliprdr_header(s, CB_FORMAT_LIST, long_names);
    out_uint32_le(s, CF_UNICODETEXT);
    if (long_names)
    {
        out_uint8s(s, 2); /* empty wsz name */
    }
    else
    {
        out_uint8s(s, 32); /* empty short name */
    }
    s_mark_end(s);
    send_clip_pdu(v, s);
    free_stream(s);
}

/******************************************************************************/
/* Does a format list from the client contain CF_UNICODETEXT? */
static int
has_unicode_text(struct wlup *v, struct stream *s)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    int format_id;
    int wc;

    while (s_check_rem(s, 4))
    {
        in_uint32_le(s, format_id);
        if ((c->capability_flags & CB_USE_LONG_FORMAT_NAMES) == 0)
        {
            in_uint8s(s, MIN(s_rem(s), 32));
        }
        else
        {
            do
            {
                wc = 0;
                if (s_check_rem(s, 2))
                {
                    in_uint16_le(s, wc);
                }
            }
            while (wc != 0);
        }
        if (format_id == CF_UNICODETEXT)
        {
            return 1;
        }
    }
    return 0;
}

/******************************************************************************/
static int
handle_format_list(struct wlup *v, struct stream *s)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    struct stream *out_s;

    c->startup_complete = 1;

    make_stream(out_s);
    init_stream(out_s, 64);
    out_cliprdr_header(out_s, CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK);
    s_mark_end(out_s);
    send_clip_pdu(v, out_s);

    if (has_unicode_text(v, s))
    {
        ++c->active_data_requests;
        init_stream(out_s, 64);
        out_cliprdr_header(out_s, CB_FORMAT_DATA_REQUEST, 0);
        out_uint32_le(out_s, CF_UNICODETEXT);
        s_mark_end(out_s);
        send_clip_pdu(v, out_s);
    }
    free_stream(out_s);
    return 0;
}

/******************************************************************************/
/* The client asks for our text. return error */
static int
handle_format_data_request(struct wlup *v, struct stream *s)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    struct stream *out_s;
    int format = 0;
    const char *p = c->text_s->data;
    unsigned int len = (unsigned int)(c->text_s->end - c->text_s->data);
    char32_t c32;
    int words;

    if (s_check_rem(s, 4))
    {
        in_uint32_le(s, format);
    }

    /* Each character becomes at most two UTF-16 words, plus a CR for
     * each LF and the terminator */
    words = (p == NULL) ? 1 : (int)len * 2 + 1;
    make_stream(out_s);
    init_stream(out_s, 64 + words * 2);
    if (out_s->data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: out of memory for clipboard data");
        free_stream(out_s);
        return 1;
    }

    if (format != CF_UNICODETEXT)
    {
        out_cliprdr_header(out_s, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_FAIL);
    }
    else
    {
        out_cliprdr_header(out_s, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK);
        while (p != NULL && len > 0)
        {
            c32 = utf8_get_next_char(&p, &len);
            if (c32 == '\n')
            {
                out_uint16_le(out_s, '\r');
            }
            if (c32 >= 0x10000)
            {
                /* Surrogate pair */
                c32 -= 0x10000;
                out_uint16_le(out_s, 0xd800 | (c32 >> 10));
                out_uint16_le(out_s, 0xdc00 | (c32 & 0x3ff));
            }
            else
            {
                out_uint16_le(out_s, c32);
            }
        }
        out_uint16_le(out_s, 0);
    }
    s_mark_end(out_s);
    send_clip_pdu(v, out_s);
    free_stream(out_s);
    return 0;
}

/******************************************************************************/
/* The client's text, in answer to our request. return error */
static int
handle_format_data_response(struct wlup *v, struct stream *s)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    struct stream *text_s;
    char u8[MAXLEN_UTF8_CHAR];
    char32_t c32;
    int wc;
    int wc2;
    int n;

    /* A new format list may have crossed an earlier request; only the
     * last response counts */
    if (c->active_data_requests == 0 || --c->active_data_requests > 0)
    {
        return 0;
    }

    /* Each UTF-16 word becomes at most three UTF-8 bytes */
    make_stream(text_s);
    init_stream(text_s, s_rem(s) / 2 * 3 + 1);
    if (text_s->data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: out of memory for clipboard data");
        free_stream(text_s);
        return 1;
    }
    while (s_check_rem(s, 2))
    {
        in_uint16_le(s, wc);
        if (wc == 0)
        {
            break;
        }
        c32 = (char32_t)wc;
        if (wc >= 0xd800 && wc <= 0xdbff && s_check_rem(s, 2))
        {
            in_uint16_le(s, wc2);
            c32 = 0x10000 + (((char32_t)wc - 0xd800) << 10) + (wc2 - 0xdc00);
        }
        if (c32 == '\n' && text_s->p > text_s->data && text_s->p[-1] == '\r')
        {
            --text_s->p; /* CR LF becomes LF */
        }
        n = utf_char32_to_utf8(c32, u8);
        out_uint8a(text_s, u8, n);
    }
    s_mark_end(text_s);

    LOG(LOG_LEVEL_DEBUG, "wlup: %d bytes of clipboard text from the client",
        (int)(text_s->end - text_s->data));

    /* The session will offer this text; if it comes back as a new
     * session selection, it is not announced back to the client */
    free_stream(c->text_s);
    c->text_s = text_s;
    text_s = NULL;
    switch (v->backend)
    {
        case WLUP_BACKEND_MUTTER:
#if defined(XRDP_WLUP_MUTTER)
            wlup_mutter_set_clipboard_text(v, c->text_s->data,
                                           (int)(c->text_s->end - c->text_s->data));
#endif
            break;
        default:
            wlup_wlr_set_clipboard_text(v, c->text_s->data,
                                        (int)(c->text_s->end - c->text_s->data));
            break;
    }
    return 0;
}

/******************************************************************************/
static int
handle_caps(struct wlup *v, struct stream *s)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    int count;
    int type;
    int length;
    int version;
    int flags;
    int i;

    if (!s_check_rem_and_log(s, 4, "Reading clip capabilities"))
    {
        return 1;
    }
    in_uint16_le(s, count);
    in_uint8s(s, 2);
    for (i = 0; i < count; ++i)
    {
        if (!s_check_rem_and_log(s, 4, "Reading capability set"))
        {
            return 1;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, length);
        length -= 4;
        if (type == CB_CAPSTYPE_GENERAL && length >= 8 &&
                s_check_rem_and_log(s, 8, "Reading general cap set"))
        {
            in_uint32_le(s, version);
            in_uint32_le(s, flags);
            length -= 8;
            if (version > 0 && version < c->capability_version)
            {
                c->capability_version = version;
            }
            c->capability_flags &= flags;
        }
        if (length > 0)
        {
            if (!s_check_rem_and_log(s, length, "cap set padding"))
            {
                return 1;
            }
            in_uint8s(s, length);
        }
    }
    return 0;
}

/******************************************************************************/
static int
process_eclip_pdu(struct wlup *v, struct stream *s)
{
    int type;
    int msg_flags;
    int datalen;

    if (!s_check_rem_and_log(s, 8, "MS-RDPECLIP PDU Header"))
    {
        return 0;
    }
    in_uint16_le(s, type);
    in_uint16_le(s, msg_flags);
    in_uint32_le(s, datalen);
    LOG(LOG_LEVEL_DEBUG, "wlup: clip PDU %s flags %d length %d",
        CB_PDUTYPE_TO_STR(type), msg_flags, datalen);
    if (s_check_rem_and_log(s, datalen, "MS-RDPECLIP PDU"))
    {
        s->end = s->p + datalen;
    }

    switch (type)
    {
        case CB_FORMAT_LIST:
            return handle_format_list(v, s);
        case CB_FORMAT_DATA_REQUEST:
            return handle_format_data_request(v, s);
        case CB_FORMAT_DATA_RESPONSE:
            if (msg_flags == CB_RESPONSE_OK)
            {
                return handle_format_data_response(v, s);
            }
            break;
        case CB_CLIP_CAPS:
            return handle_caps(v, s);
        default:
            break;
    }
    return 0;
}

/******************************************************************************/
int
wlup_clip_process_channel_data(struct wlup *v, char *data, int size,
                               int total_size, int flags)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    int first = (flags & XR_CHANNEL_FLAG_FIRST) != 0;
    int last = (flags & XR_CHANNEL_FLAG_LAST) != 0;
    int rv = 0;

    if (c == NULL || size > total_size)
    {
        return 0;
    }
    if (first && last)
    {
        struct stream s = {0};
        s.data = data;
        s.size = size;
        s.p = data;
        s.end = data + size;
        return process_eclip_pdu(v, &s);
    }
    if (first)
    {
        free_stream(c->dechunker_s);
        make_stream(c->dechunker_s);
        init_stream(c->dechunker_s, total_size);
        if (c->dechunker_s->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "wlup: out of memory dechunking a %d byte "
                "clipboard PDU", total_size);
            free_stream(c->dechunker_s);
            c->dechunker_s = NULL;
            return 0;
        }
    }
    if (c->dechunker_s == NULL ||
            !s_check_rem_out_and_log(c->dechunker_s, size, "wlup dechunker"))
    {
        return 0;
    }
    out_uint8a(c->dechunker_s, data, size);
    if (last)
    {
        s_mark_end(c->dechunker_s);
        c->dechunker_s->p = c->dechunker_s->data;
        rv = process_eclip_pdu(v, c->dechunker_s);
        free_stream(c->dechunker_s);
        c->dechunker_s = NULL;
    }
    return rv;
}

/******************************************************************************/
char *
wlup_clip_read_fd(int fd, int *len)
{
    char *buf = NULL;
    char *bigger;
    int size = 0;
    int used = 0;
    int waited = 0;
    int n;
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;
    while (waited < READ_TIMEOUT_MS)
    {
        if (poll(&pfd, 1, 100) == 0)
        {
            waited += 100;
            continue;
        }
        if (used == size)
        {
            if (size >= MAX_TEXT ||
                    (bigger = (char *)realloc(buf, size == 0 ? 4096 : size * 2))
                    == NULL)
            {
                break;
            }
            buf = bigger;
            size = size == 0 ? 4096 : size * 2;
        }
        n = read(fd, buf + used, size - used);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            break; /* end of the data */
        }
        used += n;
    }
    close(fd);
    if (waited >= READ_TIMEOUT_MS)
    {
        LOG(LOG_LEVEL_WARNING, "wlup: timed out reading the clipboard");
    }
    *len = used;
    return buf;
}

/******************************************************************************/
void
wlup_clip_session_text(struct wlup *v, const char *text, int len)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    int old_len;

    if (c == NULL || v->clip_chanid < 0 || len < 0)
    {
        return;
    }
    old_len = (int)(c->text_s->end - c->text_s->data);
    if (c->text_s->data != NULL && old_len == len &&
            memcmp(c->text_s->data, text, len) == 0)
    {
        return; /* unchanged */
    }
    init_stream(c->text_s, len + 1);
    out_uint8a(c->text_s, text, len);
    s_mark_end(c->text_s);
    LOG(LOG_LEVEL_DEBUG, "wlup: %d bytes of clipboard text from the session",
        len);
    if (c->startup_complete)
    {
        send_format_list(v);
    }
}

/******************************************************************************/
int
wlup_clip_open_channel(struct wlup *v)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;
    struct stream *s;

    v->clip_chanid = v->server_get_channel_id(v, CLIPRDR_SVC_CHANNEL_NAME);
    if (c == NULL || v->clip_chanid < 0)
    {
        LOG(LOG_LEVEL_INFO, "wlup: no clipboard channel");
        return 0;
    }
    LOG(LOG_LEVEL_INFO, "wlup: text clipboard on channel %d", v->clip_chanid);

    c->capability_version = CB_CAPS_VERSION_2;
    c->capability_flags = CB_USE_LONG_FORMAT_NAMES;

    /* Capabilities and Monitor Ready start the exchange; the client
     * answers with its capabilities and format list ([MS-RDPECLIP]
     * 1.3.2.1) */
    make_stream(s);
    init_stream(s, 64);
    out_cliprdr_header(s, CB_CLIP_CAPS, 0);
    out_uint16_le(s, 1);  /* cCapabilitiesSets */
    out_uint16_le(s, 0);  /* pad1 */
    out_uint16_le(s, CB_CAPSTYPE_GENERAL);
    out_uint16_le(s, 12); /* lengthCapability */
    out_uint32_le(s, c->capability_version);
    out_uint32_le(s, c->capability_flags);
    s_mark_end(s);
    send_clip_pdu(v, s);

    init_stream(s, 64);
    out_cliprdr_header(s, CB_MONITOR_READY, 0);
    s_mark_end(s);
    send_clip_pdu(v, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
void
wlup_clip_init(struct wlup *v)
{
    struct wlup_clip *c;

    c = (struct wlup_clip *)g_malloc(sizeof(struct wlup_clip), 1);
    make_stream(c->text_s);
    v->clip = c;
    v->clip_chanid = -1;
}

/******************************************************************************/
void
wlup_clip_exit(struct wlup *v)
{
    struct wlup_clip *c = (struct wlup_clip *)v->clip;

    if (c != NULL)
    {
        free_stream(c->text_s);
        free_stream(c->dechunker_s);
        g_free(c);
        v->clip = NULL;
    }
}
