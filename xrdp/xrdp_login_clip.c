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

#include <stdlib.h>

#include "xrdp.h"
#include "xrdp_login_clip.h"
#include "xrdp_login_lvgl.h"
#include "explicit_bzero.h"
#include "log.h"
#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"
#include "os_calls.h"
#include "scancode.h"
#include "string_calls.h"
#include "xrdp_constants.h"

/* Keysyms for 'v' and 'V'. Defined locally, as xrdp_bitmap.c does for
 * XK_BackSpace */
#define XK_v 0x0076
#define XK_V 0x0056
#define XK_a 0x0061
#define XK_z 0x007a
#define XK_A 0x0041
#define XK_Z 0x005a

/*****************************************************************************/
int
xrdp_login_clip_is_paste_key(const int *keys, int scan_code,
                             const struct xrdp_key_info *ki)
{
    int ctrl;
    int shift;
    int altgr;
    int latin_letter;

    if (keys == NULL)
    {
        return 0;
    }

    ctrl = keys[SCANCODE_INDEX_LCTRL_KEY] || keys[SCANCODE_INDEX_RCTRL_KEY];
    shift = keys[SCANCODE_INDEX_LSHIFT_KEY] || keys[SCANCODE_INDEX_RSHIFT_KEY];
    altgr = keys[SCANCODE_INDEX_RALT_KEY];
    latin_letter = ki != NULL &&
                   ((ki->sym >= XK_A && ki->sym <= XK_Z) ||
                    (ki->sym >= XK_a && ki->sym <= XK_z));

    /* Ctrl+V. AltGr is LCTRL + RALT on Windows clients, so exclude it.
     * V is matched on the keysym in both case foldings, since Caps Lock
     * and Shift select the uppercase keymap table, and on the physical
     * key as a fallback for layouts that never produce a Latin V at all
     * (Cyrillic, Greek, Hebrew, Arabic, Thai, ...). The fallback is not
     * used when the key produces some other Latin letter, so Ctrl+K on
     * Dvorak stays Ctrl+K */
    if (ctrl && !altgr &&
            ((ki != NULL && (ki->sym == XK_v || ki->sym == XK_V)) ||
             (scan_code == SCANCODE_V_KEY && !latin_letter)))
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
/* Applies the paste policy to one decoded codepoint, appending it to out
 * if it is acceptable. Returns 1 if decoding should stop here */
static int
lc_accept_char(char32_t c, char32_t *out, unsigned int *count)
{
    if (c == 0 || c == 0x0d || c == 0x0a)
    {
        /* End of string, or end of the first line */
        return 1;
    }

    /* UCS_REPLACEMENT_CHARACTER is what the decoder makes of input it
     * cannot decode, such as an unpaired surrogate. Drop it */
    if (!is_control_char(c) && c != UCS_REPLACEMENT_CHARACTER)
    {
        out[*count] = c;
        (*count)++;
    }
    return 0;
}

/*****************************************************************************/
unsigned int
xrdp_login_clip_decode_text(struct stream *s, int format,
                            char32_t *out, unsigned int out_count)
{
    unsigned int count = 0;

    if (s == NULL || out == NULL)
    {
        return 0;
    }

    if (format == CF_UNICODETEXT)
    {
        unsigned int utf8_size = in_utf16_le_terminated_as_utf8_length(s);
        char *utf8 = (char *)malloc(utf8_size);
        const char *p;
        unsigned int len;

        if (utf8 == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Login clipboard: out of memory");
            return 0;
        }
        in_utf16_le_terminated_as_utf8(s, utf8, utf8_size);

        p = utf8;
        len = utf8_size - 1; /* Without the terminator */
        while (len > 0 && count < out_count)
        {
            if (lc_accept_char(utf8_get_next_char(&p, &len), out, &count))
            {
                break;
            }
        }

        /* The UTF-8 copy can hold the user's password */
        explicit_bzero(utf8, utf8_size);
        free(utf8);
    }
    else
    {
        /* CF_TEXT - treat the bytes as CP-1252, which agrees with
         * ISO-8859-1 for everything we accept */
        while (s_check_rem(s, 1) && count < out_count)
        {
            unsigned char c;

            in_uint8(s, c);
            if (lc_accept_char(c, out, &count))
            {
                break;
            }
        }
    }

    return count;
}

/* Edit widget caption buffer size. Matches the caption1 allocation in
 * xrdp_wm_show_edits() and the limit used by xrdp_bitmap_def_proc() */
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
int
xrdp_login_clip_parse_format_list(struct stream *s, int use_long_names,
                                  int *have_unicode_text, int *have_text)
{
    int rv = 0;

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
            rv = 1;
            break;
        }
        in_uint32_le(s, format_id);

        /* Skip the format name. We never use it, but it has to be
         * stepped over by exactly the right width */
        if (use_long_names)
        {
            /* NUL-terminated UTF-16 string. Decoding stops after the
             * terminator, or at the end of the stream without one */
            const char *name = s->p;

            in_utf16_le_terminated_as_utf8(s, NULL, 0);
            if (s->p - name < 2 || s->p[-2] != 0 || s->p[-1] != 0)
            {
                LOG(LOG_LEVEL_WARNING, "Login clipboard: unterminated "
                    "format name in format list");
                rv = 1;
                break;
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
                rv = 1;
                break;
            }
            in_uint8s(s, LC_SHORT_FORMAT_NAME_LEN);
        }

        if (format_id == CF_UNICODETEXT)
        {
            *have_unicode_text = 1;
        }
        else if (format_id == CF_TEXT)
        {
            *have_text = 1;
        }
    }

    if (rv != 0)
    {
        /* A rejected list offers nothing */
        *have_unicode_text = 0;
        *have_text = 0;
    }

    return rv;
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
    /* Edit field the outstanding request will paste into, and
     * wm->login_wnd_gen at request time. Compared, never dereferenced */
    const void *target;
    unsigned int target_gen;
    /* Requests abandoned while still in flight. Responses arrive in
     * request order, so this many responses are not for the live
     * request and are discarded */
    int abandoned;
    /* Set when any data response arrives after the live request was
     * sent. See xrdp_login_clip_request_paste() for why */
    int response_seen;

    /* PDU reassembly */
    char *frag;                 /* LC_MAX_PDU_SIZE bytes */
    int frag_len;
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
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: truncated caps");
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
            LOG(LOG_LEVEL_DEBUG, "Login clipboard: truncated caps");
            return;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, length);

        if (length < 4 || !s_check_rem(s, length - 4))
        {
            LOG(LOG_LEVEL_DEBUG, "Login clipboard: bad caps set length");
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
/* Returns the edit field a paste would go into right now, or NULL if
 * there is nowhere suitable.
 *
 * For the legacy login window, focused_control is a stored pointer that
 * nothing clears when a widget is deleted, so it is validated - not
 * trusted - by confirming it is still one of the login window's
 * children before it is touched at all. */
static const void *
lc_get_target(struct xrdp_wm *wm)
{
    struct xrdp_bitmap *wnd;
    struct xrdp_bitmap *edit;

    if (wm == NULL || wm->login_state != WMLS_USER_PROMPT)
    {
        return NULL;
    }

    if (wm->login_ui != NULL)
    {
        return xrdp_login_lvgl_get_paste_target(wm);
    }

    /* A covering window means the user cannot see where the text would
     * land */
    wnd = wm->login_window;
    if (wnd == NULL || wnd->modal_dialog != NULL ||
            wm->popup_wnd != NULL || wm->log_wnd != NULL)
    {
        return NULL;
    }

    edit = wnd->focused_control;
    if (edit == NULL || wnd->child_list == NULL ||
            list_index_of(wnd->child_list, (tintptr)edit) < 0)
    {
        return NULL;
    }

    if (edit->type != WND_TYPE_EDIT)
    {
        return NULL;
    }

    return edit;
}

/*****************************************************************************/
/* Inserts pasted text into the edit field the request was made for.
 *
 * The field focused now must be the one focused at request time, and no
 * edit field may have been created or destroyed in between, so a field
 * rebuilt at the same address does not receive text meant for another */
static void
lc_insert_text(struct xrdp_login_clip *self, struct stream *s,
               const void *target, unsigned int target_gen)
{
    struct xrdp_wm *wm;
    const void *current;
    char32_t cp[LC_EDIT_CAPTION_SIZE];
    unsigned int count;
    unsigned int inserted;

    wm = self->wm;
    current = lc_get_target(wm);
    if (current == NULL || current != target ||
            wm->login_wnd_gen != target_gen)
    {
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: paste target has gone, "
            "discarding the response");
        return;
    }

    count = xrdp_login_clip_decode_text(s, self->requested_format,
                                        cp, LC_EDIT_CAPTION_SIZE);

    if (wm->login_ui != NULL)
    {
        inserted = xrdp_login_lvgl_insert_text(wm, cp, count);
    }
    else
    {
        struct xrdp_bitmap *edit = wm->login_window->focused_control;

        inserted = xrdp_login_clip_insert_codepoints(edit, cp, count);
        if (inserted > 0)
        {
            xrdp_bitmap_invalidate(edit, 0);
        }
    }
    LOG(LOG_LEVEL_DEBUG, "Login clipboard: inserted %u of %u characters",
        inserted, count);

    /* Clear the decoded password from the stack */
    explicit_bzero(cp, sizeof(cp));
}

