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
 * Tests for the login screen clipboard module
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "xrdp.h"
#include "xrdp_login_clip.h"
#include "scancode.h"
#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"

#include "test_xrdp.h"

#define XK_v 0x0076
#define XK_V 0x0056

/* Key state array as held by struct xrdp_wm */
static int g_keys[SCANCODE_MAX_INDEX + 1];

static void
clear_keys(void)
{
    g_memset(g_keys, 0, sizeof(g_keys));
}

/******************************************************************************/
START_TEST(test_is_paste_key__ctrl_v)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 1);

    clear_keys();
    g_keys[SCANCODE_INDEX_RCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 1);
}
END_TEST

/******************************************************************************/
/* Windows clients send AltGr as LCTRL + RALT. AltGr+V must not paste */
START_TEST(test_is_paste_key__altgr_v_is_not_paste)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    g_keys[SCANCODE_INDEX_RALT_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 0);
}
END_TEST

/******************************************************************************/
/* Caps Lock and Shift select the uppercase keymap table, so V comes
 * through as keysym XK_V (0x0056), not XK_v. That must still paste */
START_TEST(test_is_paste_key__ctrl_v_uppercase_keysym)
{
    struct xrdp_key_info ki = { XK_V, 'V' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_V_KEY,
                     &ki), 1);
}
END_TEST

/******************************************************************************/
/* On a non-Latin layout (here, Russian) the V key never produces a Latin
 * v/V keysym at all - scancode 0x2f maps to Cyrillic em on every one of
 * that layout's tables. The physical-key fallback must still paste */
START_TEST(test_is_paste_key__ctrl_v_non_latin_layout)
{
    /* Cyrillic_em, as instfiles/km-00000419.toml maps scancode 0x2f */
    struct xrdp_key_info ki = { 0x6cd, 0x043c };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_V_KEY,
                     &ki), 1);
}
END_TEST

/******************************************************************************/
/* The AltGr guard must not be bypassed by the new uppercase keysym arm */
START_TEST(test_is_paste_key__altgr_v_uppercase_is_not_paste)
{
    struct xrdp_key_info ki = { XK_V, 'V' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    g_keys[SCANCODE_INDEX_RALT_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_V_KEY,
                     &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__plain_v_is_not_paste)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__ctrl_other_key_is_not_paste)
{
    struct xrdp_key_info ki = { 0x0063, 'c' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2e, &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__shift_insert)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LSHIFT_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_INSERT_KEY,
                     NULL), 1);

    clear_keys();
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_INSERT_KEY,
                     NULL), 0);
}
END_TEST

/******************************************************************************/
/* A NULL key info must never be dereferenced. Uses a scan code other than
 * SCANCODE_V_KEY, since that one is now legitimately a paste on its own
 * via the physical-key fallback, NULL key info or not - see
 * test_is_paste_key__ctrl_v_non_latin_layout for that case */
START_TEST(test_is_paste_key__null_key_info)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2e, NULL), 0);
}
END_TEST

/******************************************************************************/
/* The physical-key fallback does not need key info at all - it must
 * neither dereference a NULL ki nor fail to recognise the paste */
START_TEST(test_is_paste_key__scancode_fallback_with_null_key_info)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_V_KEY,
                     NULL), 1);
}
END_TEST

/******************************************************************************/
/* Builds a UTF-16LE byte buffer from a list of 16-bit words */
static unsigned int
make_utf16(char *buf, const unsigned short *words, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        buf[i * 2] = (char)(words[i] & 0xff);
        buf[i * 2 + 1] = (char)((words[i] >> 8) & 0xff);
    }
    return count * 2;
}

/******************************************************************************/
START_TEST(test_utf16__plain_ascii)
{
    static const unsigned short words[] = { 'p', 'a', 's', 's', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 5);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 16);

    ck_assert_uint_eq(n, 4);
    ck_assert_uint_eq(out[0], 'p');
    ck_assert_uint_eq(out[3], 's');
}
END_TEST

