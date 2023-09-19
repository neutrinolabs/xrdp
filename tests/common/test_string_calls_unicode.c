/*
 * The UTF-8 decoder tests are based on the UTF-8 decoder capability
 * and stress test" 2015-08-26 by Markus Kuhn. A copy of that file
 * named "UTF-8-test.txt" should be in the source directory for this file */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "string_calls.h"

#include "test_common.h"

// Abbreviate UCS_REPLACEMENT_CHARACTER for utf8_decode_sub_test arrays
#define URC UCS_REPLACEMENT_CHARACTER

struct utf8_decode_sub_test
{
    const char *testref;
    const char *utf8str;
    // This array will contain 0 values after the initialised part
    const char32_t expected[65];
};

// Abbreviate UCS_REPLACEMENT_CHARACTER for utf8_encode_sub_test arrays
#define E_URC { 0xef, 0xbf, 0xbd }

struct utf8_encode_sub_test
{
    const char *testref;
    char32_t c32;
    unsigned int expected_len;
    char expected_str[MAXLEN_UTF8_CHAR];
};

// Used as the simple test in UTF-8-test.txt
const static char greek_kosme[] =
    "\xce\xba" // GREEK SMALL LETTER KAPPA
    "\xe1\xbd\xb9" // GREEK SMALL LETTER OMICRON WITH OXIA
    "\xcf\x83" // GREEK SMALL LETTER SIGMA
    "\xce\xbc" // GREEK SMALL LETTER MU
    "\xce\xb5"; // GREEK SMALL LETTER EPSILON

// See Issue #2603
const static char simple_test_with_emoji[] =
    "Simple Test."
    "\xf0\x9f\x98\xa5"; // U+1F625 Disappointed But Relieved Face

/******************************************************************************/
/**
 * Function to decode a UTF-8 string and check the expected result
 *
 * @param st Pointer to the sub-test to run
 */
static void
run_utf8_decode_sub_test(const struct utf8_decode_sub_test *st)
{
    char32_t c;
    const char *p = st->utf8str;
    unsigned int index = 0;

    do
    {
        c = utf8_get_next_char(&p, NULL);

        if (c != st->expected[index])
        {
            ck_abort_msg("Sub-test section %s Index %u expected %x, got %x",
                         st->testref,
                         index, st->expected[index], c);
        }
        ++index;
    }
    while (c != 0);
}

/******************************************************************************/
/**
 * Function to run an array of decode sub-tests
 *
 * @param st Pointer to the first sub-test to run
 */
static void
run_decode_sub_test_array(const struct utf8_decode_sub_test *st)
{
    while (st->utf8str != NULL)
    {
        run_utf8_decode_sub_test(st++);
    }
}

/******************************************************************************/
/**
 * Function to encode a UTF-8 value and check the expected result
 *
 * @param st Pointer to the sub-test to run
 */
static void
run_utf8_encode_sub_test(const struct utf8_encode_sub_test *st)
{
    char actual_str[MAXLEN_UTF8_CHAR];
    unsigned int index;
    unsigned int actual_len = utf_char32_to_utf8(st->c32, actual_str);

    if (actual_len != st->expected_len)
    {
        ck_abort_msg("Sub-test %s Expected length of %u, got %u",
                     st->testref,
                     st->expected_len, actual_len);
    }

    for (index = 0 ; index < actual_len; ++index)
    {
        if (actual_str[index] != st->expected_str[index])
        {
            ck_abort_msg("Sub-test %s Character %u, expected %02x got %02x",
                         st->testref, index,
                         (int)(unsigned char)st->expected_str[index],
                         (int)(unsigned char)actual_str[index]);
        }
    }
}

/******************************************************************************/
/**
 * Function to run an array of encode sub-tests
 *
 * @param st Pointer to the first sub-test to run
 */
static void
run_encode_sub_test_array(const struct utf8_encode_sub_test *st)
{
    while (st->expected_len > 0)
    {
        run_utf8_encode_sub_test(st++);
    }
}


