/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2026
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
 * Clipboard paste support for the xrdp login screen
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "xrdp_login_clip.h"
#include "log.h"
#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"
#include "os_calls.h"
#include "scancode.h"
#include "string_calls.h"
#include "xrdp_constants.h"

/* Keysym for 'v'. Defined locally, as xrdp_bitmap.c does for XK_BackSpace */
#define XK_v 0x0076

/*****************************************************************************/
int
xrdp_login_clip_is_paste_key(const int *keys, int scan_code,
                             const struct xrdp_key_info *ki)
{
    int ctrl;
    int shift;
    int altgr;

    if (keys == NULL)
    {
        return 0;
    }

    ctrl = keys[SCANCODE_INDEX_LCTRL_KEY] || keys[SCANCODE_INDEX_RCTRL_KEY];
    shift = keys[SCANCODE_INDEX_LSHIFT_KEY] || keys[SCANCODE_INDEX_RSHIFT_KEY];
    altgr = keys[SCANCODE_INDEX_RALT_KEY];

    /* Ctrl+V. AltGr is LCTRL + RALT on Windows clients, so exclude it */
    if (ctrl && !altgr && ki != NULL && ki->sym == XK_v)
    {
        return 1;
    }

    /* Shift+Insert */
    if (shift && !ctrl && !altgr && scan_code == SCANCODE_INSERT_KEY)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* Is this codepoint a control character we refuse to insert? */
static int
is_control_char(char32_t c)
{
    return c < 0x20 || c == 0x7f || (c >= 0x80 && c <= 0x9f);
}

/*****************************************************************************/
unsigned int
xrdp_login_clip_utf16_to_codepoints(const char *utf16le, unsigned int bytes,
                                    char32_t *out, unsigned int out_count)
{
    unsigned int in_pos = 0;
    unsigned int out_pos = 0;

    if (utf16le == NULL || out == NULL)
    {
        return 0;
    }

    while (in_pos + 1 < bytes && out_pos < out_count)
    {
        char32_t c = (unsigned char)utf16le[in_pos] |
                     ((char32_t)(unsigned char)utf16le[in_pos + 1] << 8);
        in_pos += 2;

        if (c == 0 || c == 0x0d || c == 0x0a)
        {
            /* End of string, or end of the first line */
            break;
        }

        if (c >= 0xd800 && c <= 0xdbff)
        {
            /* High surrogate - needs a low surrogate to follow */
            char32_t lo = 0;

            if (in_pos + 1 < bytes)
            {
                lo = (unsigned char)utf16le[in_pos] |
                     ((char32_t)(unsigned char)utf16le[in_pos + 1] << 8);
            }

            if (lo >= 0xdc00 && lo <= 0xdfff)
            {
                in_pos += 2;
                c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
            }
            else
            {
                /* Unpaired high surrogate - drop it */
                continue;
            }
        }
        else if (c >= 0xdc00 && c <= 0xdfff)
        {
            /* Unpaired low surrogate - drop it */
            continue;
        }

        if (is_control_char(c))
        {
            continue;
        }

        out[out_pos] = c;
        out_pos++;
    }

    return out_pos;
}

/* Edit widget caption buffer size. Matches the allocation at
 * xrdp_login_wnd.c:457 and the hardcoded limit at xrdp_bitmap.c:1260 */
#define LC_EDIT_CAPTION_SIZE 256

/*****************************************************************************/
unsigned int
xrdp_login_clip_insert_codepoints(struct xrdp_bitmap *edit,
                                  const char32_t *cp, unsigned int count)
{
    unsigned int index;

    if (edit == NULL || cp == NULL || edit->caption1 == NULL ||
            edit->type != WND_TYPE_EDIT)
    {
        return 0;
    }

    for (index = 0; index < count; index++)
    {
        if (!utf8_add_char_at(edit->caption1, LC_EDIT_CAPTION_SIZE,
                              cp[index], edit->edit_pos))
        {
            /* Field is full */
            break;
        }
        edit->edit_pos++;
    }

    return index;
}

/* Width of a short format name field - [MS-RDPECLIP] 2.2.3.1.1 */
#define LC_SHORT_FORMAT_NAME_LEN 32

/*****************************************************************************/
/* Note: CB_ASCII_NAMES in msg_flags changes the encoding of short format
 * names but not the width of the field, and we never read the names, so
 * the flag does not affect parsing. The parameter is kept so the call
 * site reads correctly against [MS-RDPECLIP] 2.2.3.1. */
int
xrdp_login_clip_parse_format_list(struct stream *s, int msg_flags,
                                  int use_long_names,
                                  int *have_unicode_text, int *have_text)
{
    if (have_unicode_text != NULL)
    {
        *have_unicode_text = 0;
    }
    if (have_text != NULL)
    {
        *have_text = 0;
    }

    if (s == NULL || have_unicode_text == NULL || have_text == NULL)
    {
        return 1;
    }

    while (s_rem(s) > 0)
    {
        int format_id;

        if (!s_check_rem(s, 4))
        {
            LOG(LOG_LEVEL_WARNING, "Login clipboard: truncated format list");
            return 1;
        }
        in_uint32_le(s, format_id);

        /* Skip the format name. We never use it, but it has to be
         * stepped over by exactly the right width */
        if (use_long_names)
        {
            /* NUL-terminated UTF-16 string */
            int word = -1;

            while (word != 0)
            {
                if (!s_check_rem(s, 2))
                {
                    LOG(LOG_LEVEL_WARNING, "Login clipboard: unterminated "
                        "format name in format list");
                    return 1;
                }
                in_uint16_le(s, word);
            }
        }
        else
        {
            /* Fixed-width field, the same width whether the names are
             * ASCII or UTF-16 */
            if (!s_check_rem(s, LC_SHORT_FORMAT_NAME_LEN))
            {
                LOG(LOG_LEVEL_WARNING, "Login clipboard: truncated short "
                    "format name in format list");
                return 1;
            }
            in_uint8s(s, LC_SHORT_FORMAT_NAME_LEN);
        }

        /* Only set the flag after the entry has been fully validated */
        if (format_id == CF_UNICODETEXT)
        {
            *have_unicode_text = 1;
        }
        else if (format_id == CF_TEXT)
        {
            *have_text = 1;
        }
    }

    return 0;
}

/* Largest cliprdr PDU we will reassemble. We only ever need a couple of
 * hundred bytes; anything larger is not a password */
#define LC_MAX_PDU_SIZE 16384

/* An outstanding data request older than this is considered abandoned */
#define LC_REQUEST_TIMEOUT_MS 5000

enum lc_state
{
    LC_WAIT_CAPS = 0,
    LC_WAIT_FORMATS,
    LC_READY,
    LC_WAIT_DATA
};

struct xrdp_login_clip
{
    struct xrdp_wm *wm;
    int chan_id;
    enum lc_state state;
    int client_long_names;
    int have_unicode_text;
    int have_text;
    int requested_format;
    unsigned int request_ms;    /* g_get_elapsed_ms() at request time */

    /* PDU reassembly */
    char *frag;                 /* LC_MAX_PDU_SIZE bytes */
    int frag_len;
    int frag_total;
    int frag_dropping;
};

/*****************************************************************************/
/* Sends one complete cliprdr PDU. The stream must hold the full PDU
 * including its 8-byte header */
static void
lc_send_pdu(struct xrdp_login_clip *self, int msg_type, int msg_flags,
            const char *body, int body_len)
{
    struct stream *s;
    int total;

    total = 8 + body_len;
    make_stream(s);
    init_stream(s, total);
    out_uint16_le(s, msg_type);
    out_uint16_le(s, msg_flags);
    out_uint32_le(s, body_len);
    if (body_len > 0)
    {
        out_uint8a(s, body, body_len);
    }
    s_mark_end(s);

    LOG(LOG_LEVEL_DEBUG, "Login clipboard: sending %s, %d bytes",
        CB_PDUTYPE_TO_STR(msg_type), body_len);

    /* Our PDUs are far smaller than CHANNEL_CHUNK_LENGTH, so a single
     * first-and-last chunk always suffices */
    libxrdp_send_to_channel(self->wm->session, self->chan_id,
                            s->data, total, total,
                            XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST);
    free_stream(s);
}

/*****************************************************************************/
/* Handles the client's CB_CLIP_CAPS - [MS-RDPECLIP] 2.2.2.1 */
static void
lc_handle_caps(struct xrdp_login_clip *self, struct stream *s)
{
    int set_count;
    int index;

    if (!s_check_rem(s, 4))
    {
        return;
    }
    in_uint16_le(s, set_count);
    in_uint8s(s, 2); /* pad1 */

    for (index = 0; index < set_count; index++)
    {
        int type;
        int length;

        if (!s_check_rem(s, 4))
        {
            return;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, length);

        if (length < 4 || !s_check_rem(s, length - 4))
        {
            return;
        }

        if (type == CB_CAPSTYPE_GENERAL && length >= 12)
        {
            int version;
            int general_flags;

            in_uint32_le(s, version);
            in_uint32_le(s, general_flags);
            in_uint8s(s, length - 12);
            self->client_long_names =
                (general_flags & CB_USE_LONG_FORMAT_NAMES) != 0;
            LOG(LOG_LEVEL_DEBUG, "Login clipboard: client caps version %d, "
                "long format names %d", version, self->client_long_names);
        }
        else
        {
            in_uint8s(s, length - 4);
        }
    }
}

/*****************************************************************************/
/* Inserts pasted text into the focused login screen edit field.
 *
 * The target is re-derived here rather than remembered at request time:
 * a response arrives asynchronously, and a stored widget pointer could
 * outlive the widget. */
static void
lc_insert_text(struct xrdp_login_clip *self, struct stream *s)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *edit;
    char32_t cp[LC_EDIT_CAPTION_SIZE];
    unsigned int count;
    unsigned int inserted;
    int rem;

    wm = self->wm;
    if (wm == NULL || wm->login_window == NULL ||
            wm->popup_wnd != NULL || wm->log_wnd != NULL)
    {
        return;
    }

    edit = wm->login_window->focused_control;
    if (edit == NULL || edit->type != WND_TYPE_EDIT)
    {
        return;
    }

    rem = s_rem(s);
    if (rem <= 0)
    {
        return;
    }

    if (self->requested_format == CF_UNICODETEXT)
    {
        count = xrdp_login_clip_utf16_to_codepoints(s->p, (unsigned int)rem,
                cp, LC_EDIT_CAPTION_SIZE);
    }
    else
    {
        /* CF_TEXT - treat the bytes as CP-1252, which agrees with
         * ISO-8859-1 for everything we accept */
        int index;

        count = 0;
        for (index = 0; index < rem && count < LC_EDIT_CAPTION_SIZE; index++)
        {
            char32_t c = (unsigned char)s->p[index];

            if (c == 0 || c == 0x0d || c == 0x0a)
            {
                break;
            }
            if (is_control_char(c))
            {
                continue;
            }
            cp[count] = c;
            count++;
        }
    }

    inserted = xrdp_login_clip_insert_codepoints(edit, cp, count);
    LOG(LOG_LEVEL_DEBUG, "Login clipboard: inserted %u of %u characters",
        inserted, count);

    /* Clear the decoded password from the stack */
    g_memset(cp, 0, sizeof(cp));

    if (inserted > 0)
    {
        xrdp_bitmap_invalidate(edit, 0);
    }
}