/******************************************************************************/
/* A password manager typically appends a newline. Stop at the first one */
START_TEST(test_utf16__stops_at_crlf)
{
    static const unsigned short words[] = { 'a', 'b', 0x000d, 0x000a, 'c', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 6);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
START_TEST(test_utf16__drops_control_chars)
{
    static const unsigned short words[] = { 'a', 0x0009, 0x007f, 0x0085, 'b', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 6);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[0], 'a');
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
/* U+1F600 GRINNING FACE, as a surrogate pair */
START_TEST(test_utf16__decodes_surrogate_pair)
{
    static const unsigned short words[] = { 0xd83d, 0xde00, 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 3);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 16);

    ck_assert_uint_eq(n, 1);
    ck_assert_uint_eq(out[0], 0x1f600);
}
END_TEST

/******************************************************************************/
START_TEST(test_utf16__drops_unpaired_surrogates)
{
    /* High surrogate with no low surrogate, then a low surrogate alone */
    static const unsigned short words[] = { 0xd83d, 'a', 0xde00, 'b', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 5);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[0], 'a');
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
START_TEST(test_utf16__respects_out_capacity)
{
    static const unsigned short words[] = { 'a', 'b', 'c', 'd', 0 };
    char buf[32];
    char32_t out[2];
    unsigned int len = make_utf16(buf, words, 5);
    unsigned int n = xrdp_login_clip_utf16_to_codepoints(buf, len, out, 2);

    ck_assert_uint_eq(n, 2);
}
END_TEST

/******************************************************************************/
/* An odd trailing byte must not be read as half a word */
START_TEST(test_utf16__odd_length_and_empty)
{
    char buf[4] = { 'a', 0, 'b' };
    char32_t out[16];

    ck_assert_uint_eq(xrdp_login_clip_utf16_to_codepoints(buf, 3, out, 16), 1);
    ck_assert_uint_eq(xrdp_login_clip_utf16_to_codepoints(buf, 0, out, 16), 0);
    ck_assert_uint_eq(xrdp_login_clip_utf16_to_codepoints(NULL, 8, out, 16), 0);
}
END_TEST

/******************************************************************************/
/* Builds a minimal edit widget, as xrdp_login_wnd.c:457 does */
static void
setup_edit(struct xrdp_bitmap *edit, char *caption, const char *initial)
{
    g_memset(edit, 0, sizeof(*edit));
    g_memset(caption, 0, 256);
    g_strncpy(caption, initial, 255);
    edit->type = WND_TYPE_EDIT;
    edit->caption1 = caption;
    edit->edit_pos = utf8_char_count(caption);
}

/******************************************************************************/
START_TEST(test_insert__appends_at_caret)
{
    struct xrdp_bitmap edit;
    char caption[256];
    static const char32_t cp[] = { 'p', 'w', 'd' };

    setup_edit(&edit, caption, "");
    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(&edit, cp, 3), 3);
    ck_assert_str_eq(caption, "pwd");
    ck_assert_int_eq(edit.edit_pos, 3);
}
END_TEST

/******************************************************************************/
START_TEST(test_insert__inserts_mid_string)
{
    struct xrdp_bitmap edit;
    char caption[256];
    static const char32_t cp[] = { 'X' };

    setup_edit(&edit, caption, "ab");
    edit.edit_pos = 1;
    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(&edit, cp, 1), 1);
    ck_assert_str_eq(caption, "aXb");
    ck_assert_int_eq(edit.edit_pos, 2);
}
END_TEST

/******************************************************************************/
/* Non-ASCII must survive as UTF-8 */
START_TEST(test_insert__non_ascii)
{
    struct xrdp_bitmap edit;
    char caption[256];
    static const char32_t cp[] = { 0x00e9 }; /* e-acute */

    setup_edit(&edit, caption, "");
    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(&edit, cp, 1), 1);
    ck_assert_str_eq(caption, "\xc3\xa9");
    ck_assert_int_eq(edit.edit_pos, 1);
}
END_TEST

/******************************************************************************/
/* A paste bigger than the field must fill it and stop, not overflow */
START_TEST(test_insert__stops_when_field_full)
{
    struct xrdp_bitmap edit;
    char caption[256];
    char32_t cp[300];
    unsigned int i;
    unsigned int n;

    for (i = 0; i < 300; i++)
    {
        cp[i] = 'x';
    }

    setup_edit(&edit, caption, "");
    n = xrdp_login_clip_insert_codepoints(&edit, cp, 300);

    ck_assert_uint_lt(n, 300);
    ck_assert_uint_eq(g_strlen(caption), n);
    ck_assert_uint_lt(g_strlen(caption), 256);
    ck_assert_int_eq(edit.edit_pos, (int)n);
}
END_TEST