/******************************************************************************/
START_TEST(test_get_next_char__section_1)
{
    const struct utf8_decode_sub_test st =
    {
        "1",
        greek_kosme,
        {
            0x03ba, // GREEK SMALL LETTER KAPPA
            0x1f79, // GREEK SMALL LETTER OMICRON WITH OXIA
            0x03c3, // GREEK SMALL LETTER SIGMA
            0x03bc, // GREEK SMALL LETTER MU
            0x03b5  // GREEK SMALL LETTER EPSILON
        }
    };

    run_utf8_decode_sub_test(&st);
}
END_TEST

/******************************************************************************/
START_TEST(test_get_next_char__section_2)
{
    struct utf8_decode_sub_test tests[] =
    {
        // 2.1  First possible sequence of a certain length
        //
        // (2.1.1 Is tested separately)
        { "2.1.2", "\xc2\x80", { 0x80 } },
        { "2.1.3", "\xe0\xa0\x80", { 0x800 } },
        { "2.1.4", "\xf0\x90\x80\x80", { 0x10000 } },
        { "2.1.5", "\xf8\x88\x80\x80\x80", { URC } },
        { "2.1.6", "\xfc\x84\x80\x80\x80\x80", { URC } },

        // 2.2  Last possible sequence of a certain length
        { "2.2.1", "\x7f", { 0x7f } },
        { "2.2.2", "\xdf\xbf", { 0x7ff } },
        // Use U+0000FFFC instead of U+0000FFFF as our decoder
        // treats non-characters as an input error
        { "2.2.3", "\xef\xbf\xbc", { 0xfffc } },
        // U+001FFFFF is out-of-range
        { "2.2.4", "\xf7\xbf\xbf\xbf", { URC } },
        { "2.2.5", "\xfb\xbf\xbf\xbf\xbf", { URC } },
        { "2.2.6", "\xfd\xbf\xbf\xbf\xbf\xbf", { URC } },

        // 2.3  Other boundary conditions
        { "2.3.1", "\xed\x9f\xbf", { 0xd7ff } },
        { "2.3.2", "\xee\x80\x80", { 0xe000 } },
        { "2.3.3", "\xef\xbf\xbd", { 0xfffd } },
        // Don't use U+10FFFF (non-character)
        { "2.3.4", "\xf4\x8f\xbf\xbd", { 0x10fffd } },
        { "2.3.5", "\xf4\x90\x80\x80", { URC } },
        // Terminator
        { 0 }
    };

    // 2.1.1 is a '\0' which we use to terminate our strings. Test
    // it separately
    {
        const char *p = "";

        ck_assert_int_eq(utf8_get_next_char(&p, NULL), 0);
    }

    // Do the rest of the section 2 tests
    run_decode_sub_test_array(tests);
}
END_TEST

