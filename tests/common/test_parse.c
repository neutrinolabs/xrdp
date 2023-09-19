#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "parse.h"

#include "test_common.h"

#define ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

const static char
utf8_simple_test_with_emoji[] =
    "Simple Test."
    "\xf0\x9f\x98\xa5"; // U+1F625 Disappointed But Relieved Face

const static char16_t
utf16_simple_test_with_emoji[] =
{
    'S', 'i', 'm', 'p', 'l', 'e', ' ', 'T', 'e', 's', 't', '.',
    0xd83d, 0xde25, // U+1F625
    0 // terminator
};

/******************************************************************************/
START_TEST(test_out_utf8_as_utf16_le)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);
    out_utf8_as_utf16_le(s, utf8_simple_test_with_emoji,
                         sizeof(utf8_simple_test_with_emoji)); // Include term
    s_mark_end(s);

    // Rewind the stream
    s->p = s->data;
    unsigned int i;

    for (i = 0; i < ELEMENTS(utf16_simple_test_with_emoji); ++i)
    {
        char16_t val;
        in_uint16_le(s, val);
        if (val != utf16_simple_test_with_emoji[i])
        {
            ck_abort_msg("test_out_utf8_as_utf16_le: "
                         "Index %u expected %x, got %x",
                         i, utf16_simple_test_with_emoji[i], val);
        }
    }

    ck_assert_int_eq(s_check_end(s), 1);

    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_in_utf16_le_fixed_as_utf8)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);

    // Write the stream without a terminator
    unsigned int i;
    for (i = 0; i < ELEMENTS(utf16_simple_test_with_emoji) - 1; ++i)
    {
        out_uint16_le(s, utf16_simple_test_with_emoji[i]);
    }
    s_mark_end(s);

    // Rewind the stream
    s->p = s->data;

    char buff[256];
    unsigned int len;

    // Check the length call
    len = in_utf16_le_fixed_as_utf8_length(s, i);
    ck_assert_int_eq(len, sizeof(utf8_simple_test_with_emoji));

    // Now read the string, checking for the same length
    unsigned int read_len;
    read_len = in_utf16_le_fixed_as_utf8(s, i, buff, sizeof(buff));
    ck_assert_int_eq(len, read_len);

    // Should be at the end of the buffer
    ck_assert_int_eq(s_check_end(s), 1);

    // Check the contents are as expected
    int cmp = memcmp(buff, utf8_simple_test_with_emoji,
                     sizeof(utf8_simple_test_with_emoji));
    ck_assert_int_eq(cmp, 0);

    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_in_utf16_le_terminated_as_utf8)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);

    // Write the stream with the terminator
    unsigned int i;
    for (i = 0; i < ELEMENTS(utf16_simple_test_with_emoji); ++i)
    {
        out_uint16_le(s, utf16_simple_test_with_emoji[i]);
    }
    s_mark_end(s);

    // Rewind the stream
    s->p = s->data;

    char buff[256];
    unsigned int len;

    // Check the length call
    len = in_utf16_le_terminated_as_utf8_length(s);
    ck_assert_int_eq(len, sizeof(utf8_simple_test_with_emoji));

    // Now read the string, checking for the same length
    unsigned int read_len;
    read_len = in_utf16_le_terminated_as_utf8(s, buff, sizeof(buff));
    ck_assert_int_eq(len, read_len);

    // Should be at the end of the buffer
    ck_assert_int_eq(s_check_end(s), 1);

    // Check the contents are as expected
    int cmp = memcmp(buff, utf8_simple_test_with_emoji,
                     sizeof(utf8_simple_test_with_emoji));
    ck_assert_int_eq(cmp, 0);

    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_in_utf16_le_significant_chars)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);

    const struct
    {
        struct
        {
            char16_t high; // Set to 0 for a single UTF-16 word
            char16_t low;
        } pair;
        char32_t expected;
    } tests[] =
    {
        // Single high surrogates are bad
        { { 0, 0xd800 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xdbff }, UCS_REPLACEMENT_CHARACTER },
        // Single low surrogates are bad
        { { 0, 0xdc00 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xdfff }, UCS_REPLACEMENT_CHARACTER },
        // Values before and after surrogate range
        { { 0, 0xd7ff }, 0xd7ff },
        { { 0, 0xe000 }, 0xe000 },
        // First and last non-surrogate pair values (don't use
        // 0xfffe and 0xffff for this test as they are non-characters,
        // and 0xfffd is the replacement character)
        { { 0, 0 }, 0 },
        { { 0, 0xfffc }, 0xfffc },
        { { 0, 0xfffd }, UCS_REPLACEMENT_CHARACTER },
        // First and last surrogate pair values (don't use
        // 0x10fffe and 0x10ffff for this test as they are non-characters)
        { { 0xd800, 0xdc00 }, 0x10000 },
        { { 0xdbff, 0xdffd }, 0x10fffd },
        // End-of-plane non-characters (BMP) and the characters before them
        { { 0xd83f, 0xdffd }, 0x1fffd },
        { { 0xd83f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+1FFFE
        { { 0xd83f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+1FFFF
        { { 0xd87f, 0xdffd }, 0x2fffd },
        { { 0xd87f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+2FFFE
        { { 0xd87f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+2FFFF
        { { 0xd8bf, 0xdffd }, 0x3fffd },
        { { 0xd8bf, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+3FFFE
        { { 0xd8bf, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+3FFFF
        { { 0xd8ff, 0xdffd }, 0x4fffd },
        { { 0xd8ff, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+4FFFE
        { { 0xd8ff, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+4FFFF
        { { 0xd93f, 0xdffd }, 0x5fffd },
        { { 0xd93f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+5FFFE
        { { 0xd93f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+5FFFF
        { { 0xd97f, 0xdffd }, 0x6fffd },
        { { 0xd97f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+6FFFE
        { { 0xd97f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+6FFFF
        { { 0xd9bf, 0xdffd }, 0x7fffd },
        { { 0xd9bf, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+7FFFE
        { { 0xd9bf, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+7FFFF
        { { 0xd9ff, 0xdffd }, 0x8fffd },
        { { 0xd9ff, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+8FFFE
        { { 0xd9ff, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+8FFFF
        { { 0xda3f, 0xdffd }, 0x9fffd },
        { { 0xda3f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+9FFFE
        { { 0xda3f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+9FFFF
        { { 0xda7f, 0xdffd }, 0xafffd },
        { { 0xda7f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+AFFFE
        { { 0xda7f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+AFFFF
        { { 0xdabf, 0xdffd }, 0xbfffd },
        { { 0xdabf, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+BFFFE
        { { 0xdabf, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+BFFFF
        { { 0xdaff, 0xdffd }, 0xcfffd },
        { { 0xdaff, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+CFFFE
        { { 0xdaff, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+CFFFF
        { { 0xdb3f, 0xdffd }, 0xdfffd },
        { { 0xdb3f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+DFFFE
        { { 0xdb3f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+DFFFF
        { { 0xdb7f, 0xdffd }, 0xefffd },
        { { 0xdb7f, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+EFFFE
        { { 0xdb7f, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+EFFFF
        { { 0xdbbf, 0xdffd }, 0xffffd },
        { { 0xdbbf, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+FFFFE
        { { 0xdbbf, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+FFFFF
        { { 0xdbff, 0xdffd }, 0x10fffd },
        { { 0xdbff, 0xdffe }, UCS_REPLACEMENT_CHARACTER }, // U+10FFFE
        { { 0xdbff, 0xdfff }, UCS_REPLACEMENT_CHARACTER }, // U+10FFFF
        // Non-characters in "Arabic Presentation Forms-A"
        { { 0, 0xfdd0 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd1 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd2 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd3 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd4 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd5 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd6 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd7 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd8 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdd9 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdda }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfddb }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfddc }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfddd }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdde }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfddf }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde0 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde1 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde2 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde3 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde4 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde5 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde6 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde7 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde8 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfde9 }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdea }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdeb }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdec }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfded }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdee }, UCS_REPLACEMENT_CHARACTER },
        { { 0, 0xfdef }, UCS_REPLACEMENT_CHARACTER }
    };

    unsigned int i;
    for (i = 0; i < ELEMENTS(tests); ++i)
    {
        char buff[256];
        unsigned int word_count;
        init_stream(s, 8192);

        word_count = 0;
        if (tests[i].pair.high != 0)
        {
            out_uint16_le(s, tests[i].pair.high);
            ++word_count;
        }
        out_uint16_le(s, tests[i].pair.low);
        ++word_count;
        s_mark_end(s);

        // Rewind the stream
        s->p = s->data;

        // Read in one UTF-16 LE character as UTF-32
        in_utf16_le_fixed_as_utf8(s, word_count, buff, sizeof(buff));
        const char *p = buff;
        char32_t c32 = utf8_get_next_char(&p, NULL);

        if (c32 != tests[i].expected)
        {
            ck_abort_msg("test_in_utf16_le_significant_chars: "
                         "Index %u for {%x, %x}, expected %x, got %x",
                         i,  tests[i].pair.high, tests[i].pair.low,
                         tests[i].expected, c32);
        }
    }

    free_stream(s);
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_parse(void)
{
    Suite *s;
    TCase *tc_unicode;

    s = suite_create("Parse");

    tc_unicode = tcase_create("Unicode");
    suite_add_tcase(s, tc_unicode);
    tcase_add_test(tc_unicode, test_out_utf8_as_utf16_le);
    tcase_add_test(tc_unicode, test_in_utf16_le_fixed_as_utf8);
    tcase_add_test(tc_unicode, test_in_utf16_le_terminated_as_utf8);
    tcase_add_test(tc_unicode, test_in_utf16_le_significant_chars);

    return s;
}
