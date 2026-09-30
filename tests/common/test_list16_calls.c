#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "list16.h"

#include "os_calls.h"
#include "test_common.h"
#include "string_calls.h"

#define TEST_LIST16_SIZE 1000

START_TEST(test_list16__simple)
{
    struct list16 *lst = list16_create();
    int i;
    tui16 val;
    int res;
    for (i = 0 ; i < TEST_LIST16_SIZE ; ++i)
    {
        res = list16_add_item(lst, (tui16)i);
        ck_assert_int_ne(res, 0);
    }

    ck_assert_int_eq(lst->count, TEST_LIST16_SIZE);
    for (i = 0 ; i < TEST_LIST16_SIZE ; ++i)
    {
        ck_assert_int_eq(lst->items[i], (tui16)i);
        // Also check get method
        val = list16_get_item(lst, i);
        ck_assert_int_eq(val, (tui16)i);
    }

    /* Out-of-bounds test */
    val = list16_get_item(lst, TEST_LIST16_SIZE);
    ck_assert_int_eq(val, 0);

    i = list16_index_of(lst, 50);
    ck_assert_int_eq(i, 50);

    list16_remove_item(lst, 10);
    ck_assert_int_eq(lst->count, TEST_LIST16_SIZE - 1);
    /* Check values before the deleted item */
    for (i = 0; i < 10; ++i)
    {
        val = list16_get_item(lst, i);
        ck_assert_int_eq(val, (tui16)i);
    }
    /* Check values after the deleted item */
    for (i = 10; i < lst->count; ++i)
    {
        val = list16_get_item(lst, i);
        ck_assert_int_eq(val, (tui16)(i + 1));
    }

    list16_insert_item(lst, 10, 10);
    ck_assert_int_eq(lst->count, TEST_LIST16_SIZE);
    /* Re-check all values */
    for (i = 0; i < lst->count; ++i)
    {
        val = list16_get_item(lst, i);
        ck_assert_int_eq(val, (tui16)i);
    }

    list16_insert_item(lst, 0, 99);
    ck_assert_int_eq(lst->count, TEST_LIST16_SIZE + 1);
    val = list16_get_item(lst, 10);
    ck_assert_int_eq(val, 9);

    list16_clear(lst);
    ck_assert_int_eq(lst->count, 0);
    list16_delete(lst);
}
END_TEST

/* Tests stack calls work as expected. Run under valgrind to check memory */
START_TEST(test_list16__stack)
{
    int i;
    int res;
    struct list16 lst;

    list16_init(&lst);
    ck_assert_int_eq(lst.count, 0);
    list16_deinit(&lst);
    list16_deinit(&lst); // Shouldn't happen, but could result in double-free

    list16_init(&lst);
    for (i = 0 ; i < TEST_LIST16_SIZE ; ++i)
    {
        res = list16_add_item(&lst, (tui16)i);
        ck_assert_int_ne(res, 0);
    }
    ck_assert_int_eq(lst.count, TEST_LIST16_SIZE);
    list16_deinit(&lst);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_list16(void)
{
    Suite *s;
    TCase *tc_simple;

    s = suite_create("List16");

    tc_simple = tcase_create("simple");
    suite_add_tcase(s, tc_simple);
    tcase_add_test(tc_simple, test_list16__simple);
    tcase_add_test(tc_simple, test_list16__stack);

    return s;
}