/******************************************************************************/
START_TEST(test_insert__rejects_wrong_widget_type)
{
    struct xrdp_bitmap edit;
    char caption[256];
    static const char32_t cp[] = { 'a' };

    setup_edit(&edit, caption, "");
    edit.type = WND_TYPE_BUTTON;
    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(&edit, cp, 1), 0);

    setup_edit(&edit, caption, "");
    edit.caption1 = NULL;
    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(&edit, cp, 1), 0);

    ck_assert_uint_eq(xrdp_login_clip_insert_codepoints(NULL, cp, 1), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_format_list__long_names)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    /* CF_UNICODETEXT with an empty long (UTF-16) name */
    out_uint32_le(s, CF_UNICODETEXT);
    out_uint16_le(s, 0);
    /* CF_LOCALE with an empty long name */
    out_uint32_le(s, CF_LOCALE);
    out_uint16_le(s, 0);
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 1,
                     &have_unicode, &have_text), 0);
    ck_assert_int_eq(have_unicode, 1);
    ck_assert_int_eq(have_text, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_format_list__short_unicode_names)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    /* CF_TEXT with a 32-byte name field */
    out_uint32_le(s, CF_TEXT);
    out_uint8s(s, 32);
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 0,
                     &have_unicode, &have_text), 0);
    ck_assert_int_eq(have_unicode, 0);
    ck_assert_int_eq(have_text, 1);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* CB_ASCII_NAMES uses the same 32-byte width, so both ids must be found */
START_TEST(test_format_list__short_ascii_names)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, CF_TEXT);
    out_uint8s(s, 32);
    out_uint32_le(s, CF_UNICODETEXT);
    out_uint8s(s, 32);
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, CB_ASCII_NAMES, 0,
                     &have_unicode, &have_text), 0);
    ck_assert_int_eq(have_unicode, 1);
    ck_assert_int_eq(have_text, 1);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* A long name with real content must be skipped to its terminator */
START_TEST(test_format_list__long_name_with_content)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, 0xc0de);       /* private format */
    out_uint16_le(s, 'H');
    out_uint16_le(s, 'i');
    out_uint16_le(s, 0);            /* terminator */
    out_uint32_le(s, CF_UNICODETEXT);
    out_uint16_le(s, 0);
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 1,
                     &have_unicode, &have_text), 0);
    ck_assert_int_eq(have_unicode, 1);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* Truncated PDUs must be reported, not read past */
