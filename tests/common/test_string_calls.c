
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "test_common.h"
#include "string_calls.h"

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
Suite *
make_suite_test_string(void)
{
    Suite *s;
    TCase *tc_strnjoin;

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

    return s;
}
