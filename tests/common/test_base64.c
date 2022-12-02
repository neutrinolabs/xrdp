
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "os_calls.h"
#include "string_calls.h"
#include "base64.h"

#include "test_common.h"
/*
* These are the example test strings in RFC4648(10)
*/
static const char *rfc4648_ex1_text = "";
static const char *rfc4648_ex1_b64 = "";
static const char *rfc4648_ex2_text = "f";
static const char *rfc4648_ex2_b64 = "Zg==";
static const char *rfc4648_ex3_text = "fo";
static const char *rfc4648_ex3_b64 = "Zm8=";
static const char *rfc4648_ex4_text = "foo";
static const char *rfc4648_ex4_b64 = "Zm9v";
static const char *rfc4648_ex5_text = "foob";
static const char *rfc4648_ex5_b64 = "Zm9vYg==";
static const char *rfc4648_ex6_text = "fooba";
static const char *rfc4648_ex6_b64 = "Zm9vYmE=";
static const char *rfc4648_ex7_text = "foobar";
static const char *rfc4648_ex7_b64 = "Zm9vYmFy";

/* Every single valid base64 character, except padding */
static const char *all_b64 =
    "ABCDEFGHIJKL"
    "MNOPQRSTUVWX"
    "YZabcdefghij"
    "klmnopqrstuv"
    "wxyz01234567"
    "89+/";

/* What we should get as binary if we decode this */
static const char all_b64_decoded[] =
{
    '\x00', '\x10', '\x83', '\x10', '\x51', '\x87', '\x20', '\x92', '\x8b',
    '\x30', '\xd3', '\x8f', '\x41', '\x14', '\x93', '\x51', '\x55', '\x97',
    '\x61', '\x96', '\x9b', '\x71', '\xd7', '\x9f', '\x82', '\x18', '\xa3',
    '\x92', '\x59', '\xa7', '\xa2', '\x9a', '\xab', '\xb2', '\xdb', '\xaf',
    '\xc3', '\x1c', '\xb3', '\xd3', '\x5d', '\xb7', '\xe3', '\x9e', '\xbb',
    '\xf3', '\xdf', '\xbf'
};

static void
test_rfc4648_to_b64(const char *plaintext, size_t len, const char *b64)
{
    char buff[256];
    size_t result;

    result = base64_encode(plaintext, len, buff, sizeof(buff));
    ck_assert_int_eq(result, len);
    ck_assert_str_eq(buff, b64);

}

/* Text-only encoder wrapper */
static void
test_rfc4648_to_b64_text(const char *plaintext, const char *b64)
{
    test_rfc4648_to_b64(plaintext, g_strlen(plaintext), b64);
}