/******************************************************************************/
START_TEST(test_get_next_char__section_3)
{
    struct utf8_decode_sub_test tests[] =
    {
        // 3.1  Unexpected continuation bytes
        //
        // Each unexpected continuation byte should be separately
        // signalled as a malformed sequence of its own.
        { "3.1.1", "\x80", { URC } },
        { "3.1.2", "\xbf", { URC } },
        { "3.1.3", "\x80\xbf", { URC, URC } },
        { "3.1.4", "\x80\xbf\x80", { URC, URC, URC } },
        { "3.1.5", "\x80\xbf\x80\xbf", { URC, URC, URC, URC } },
        { "3.1.6", "\x80\xbf\x80\xbf\x80", { URC, URC, URC, URC, URC } },
        {
            "3.1.7",
            "\x80\xbf\x80\xbf\x80\xbf",
            { URC, URC, URC, URC, URC, URC }
        },
        {
            "3.1.8",
            "\x80\xbf\x80\xbf\x80\xbf\x80",
            { URC, URC, URC, URC, URC, URC, URC }
        },
        {
            "3.1.9",
            "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
            "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
            "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
            "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf",
            {
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC
            }
        },

        // 3.2  Lonely start characters
        {
            "3.2.1",
            "\xc0 \xc1 \xc2 \xc3 \xc4 \xc5 \xc6 \xc7 "
            "\xc8 \xc9 \xca \xcb \xcc \xcd \xce \xcf "
            "\xd0 \xd1 \xd2 \xd3 \xd4 \xd5 \xd6 \xd7 "
            "\xd8 \xd9 \xda \xdb \xdc \xdd \xde \xdf ",
            {
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' '
            }
        },
        {
            "3.2.2",
            "\xe0 \xe1 \xe2 \xe3 \xe4 \xe5 \xe6 \xe7 "
            "\xe8 \xe9 \xea \xeb \xec \xed \xee \xef ",
            {
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' '
            }
        },
        {
            "3.2.3",
            "\xf0 \xf1 \xf2 \xf3 \xf4 \xf5 \xf6 \xf7 ",
            {
                URC, ' ', URC, ' ', URC, ' ', URC, ' ',
                URC, ' ', URC, ' ', URC, ' ', URC, ' '
            }
        },
        {
            "3.2.4",
            "\xf8 \xf9 \xfa \xfb ",
            {
                URC, ' ', URC, ' ', URC, ' ', URC, ' '
            }
        },
        {
            "3.2.5", "\xfc \xfd ", { URC, ' ', URC, ' ' }
        },

        // 3.3  Sequences with last continuation byte missing
        //
        // From  UTF-8-test.txt:-
        //     All bytes of an incomplete sequence should be signalled as
        //     a single malformed sequence, i.e., you should see only a
        //     single replacement character in each of the next 10 tests.
        { "3.3.1", "\xc0", { URC } },
        { "3.3.2", "\xe0\x80", { URC } },
        { "3.3.3", "\xf0\x80\x80", { URC } },
        { "3.3.4", "\xf8\x80\x80\x80", { URC } },
        { "3.3.5", "\xfc\x80\x80\x80\x80", { URC } },

        { "3.3.6", "\xdf", { URC } },
        { "3.3.7", "\xef\xbf", { URC } },
        { "3.3.8", "\xf7\xbf\xbf", { URC} },
        { "3.3.9", "\xfb\xbf\xbf\xbf", { URC } },
        { "3.3.10", "\xfd\xbf\xbf\xbf\xbf", { URC } },

        // 3.4  Concatenation of incomplete sequences
        {
            "3,4",
            "\xc0"
            "\xe0\x80"
            "\xf0\x80\x80"
            "\xf8\x80\x80\x80"
            "\xfc\x80\x80\x80\x80"
            "\xdf"
            "\xef\xbf"
            "\xf7\xbf\xbf"
            "\xfb\xbf\xbf\xbf"
            "\xfd\xbf\xbf\xbf\xbf",
            {
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC
            }
        },

        // 3.5  Impossible bytes
        { "3.5.1", "\xfe", { URC } },
        { "3.5.2", "\xff", { URC } },
        { "3.5.3", "\xfe\xfe\xff\xff", { URC, URC, URC, URC } },
        // Terminator
        { 0 }
    };

    run_decode_sub_test_array(tests);
}
END_TEST

/******************************************************************************/
START_TEST(test_get_next_char__section_4)
{
    struct utf8_decode_sub_test tests[] =
    {
        // 4.1  Examples of an overlong ASCII character
        //
        // With a safe UTF-8 decoder, all of the following five
        // overlong representations of the ASCII character slash ("/")
        // should be rejected like a malformed UTF-8 sequence, for
        // instance by substituting it with a replacement character. If
        // you see a slash below, you do not have a safe UTF-8 decoder!
        { "4.1.1", "\xc0\xaf", { URC } },
        { "4.1.2", "\xe0\x80\xaf", { URC } },
        { "4.1.3", "\xf0\x80\x80\xaf", { URC } },
        { "4.1.4", "\xf8\x80\x80\x80\xaf", { URC } },
        { "4.1.5", "\xfc\x80\x80\x80\x80\xaf", { URC } },

        // 4.2  Maximum overlong sequences

        // Below you see the highest Unicode value that is still resulting
        // in an overlong sequence if represented with the given number of
        // bytes. This is a boundary test for safe UTF-8 decoders. All
        // five characters should be rejected like malformed UTF-8
        // sequences.
        { "4.2.1", "\xc1\xbf", { URC } },
        { "4.2.2", "\xe0\x9f\xbf", { URC } },
        { "4.2.3", "\xf0\x8f\xbf\xbf", { URC } },
        { "4.2.4", "\xf8\x87\xbf\xbf\xbf", { URC } },
        { "4.2.5", "\xfc\x83\xbf\xbf\xbf\xbf", { URC } },

        // 4.3  Overlong representation of the NUL character

        // The following five sequences should also be rejected like
        // malformed UTF-8 sequences and should not be treated like the
        // ASCII NUL character.
        { "4.3.1", "\xc0\x80", { URC } },
        { "4.3.2", "\xe0\x80\x80", { URC } },
        { "4.3.3", "\xf0\x80\x80\x80", { URC } },
        { "4.3.4", "\xf8\x80\x80\x80\x80", { URC } },
        { "4.3.5", "\xfc\x80\x80\x80\x80\x80", { URC } },

        // Terminator
        { 0 }
    };

    run_decode_sub_test_array(tests);
}
END_TEST