START_TEST(test_format_list__truncated)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint16_le(s, CF_UNICODETEXT);   /* only half a format id */
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 1,
                     &have_unicode, &have_text), 1);
    ck_assert_int_eq(have_unicode, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* An empty format list is legal - the client has an empty clipboard */
START_TEST(test_format_list__empty)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 1,
                     &have_unicode, &have_text), 0);
    ck_assert_int_eq(have_unicode, 0);
    ck_assert_int_eq(have_text, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* Valid format id followed by unterminated long name must fail cleanly */
START_TEST(test_format_list__valid_id_unterminated_long_name)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, CF_UNICODETEXT);
    out_uint16_le(s, 'H');      /* start of name, but no terminator */
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 1,
                     &have_unicode, &have_text), 1);
    ck_assert_int_eq(have_unicode, 0);
    ck_assert_int_eq(have_text, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* Valid format id followed by truncated short name must fail cleanly */
START_TEST(test_format_list__valid_id_truncated_short_name)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, CF_TEXT);
    out_uint8s(s, 16);          /* only half a 32-byte name field */
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0, 0,
                     &have_unicode, &have_text), 1);
    ck_assert_int_eq(have_unicode, 0);
    ck_assert_int_eq(have_text, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* Captures PDUs the module sends, in place of libxrdp_send_to_channel() */

#define LC_CAPTURE_MAX 8
#define LC_CAPTURE_SIZE 1024

static char g_sent[LC_CAPTURE_MAX][LC_CAPTURE_SIZE];
static int g_sent_len[LC_CAPTURE_MAX];
static int g_sent_count;

int
__wrap_libxrdp_send_to_channel(struct xrdp_session *session, int channel_id,
                               char *data, int data_len,
                               int total_data_len, int flags)
{
    if (g_sent_count < LC_CAPTURE_MAX && data_len <= LC_CAPTURE_SIZE)
    {
        g_memcpy(g_sent[g_sent_count], data, data_len);
        g_sent_len[g_sent_count] = data_len;
        g_sent_count++;
    }
    return 0;
}

/* Counts repaints, in place of xrdp_bitmap_invalidate() */
static int g_invalidate_count;

int
__wrap_xrdp_bitmap_invalidate(struct xrdp_bitmap *self, struct xrdp_rect *rect)
{
    g_invalidate_count++;
    return 0;
}

static void
clear_sent(void)
{
    g_memset(g_sent, 0, sizeof(g_sent));
    g_memset(g_sent_len, 0, sizeof(g_sent_len));
    g_sent_count = 0;
    g_invalidate_count = 0;
}

/* msgType of a captured PDU */
static int
sent_type(int index)
{
    if (index >= g_sent_count)
    {
        return -1;
    }
    return (unsigned char)g_sent[index][0] |
           ((unsigned char)g_sent[index][1] << 8);
}

/* msgFlags of a captured PDU */
static int
sent_flags(int index)
{
    if (index >= g_sent_count)
    {
        return -1;
    }
    return (unsigned char)g_sent[index][2] |
           ((unsigned char)g_sent[index][3] << 8);
}

/******************************************************************************/
/* Feeds one complete cliprdr PDU to the module */
static void
feed_pdu(struct xrdp_login_clip *lc, int msg_type, int msg_flags,
         const char *payload, int payload_len)
{
    char buf[LC_CAPTURE_SIZE];
    int total = 8 + payload_len;

    buf[0] = (char)(msg_type & 0xff);
    buf[1] = (char)((msg_type >> 8) & 0xff);
    buf[2] = (char)(msg_flags & 0xff);
    buf[3] = (char)((msg_flags >> 8) & 0xff);
    buf[4] = (char)(payload_len & 0xff);
    buf[5] = (char)((payload_len >> 8) & 0xff);
    buf[6] = (char)((payload_len >> 16) & 0xff);
    buf[7] = (char)((payload_len >> 24) & 0xff);
    if (payload_len > 0)
    {
        g_memcpy(buf + 8, payload, payload_len);
    }

    xrdp_login_clip_process_channel_data(lc,
                                         XR_CHANNEL_FLAG_FIRST |
                                         XR_CHANNEL_FLAG_LAST,
                                         buf, total, total);
}

/* Client capabilities PDU body: 1 general capability set, version 2,
 * long format names */
static void
feed_client_caps(struct xrdp_login_clip *lc)
{
    char body[16];

    g_memset(body, 0, sizeof(body));
    body[0] = 1;                                /* cCapabilitiesSets */
    body[4] = CB_CAPSTYPE_GENERAL;              /* capabilitySetType */
    body[6] = 12;                               /* lengthCapability */
    body[8] = CB_CAPS_VERSION_2;                /* version */
    body[12] = CB_USE_LONG_FORMAT_NAMES;        /* generalFlags */
    feed_pdu(lc, CB_CLIP_CAPS, 0, body, 16);
}

/* Format list offering CF_UNICODETEXT with an empty long name */
static void
feed_format_list(struct xrdp_login_clip *lc)
{
    char body[6];

    g_memset(body, 0, sizeof(body));
    body[0] = CF_UNICODETEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, body, 6);
}

/* Drives a fresh object to the READY state */
static struct xrdp_login_clip *
make_ready_lc(struct xrdp_wm *wm)
{
    struct xrdp_login_clip *lc;

    g_memset(wm, 0, sizeof(*wm));
    clear_sent();
    lc = xrdp_login_clip_create(wm, 5);
    feed_client_caps(lc);
    feed_format_list(lc);
    clear_sent();
    return lc;
}

/******************************************************************************/
START_TEST(test_state__create_sends_caps_then_monitor_ready)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();

    lc = xrdp_login_clip_create(&wm, 5);
    ck_assert_ptr_nonnull(lc);
    ck_assert_int_eq(g_sent_count, 2);
    ck_assert_int_eq(sent_type(0), CB_CLIP_CAPS);
    ck_assert_int_eq(sent_type(1), CB_MONITOR_READY);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
