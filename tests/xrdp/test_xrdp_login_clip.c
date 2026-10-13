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
#include "string_calls.h"

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
/* On Dvorak the physical V key produces 'k'. Ctrl+K must stay Ctrl+K, and
 * Ctrl+V - on whichever physical key produces 'v' - must paste */
START_TEST(test_is_paste_key__dvorak)
{
    struct xrdp_key_info k = { 0x006b, 'k' };
    struct xrdp_key_info v = { XK_v, 'v' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_V_KEY,
                     &k), 0);
    /* Dvorak 'v' is on the QWERTY '.' key, scancode 0x34 */
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x34, &v), 1);
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
/* Decodes len bytes of buf as format into out, through a stream as the
 * module does */
static unsigned int
decode(int format, const char *buf, unsigned int len,
       char32_t *out, unsigned int out_count)
{
    struct stream *s;
    unsigned int n;

    make_stream(s);
    init_stream(s, len + 1);
    out_uint8a(s, buf, len);
    s_mark_end(s);
    s->p = s->data;
    n = xrdp_login_clip_decode_text(s, format, out, out_count);
    free_stream(s);
    return n;
}

/******************************************************************************/
START_TEST(test_decode__plain_ascii)
{
    static const unsigned short words[] = { 'p', 'a', 's', 's', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 5);
    unsigned int n = decode(CF_UNICODETEXT, buf, len, out, 16);

    ck_assert_uint_eq(n, 4);
    ck_assert_uint_eq(out[0], 'p');
    ck_assert_uint_eq(out[3], 's');
}
END_TEST