/******************************************************************************/
START_TEST(test_get_next_char__section_5)
{
    struct utf8_decode_sub_test tests[] =
    {
        // 5  Illegal code positions

        // The following UTF-8 sequences should be rejected like
        // malformed sequences, because they never represent valid
        // ISO 10646 characters and a UTF-8 decoder that accepts them
        // might introduce security problems comparable to overlong
        // UTF-8 sequences.

        // 5.1 Single UTF-16 surrogates
        { "5.1.1", "\xed\xa0\x80", { URC } },
        { "5.1.2", "\xed\xad\xbf", { URC } },
        { "5.1.3", "\xed\xae\x80", { URC } },
        { "5.1.4", "\xed\xaf\xbf", { URC } },
        { "5.1.5", "\xed\xb0\x80", { URC } },
        { "5.1.6", "\xed\xbe\x80", { URC } },
        { "5.1.7", "\xed\xbf\xbf", { URC } },

        // 5.2 Paired UTF-16 surrogates
        { "5.2.1", "\xed\xa0\x80\xed\xb0\x80", { URC, URC } },
        { "5.2.2", "\xed\xa0\x80\xed\xbf\xbf", { URC, URC } },
        { "5.2.3", "\xed\xad\xbf\xed\xb0\x80", { URC, URC } },
        { "5.2.4", "\xed\xad\xbf\xed\xbf\xbf", { URC, URC } },
        { "5.2.5", "\xed\xae\x80\xed\xb0\x80", { URC, URC } },
        { "5.2.6", "\xed\xae\x80\xed\xbf\xbf", { URC, URC } },
        { "5.2.7", "\xed\xaf\xbf\xed\xb0\x80", { URC, URC } },
        { "5.2.8", "\xed\xaf\xbf\xed\xbf\xbf", { URC, URC } },

        // 5.3 Noncharacter code positions

        // The following "noncharacters" are "reserved for internal
        // use" by applications, and according to older versions of
        // the Unicode Standard "should never be interchanged". Unicode
        // Corrigendum #9 dropped the latter restriction. Nevertheless,
        // their presence in incoming UTF-8 data can remain a potential
        // security risk, depending on what use is made of these codes
        // subsequently. Examples of such internal use:
        //
        //  - Some file APIs with 16-bit characters may use the integer
        //    value -1 = U+FFFF to signal an end-of-file (EOF) or error
        //    condition.
        //
        //  - In some UTF-16 receivers, code point U+FFFE might trigger
        //    a byte-swap operation (to convert between UTF-16LE and
        //    UTF-16BE).
        // With such internal use of noncharacters, it may be desirable
        // and safer to block those code points in UTF-8 decoders, as
        // they should never occur legitimately in incoming UTF-8 data,
        // and could trigger unsafe behaviour in subsequent processing.

        // Particularly problematic noncharacters in 16-bit applications:
        { "5.3.1", "\xef\xbf\xbe", { URC } },
        { "5.3.2", "\xef\xbf\xbf", { URC } },

        // Other noncharacters:
        {
            "5.3.3",
            // Non-characters in "Arabic Presentation Forms-A" (BMP)
            "\xef\xb7\x90" "\xef\xb7\x91" "\xef\xb7\x92" "\xef\xb7\x93"
            "\xef\xb7\x94" "\xef\xb7\x95" "\xef\xb7\x96" "\xef\xb7\x97"
            "\xef\xb7\x98" "\xef\xb7\x99" "\xef\xb7\x9a" "\xef\xb7\x9b"
            "\xef\xb7\x9c" "\xef\xb7\x9d" "\xef\xb7\x9e" "\xef\xb7\x9f"
            "\xef\xb7\xa0" "\xef\xb7\xa1" "\xef\xb7\xa2" "\xef\xb7\xa3"
            "\xef\xb7\xa4" "\xef\xb7\xa5" "\xef\xb7\xa6" "\xef\xb7\xa7"
            "\xef\xb7\xa8" "\xef\xb7\xa9" "\xef\xb7\xaa" "\xef\xb7\xab"
            "\xef\xb7\xac" "\xef\xb7\xad" "\xef\xb7\xae" "\xef\xb7\xaf",
            {
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC
            }
        },

        {
            "5.3.4",
            "\xf0\x9f\xbf\xbe" "\xf0\x9f\xbf\xbf" // U+0001FFFE U+0001FFFF
            "\xf0\xaf\xbf\xbe" "\xf0\xaf\xbf\xbf" // U+0002FFFE U+0002FFFF
            "\xf0\xbf\xbf\xbe" "\xf0\xbf\xbf\xbf" // U+0003FFFE U+0003FFFF
            "\xf1\x8f\xbf\xbe" "\xf1\x8f\xbf\xbf" // U+0004FFFE U+0004FFFF
            "\xf1\x9f\xbf\xbe" "\xf1\x9f\xbf\xbf" // U+0005FFFE U+0005FFFF
            "\xf1\xaf\xbf\xbe" "\xf1\xaf\xbf\xbf" // U+0006FFFE U+0006FFFF
            "\xf1\xbf\xbf\xbe" "\xf1\xbf\xbf\xbf" // U+0007FFFE U+0007FFFF
            "\xf2\x8f\xbf\xbe" "\xf2\x8f\xbf\xbf" // U+0008FFFE U+0008FFFF
            "\xf2\x9f\xbf\xbe" "\xf2\x9f\xbf\xbf" // U+0009FFFE U+0009FFFF
            "\xf2\xaf\xbf\xbe" "\xf2\xaf\xbf\xbf" // U+000AFFFE U+000AFFFF
            "\xf2\xbf\xbf\xbe" "\xf2\xbf\xbf\xbf" // U+000BFFFE U+000BFFFF
            "\xf3\x8f\xbf\xbe" "\xf3\x8f\xbf\xbf" // U+000CFFFE U+000CFFFF
            "\xf3\x9f\xbf\xbe" "\xf3\x9f\xbf\xbf" // U+000DFFFE U+000DFFFF
            "\xf3\xaf\xbf\xbe" "\xf3\xaf\xbf\xbf" // U+000EFFFE U+000EFFFF
            "\xf3\xbf\xbf\xbe" "\xf3\xbf\xbf\xbf" // U+000FFFFE U+000FFFFF
            "\xf4\x8f\xbf\xbe" "\xf4\x8f\xbf\xbf",// U+0010FFFE U+0010FFFF
            {
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC,
                URC, URC, URC, URC, URC, URC, URC, URC
            }
        },

        // Last line of UTF8-test.txt
        { "TheEnd", "THE END\n", { 'T', 'H', 'E', ' ', 'E', 'N', 'D', '\n'} },

        // Terminator
        { 0 }

    };

    run_decode_sub_test_array(tests);
}
END_TEST