START_TEST(test_state__format_list_is_acknowledged)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    feed_client_caps(lc);
    clear_sent();

    feed_format_list(lc);
    ck_assert_int_eq(g_sent_count, 1);
    ck_assert_int_eq(sent_type(0), CB_FORMAT_LIST_RESPONSE);
    ck_assert_int_eq(sent_flags(0), CB_RESPONSE_OK);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
START_TEST(test_state__paste_requests_unicode_text)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    ck_assert_int_eq(g_sent_count, 1);
    ck_assert_int_eq(sent_type(0), CB_FORMAT_DATA_REQUEST);
    /* requestedFormatId is the first field of the body, at offset 8 */
    ck_assert_int_eq((unsigned char)g_sent[0][8], CF_UNICODETEXT);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Before the handshake completes there is nothing to ask for */
START_TEST(test_state__paste_before_ready_sends_nothing)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    clear_sent();

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

    /* And a NULL object must be safe, for the disabled case */
    ck_assert_int_eq(xrdp_login_clip_request_paste(NULL), 0);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Only one request in flight */
START_TEST(test_state__second_paste_while_waiting_sends_nothing)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A client with no text on its clipboard */
START_TEST(test_state__paste_with_no_text_format_sends_nothing)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;
    char body[6];

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    feed_client_caps(lc);

    /* Format list offering only a private format */
    g_memset(body, 0, sizeof(body));
    body[0] = 0xde;
    body[1] = 0xc0;
    feed_pdu(lc, CB_FORMAT_LIST, 0, body, 6);
    clear_sent();

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A response we never asked for must be ignored, not inserted */
START_TEST(test_state__unsolicited_data_response_is_ignored)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    char caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char text[8];

    setup_edit(&edit, caption, "");
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    wm.login_window = &login_window;

    g_memset(text, 0, sizeof(text));
    text[0] = 'a';
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, 4);

    ck_assert_str_eq(caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* The happy path, end to end through the module */
START_TEST(test_state__data_response_inserts_into_focused_edit)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    struct list *child_list;
    char caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char text[10];
    unsigned int len;
    static const unsigned short words[] = { 's', 'e', 'c', 0x000d, 0x000a };

    setup_edit(&edit, caption, "");
    child_list = list_create();
    list_add_item(child_list, (long)&edit);
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    login_window.child_list = child_list;
    wm.login_window = &login_window;

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    len = make_utf16(text, words, 5);
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, (int)len);

    ck_assert_str_eq(caption, "sec");
    ck_assert_int_eq(edit.edit_pos, 3);
    /* One repaint for the whole paste, not one per character */
    ck_assert_int_eq(g_invalidate_count, 1);

    /* A second paste is allowed once the first completed */
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
    list_delete(child_list);
}
END_TEST

/******************************************************************************/
/* A covering window means the user cannot see where the text would land */
START_TEST(test_state__data_response_dropped_when_popup_is_up)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    struct xrdp_bitmap popup;
    char caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char text[10];
    unsigned int len;
    static const unsigned short words[] = { 'x', 0 };

    setup_edit(&edit, caption, "");
    g_memset(&login_window, 0, sizeof(login_window));
    g_memset(&popup, 0, sizeof(popup));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    wm.login_window = &login_window;
    wm.popup_wnd = &popup;

    xrdp_login_clip_request_paste(lc);
    len = make_utf16(text, words, 2);
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, (int)len);

    ck_assert_str_eq(caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
START_TEST(test_state__failed_data_response_is_absorbed)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    char caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    setup_edit(&edit, caption, "");
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    wm.login_window = &login_window;

    xrdp_login_clip_request_paste(lc);
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_FAIL, NULL, 0);

    ck_assert_str_eq(caption, "");
    /* State returned to READY, so a retry is possible */
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* An oversized PDU must be discarded without allocating for it */
START_TEST(test_state__oversized_pdu_is_discarded)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char chunk[64];

    g_memset(chunk, 0, sizeof(chunk));
    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_FIRST,
                                         chunk, 64, 1024 * 1024);
    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_LAST,
                                         chunk, 64, 1024 * 1024);

    /* Still usable afterwards */
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A PDU too short to hold a cliprdr header must be dropped, not read past */
START_TEST(test_state__truncated_header_is_dropped)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char buf[4];

    g_memset(buf, 0, sizeof(buf));
    buf[0] = CB_FORMAT_DATA_RESPONSE;
    xrdp_login_clip_process_channel_data(lc,
                                         XR_CHANNEL_FLAG_FIRST |
                                         XR_CHANNEL_FLAG_LAST,
                                         buf, 4, 4);

    ck_assert_int_eq(g_sent_count, 0);

    /* Still usable afterwards */
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A PDU split across two channel chunks must be reassembled */
START_TEST(test_state__reassembles_split_pdu)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    struct list *child_list;
    char caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char buf[16];
    int total;

    setup_edit(&edit, caption, "");
    child_list = list_create();
    list_add_item(child_list, (long)&edit);
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    login_window.child_list = child_list;
    wm.login_window = &login_window;

    xrdp_login_clip_request_paste(lc);

    /* CB_FORMAT_DATA_RESPONSE carrying "hi" in UTF-16LE, split 6 + 6 */
    g_memset(buf, 0, sizeof(buf));
    buf[0] = CB_FORMAT_DATA_RESPONSE;
    buf[2] = CB_RESPONSE_OK;
    buf[4] = 4;                 /* dataLen */
    buf[8] = 'h';
    buf[10] = 'i';
    total = 12;

    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_FIRST,
                                         buf, 6, total);
    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_LAST,
                                         buf + 6, 6, total);

    ck_assert_str_eq(caption, "hi");

    xrdp_login_clip_delete(lc);
    list_delete(child_list);
}
END_TEST