/*****************************************************************************/
/* Dispatches one complete, reassembled cliprdr PDU */
static void
lc_handle_pdu(struct xrdp_login_clip *self, struct stream *s)
{
    int msg_type;
    int msg_flags;
    int data_len;

    if (!s_check_rem(s, 8))
    {
        return;
    }
    in_uint16_le(s, msg_type);
    in_uint16_le(s, msg_flags);
    in_uint32_le(s, data_len);

    LOG(LOG_LEVEL_DEBUG, "Login clipboard: received %s, flags %d, %d bytes",
        CB_PDUTYPE_TO_STR(msg_type), msg_flags, data_len);

    switch (msg_type)
    {
        case CB_CLIP_CAPS:
            lc_handle_caps(self, s);
            if (self->state == LC_WAIT_CAPS)
            {
                self->state = LC_WAIT_FORMATS;
            }
            break;

        case CB_FORMAT_LIST:
            if (xrdp_login_clip_parse_format_list(s, msg_flags,
                                                  self->client_long_names,
                                                  &self->have_unicode_text,
                                                  &self->have_text) == 0)
            {
                lc_send_pdu(self, CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK,
                            NULL, 0);
            }
            else
            {
                lc_send_pdu(self, CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_FAIL,
                            NULL, 0);
            }
            /* A new format list can arrive at any time. It always leaves
             * us ready and with no request outstanding */
            self->state = LC_READY;
            self->request_ms = 0;
            break;

        case CB_FORMAT_DATA_RESPONSE:
            if (self->state != LC_WAIT_DATA)
            {
                /* Not ours - ignore it rather than inserting it */
                LOG(LOG_LEVEL_DEBUG, "Login clipboard: ignoring unsolicited "
                    "format data response");
                break;
            }
            self->state = LC_READY;
            self->request_ms = 0;
            if ((msg_flags & CB_RESPONSE_OK) != 0)
            {
                lc_insert_text(self, s);
            }
            break;

        default:
            /* Everything else, including all file transfer and clipboard
             * locking PDUs, is of no interest to a login screen */
            break;
    }
}

