#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "list.h"

#include "os_calls.h"
#include "test_common.h"
#include "string_calls.h"

#define TEST_LIST_SIZE 1000

START_TEST(test_list__simple)
{
    struct list *lst = list_create();
    int i;
    int val;
    for (i = 0 ; i < TEST_LIST_SIZE ; ++i)
    {
        list_add_item(lst, i);
    }

    ck_assert_int_eq(lst->count, TEST_LIST_SIZE);
    for (i = 0 ; i < TEST_LIST_SIZE ; ++i)
    {
        ck_assert_int_eq(lst->items[i], i);
        // Also check get method
        val = list_get_item(lst, i);
        ck_assert_int_eq(val, i);
    }

    i = list_index_of(lst, 50);
    ck_assert_int_eq(i, 50);

    list_remove_item(lst, 10);
    ck_assert_int_eq(lst->count, TEST_LIST_SIZE - 1);
    val = list_get_item(lst, 10);
    ck_assert_int_eq(val, 11);

    list_insert_item(lst, 10, 10);
    ck_assert_int_eq(lst->count, TEST_LIST_SIZE);
    val = list_get_item(lst, 10);
    ck_assert_int_eq(val, 10);

    list_insert_item(lst, 0, 99);
    ck_assert_int_eq(lst->count, TEST_LIST_SIZE + 1);
    val = list_get_item(lst, 10);
    ck_assert_int_eq(val, 9);

    list_clear(lst);
    ck_assert_int_eq(lst->count, 0);
    list_delete(lst);
}
END_TEST

// This needs to be run through valgrind to truly check all memory is
// being de-allocated
START_TEST(test_list__simple_auto_free)
{
    struct list *lst = list_create();
    lst->auto_free = 1;

    int i;
    for (i = 0 ; i < TEST_LIST_SIZE ; ++i)
    {
        char strval[64];
        g_snprintf(strval, sizeof(strval), "%d", i);
        // Odds, use list_add_item/strdup, evens use list_add_strdup
        if ((i % 2) != 0)
        {
            list_add_item(lst, (tintptr)g_strdup(strval));
        }
        else
        {
            list_add_strdup(lst, strval);
        }
    }

    list_remove_item(lst, 0);
    list_remove_item(lst, 0);
    list_remove_item(lst, 0);
    list_remove_item(lst, 0);
    list_remove_item(lst, 0);
    ck_assert_int_eq(lst->count, TEST_LIST_SIZE - 5);
    list_delete(lst);
}
END_TEST

START_TEST(test_list__simple_append_list)
{
    int i;
    struct list *src = list_create();
    struct list *dst = list_create();
    src->auto_free = 0;
    dst->auto_free = 1;

    list_add_item(src, (tintptr)"6");
    list_add_item(src, (tintptr)"7");
    list_add_item(src, (tintptr)"8");
    list_add_item(src, (tintptr)"9");
    list_add_item(src, (tintptr)"10");
    list_add_item(src, (tintptr)"11");

    list_add_item(dst, (tintptr)g_strdup("0"));
    list_add_item(dst, (tintptr)g_strdup("1"));
    list_add_item(dst, (tintptr)g_strdup("2"));
    list_add_item(dst, (tintptr)g_strdup("3"));
    list_add_item(dst, (tintptr)g_strdup("4"));
    list_add_item(dst, (tintptr)g_strdup("5"));

    list_append_list_strdup(src, dst, 0);

    ck_assert_int_eq(dst->count, 12);

    for (i = 0 ; i < dst->count; ++i)
    {
        int val = g_atoi((const char *)list_get_item(dst, i));
        ck_assert_int_eq(val, i);
    }

    list_delete(src);
    list_clear(dst);  // Exercises auto_free code paths in list.c
    list_delete(dst);
}
END_TEST

START_TEST(test_list__simple_strdup_multi)
{
    int i;
    struct list *lst = list_create();
    lst->auto_free = 1;

    list_add_strdup_multi(lst,
                          "0", "1", "2", "3", "4", "5",
                          "6", "7", "8", "9", "10", "11",
                          NULL);

    ck_assert_int_eq(lst->count, 12);

    for (i = 0 ; i < lst->count; ++i)
    {
        int val = g_atoi((const char *)list_get_item(lst, i));
        ck_assert_int_eq(val, i);
    }

    list_delete(lst);
}
END_TEST

START_TEST(test_list__append_fragment)
{
    struct list *l = list_create();
    l->auto_free = 1;

    const char *test_string = "split this";

    int fragment_ret = split_string_append_fragment(&test_string,
                       test_string + 5, l);
    ck_assert_int_eq(fragment_ret, 1);
    ck_assert_str_eq((const char *)l->items[0], "split");

    fragment_ret = split_string_append_fragment(&test_string,
                   test_string + 1000, l);
    ck_assert_int_eq(fragment_ret, 1);
    ck_assert_str_eq((const char *)l->items[1], "this");

    list_delete(l);
}
END_TEST

START_TEST(test_list__split_string_into_list)
{
    struct list *l = split_string_into_list("The fat cat sat on my hat.", ' ');

    ck_assert_int_eq(l->count, 7);
    ck_assert_str_eq((const char *)l->items[0], "The");
    ck_assert_str_eq((const char *)l->items[1], "fat");
    ck_assert_str_eq((const char *)l->items[2], "cat");
    ck_assert_str_eq((const char *)l->items[3], "sat");
    ck_assert_str_eq((const char *)l->items[4], "on");
    ck_assert_str_eq((const char *)l->items[5], "my");
    ck_assert_str_eq((const char *)l->items[6], "hat.");

    list_delete(l);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_list(void)
{
    Suite *s;
    TCase *tc_simple;

    s = suite_create("List");

    tc_simple = tcase_create("simple");
    suite_add_tcase(s, tc_simple);
    tcase_add_test(tc_simple, test_list__simple);
    tcase_add_test(tc_simple, test_list__simple_auto_free);
    tcase_add_test(tc_simple, test_list__simple_append_list);
    tcase_add_test(tc_simple, test_list__simple_strdup_multi);
    tcase_add_test(tc_simple, test_list__append_fragment);
    tcase_add_test(tc_simple, test_list__split_string_into_list);

    return s;
}