/******************************************************************************/
/* A malformed CB_CLIP_CAPS body must not stop the handshake or crash;
 * the object must still be usable afterwards */
START_TEST(test_state__malformed_caps_pdu_still_advances_handshake)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;
    char body[2];
    /* Short-name framing (32-byte fixed name field): client_long_names
     * stays at its default of 0 because the malformed caps body below
     * is never parsed */
    char format_body[36];

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    clear_sent();

    /* Too short to hold even cCapabilitiesSets + pad1 */
    g_memset(body, 0, sizeof(body));
    feed_pdu(lc, CB_CLIP_CAPS, 0, body, 2);

    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_UNICODETEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 36);
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A capability set claiming a length below its own 4-byte header must be
 * rejected outright, not underflow */
START_TEST(test_state__caps_length_below_header_is_rejected)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc;
    char body[8];
    /* Short-name framing: the malformed capability set below bails
     * before general_flags is ever read, so client_long_names stays
     * at its default of 0 */
    char format_body[36];

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    clear_sent();

    g_memset(body, 0, sizeof(body));
    body[0] = 1;                    /* cCapabilitiesSets */
    body[4] = CB_CAPSTYPE_GENERAL;  /* capabilitySetType */
    body[6] = 3;                    /* lengthCapability - below its own
                                      * 4-byte header */
    feed_pdu(lc, CB_CLIP_CAPS, 0, body, 8);

    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_UNICODETEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 36);
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* The CF_TEXT branch is only reachable when the client offers CF_TEXT and
 * not CF_UNICODETEXT - drive it end to end */
START_TEST(test_state__cf_text_paste_inserts_into_focused_edit)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    struct list *child_list;
    char caption[256];
    struct xrdp_login_clip *lc;
    char format_body[6];
    char text[4];

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    feed_client_caps(lc);

    /* Format list offering only CF_TEXT */
    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_TEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 6);
    clear_sent();

    setup_edit(&edit, caption, "");
    child_list = list_create();
    list_add_item(child_list, (long)&edit);
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    login_window.child_list = child_list;
    wm.login_window = &login_window;

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    /* requestedFormatId is the first field of the body, at offset 8 */
    ck_assert_int_eq((unsigned char)g_sent[0][8], CF_TEXT);

    text[0] = 's';
    text[1] = 'e';
    text[2] = 'c';
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, 3);

    ck_assert_str_eq(caption, "sec");

    xrdp_login_clip_delete(lc);
    list_delete(child_list);
}
END_TEST

/******************************************************************************/
/* Pins the CP-1252/ISO-8859-1 boundary: 0x9f is a C1 control byte in that
 * range and must be dropped, not inserted */
