
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <limits.h>
#include <signal.h>

#include "os_calls.h"
#include "string_calls.h"
#include "ms-rdpbcgr.h"

#include "test_common.h"

#define RESULT_LEN 1024

/* Universal character names need a C99 compiler */
#if __STDC_VERSION__ >= 199901L
#   define CJK_UNIFIED_IDEOGRAPH_5E78 "\u5e78"
#   define CJK_UNIFIED_IDEOGRAPH_798F "\u798f"
#   define CJK_UNIFIED_IDEOGRAPH_5B89 "\u5b89"
#   define CJK_UNIFIED_IDEOGRAPH_5EB7 "\u5eb7"
#else
// Assume we're using UTF-8
#   define CJK_UNIFIED_IDEOGRAPH_5E78 "\xe5\xb9\xb8"
#   define CJK_UNIFIED_IDEOGRAPH_798F "\xe7\xa6\x8f"
#   define CJK_UNIFIED_IDEOGRAPH_5B89 "\xe5\xae\x89"
#   define CJK_UNIFIED_IDEOGRAPH_5EB7 "\xe5\xba\xb7"
#endif

#define HAPPINESS_AND_WELL_BEING  \
    CJK_UNIFIED_IDEOGRAPH_5E78 CJK_UNIFIED_IDEOGRAPH_798F \
    CJK_UNIFIED_IDEOGRAPH_5B89 CJK_UNIFIED_IDEOGRAPH_5EB7

START_TEST(test_strnjoin__when_src_is_null__returns_empty_string)
{
    /* setup */

    const char **src = NULL;
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, " ", src, 0);

    /* verify */

    ck_assert_str_eq(result, "");
}
END_TEST

START_TEST(test_strnjoin__when_src_has_null_item__returns_joined_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, NULL };
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, " ", src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_value ");
}
END_TEST

START_TEST(test_strnjoin__when_src_has_one_item__returns_copied_source_string)
{
    /* setup */

    const char *expected_value = "test_value";
    const char *src[] = { expected_value };
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, " ", src, 1);

    /* verify */

    ck_assert_str_eq(result, expected_value);
}
END_TEST

START_TEST(test_strnjoin__when_src_has_two_items__returns_joined_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, test_value };
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, " ", src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_value test_value");
}
END_TEST

START_TEST(test_strnjoin__when_joiner_is_empty_string__returns_joined_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, test_value };
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, "", src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_valuetest_value");
}
END_TEST

START_TEST(test_strnjoin__when_joiner_is_NULL__returns_joined_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, test_value };
    char result[RESULT_LEN];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, RESULT_LEN, NULL, src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_valuetest_value");
}
END_TEST

START_TEST(test_strnjoin__when_destination_is_NULL__returns_NULL)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value };
    char *result = NULL;

    /* test */

    g_strnjoin(result, 0, " ", src, 1);

    /* verify */

    ck_assert_ptr_eq(result, NULL);
}
END_TEST

START_TEST(test_strnjoin__when_destination_is_shorter_than_src__returns_partial_src_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value };
    char result[5];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, 5, " ", src, 1);

    /* verify */

    ck_assert_str_eq(result, "test");
}
END_TEST

START_TEST(test_strnjoin__when_destination_is_shorter_than_src_plus_joiner__returns_partial_joined_string)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, test_value};
    char result[16];
    result[0] = '\0';

    /* test */

    g_strnjoin(result, 16, " joiner ", src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_value join");
}
END_TEST

START_TEST(test_strnjoin__when_destination_has_contents__returns_joined_string_with_content_overwritten)
{
    /* setup */

    const char *test_value = "test_value";
    const char *src[] = { test_value, test_value};
    char result[RESULT_LEN] = "1234567890";

    /* test */

    g_strnjoin(result, RESULT_LEN, " ", src, 2);

    /* verify */

    ck_assert_str_eq(result, "test_value test_value");
}
END_TEST

START_TEST(test_strnjoin__when_always__then_doesnt_write_beyond_end_of_destination)
{
    /* setup */

    const char *src[] = { "a", "b", "c"};
    char result[5 + 1 + 1]; /* a-b-c + term null + guard value */

    /* test */

    result[6] = '\x7f';
    g_strnjoin(result, 5 + 1, "-", src, 3);

    /* verify */

    ck_assert_int_eq(result[6], '\x7f');
}
END_TEST