/******************************************************************************/
START_TEST(test_utf_char32_to_utf8)
{
    struct utf8_encode_sub_test tests[] =
    {

        // E2.1  First possible sequence of a certain length
        //
        { "E2.1.1", 0, 1, { 0 } },
        { "E2.1.2", 0x80, 2, { 0xc2, 0x80 } },
        { "E2.1.3", 0x800, 3, { 0xe0, 0xa0, 0x80 } },
        { "E2.1.4", 0x10000, 4, { 0xf0, 0x90, 0x80, 0x80 } },

        // E2.2  Last possible sequence of a certain length
        { "E2.2.1", 0x7f, 1, { 0x7f } },
        { "E2.2.2", 0x7ff, 2, { 0xdf, 0xbf } },
        { "E2.2.3", 0xfffc, 3, { 0xef, 0xbf, 0xbc } }, // See 2.1.3 above
        { "E2.2.4", 0x1FFFFF, 3, E_URC }, // out-of-range

        // E2.3  Other boundary conditions
        { "E2.3.1", 0xd7ff, 3, { 0xed, 0x9f, 0xbf } },
        { "E2.3.2", 0xe000, 3, { 0xee, 0x80, 0x80 } },
        { "E2.3.3", 0xfffd, 3, { 0xef, 0xbf, 0xbd } },
        { "E2.3.4", 0x10fffd, 4, { 0xf4, 0x8f, 0xbf, 0xbd } }, // See 2.3.4 above
        // E2.3.5 - not tested

        // E5.1 Single UTF-16 surrogates
        { "E5.1.1", 0xd800, 3, E_URC },
        { "E5.1.2", 0xdb7f, 3, E_URC },
        { "E5.1.3", 0xdb80, 3, E_URC },
        { "E5.1.4", 0xdbff, 3, E_URC },
        { "E5.1.5", 0xdc00, 3, E_URC },
        { "E5.1.6", 0xdf80, 3, E_URC },
        { "E5.1.7", 0xdfff, 3, E_URC },

        // E5.3 Non-character code positions
        { "E5.3.3(0)", 0xfdd0, 3, E_URC },
        { "E5.3.3(1)", 0xfdd1, 3, E_URC },
        { "E5.3.3(2)", 0xfdd2, 3, E_URC },
        { "E5.3.3(3)", 0xfdd3, 3, E_URC },
        { "E5.3.3(4)", 0xfdd4, 3, E_URC },
        { "E5.3.3(5)", 0xfdd5, 3, E_URC },
        { "E5.3.3(6)", 0xfdd6, 3, E_URC },
        { "E5.3.3(7)", 0xfdd7, 3, E_URC },
        { "E5.3.3(8)", 0xfdd8, 3, E_URC },
        { "E5.3.3(9)", 0xfdd9, 3, E_URC },
        { "E5.3.3(10)", 0xfdda, 3, E_URC },
        { "E5.3.3(11)", 0xfddb, 3, E_URC },
        { "E5.3.3(12)", 0xfddc, 3, E_URC },
        { "E5.3.3(13)", 0xfddd, 3, E_URC },
        { "E5.3.3(14)", 0xfdde, 3, E_URC },
        { "E5.3.3(15)", 0xfddf, 3, E_URC },
        { "E5.3.3(16)", 0xfde0, 3, E_URC },
        { "E5.3.3(17)", 0xfde1, 3, E_URC },
        { "E5.3.3(18)", 0xfde2, 3, E_URC },
        { "E5.3.3(19)", 0xfde3, 3, E_URC },
        { "E5.3.3(20)", 0xfde4, 3, E_URC },
        { "E5.3.3(21)", 0xfde5, 3, E_URC },
        { "E5.3.3(22)", 0xfde6, 3, E_URC },
        { "E5.3.3(23)", 0xfde7, 3, E_URC },
        { "E5.3.3(24)", 0xfde8, 3, E_URC },
        { "E5.3.3(25)", 0xfde9, 3, E_URC },
        { "E5.3.3(26)", 0xfdea, 3, E_URC },
        { "E5.3.3(27)", 0xfdeb, 3, E_URC },
        { "E5.3.3(28)", 0xfdec, 3, E_URC },
        { "E5.3.3(29)", 0xfded, 3, E_URC },
        { "E5.3.3(30)", 0xfdee, 3, E_URC },
        { "E5.3.3(31)", 0xfdef, 3, E_URC },
        { "E5.3.4(0)", 0x1fffe, 3, E_URC },
        { "E5.3.4(1)", 0x1ffff, 3, E_URC },
        { "E5.3.4(2)", 0x2fffe, 3, E_URC },
        { "E5.3.4(3)", 0x2ffff, 3, E_URC },
        { "E5.3.4(4)", 0x3fffe, 3, E_URC },
        { "E5.3.4(5)", 0x3ffff, 3, E_URC },
        { "E5.3.4(6)", 0x4fffe, 3, E_URC },
        { "E5.3.4(7)", 0x4ffff, 3, E_URC },
        { "E5.3.4(8)", 0x5fffe, 3, E_URC },
        { "E5.3.4(9)", 0x5ffff, 3, E_URC },
        { "E5.3.4(10)", 0x6fffe, 3, E_URC },
        { "E5.3.4(11)", 0x6ffff, 3, E_URC },
        { "E5.3.4(12)", 0x7fffe, 3, E_URC },
        { "E5.3.4(13)", 0x7ffff, 3, E_URC },
        { "E5.3.4(14)", 0x8fffe, 3, E_URC },
        { "E5.3.4(15)", 0x8ffff, 3, E_URC },
        { "E5.3.4(16)", 0x9fffe, 3, E_URC },
        { "E5.3.4(17)", 0x9ffff, 3, E_URC },
        { "E5.3.4(18)", 0xafffe, 3, E_URC },
        { "E5.3.4(19)", 0xaffff, 3, E_URC },
        { "E5.3.4(20)", 0xbfffe, 3, E_URC },
        { "E5.3.4(21)", 0xbffff, 3, E_URC },
        { "E5.3.4(22)", 0xcfffe, 3, E_URC },
        { "E5.3.4(23)", 0xcffff, 3, E_URC },
        { "E5.3.4(24)", 0xdfffe, 3, E_URC },
        { "E5.3.4(25)", 0xdffff, 3, E_URC },
        { "E5.3.4(26)", 0xefffe, 3, E_URC },
        { "E5.3.4(27)", 0xeffff, 3, E_URC },
        { "E5.3.4(28)", 0xffffe, 3, E_URC },
        { "E5.3.4(29)", 0xfffff, 3, E_URC },
        { "E5.3.4(30)", 0x10fffe, 3, E_URC },
        { "E5.3.4(31)", 0x10ffff, 3, E_URC },
        { "E5.99.0", 'T', 1, { 'T' } },
        { "E5.99.1", 'H', 1, { 'H' } },
        { "E5.99.2", 'E', 1, { 'E' } },
        { "E5.99.3", ' ', 1, { ' ' } },
        { "E5.99.4", 'E', 1, { 'E' } },
        { "E5.99.5", 'N', 1, { 'N' } },
        { "E5.99.6", 'D', 1, { 'D' } },

        // Terminator
        { 0 }
    };

    run_encode_sub_test_array(tests);
}
END_TEST

