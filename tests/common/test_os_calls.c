
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include "os_calls.h"

#include "test_common.h"

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

START_TEST(test_g_file_get_size__1GiB)
{
    const char *file = "./file_1GiB.dat";
    unsigned int block_size = 4096;
    unsigned int block_count = 262144;
    /* C90 5.2.4.2.1 guarantees long long is at least 64 bits */
    const long long expected_size =
        (long long)block_size * block_count;
    long long size;
    int system_rv;

    char cmd[256];

    /* Create a sparse file of the expected size with one block of data
     * at the end */
    g_snprintf(cmd, sizeof(cmd),
               "dd if=/dev/zero of=%s bs=%u seek=%u count=1",
               file,
               block_size,
               block_count - 1);

    system_rv = system(cmd);
    size = g_file_get_size(file);
    g_file_delete(file);
    ck_assert_int_eq(system_rv, 0);
    ck_assert_int_eq(size, expected_size);
}
END_TEST

START_TEST(test_g_file_get_size__just_less_than_2GiB)
{
    const char *file = "./file_2GiB.dat";
    unsigned int block_size = 4096;
    unsigned int block_count = 524287; /* 4096 * 52428__8__ = 2GiB */
    /* C90 5.2.4.2.1 guarantees long long is at least 64 bits */
    const long long expected_size =
        (long long)block_size * block_count;
    long long size;
    int system_rv;

    char cmd[256];

    /* Create a sparse file of the expected size with one block of data
     * at the end */
    g_snprintf(cmd, sizeof(cmd),
               "dd if=/dev/zero of=%s bs=%u seek=%u count=1",
               file,
               block_size,
               block_count - 1);

    system_rv = system(cmd);
    size = g_file_get_size(file);
    g_file_delete(file);
    ck_assert_int_eq(system_rv, 0);
    ck_assert_int_eq(size, expected_size);
}
END_TEST

/* skip these tests until g_file_get_size() supports large files*/
#if 0
START_TEST(test_g_file_get_size__2GiB)
{
    const char *file = "./file_2GiB.dat";
    unsigned int block_size = 4096;
    unsigned int block_count = 524288;
    /* C90 5.2.4.2.1 guarantees long long is at least 64 bits */
    const long long expected_size =
        (long long)block_size * block_count;
    long long size;
    int system_rv;

    char cmd[256];

    /* Create a sparse file of the expected size with one block of data
     * at the end */
    g_snprintf(cmd, sizeof(cmd),
               "dd if=/dev/zero of=%s bs=%u seek=%u count=1",
               file,
               block_size,
               block_count - 1);

    system_rv = system(cmd);
    size = g_file_get_size(file);
    g_file_delete(file);
    ck_assert_int_eq(system_rv, 0);
    ck_assert_int_eq(size, expected_size);
}
END_TEST

START_TEST(test_g_file_get_size__5GiB)
{
    const char *file = "./file_5GiB.dat";
    unsigned int block_size = 4096;
    unsigned int block_count = 1310720;
    /* C90 5.2.4.2.1 guarantees long long is at least 64 bits */
    const long long expected_size =
        (long long)block_size * block_count;
    long long size;
    int system_rv;

    char cmd[256];

    /* Create a sparse file of the expected size with one block of data
     * at the end */
    g_snprintf(cmd, sizeof(cmd),
               "dd if=/dev/zero of=%s bs=%u seek=%u count=1",
               file,
               block_size,
               block_count - 1);

    system_rv = system(cmd);
    size = g_file_get_size(file);
    g_file_delete(file);
    ck_assert_int_eq(system_rv, 0);
    ck_assert_int_eq(size, expected_size);
}
END_TEST
#endif

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
    tcase_add_test(tc_os_calls, test_g_file_get_size__1GiB);
    tcase_add_test(tc_os_calls, test_g_file_get_size__just_less_than_2GiB);
#if 0
    tcase_add_test(tc_os_calls, test_g_file_get_size__2GiB);
    tcase_add_test(tc_os_calls, test_g_file_get_size__5GiB);
#endif

    return s;
}