/******************************************************************************/
/* A password manager typically appends a newline. Stop at the first one */
START_TEST(test_decode__stops_at_crlf)
{
    static const unsigned short words[] = { 'a', 'b', 0x000d, 0x000a, 'c', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 6);
    unsigned int n = decode(CF_UNICODETEXT, buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
START_TEST(test_decode__drops_control_chars)
{
    static const unsigned short words[] = { 'a', 0x0009, 0x007f, 0x0085, 'b', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 6);
    unsigned int n = decode(CF_UNICODETEXT, buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[0], 'a');
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
/* U+1F600 GRINNING FACE, as a surrogate pair */
START_TEST(test_decode__surrogate_pair)
{
    static const unsigned short words[] = { 0xd83d, 0xde00, 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 3);
    unsigned int n = decode(CF_UNICODETEXT, buf, len, out, 16);

    ck_assert_uint_eq(n, 1);
    ck_assert_uint_eq(out[0], 0x1f600);
}
END_TEST

/******************************************************************************/
START_TEST(test_decode__drops_unpaired_surrogates)
{
    /* High surrogate with no low surrogate, then a low surrogate alone */
    static const unsigned short words[] = { 0xd83d, 'a', 0xde00, 'b', 0 };
    char buf[32];
    char32_t out[16];
    unsigned int len = make_utf16(buf, words, 5);
    unsigned int n = decode(CF_UNICODETEXT, buf, len, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[0], 'a');
    ck_assert_uint_eq(out[1], 'b');
}
END_TEST

/******************************************************************************/
START_TEST(test_decode__respects_out_capacity)
{
    static const unsigned short words[] = { 'a', 'b', 'c', 'd', 0 };
    char buf[32];
    char32_t out[2];
    unsigned int len = make_utf16(buf, words, 5);

    ck_assert_uint_eq(decode(CF_UNICODETEXT, buf, len, out, 2), 2);
    ck_assert_uint_eq(decode(CF_TEXT, "abcd", 4, out, 2), 2);
}
END_TEST

/******************************************************************************/
/* An odd trailing byte must not be read as half a word */
START_TEST(test_decode__odd_length_and_empty)
{
    char buf[4] = { 'a', 0, 'b' };
    char32_t out[16];

    ck_assert_uint_eq(decode(CF_UNICODETEXT, buf, 3, out, 16), 1);
    ck_assert_uint_eq(decode(CF_UNICODETEXT, buf, 0, out, 16), 0);
    ck_assert_uint_eq(xrdp_login_clip_decode_text(NULL, CF_UNICODETEXT,
                      out, 16), 0);
}
END_TEST

/******************************************************************************/
/* CF_TEXT is CP-1252: 0x9f is a C1 control byte in ISO-8859-1 terms and
 * is dropped; 0xe9 is e-acute; a CR ends the paste */
START_TEST(test_decode__cf_text)
{
    char32_t out[16];
    unsigned int n = decode(CF_TEXT, "a\x9f\xe9\rb", 5, out, 16);

    ck_assert_uint_eq(n, 2);
    ck_assert_uint_eq(out[0], 'a');
    ck_assert_uint_eq(out[1], 0xe9);
}
END_TEST

/******************************************************************************/
/* Builds a minimal edit widget, as xrdp_wm_show_edits() does */
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 0,
                     &have_unicode, &have_text), 1);
    ck_assert_int_eq(have_unicode, 0);
    ck_assert_int_eq(have_text, 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/* A list rejected part way through must not offer the formats from the
 * entries before the bad one */
START_TEST(test_format_list__rejected_list_offers_nothing)
{
    struct stream *s;
    int have_unicode = 0;
    int have_text = 0;

    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, CF_UNICODETEXT);
    out_uint16_le(s, 0);
    out_uint32_le(s, CF_TEXT);
    out_uint16_le(s, 'H');      /* unterminated name */
    s_mark_end(s);
    s->p = s->data;

    ck_assert_int_eq(xrdp_login_clip_parse_format_list(s, 1,
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

extern int
__wrap_libxrdp_send_to_channel(struct xrdp_session *session, int channel_id,
                               char *data, int data_len,
                               int total_data_len, int flags);
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

extern int
__wrap_xrdp_bitmap_invalidate(struct xrdp_bitmap *self,
                              struct xrdp_rect *rect);
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

/* A login window holding one focused edit field, at the user prompt */
static struct xrdp_bitmap g_login_window;
static struct xrdp_bitmap g_edit;
static char g_caption[256];

static void
setup_login_window(struct xrdp_wm *wm)
{
    setup_edit(&g_edit, g_caption, "");
    g_memset(&g_login_window, 0, sizeof(g_login_window));
    g_login_window.type = WND_TYPE_WND;
    g_login_window.focused_control = &g_edit;
    g_login_window.child_list = list_create();
    list_add_item(g_login_window.child_list, (tintptr)&g_edit);
    wm->login_window = &g_login_window;
    wm->login_state = WMLS_USER_PROMPT;
}

/* calloc() in place of the real one, failing the Nth call from now when
 * g_calloc_fail_at is N (1 = the next call), and never when it is 0 */
static int g_calloc_fail_at;

extern void *
__real_calloc(size_t nmemb, size_t size);
extern void *
__wrap_calloc(size_t nmemb, size_t size);
void *
__wrap_calloc(size_t nmemb, size_t size)
{
    if (g_calloc_fail_at > 0 && --g_calloc_fail_at == 0)
    {
        return NULL;
    }
    return __real_calloc(nmemb, size);
}

/* g_get_elapsed_ms() in place of the real one, running g_clock_skew_ms
 * ahead of it so the tests can move time on */
static unsigned int g_clock_skew_ms;

extern unsigned int
__real_g_get_elapsed_ms(void);
extern unsigned int
__wrap_g_get_elapsed_ms(void);
unsigned int
__wrap_g_get_elapsed_ms(void)
{
    return __real_g_get_elapsed_ms() + g_clock_skew_ms;
}

/* Checked fixture teardown. Also undoes the wrappers' test settings, so a
 * failed test cannot leak them into the next one under CK_FORK=no */
static void
teardown_state(void)
{
    list_delete(g_login_window.child_list);
    g_login_window.child_list = NULL;
    g_calloc_fail_at = 0;
    g_clock_skew_ms = 0;
}

/* Creates an object for a fresh wm with a login window */
static struct xrdp_login_clip *
make_lc(struct xrdp_wm *wm)
{
    struct xrdp_login_clip *lc;

    g_memset(wm, 0, sizeof(*wm));
    setup_login_window(wm);
    clear_sent();
    lc = xrdp_login_clip_create(wm, 5);
    clear_sent();
    return lc;
}

/* Drives a fresh object to the READY state */
static struct xrdp_login_clip *
make_ready_lc(struct xrdp_wm *wm)
{
    struct xrdp_login_clip *lc = make_lc(wm);

    feed_client_caps(lc);
    feed_format_list(lc);
    clear_sent();
    return lc;
}

/* Feeds a successful CF_UNICODETEXT data response carrying text */
static void
feed_unicode_response(struct xrdp_login_clip *lc, const char *text)
{
    unsigned short words[32];
    char buf[64];
    unsigned int i;

    for (i = 0; text[i] != '\0' && i < 31; i++)
    {
        words[i] = (unsigned char)text[i];
    }
    words[i++] = 0;
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK,
             buf, (int)make_utf16(buf, words, i));
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
/* Running out of memory costs only the paste feature: create fails
 * cleanly, and says nothing to the client */
START_TEST(test_state__create_fails_cleanly_out_of_memory)
{
    struct xrdp_wm wm;

    g_memset(&wm, 0, sizeof(wm));

    /* The object itself */
    clear_sent();
    g_calloc_fail_at = 1;
    ck_assert_ptr_null(xrdp_login_clip_create(&wm, 5));
    ck_assert_int_eq(g_sent_count, 0);

    /* Its fragment buffer */
    clear_sent();
    g_calloc_fail_at = 2;
    ck_assert_ptr_null(xrdp_login_clip_create(&wm, 5));
    ck_assert_int_eq(g_sent_count, 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_state__format_list_is_acknowledged)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_lc(&wm);

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
/* A rejected format list must leave nothing to paste, even though its
 * first entry offered CF_UNICODETEXT */
START_TEST(test_state__rejected_format_list_disables_paste)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char body[8];

    g_memset(body, 0, sizeof(body));
    body[0] = CF_UNICODETEXT;   /* then an empty name, then half an id */
    body[6] = CF_TEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, body, 8);
    ck_assert_int_eq(sent_type(0), CB_FORMAT_LIST_RESPONSE);
    ck_assert_int_eq(sent_flags(0), CB_RESPONSE_FAIL);

    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

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
    struct xrdp_login_clip *lc = make_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

    /* And a NULL object must be safe, for the disabled case */
    ck_assert_int_eq(xrdp_login_clip_request_paste(NULL), 0);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* The client's clipboard is not read when there is nowhere to put it */
START_TEST(test_state__paste_with_no_target_sends_nothing)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap button;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    /* Not at the user prompt */
    wm.login_state = WMLS_START_CONNECT;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    wm.login_state = WMLS_USER_PROMPT;

    /* Focus on something other than an edit field */
    g_memset(&button, 0, sizeof(button));
    button.type = WND_TYPE_BUTTON;
    list_add_item(g_login_window.child_list, (tintptr)&button);
    g_login_window.focused_control = &button;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);

    /* No login window at all */
    wm.login_window = NULL;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);

    ck_assert_int_eq(g_sent_count, 0);

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
    struct xrdp_login_clip *lc = make_lc(&wm);
    char body[6];

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
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    feed_unicode_response(lc, "a");

    ck_assert_str_eq(g_caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* The happy path, end to end through the module */
START_TEST(test_state__data_response_inserts_into_focused_edit)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char text[10];
    unsigned int len;
    static const unsigned short words[] = { 's', 'e', 'c', 0x000d, 0x000a };

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    len = make_utf16(text, words, 5);
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, (int)len);

    ck_assert_str_eq(g_caption, "sec");
    ck_assert_int_eq(g_edit.edit_pos, 3);
    /* One repaint for the whole paste, not one per character */
    ck_assert_int_eq(g_invalidate_count, 1);

    /* A second paste is allowed once the first completed */
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* focused_control can outlive the widget it once pointed to: nothing
 * clears it when a widget is deleted from the login window's children.
 * Here the widget leaves child_list while the request is in flight, and
 * the response must be dropped, not written through the stale pointer */
START_TEST(test_state__data_response_dropped_when_focused_control_not_a_child)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    list_clear(g_login_window.child_list);
    feed_unicode_response(lc, "sec");

    ck_assert_str_eq(g_caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Focus moved to another field while the request was in flight. The text
 * was asked for on behalf of the first field, so neither gets it */
START_TEST(test_state__data_response_dropped_when_focus_moved)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap other;
    char other_caption[256];
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    setup_edit(&other, other_caption, "");
    list_add_item(g_login_window.child_list, (tintptr)&other);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    g_login_window.focused_control = &other;
    feed_unicode_response(lc, "sec");

    ck_assert_str_eq(g_caption, "");
    ck_assert_str_eq(other_caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* The edit fields were rebuilt while the request was in flight (as for a
 * session type change), and the focused field happens to sit at the old
 * address. The generation count catches that */
START_TEST(test_state__data_response_dropped_when_edits_rebuilt)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap *edit;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    edit = xrdp_bitmap_create(10, 10, 32, WND_TYPE_EDIT, &wm);
    ck_assert_ptr_nonnull(edit);
    xrdp_bitmap_delete(edit);
    feed_unicode_response(lc, "sec");

    ck_assert_str_eq(g_caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A covering window means the user cannot see where the text would land */
START_TEST(test_state__data_response_dropped_when_popup_is_up)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap popup;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    g_memset(&popup, 0, sizeof(popup));
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    wm.popup_wnd = &popup;
    feed_unicode_response(lc, "x");

    ck_assert_str_eq(g_caption, "");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Nor may text go behind the Help dialog */
START_TEST(test_state__data_response_dropped_when_modal_dialog_is_up)
{
    struct xrdp_wm wm;
    struct xrdp_bitmap help;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    g_memset(&help, 0, sizeof(help));
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    g_login_window.modal_dialog = &help;
    feed_unicode_response(lc, "x");

    ck_assert_str_eq(g_caption, "");

    /* And no new request is made while it is up */
    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    ck_assert_int_eq(g_sent_count, 0);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A new format list arriving while a request is in flight abandons it.
 * The old response still arrives first, and must not be decoded as the
 * answer to the next request, which here asks for a different format */
START_TEST(test_state__stale_response_after_new_format_list_is_discarded)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char format_body[6];

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    /* The clipboard now holds CF_TEXT only */
    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_TEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 6);

    clear_sent();
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    ck_assert_int_eq((unsigned char)g_sent[0][8], CF_TEXT);

    /* Response to the abandoned CF_UNICODETEXT request */
    feed_unicode_response(lc, "old");
    ck_assert_str_eq(g_caption, "");

    /* Response to the live CF_TEXT request */
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, "new", 3);
    ck_assert_str_eq(g_caption, "new");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Leaving the prompt abandons the request. Its response must not land in
 * the prompt that comes back later */
START_TEST(test_state__abandoned_paste_response_is_discarded)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    xrdp_login_clip_abandon_paste(lc);
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    feed_unicode_response(lc, "old");
    ck_assert_str_eq(g_caption, "");
    feed_unicode_response(lc, "new");
    ck_assert_str_eq(g_caption, "new");

    /* Safe with nothing outstanding, and with no object */
    xrdp_login_clip_abandon_paste(lc);
    xrdp_login_clip_abandon_paste(NULL);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
START_TEST(test_state__failed_data_response_is_absorbed)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    xrdp_login_clip_request_paste(lc);
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_FAIL, NULL, 0);

    ck_assert_str_eq(g_caption, "");
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
    struct xrdp_login_clip *lc = make_ready_lc(&wm);
    char buf[16];
    int total;

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

    ck_assert_str_eq(g_caption, "hi");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A malformed CB_CLIP_CAPS body must not stop the handshake or crash;
 * the object must still be usable afterwards */
START_TEST(test_state__malformed_caps_pdu_still_advances_handshake)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_lc(&wm);
    char body[2];
    /* Short-name framing (32-byte fixed name field): client_long_names
     * stays at its default of 0 because the malformed caps body below
     * is never parsed */
    char format_body[36];

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
    struct xrdp_login_clip *lc = make_lc(&wm);
    char body[8];
    /* Short-name framing: the malformed capability set below bails
     * before general_flags is ever read, so client_long_names stays
     * at its default of 0 */
    char format_body[36];

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
    struct xrdp_login_clip *lc = make_lc(&wm);
    char format_body[6];
    char text[4];

    feed_client_caps(lc);

    /* Format list offering only CF_TEXT */
    g_memset(format_body, 0, sizeof(format_body));
    format_body[0] = CF_TEXT;
    feed_pdu(lc, CB_FORMAT_LIST, 0, format_body, 6);
    clear_sent();

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    /* requestedFormatId is the first field of the body, at offset 8 */
    ck_assert_int_eq((unsigned char)g_sent[0][8], CF_TEXT);

    text[0] = 's';
    text[1] = 'e';
    text[2] = 'c';
    feed_pdu(lc, CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text, 3);

    ck_assert_str_eq(g_caption, "sec");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Feeds the first and last chunks of a data response too big to
 * reassemble */
static void
feed_oversized_response(struct xrdp_login_clip *lc)
{
    char chunk[64];
    int total = 8 + 20000;

    g_memset(chunk, 0, sizeof(chunk));
    chunk[0] = CB_FORMAT_DATA_RESPONSE;
    chunk[2] = CB_RESPONSE_OK;
    chunk[4] = (char)(20000 & 0xff);
    chunk[5] = (char)(20000 >> 8);
    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_FIRST,
                                         chunk, 64, total);
    xrdp_login_clip_process_channel_data(lc, XR_CHANNEL_FLAG_LAST,
                                         chunk, 64, total);
}

/******************************************************************************/
/* A response too big to reassemble still answers its request. Otherwise
 * the request stays outstanding, the next copy abandons it, and from then
 * on every response is taken for the one before it */
START_TEST(test_state__oversized_response_answers_its_request)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_oversized_response(lc);
    ck_assert_str_eq(g_caption, "");

    /* The user copies a password and pastes it, twice */
    feed_format_list(lc);
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "pw1");
    ck_assert_str_eq(g_caption, "pw1");

    feed_format_list(lc);
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "pw2");
    ck_assert_str_eq(g_caption, "pw1pw2");

    /* An oversized response to an abandoned request is matched too */
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_format_list(lc);
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_oversized_response(lc);
    feed_unicode_response(lc, "3");
    ck_assert_str_eq(g_caption, "pw1pw23");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A client that answers late: the timed out request is abandoned, and its
 * late response must not be taken for the next one */