/******************************************************************************/
START_TEST(test_utf8_char_count)
{
    // Check function can cope with NULL argument
    ck_assert_int_eq(utf8_char_count(NULL), 0);

    unsigned int kosme_strlen = strlen(greek_kosme);
    unsigned int kosme_len = utf8_char_count(greek_kosme);

    // All characters map to two bytes except for the 'omicrom with oxia'
    // which maps to three
    ck_assert_int_eq(kosme_strlen, 2 + 3 + 2 + 2 + 2);
    ck_assert_int_eq(kosme_len, 5);

    unsigned int simple_test_strlen = strlen(simple_test_with_emoji);
    unsigned int simple_test_len = utf8_char_count(simple_test_with_emoji);

    ck_assert_int_eq(simple_test_strlen,
                     (1 + 1 + 1 + 1 + 1 + 1 ) + // Simple
                     1 +
                     (1 + 1 + 1 + 1 ) + // Test
                     1 +
                     4);               // emoji
    // The emoji is 4 bytes - all others are 1
    ck_assert_int_eq(simple_test_len, simple_test_strlen - 3);
}
END_TEST

/******************************************************************************/
START_TEST(test_utf8_as_utf16_word_count)
{
    unsigned int kosme_count =
        utf8_as_utf16_word_count(greek_kosme, strlen(greek_kosme));

    ck_assert_int_eq(kosme_count, 5); // All characters in BMP

    unsigned int simple_test_count =
        utf8_as_utf16_word_count(simple_test_with_emoji,
                                 strlen(simple_test_with_emoji));

    ck_assert_int_eq(simple_test_count,
                     (1 + 1 + 1 + 1 + 1 + 1 ) + // Simple
                     1 +
                     (1 + 1 + 1 + 1 ) + // Test
                     1 +
                     2); // emoji
}
END_TEST