/* Text-only decoder wrapper for valid base64 */
static void
test_rfc4648_from_b64_text(const char *b64, const char *text)
{
    char buff[256];
    size_t actual_len;
    int result;

    result = base64_decode(b64, buff, sizeof(buff), &actual_len);
    ck_assert_int_eq(result, 0);
    ck_assert_int_lt(actual_len, sizeof(buff));
    buff[actual_len] = '\0';
    ck_assert_str_eq(buff, text);
}

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex1_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex1_text, rfc4648_ex1_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex1_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex1_b64, rfc4648_ex1_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex2_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex2_text, rfc4648_ex2_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex2_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex2_b64, rfc4648_ex2_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex3_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex3_text, rfc4648_ex3_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex3_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex3_b64, rfc4648_ex3_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex4_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex4_text, rfc4648_ex4_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex4_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex4_b64, rfc4648_ex4_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex5_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex5_text, rfc4648_ex5_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex5_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex5_b64, rfc4648_ex5_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex6_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex6_text, rfc4648_ex6_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex6_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex6_b64, rfc4648_ex6_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex7_to)
{
    test_rfc4648_to_b64_text(rfc4648_ex7_text, rfc4648_ex7_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_rfc4648_ex7_from)
{
    test_rfc4648_from_b64_text(rfc4648_ex7_b64, rfc4648_ex7_text);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_all_valid_from)
{
    char buff[256];
    size_t actual_len;
    int result;
    char *str_result;
    char *str_expected;

    result = base64_decode(all_b64, buff, sizeof(buff), &actual_len);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(actual_len, sizeof(all_b64_decoded));
    str_result = bin_to_hex(buff, actual_len);
    str_expected = bin_to_hex(all_b64_decoded, sizeof(all_b64_decoded));
    ck_assert_str_eq(str_result, str_expected);
    free(str_result);
    free(str_expected);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_all_valid_to)
{
    char buff[256];
    size_t result;

    result = base64_encode(all_b64_decoded, sizeof(all_b64_decoded),
                           buff, sizeof(buff));
    ck_assert_int_eq(result, sizeof(all_b64_decoded));
    ck_assert_str_eq(buff, all_b64);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_all_invalid)
{
    char buff[256];
    size_t actual_len;
    char valid[256] = {0};
    char encoded[5] = { "T0sh" }; /* Decodes to 'OK!' */
    int result;

    /* Make a note of all the valid b64 characters */
    unsigned int i = 0;
    unsigned char c;
    while ((c = all_b64[i]) != '\0')
    {
        valid[c] = 1;
        ++i;
    }

    /* Check the decoder's working on a simple string...*/
    result = base64_decode(encoded, buff, sizeof(buff), &actual_len);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(actual_len, 3);
    buff[actual_len] = '\0';
    ck_assert_str_eq(buff, "OK!");

    /* Now replace the 1st character with all invalid characters in turn,
     * and check they're rejected */
    for (i = 0 ; i < 256; ++i)
    {
        if (i != '\0' && !valid[i]) /* Don't try the string terminator char! */
        {
            encoded[0] = i;
            result = base64_decode(encoded, buff, sizeof(buff), &actual_len);
            if (result == 0)
            {
                ck_abort_msg("Character 0x%02x was not rejected", i);
            }
        }
    }
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_small_buffer_encode)
{
    char buff[10 * 4 + 1]; /* Enough space for 10 quanta */

    size_t result;

    result = base64_encode(all_b64_decoded, sizeof(all_b64_decoded),
                           buff, sizeof(buff));
    /* Should have read 10 lots of 24 bits from the input */
    ck_assert_int_eq(result, 10 * 3);
    ck_assert_str_eq(buff, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn");
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_small_buffer_decode)
{
    char buff[10]; /* Enough space for 10 chars */

    size_t actual_len;
    int result;
    char *str_result;
    char *str_expected;

    result = base64_decode(all_b64, buff, sizeof(buff), &actual_len);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(actual_len, sizeof(all_b64_decoded));
    str_result = bin_to_hex(buff, sizeof(buff));
    str_expected = bin_to_hex(all_b64_decoded, sizeof(buff));
    ck_assert_str_eq(str_result, str_expected);
    free(str_result);
    free(str_expected);
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_add_pad_one)
{
    /* Check that a missing trailing '=' is added when decoding */
    test_rfc4648_from_b64_text("Zm8", "fo"); /* RFC4648 example 3 */
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_add_pad_two)
{
    /* Check that two missing trailing '=' chars are added when decoding */
    test_rfc4648_from_b64_text("Zg", "f"); /* RFC4648 example 2 */
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_bad_pad)
{
    char buff[16];
    size_t actual_len;

    /* Check all bad quanta with padding chars */
    static const char *bad_pad[] =
    {
        "=AAA",
        "A=AA",
        "AA=A",
        "==AA",
        "=A=A",
        "=AA=",
        "A==A",
        "A=A=",
        "===A",
        "A===",
        NULL
    };
    const char **p;

    for (p = bad_pad ; *p != NULL ; ++p)
    {
        int result = base64_decode(*p, buff, sizeof(buff), &actual_len);
        if (result == 0)
        {
            ck_abort_msg("Padding '%s' was not rejected", *p);
        }
    }
}
END_TEST

/******************************************************************************/
START_TEST(test_b64_concat_pad)
{
    const char *src =
        "VGVzdA=="  /* Test */
        "IA=="      /* <space> */
        "Y29uY2F0ZW5hdGVk" /* concatenated */
        "IA=="      /* <space> */
        "cGFkZGluZw=="; /*padding */
    const char *expected = "Test concatenated padding";
    char buff[64];
    size_t actual_len;
    int result;

    result = base64_decode(src, buff, sizeof(buff), &actual_len);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(actual_len, g_strlen(expected));
    buff[actual_len] = '\0';
    ck_assert_str_eq(buff, expected);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_base64(void)
{
    Suite *s;
    TCase *tc_b64;

    s = suite_create("base64");

    tc_b64 = tcase_create("base64");
    suite_add_tcase(s, tc_b64);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex1_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex1_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex2_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex2_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex3_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex3_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex4_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex4_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex5_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex5_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex6_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex6_from);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex7_to);
    tcase_add_test(tc_b64, test_b64_rfc4648_ex7_from);
    tcase_add_test(tc_b64, test_b64_all_valid_from);
    tcase_add_test(tc_b64, test_b64_all_valid_to);
    tcase_add_test(tc_b64, test_b64_all_invalid);
    tcase_add_test(tc_b64, test_b64_small_buffer_encode);
    tcase_add_test(tc_b64, test_b64_small_buffer_decode);
    tcase_add_test(tc_b64, test_b64_add_pad_one);
    tcase_add_test(tc_b64, test_b64_add_pad_two);
    tcase_add_test(tc_b64, test_b64_bad_pad);
    tcase_add_test(tc_b64, test_b64_concat_pad);

    return s;
}