START_TEST(test_state__cf_text_drops_0x9f_byte)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap login_window;
    struct xrdp_bitmap edit;
    struct list *child_list;
    char caption[256];
    struct xrdp_login_clip *lc;
    char format_body[6];
    char text[4];

    g_memset(&wm, 0, sizeof(wm));
    clear_sent();
    lc = xrdp_login_clip_create(&wm, 5);
    feed_client_caps(lc);

    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_TEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 6);
    clear_sent();

    setup_edit(&edit, caption, "");
    child_list = list_create();
    list_add_item(child_list, (long)&edit);
    g_memset(&login_window, 0, sizeof(login_window));
    login_window.type = WND_TYPE_WND;
    login_window.focused_control = &edit;
    login_window.child_list = child_list;
    wm.login_window = &login_window;

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    text[0] = 'a';
    text[1] = (char)0x9f;
    text[2] = 'b';
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, 3);

    ck_assert_str_eq(caption, "ab");

    xrdp_login_clip_delete(lc);
    list_delete(child_list);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_login_clip(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("LoginClip");

    tc = tcase_create("xrdp_login_clip");
    tcase_add_test(tc, test_is_paste_key__ctrl_v);
    tcase_add_test(tc, test_is_paste_key__altgr_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__ctrl_v_uppercase_keysym);
    tcase_add_test(tc, test_is_paste_key__ctrl_v_non_latin_layout);
    tcase_add_test(tc, test_is_paste_key__altgr_v_uppercase_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__plain_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__ctrl_other_key_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__shift_insert);
    tcase_add_test(tc, test_is_paste_key__null_key_info);
    tcase_add_test(tc, test_is_paste_key__scancode_fallback_with_null_key_info);
    tcase_add_test(tc, test_utf16__plain_ascii);
    tcase_add_test(tc, test_utf16__stops_at_crlf);
    tcase_add_test(tc, test_utf16__drops_control_chars);
    tcase_add_test(tc, test_utf16__decodes_surrogate_pair);
    tcase_add_test(tc, test_utf16__drops_unpaired_surrogates);
    tcase_add_test(tc, test_utf16__respects_out_capacity);
    tcase_add_test(tc, test_utf16__odd_length_and_empty);
    tcase_add_test(tc, test_insert__appends_at_caret);
    tcase_add_test(tc, test_insert__inserts_mid_string);
    tcase_add_test(tc, test_insert__non_ascii);
    tcase_add_test(tc, test_insert__stops_when_field_full);
    tcase_add_test(tc, test_insert__rejects_wrong_widget_type);
    tcase_add_test(tc, test_format_list__long_names);
    tcase_add_test(tc, test_format_list__short_unicode_names);
    tcase_add_test(tc, test_format_list__short_ascii_names);
    tcase_add_test(tc, test_format_list__long_name_with_content);
    tcase_add_test(tc, test_format_list__truncated);
    tcase_add_test(tc, test_format_list__empty);
    tcase_add_test(tc, test_format_list__valid_id_unterminated_long_name);
    tcase_add_test(tc, test_format_list__valid_id_truncated_short_name);
    tcase_add_test(tc, test_state__create_sends_caps_then_monitor_ready);
    tcase_add_test(tc, test_state__format_list_is_acknowledged);
    tcase_add_test(tc, test_state__paste_requests_unicode_text);
    tcase_add_test(tc, test_state__paste_before_ready_sends_nothing);
    tcase_add_test(tc, test_state__second_paste_while_waiting_sends_nothing);
    tcase_add_test(tc, test_state__paste_with_no_text_format_sends_nothing);
    tcase_add_test(tc, test_state__unsolicited_data_response_is_ignored);
    tcase_add_test(tc, test_state__data_response_inserts_into_focused_edit);
    tcase_add_test(tc, test_state__data_response_dropped_when_popup_is_up);
    tcase_add_test(tc, test_state__failed_data_response_is_absorbed);
    tcase_add_test(tc, test_state__oversized_pdu_is_discarded);
    tcase_add_test(tc, test_state__truncated_header_is_dropped);
    tcase_add_test(tc, test_state__reassembles_split_pdu);
    tcase_add_test(tc, test_state__malformed_caps_pdu_still_advances_handshake);
    tcase_add_test(tc, test_state__caps_length_below_header_is_rejected);
    tcase_add_test(tc, test_state__cf_text_paste_inserts_into_focused_edit);
    tcase_add_test(tc, test_state__cf_text_drops_0x9f_byte);

    suite_add_tcase(s, tc);

    return s;
}
