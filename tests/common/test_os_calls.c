
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include "test_common.h"
#include "os_calls.h"

#ifndef TOP_SRCDIR
#define TOP_SRCDIR "."
#endif

START_TEST(test_g_file_get_size__returns_file_size)
{
    unsigned long long size;

    size = g_file_get_size(TOP_SRCDIR "/xrdp/xrdp256.bmp");
    ck_assert_int_eq(size, 49278);


    size = g_file_get_size(TOP_SRCDIR "/xrdp/ad256.bmp");
    ck_assert_int_eq(size, 19766);

}
END_TEST

START_TEST(test_g_file_get_size__more_than_4GiB)
{
    unsigned long long size;

    system("dd if=/dev/zero of=./file_5GiB.dat bs=4k count=1310720");

    size = g_file_get_size("./file_5GiB.dat");
    ck_assert_int_eq(size, 5368709120);
}
END_TEST


/******************************************************************************/
Suite *
make_suite_test_os_calls(void)
{
    Suite *s;
    TCase *tc_os_calls;

    s = suite_create("OS-Calls");

    tc_os_calls = tcase_create("oscalls-file");
    suite_add_tcase(s, tc_os_calls);
    tcase_add_test(tc_os_calls, test_g_file_get_size__returns_file_size);
    tcase_add_test(tc_os_calls, test_g_file_get_size__more_than_4GiB);

    return s;
}