/******************************************************************************/
START_TEST(test_utf8_add_char_at)
{
#define TEST_SIZE sizeof(simple_test_with_emoji)

    // Type pairing a string position with a Unicode char
    struct pos_to_char_map
    {
        unsigned int pos;
        char32_t c32;
    };

    // Buffer for constructing the string
    char buff[TEST_SIZE];

    // A pseudo-random map of the characters in simple_test_with_emoji
    const struct pos_to_char_map map[] =
    {
        { 0, 'l' },
        { 0, 'S' },
        { 1, 'i' },
        { 2, 'm' },
        { 4, 0x1f625 },
        { 4, '.' },
        { 4, 'e' },
        { 5, 'T' },
        { 3, 'p' },
        { 7, 't' },
        { 7, 'e' },
        { 8, 's' },
        { 6, ' ' },
        { 0 }
    };

    buff[0] = '\0';

    // Construct the string in a pseudo-random fashion

    const struct pos_to_char_map *p;
    for (p = map; p->c32 != 0 ; ++p)
    {
        if (!utf8_add_char_at(buff, TEST_SIZE, p->c32, p->pos))
        {
            ck_abort_msg("test_utf8_add_char_at: "
                         "Can't insert char %x at pos %u",
                         p->c32,
                         p->pos);
        }
    }

    // Should have reached the buffer size by now
    ck_assert_int_eq(strlen(buff), TEST_SIZE - 1);

    // Check the string is what we expect
    ck_assert_int_eq(strcmp(buff, simple_test_with_emoji), 0);

    // Try to insert another character
    if (utf8_add_char_at(buff, TEST_SIZE, ' ', 0))
    {
        ck_abort_msg("test_utf8_add_char_at: "
                     "Insert succeeded but should have failed");
    }

#undef TEST_SIZE
}
END_TEST