/*****************************************************************************/
/* Gives up on the outstanding request, if any. Its response is still on
 * its way and will be discarded when it arrives */
static void
lc_abandon_request(struct xrdp_login_clip *self)
{
    if (self->state == LC_WAIT_DATA)
    {
        self->abandoned++;
        self->state = LC_READY;
    }
    self->request_ms = 0;
    self->target = NULL;
}

/*****************************************************************************/
/* Accounts for a CB_FORMAT_DATA_RESPONSE, whether or not it can be read.
 * Returns 1 if it answers the live request, which is then complete, with
 * its target in *target; 0 if it is to be discarded */
static int
lc_response_arrived(struct xrdp_login_clip *self, const void **target)
{
    self->response_seen = 1;

    if (self->abandoned > 0)
    {
        /* The response to a request we gave up on */
        self->abandoned--;
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: discarding the "
            "response to an abandoned request");
        return 0;
    }

    if (self->state != LC_WAIT_DATA)
    {
        /* Not ours - ignore it rather than inserting it */
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: ignoring unsolicited "
            "format data response");
        return 0;
    }

    *target = self->target;
    self->state = LC_READY;
    self->target = NULL;
    return 1;
}

/*****************************************************************************/
/* Accounts for a PDU too big to reassemble, from its first bytes. A data
 * response still has to be matched to its request, or every later
 * response would be matched to the wrong one */
