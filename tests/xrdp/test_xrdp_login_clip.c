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
#include "ms-rdpeclip.h"

#include "test_xrdp.h"

#define XK_v 0x0076

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
/* A NULL key info must never be dereferenced */
START_TEST(test_is_paste_key__null_key_info)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, NULL), 0);
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
Suite *
make_suite_login_clip(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("LoginClip");

    tc = tcase_create("xrdp_login_clip");
    tcase_add_test(tc, test_is_paste_key__ctrl_v);
    tcase_add_test(tc, test_is_paste_key__altgr_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__plain_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__ctrl_other_key_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__shift_insert);
    tcase_add_test(tc, test_is_paste_key__null_key_info);
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

    suite_add_tcase(s, tc);

    return s;
}