START_TEST(test_state__late_response_after_timeout_is_discarded)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    /* Too soon to ask again */
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 0);
    g_clock_skew_ms += 6000;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    feed_unicode_response(lc, "old");
    ck_assert_str_eq(g_caption, "");
    feed_unicode_response(lc, "new");
    ck_assert_str_eq(g_caption, "new");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A client that never answers an abandoned request puts the count out by
 * one, so the next response is discarded in place of the missing one.
 * Paste must recover once that request times out, not stay dead */
START_TEST(test_state__unanswered_abandoned_request_does_not_kill_paste)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    /* Abandoned by a copy, and never answered */
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_format_list(lc);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "lost");
    ck_assert_str_eq(g_caption, "");

    g_clock_skew_ms += 6000;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "pw");
    ck_assert_str_eq(g_caption, "pw");

    /* And it keeps working */
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "2");
    ck_assert_str_eq(g_caption, "pw2");

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* A client that never answers at all just costs the timeout each time */
START_TEST(test_state__silent_client_allows_retry_after_timeout)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    g_clock_skew_ms += 6000;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    g_clock_skew_ms += 6000;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    ck_assert_int_eq(g_sent_count, 3);

    xrdp_login_clip_delete(lc);
}
END_TEST

/******************************************************************************/
/* Channel data reaches the login screen handler even with use_chansrv
 * still set, as it is after a failed login */