/******************************************************************************/
START_TEST(test_bm2str__no_bits_defined)
{
    int rv;
    char buff[64];

    static const struct bitmask_string bits[] =
    {
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_bitmask_to_str(0xffff, bits, ',', buff, sizeof(buff));

    ck_assert_str_eq(buff, "0xffff");
    ck_assert_int_eq(rv, 6);
}
END_TEST

START_TEST(test_bm2str__all_bits_defined)
{
    int rv;
    char buff[64];

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 6, "BIT_6"},
        {1 << 7, "BIT_7"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 6 | 1 << 7;

    rv = g_bitmask_to_str(bitmask, bits, '|', buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_0|BIT_1|BIT_6|BIT_7");
    ck_assert_int_eq(rv, (6 * 4) - 1);
}
END_TEST

START_TEST(test_bm2str__some_bits_undefined)
{
    int rv;
    char buff[64];

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 6, "BIT_6"},
        {1 << 7, "BIT_7"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 16;

    rv = g_bitmask_to_str(bitmask, bits, ':', buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_0:BIT_1:0x10000");
    ck_assert_int_eq(rv, (6 * 2) + 7);
}
END_TEST

START_TEST(test_bm2str__overflow_all_bits_defined)
{
    int rv;
    char buff[10];

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 6, "BIT_6"},
        {1 << 7, "BIT_7"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 6 | 1 << 7;

    rv = g_bitmask_to_str(bitmask, bits, '+', buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_0+BIT");
    ck_assert_int_eq(rv, (4 * 6) - 1);
}
END_TEST

START_TEST(test_bm2str__overflow_some_bits_undefined)
{
    int rv;
    char buff[16];

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 6, "BIT_6"},
        {1 << 7, "BIT_7"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 16;

    rv = g_bitmask_to_str(bitmask, bits, '#', buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_0#BIT_1#0x1");
    ck_assert_int_eq(rv, (6 * 2) + 7);
}
END_TEST

START_TEST(test_str2bm__null_string)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask(NULL, bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__empty_string)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask("", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__null_bitdefs)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    rv = g_str_to_bitmask("BIT_0", NULL, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__null_delim)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask("BIT_0", bits, NULL, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__null_buffer)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask("BIT_0", bits, ",", NULL, sizeof(buff));

    ck_assert_str_eq(buff, "dummy");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__zero_buffer)
{
    int rv;
    char buff[1];

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask("BIT_0", bits, ",", buff, 0);

    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__zero_mask)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {0,      "ZERO MASK"}, /* mask 0 should not be detected as end of list */
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0;
    rv = g_str_to_bitmask("BIT_0", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__all_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1;
    rv = g_str_to_bitmask("BIT_0,BIT_1", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__no_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 0;
    rv = g_str_to_bitmask("BIT_2,BIT_3", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_2,BIT_3");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__some_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 2, "BIT_2"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 1;
    rv = g_str_to_bitmask("a,BIT_1,b", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "a,b");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__trim_space)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 2, "BIT_2"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 2;
    rv = g_str_to_bitmask("BIT_0 , BIT_1 , BIT_2", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__overflow_undefined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 2, "BIT_2"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 1;
    rv = g_str_to_bitmask("123456789,BIT_1,abcdef", bits, ",", buff, sizeof(buff));

    /* abcdef is not filled */
    ck_assert_str_eq(buff, "123456789");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__delim_slash)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 2, "BIT_2"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 2;
    rv = g_str_to_bitmask("BIT_0/BIT_1/BIT_2", bits, "/", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__no_delim)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        BITMASK_STRING_END_OF_LIST
    };

    rv = g_str_to_bitmask("BIT_0,BIT_1", bits, "", buff, sizeof(buff));

    ck_assert_str_eq(buff, "BIT_0,BIT_1");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_str2bm__multiple_delim)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        {1 << 2, "BIT_2"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 2;
    rv = g_str_to_bitmask("BIT_0/BIT_1,BIT_2", bits, ",/", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__first_delim_is_semicolon)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        {1 << 1, "BIT_1"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 1;
    rv = g_str_to_bitmask("a;b;BIT_1;c", bits, ";,", buff, sizeof(buff));

    ck_assert_str_eq(buff, "a;b;c");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_str2bm__empty_token)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_string bits[] =
    {
        {1 << 0, "BIT_0"},
        BITMASK_STRING_END_OF_LIST
    };

    int bitmask = 1 << 0;
    rv = g_str_to_bitmask(",BIT_0, ,", bits, ",", buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

/******************************************************************************/
START_TEST(test_bm2char__no_bits_defined)
{
    int rv;
    char buff[64];
    int rest;

    static const struct bitmask_char bits[] =
    {
        BITMASK_CHAR_END_OF_LIST
    };

    rv = g_bitmask_to_charstr(0xffff, bits, buff, sizeof(buff), &rest);

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(rest, 0xffff);
}
END_TEST

START_TEST(test_bm2char__all_bits_defined)
{
    int rv;
    char buff[64];
    int rest;

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 6, 'C'},
        {1 << 7, 'D'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 6 | 1 << 7;

    rv = g_bitmask_to_charstr(bitmask, bits, buff, sizeof(buff), &rest);

    ck_assert_str_eq(buff, "ABCD");
    ck_assert_int_eq(rv, 4);
    ck_assert_int_eq(rest, 0);
}
END_TEST

START_TEST(test_bm2char__some_bits_undefined)
{
    int rv;
    char buff[64];
    int rest;

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 6, 'C'},
        {1 << 7, 'D'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 16;

    rv = g_bitmask_to_charstr(bitmask, bits, buff, sizeof(buff), &rest);

    ck_assert_str_eq(buff, "AB");
    ck_assert_int_eq(rv, 2);
    ck_assert_int_eq(rest, (1 << 16));
}
END_TEST

START_TEST(test_bm2char__overflow_all_bits_defined)
{
    int rv;
    char buff[3];
    int rest;

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 6, 'C'},
        {1 << 7, 'D'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 6 | 1 << 7;

    rv = g_bitmask_to_charstr(bitmask, bits, buff, sizeof(buff), &rest);

    ck_assert_str_eq(buff, "AB");
    ck_assert_int_eq(rv, 4);
    ck_assert_int_eq(rest, 0);
}
END_TEST

START_TEST(test_bm2char__overflow_some_bits_undefined)
{
    int rv;
    char buff[2];
    int rest;

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 6, 'C'},
        {1 << 7, 'D'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 16;

    rv = g_bitmask_to_charstr(bitmask, bits, buff, sizeof(buff), &rest);

    ck_assert_str_eq(buff, "A");
    ck_assert_int_eq(rv, 2);
    ck_assert_int_eq(rest, (1 << 16));
}
END_TEST

START_TEST(test_bm2char__null_rest_param)
{
    int rv;
    char buff[10];

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 6, 'C'},
        {1 << 7, 'D'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1 | 1 << 16;

    rv = g_bitmask_to_charstr(bitmask, bits, buff, sizeof(buff), NULL);

    ck_assert_str_eq(buff, "AB");
    ck_assert_int_eq(rv, 2);
}
END_TEST

/******************************************************************************/
START_TEST(test_char2bm__null_string)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        BITMASK_CHAR_END_OF_LIST
    };

    rv = g_charstr_to_bitmask(NULL, bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_char2bm__empty_string)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        BITMASK_CHAR_END_OF_LIST
    };

    rv = g_charstr_to_bitmask("", bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_char2bm__null_bitdefs)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    rv = g_charstr_to_bitmask("A", NULL, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_char2bm__null_buffer)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        BITMASK_CHAR_END_OF_LIST
    };

    rv = g_charstr_to_bitmask("B", bits, NULL, sizeof(buff));

    ck_assert_str_eq(buff, "dummy");
    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_char2bm__zero_buffer)
{
    int rv;
    char buff[1];

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        BITMASK_CHAR_END_OF_LIST
    };

    rv = g_charstr_to_bitmask("B", bits, buff, 0);

    ck_assert_int_eq(rv, 0);
}
END_TEST

START_TEST(test_char2bm__zero_mask)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {0,      'A'}, /* mask 0 should not be detected as end of list */
        {1 << 0, 'B'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0;
    rv = g_charstr_to_bitmask("B", bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_char2bm__all_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 0 | 1 << 1;
    rv = g_charstr_to_bitmask("AB", bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_char2bm__no_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 0;
    rv = g_charstr_to_bitmask("CD", bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "CD");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_char2bm__some_defined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 2, 'C'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 1;
    rv = g_charstr_to_bitmask("0B1", bits, buff, sizeof(buff));

    ck_assert_str_eq(buff, "01");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

START_TEST(test_char2bm__overflow_undefined)
{
    int rv;
    char buff[16] = { 'd', 'u', 'm', 'm', 'y' };

    static const struct bitmask_char bits[] =
    {
        {1 << 0, 'A'},
        {1 << 1, 'B'},
        {1 << 2, 'C'},
        BITMASK_CHAR_END_OF_LIST
    };

    int bitmask = 1 << 1;
    rv = g_charstr_to_bitmask("123456789Bvwxyz", bits, buff, 10);

    /* vwxyz is not filled */
    ck_assert_str_eq(buff, "123456789");
    ck_assert_int_eq(rv, bitmask);
}
END_TEST

/******************************************************************************/

START_TEST(test_strtrim__trim_left)
{
    /* setup */
    char output[] = "\t\t    \tDone is better than perfect.\t\t    \n\n";

    /* test */
    g_strtrim(output, 1);

    /* verify */
    ck_assert_str_eq(output, "Done is better than perfect.\t\t    \n\n");
}
END_TEST

START_TEST(test_strtrim__trim_right)
{
    /* setup */
    char output[] = "\t\t    \tDone is better than perfect.\t\t    \n\n";

    /* test */
    g_strtrim(output, 2);

    /* verify */
    ck_assert_str_eq(output, "\t\t    \tDone is better than perfect.");
}
END_TEST

START_TEST(test_strtrim__trim_both)
{
    /* setup */
    char output[] = "\t\t    \tDone is better than perfect.\t\t    \n\n";

    /* test */
    g_strtrim(output, 3);

    /* verify */
    ck_assert_str_eq(output, "Done is better than perfect.");
}
END_TEST

START_TEST(test_strtrim__trim_through)
{
    /* setup */
    char output[] = "\t\t    \tDone is better than perfect.\t\t    \n\n";

    /* test */
    g_strtrim(output, 4);

    /* verify */
    ck_assert_str_eq(output, "Doneisbetterthanperfect.");
}
END_TEST

START_TEST(test_strtrim__chinese_chars)
{
    /* setup */
    char output[] = "\t\t    \t" HAPPINESS_AND_WELL_BEING "\t\t    \n\n";

    /* test */
    g_strtrim(output, 4);

    /* verify */
    ck_assert_str_eq(output, HAPPINESS_AND_WELL_BEING);
}
END_TEST

/******************************************************************************/

START_TEST(test_sigs__common)
{
    char name[MAXSTRSIGLEN];
    char *res;

    // Check some common POSIX signals
    res = g_sig2text(SIGHUP, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGHUP");

    res = g_sig2text(SIGCHLD, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGCHLD");

    res = g_sig2text(SIGXFSZ, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGXFSZ");

    res = g_sig2text(SIGRTMIN, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGRTMIN");

    res = g_sig2text(SIGRTMIN + 2, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGRTMIN+2");

    res = g_sig2text(SIGRTMAX, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIGRTMAX");

    // Should be invalid
    res = g_sig2text(0, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIG#0");

    res = g_sig2text(65535, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIG#65535");


    // POSIX defines signals as ints, but insists they are positive. So
    // we ought to trest we get sane behaviour for -ve numbers
    res = g_sig2text(-1, name);
    ck_assert_ptr_eq(res, name);
    ck_assert_str_eq(res, "SIG#-1");
}
END_TEST

START_TEST(test_sigs__bigint)
{
    char name[MAXSTRSIGLEN];
    char name2[1024];

    // Check that big integers aren't being truncated by the definition
    // of MAXSTRSIGLEN
    int i = INT_MAX;

    g_sig2text(i, name);
    g_snprintf(name2, sizeof(name2), "SIG#%d", i);
    ck_assert_str_eq(name, name2);

    i = INT_MIN;
    g_sig2text(i, name);
    g_snprintf(name2, sizeof(name2), "SIG#%d", i);
    ck_assert_str_eq(name, name2);
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_string(void)
{
    Suite *s;
    TCase *tc_strnjoin;
    TCase *tc_bm2str;
    TCase *tc_str2bm;
    TCase *tc_bm2char;
    TCase *tc_char2bm;
    TCase *tc_strtrim;
    TCase *tc_sigs;

    s = suite_create("String");

    tc_strnjoin = tcase_create("strnjoin");
    suite_add_tcase(s, tc_strnjoin);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_src_is_null__returns_empty_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_src_has_null_item__returns_joined_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_src_has_one_item__returns_copied_source_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_src_has_two_items__returns_joined_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_joiner_is_empty_string__returns_joined_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_joiner_is_NULL__returns_joined_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_destination_is_NULL__returns_NULL);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_destination_is_shorter_than_src__returns_partial_src_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_destination_is_shorter_than_src_plus_joiner__returns_partial_joined_string);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_destination_has_contents__returns_joined_string_with_content_overwritten);
    tcase_add_test(tc_strnjoin, test_strnjoin__when_always__then_doesnt_write_beyond_end_of_destination);

    tc_bm2str = tcase_create("bm2str");
    suite_add_tcase(s, tc_bm2str);
    tcase_add_test(tc_bm2str, test_bm2str__no_bits_defined);
    tcase_add_test(tc_bm2str, test_bm2str__all_bits_defined);
    tcase_add_test(tc_bm2str, test_bm2str__some_bits_undefined);
    tcase_add_test(tc_bm2str, test_bm2str__overflow_all_bits_defined);
    tcase_add_test(tc_bm2str, test_bm2str__overflow_some_bits_undefined);
    tc_str2bm = tcase_create("str2bm");
    suite_add_tcase(s, tc_str2bm);
    tcase_add_test(tc_str2bm, test_str2bm__null_string);
    tcase_add_test(tc_str2bm, test_str2bm__empty_string);
    tcase_add_test(tc_str2bm, test_str2bm__null_bitdefs);
    tcase_add_test(tc_str2bm, test_str2bm__null_delim);
    tcase_add_test(tc_str2bm, test_str2bm__null_buffer);
    tcase_add_test(tc_str2bm, test_str2bm__zero_buffer);
    tcase_add_test(tc_str2bm, test_str2bm__zero_mask);
    tcase_add_test(tc_str2bm, test_str2bm__all_defined);
    tcase_add_test(tc_str2bm, test_str2bm__no_defined);
    tcase_add_test(tc_str2bm, test_str2bm__some_defined);
    tcase_add_test(tc_str2bm, test_str2bm__trim_space);
    tcase_add_test(tc_str2bm, test_str2bm__overflow_undefined);
    tcase_add_test(tc_str2bm, test_str2bm__no_delim);
    tcase_add_test(tc_str2bm, test_str2bm__delim_slash);
    tcase_add_test(tc_str2bm, test_str2bm__multiple_delim);
    tcase_add_test(tc_str2bm, test_str2bm__first_delim_is_semicolon);
    tcase_add_test(tc_str2bm, test_str2bm__empty_token);

    tc_bm2char = tcase_create("bm2char");
    suite_add_tcase(s, tc_bm2char);
    tcase_add_test(tc_bm2char, test_bm2char__no_bits_defined);
    tcase_add_test(tc_bm2char, test_bm2char__all_bits_defined);
    tcase_add_test(tc_bm2char, test_bm2char__some_bits_undefined);
    tcase_add_test(tc_bm2char, test_bm2char__overflow_all_bits_defined);
    tcase_add_test(tc_bm2char, test_bm2char__overflow_some_bits_undefined);
    tcase_add_test(tc_bm2char, test_bm2char__null_rest_param);
    tc_char2bm = tcase_create("char2bm");
    suite_add_tcase(s, tc_char2bm);
    tcase_add_test(tc_char2bm, test_char2bm__null_string);
    tcase_add_test(tc_char2bm, test_char2bm__empty_string);
    tcase_add_test(tc_char2bm, test_char2bm__null_bitdefs);
    tcase_add_test(tc_char2bm, test_char2bm__null_buffer);
    tcase_add_test(tc_char2bm, test_char2bm__zero_buffer);
    tcase_add_test(tc_char2bm, test_char2bm__zero_mask);
    tcase_add_test(tc_char2bm, test_char2bm__all_defined);
    tcase_add_test(tc_char2bm, test_char2bm__no_defined);
    tcase_add_test(tc_char2bm, test_char2bm__some_defined);
    tcase_add_test(tc_char2bm, test_char2bm__overflow_undefined);

    tc_strtrim = tcase_create("strtrim");
    suite_add_tcase(s, tc_strtrim);
    tcase_add_test(tc_strtrim, test_strtrim__trim_left);
    tcase_add_test(tc_strtrim, test_strtrim__trim_right);
    tcase_add_test(tc_strtrim, test_strtrim__trim_both);
    tcase_add_test(tc_strtrim, test_strtrim__trim_through);
    tcase_add_test(tc_strtrim, test_strtrim__chinese_chars);

    tc_sigs = tcase_create("signals");
    suite_add_tcase(s, tc_sigs);
    tcase_add_test(tc_sigs, test_sigs__common);
    tcase_add_test(tc_sigs, test_sigs__bigint);

    return s;
}
