
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "explicit_bzero.h"

#include "test_common.h"

/******************************************************************************/

START_TEST(test_basic_explicit_bzero)
{
    // We can't easily check that explicit_bzero isn't elided away, as the
    // compiler will spot that and not do it! We will, however, check the basic
    // operation of the function

    char data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    explicit_bzero(&data[4], 3);

    ck_assert_int_eq(data[0], 1);
    ck_assert_int_eq(data[1], 2);
    ck_assert_int_eq(data[2], 3);
    ck_assert_int_eq(data[3], 4);
    ck_assert_int_eq(data[4], 0);
    ck_assert_int_eq(data[5], 0);
    ck_assert_int_eq(data[6], 0);
    ck_assert_int_eq(data[7], 8);
    ck_assert_int_eq(data[8], 9);
    ck_assert_int_eq(data[9], 10);
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_explicit_bzero(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("explicit_bzero");

    tc = tcase_create("basic");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_basic_explicit_bzero);

    return s;
}