/*****************************************************************************/
struct xrdp_login_clip *
xrdp_login_clip_create(struct xrdp_wm *wm, int chan_id)
{
    struct xrdp_login_clip *self;
    char body[16];

    if (wm == NULL)
    {
        return NULL;
    }

    self = g_new0(struct xrdp_login_clip, 1);
    self->wm = wm;
    self->chan_id = chan_id;
    self->state = LC_WAIT_CAPS;
    self->frag = (char *)g_malloc(LC_MAX_PDU_SIZE, 1);

    LOG(LOG_LEVEL_INFO, "Login screen clipboard paste enabled on "
        "channel id %d", chan_id);

    /* CLIPRDR_CAPS with one CLIPRDR_GENERAL_CAPABILITY set.
     * [MS-RDPECLIP] 2.2.2.1 */
    g_memset(body, 0, sizeof(body));
    body[0] = 1;                            /* cCapabilitiesSets */
    body[4] = CB_CAPSTYPE_GENERAL;          /* capabilitySetType */
    body[6] = 12;                           /* lengthCapability */
    body[8] = CB_CAPS_VERSION_2;            /* version */
    body[12] = CB_USE_LONG_FORMAT_NAMES;    /* generalFlags */
    lc_send_pdu(self, CB_CLIP_CAPS, 0, body, 16);

    /* Tell the client it may start. Until this is sent, a conforming
     * client sends us nothing at all */
    lc_send_pdu(self, CB_MONITOR_READY, 0, NULL, 0);

    return self;
}