/******************************************************************************/
START_TEST(test_utf8_remove_char_at)
{
#define TEST_SIZE sizeof(simple_test_with_emoji)
    // Type pairing a string position with a Unicode char
    struct pos_to_char_map
    {
        unsigned int pos;
        char32_t c32;
    };

    // Buffer for deconstructing the string
    char buff[TEST_SIZE];

    // A pseudo-random map of the characters in simple_test_with_emoji
    const struct pos_to_char_map map[] =
    {
        { 2, 'm' },
        { 7, 'e' },
        { 5, ' ' },
        { 1, 'i' },
        { 2, 'l' },
        { 3, 'T' },
        { 6, 0x1f625 },
        { 2, 'e' },
        { 3, 't' },
        { 3, '.' },
        { 2, 's' },
        { 1, 'p' },
        { 0, 'S' },
        { 0 }
    };

    char32_t c32;

    strcpy(buff, simple_test_with_emoji);

    // Deconstruct the string in a pseudo-random fashion
    const struct pos_to_char_map *p;
    for (p = map; p->c32 != 0 ; ++p)
    {
        c32 = utf8_remove_char_at(buff, p->pos);
        if (c32 != p->c32)
        {
            ck_abort_msg("test_utf8_remove_char_at: "
                         "remove char at pos %u was %x, expected %x",
                         p->pos, c32, p->c32);
        }
    }

    // Should have emptied the buffer by now
    ck_assert_int_eq(buff[0], '\0');

    // Try to remove other characters
    c32 = utf8_remove_char_at(buff, 0);
    ck_assert_int_eq(c32, 0);
    c32 = utf8_remove_char_at(buff, 99);
    ck_assert_int_eq(c32, 0);
    ck_assert_int_eq(buff[0], '\0');

#undef TEST_SIZE
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_string_unicode(void)
{
    Suite *s;
    TCase *tc_unicode;

    s = suite_create("String");

    tc_unicode = tcase_create("Unicode");
    suite_add_tcase(s, tc_unicode);
    tcase_add_test(tc_unicode, test_get_next_char__section_1);
    tcase_add_test(tc_unicode, test_get_next_char__section_2);
    tcase_add_test(tc_unicode, test_get_next_char__section_3);
    tcase_add_test(tc_unicode, test_get_next_char__section_4);
    tcase_add_test(tc_unicode, test_get_next_char__section_5);
    tcase_add_test(tc_unicode, test_utf_char32_to_utf8);
    tcase_add_test(tc_unicode, test_utf8_char_count);
    tcase_add_test(tc_unicode, test_utf8_as_utf16_word_count);
    tcase_add_test(tc_unicode, test_utf8_add_char_at);
    tcase_add_test(tc_unicode, test_utf8_remove_char_at);

    return s;
}
