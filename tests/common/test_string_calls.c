
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "string_calls.h"

#include "test_common.h"

#define RESULT_LEN 1024

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

/******************************************************************************/

Suite *
make_suite_test_string(void)
{
    Suite *s;
    TCase *tc_strnjoin;
    TCase *tc_bm2str;
    TCase *tc_str2bm;
    TCase *tc_strtrim;

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

    tc_strtrim = tcase_create("strtrim");
    suite_add_tcase(s, tc_strtrim);
    tcase_add_test(tc_strtrim, test_strtrim__trim_left);
    tcase_add_test(tc_strtrim, test_strtrim__trim_right);
    tcase_add_test(tc_strtrim, test_strtrim__trim_both);
    tcase_add_test(tc_strtrim, test_strtrim__trim_through);

    return s;
}