START_TEST(test_wm__login_clip_gets_channel_data_before_chansrv)
{
    struct xrdp_process pro;
    struct xrdp_mm mm;
    struct xrdp_wm wm;
    char buf[8 + 6];

    g_memset(&pro, 0, sizeof(pro));
    g_memset(&mm, 0, sizeof(mm));
    g_memset(&wm, 0, sizeof(wm));
    mm.use_chansrv = 1;
    wm.mm = &mm;
    wm.clip_chan_id = 5;
    pro.wm = &wm;
    wm.login_clip = xrdp_login_clip_create(&wm, wm.clip_chan_id);
    clear_sent();

    /* A CB_FORMAT_LIST, which the handler acknowledges */
    g_memset(buf, 0, sizeof(buf));
    buf[0] = CB_FORMAT_LIST;
    buf[4] = 6;
    buf[8] = CF_UNICODETEXT;
    callback(&pro, 0x5555,
             MAKELONG(wm.clip_chan_id,
                      XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST),
             sizeof(buf), (intptr_t)buf, sizeof(buf));
    ck_assert_int_eq(g_sent_count, 1);
    ck_assert_int_eq(sent_type(0), CB_FORMAT_LIST_RESPONSE);

    /* After handover, nothing in xrdp answers any more */
    xrdp_wm_login_clip_handover(&wm);
    ck_assert_ptr_null(wm.login_clip);
    clear_sent();
    callback(&pro, 0x5555,
             MAKELONG(wm.clip_chan_id,
                      XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST),
             sizeof(buf), (intptr_t)buf, sizeof(buf));
    ck_assert_int_eq(g_sent_count, 0);

    /* Safe to repeat */
    xrdp_wm_login_clip_handover(&wm);
}
END_TEST