static void
lc_drop_pdu(struct xrdp_login_clip *self, const char *start, int len)
{
    const void *target;

    LOG(LOG_LEVEL_DEBUG, "Login clipboard: discarding a PDU over the "
        "%d byte limit", LC_MAX_PDU_SIZE);

    if (len >= 2 &&
            ((unsigned char)start[0] | ((unsigned char)start[1] << 8)) ==
            CB_FORMAT_DATA_RESPONSE)
    {
        if (lc_response_arrived(self, &target))
        {
            self->request_ms = 0;
        }
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
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: truncated PDU header");
        return;
    }
    in_uint16_le(s, msg_type);
    in_uint16_le(s, msg_flags);
    in_uint32_le(s, data_len);

    LOG(LOG_LEVEL_DEBUG, "Login clipboard: received %s, flags %d, %d bytes",
        CB_PDUTYPE_TO_STR(msg_type), msg_flags, data_len);

    if (data_len >= 0 && data_len <= s_rem(s))
    {
        /* Bring the PDU framing (dataLen) into agreement with the
         * transport framing (the reassembled length), so trailing bytes
         * beyond the advertised length are never parsed as more data */
        s->end = s->p + data_len;
    }

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
            if (xrdp_login_clip_parse_format_list(s, self->client_long_names,
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
            /* A new format list can arrive at any time. The clipboard has
             * changed under any outstanding request, so give up on it */
            lc_abandon_request(self);
            self->state = LC_READY;
            break;

        case CB_FORMAT_DATA_RESPONSE:
        {
            const void *target;

            if (lc_response_arrived(self, &target))
            {
                /* LC_REQUEST_TIMEOUT_MS already bounds how long a request
                 * is left outstanding on the send side (see
                 * xrdp_login_clip_request_paste()). Apply it here too, so
                 * a response that arrives long after the keypress that
                 * asked for it cannot be spliced into whatever field
                 * happens to be focused by then */
                unsigned int elapsed = g_get_elapsed_ms() - self->request_ms;

                self->request_ms = 0;
                if (elapsed >= LC_REQUEST_TIMEOUT_MS)
                {
                    LOG(LOG_LEVEL_DEBUG, "Login clipboard: ignoring format "
                        "data response %u ms after the request, past the "
                        "%d ms timeout", elapsed, LC_REQUEST_TIMEOUT_MS);
                }
                else if ((msg_flags & CB_RESPONSE_OK) != 0)
                {
                    lc_insert_text(self, s, target, self->target_gen);
                }
            }
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

    /* Not g_new0() / g_malloc(), which exit on failure. Running out of
     * memory here only costs the login screen its paste support */
    self = (struct xrdp_login_clip *)calloc(1, sizeof(*self));
    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Login clipboard: out of memory");
        return NULL;
    }
    self->frag = (char *)calloc(1, LC_MAX_PDU_SIZE);
    if (self->frag == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Login clipboard: out of memory");
        free(self);
        return NULL;
    }
    self->wm = wm;
    self->chan_id = chan_id;
    self->state = LC_WAIT_CAPS;

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
        explicit_bzero(self->frag, LC_MAX_PDU_SIZE);
        free(self->frag);
    }
    g_memset(self, 0, sizeof(*self));
    free(self);
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
        /* Any unfinished PDU is abandoned. Do not leave it lying in the
         * buffer */
        explicit_bzero(self->frag, self->frag_len);
        self->frag_len = 0;
        self->frag_dropping = 0;

        if (total_len > LC_MAX_PDU_SIZE || total_len < 0)
        {
            lc_drop_pdu(self, data, len);
            self->frag_dropping = (flags & XR_CHANNEL_FLAG_LAST) == 0;
            return 0;
        }
    }

    if (self->frag_dropping)
    {
        if ((flags & XR_CHANNEL_FLAG_LAST) != 0)
        {
            /* The dropped PDU has ended - a drop is strictly per-PDU */
            self->frag_dropping = 0;
        }
        return 0;
    }

    if (self->frag_len + len > LC_MAX_PDU_SIZE)
    {
        lc_drop_pdu(self, self->frag, self->frag_len);
        /* Do not leave clipboard content already copied for this PDU
         * lying in the buffer */
        explicit_bzero(self->frag, self->frag_len);
        self->frag_len = 0;
        self->frag_dropping = (flags & XR_CHANNEL_FLAG_LAST) == 0;
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
    explicit_bzero(self->frag, self->frag_len);
    self->frag_len = 0;

    return 0;
}

/*****************************************************************************/
int
xrdp_login_clip_request_paste(struct xrdp_login_clip *self)
{
    char body[4];
    int format;
    const void *target;

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
        if (self->response_seen)
        {
            /* Responses are arriving, but none was taken for this request:
             * a client that never answered an abandoned request has put
             * the count out by one, and this request's response was
             * discarded in its place. Start counting afresh */
            LOG(LOG_LEVEL_DEBUG, "Login clipboard: previous request "
                "unanswered after %u ms, resetting", elapsed);
            self->abandoned = 0;
            self->state = LC_READY;
        }
        else
        {
            /* The client may still answer. Expect that */
            LOG(LOG_LEVEL_DEBUG, "Login clipboard: previous request "
                "abandoned after %u ms", elapsed);
            lc_abandon_request(self);
        }
    }
    else if (self->state != LC_READY)
    {
        /* Handshake has not finished. Nothing to ask, and nowhere to ask */
        return 0;
    }

    target = lc_get_target(self->wm);
    if (target == NULL)
    {
        /* Do not read the client's clipboard with nowhere to put it */
        LOG(LOG_LEVEL_DEBUG, "Login clipboard: no edit field to paste into");
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
    self->target = target;
    self->target_gen = self->wm->login_wnd_gen;
    self->response_seen = 0;

    return 1;
}

/*****************************************************************************/
void
xrdp_login_clip_abandon_paste(struct xrdp_login_clip *self)
{
    if (self != NULL)
    {
        lc_abandon_request(self);
    }
}