/*****************************************************************************/
void
xrdp_login_clip_delete(struct xrdp_login_clip *self)
{
    if (self == NULL)
    {
        return;
    }

    if (self->frag != NULL)
    {
        /* The fragment buffer can hold the user's password */
        g_memset(self->frag, 0, LC_MAX_PDU_SIZE);
        g_free(self->frag);
    }
    g_memset(self, 0, sizeof(*self));
    g_free(self);
}

/*****************************************************************************/
int
xrdp_login_clip_process_channel_data(struct xrdp_login_clip *self,
                                     int flags, const char *data,
                                     int len, int total_len)
{
    struct stream stream_obj;
    struct stream *s;

    if (self == NULL || data == NULL || len < 0)
    {
        return 0;
    }

    if ((flags & XR_CHANNEL_FLAG_FIRST) != 0)
    {
        self->frag_len = 0;
        self->frag_total = total_len;
        self->frag_dropping = 0;

        if (total_len > LC_MAX_PDU_SIZE || total_len < 0)
        {
            LOG(LOG_LEVEL_WARNING, "Login clipboard: discarding %d byte PDU, "
                "over the %d byte limit", total_len, LC_MAX_PDU_SIZE);
            self->frag_dropping = 1;
        }
    }

    if (self->frag_dropping)
    {
        return 0;
    }

    if (self->frag_len + len > LC_MAX_PDU_SIZE)
    {
        LOG(LOG_LEVEL_WARNING, "Login clipboard: PDU longer than advertised, "
            "discarding");
        self->frag_dropping = 1;
        return 0;
    }

    g_memcpy(self->frag + self->frag_len, data, len);
    self->frag_len += len;

    if ((flags & XR_CHANNEL_FLAG_LAST) == 0)
    {
        /* More chunks to come */
        return 0;
    }

    /* Wrap the reassembled bytes in a stream for parsing. No copy, and
     * no allocation driven by a client-supplied length */
    g_memset(&stream_obj, 0, sizeof(stream_obj));
    s = &stream_obj;
    s->data = self->frag;
    s->p = self->frag;
    s->end = self->frag + self->frag_len;
    s->size = self->frag_len;

    lc_handle_pdu(self, s);

    /* Do not leave clipboard content lying in the buffer */
    g_memset(self->frag, 0, self->frag_len);
    self->frag_len = 0;

    return 0;
}

/*****************************************************************************/
int
xrdp_login_clip_request_paste(struct xrdp_login_clip *self)
{
    char body[4];
    int format;

    if (self == NULL)
    {
        return 0;
    }

    if (self->state == LC_WAIT_DATA)
    {
        unsigned int elapsed = g_get_elapsed_ms() - self->request_ms;

        if (elapsed < LC_REQUEST_TIMEOUT_MS)
        {
            /* One request at a time */
            return 0;
        }
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: previous request abandoned "
            "after %u ms", elapsed);
    }
    else if (self->state != LC_READY)
    {
        /* Handshake has not finished. Nothing to ask, and nowhere to ask */
        return 0;
    }

    if (self->have_unicode_text)
    {
        format = CF_UNICODETEXT;
    }
    else if (self->have_text)
    {
        format = CF_TEXT;
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: client offers no text format");
        return 0;
    }

    g_memset(body, 0, sizeof(body));
    body[0] = (char)(format & 0xff);
    body[1] = (char)((format >> 8) & 0xff);
    lc_send_pdu(self, CB_FORMAT_DATA_REQUEST, 0, body, 4);

    self->requested_format = format;
    self->state = LC_WAIT_DATA;
    self->request_ms = g_get_elapsed_ms();

    return 1;
}