/******************************************************************************/
/* Leaving the prompt keeps the handler, so it goes on answering the
 * client through a login attempt, but abandons any paste in flight */
START_TEST(test_wm__login_state_change_keeps_login_clip)
{
    struct xrdp_wm wm;
    struct xrdp_login_clip *lc = make_ready_lc(&wm);

    wm.login_clip = lc;
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);

    xrdp_wm_set_login_state(&wm, WMLS_START_CONNECT);
    ck_assert_ptr_eq(wm.login_clip, lc);
    xrdp_wm_set_login_state(&wm, WMLS_RESET);
    xrdp_wm_set_login_state(&wm, WMLS_USER_PROMPT);

    /* The old response is discarded, and a new paste works */
    feed_unicode_response(lc, "old");
    ck_assert_str_eq(g_caption, "");
    ck_assert_int_eq(xrdp_login_clip_request_paste(lc), 1);
    feed_unicode_response(lc, "new");
    ck_assert_str_eq(g_caption, "new");

    xrdp_wm_login_clip_handover(&wm);
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
    tcase_add_checked_fixture(tc, NULL, teardown_state);
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
    tcase_add_test(tc, test_is_paste_key__dvorak);
    tcase_add_test(tc, test_decode__plain_ascii);
    tcase_add_test(tc, test_decode__stops_at_crlf);
    tcase_add_test(tc, test_decode__drops_control_chars);
    tcase_add_test(tc, test_decode__surrogate_pair);
    tcase_add_test(tc, test_decode__drops_unpaired_surrogates);
    tcase_add_test(tc, test_decode__respects_out_capacity);
    tcase_add_test(tc, test_decode__odd_length_and_empty);
    tcase_add_test(tc, test_decode__cf_text);
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
    tcase_add_test(tc, test_format_list__rejected_list_offers_nothing);
    tcase_add_test(tc, test_state__create_sends_caps_then_monitor_ready);
    tcase_add_test(tc, test_state__create_fails_cleanly_out_of_memory);
    tcase_add_test(tc, test_state__format_list_is_acknowledged);
    tcase_add_test(tc, test_state__rejected_format_list_disables_paste);
    tcase_add_test(tc, test_state__paste_requests_unicode_text);
    tcase_add_test(tc, test_state__paste_before_ready_sends_nothing);
    tcase_add_test(tc, test_state__paste_with_no_target_sends_nothing);
    tcase_add_test(tc, test_state__second_paste_while_waiting_sends_nothing);
    tcase_add_test(tc, test_state__paste_with_no_text_format_sends_nothing);
    tcase_add_test(tc, test_state__unsolicited_data_response_is_ignored);
    tcase_add_test(tc, test_state__data_response_inserts_into_focused_edit);
    tcase_add_test(tc,
                   test_state__data_response_dropped_when_focused_control_not_a_child);
    tcase_add_test(tc, test_state__data_response_dropped_when_focus_moved);
    tcase_add_test(tc, test_state__data_response_dropped_when_edits_rebuilt);
    tcase_add_test(tc, test_state__data_response_dropped_when_popup_is_up);
    tcase_add_test(tc, test_state__data_response_dropped_when_modal_dialog_is_up);
    tcase_add_test(tc,
                   test_state__stale_response_after_new_format_list_is_discarded);
    tcase_add_test(tc, test_state__abandoned_paste_response_is_discarded);
    tcase_add_test(tc, test_state__failed_data_response_is_absorbed);
    tcase_add_test(tc, test_state__oversized_pdu_is_discarded);
    tcase_add_test(tc, test_state__truncated_header_is_dropped);
    tcase_add_test(tc, test_state__reassembles_split_pdu);
    tcase_add_test(tc, test_state__malformed_caps_pdu_still_advances_handshake);
    tcase_add_test(tc, test_state__caps_length_below_header_is_rejected);
    tcase_add_test(tc, test_state__cf_text_paste_inserts_into_focused_edit);
    tcase_add_test(tc, test_state__oversized_response_answers_its_request);
    tcase_add_test(tc, test_state__late_response_after_timeout_is_discarded);
    tcase_add_test(tc,
                   test_state__unanswered_abandoned_request_does_not_kill_paste);
    tcase_add_test(tc, test_state__silent_client_allows_retry_after_timeout);
    tcase_add_test(tc, test_wm__login_clip_gets_channel_data_before_chansrv);
    tcase_add_test(tc, test_wm__login_state_change_keeps_login_clip);

    suite_add_tcase(s, tc);

    return s;
}
