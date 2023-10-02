
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <poll.h>

#include "os_calls.h"
#include "list.h"

#include "test_common.h"

#ifndef TOP_SRCDIR
#define TOP_SRCDIR "."
#endif

// File for testing ro/rw opens
#define RO_RW_FILE "./test_ro_rw"

/******************************************************************************/
/***
 * Gets the number of open file descriptors for the current process */
static unsigned int
get_open_fd_count(void)
{
    unsigned int i;
    unsigned int rv;

    // What's the max number of file descriptors?
    struct rlimit nofile;
    if (getrlimit(RLIMIT_NOFILE, &nofile) < 0)
    {
        const char *errstr = g_get_strerror();
        ck_abort_msg("Can't create socketpair [%s]", errstr);
    }

    struct pollfd *fds =
        (struct pollfd *)g_malloc(sizeof(struct pollfd) * nofile.rlim_cur, 0);
    ck_assert_ptr_nonnull(fds);

    for (i = 0 ; i < nofile.rlim_cur; ++i)
    {
        fds[i].fd = i;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    if (poll(fds, nofile.rlim_cur, 0) < 0)
    {
        const char *errstr = g_get_strerror();
        ck_abort_msg("Can't poll fds [%s]", errstr);
    }

    rv = nofile.rlim_cur;
    for (i = 0 ; i < nofile.rlim_cur; ++i)
    {
        if (fds[i].revents == POLLNVAL)
        {
            --rv;
        }
    }

    g_free(fds);

    return rv;
}

/******************************************************************************/
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
/* Test we can write to a file which is opened for write */
START_TEST(test_g_file_rw)
{
    const char data[] = "File data\n";

    int fd = g_file_open_rw(RO_RW_FILE);
    ck_assert(fd >= 0);

    int status = g_file_write(fd, data, sizeof(data) - 1);
    g_file_close(fd);

    // Assume no signals have occurred
    ck_assert_int_eq(status, sizeof(data) - 1);

    // Leave file in place for test_g_file_ro
}
END_TEST

/******************************************************************************/
/* Test we can't write to a file which is opened read only */
START_TEST(test_g_file_ro)
{
    const char data[] = "File data\n";

    int fd = g_file_open_ro(RO_RW_FILE);
    ck_assert(fd >= 0);

    int status = g_file_write(fd, data, sizeof(data) - 1);
    g_file_close(fd);

    // Write must fail
    ck_assert_int_lt(status, 0);

    // Tidy-up (not checked)
    g_file_delete(RO_RW_FILE);
}
END_TEST

/******************************************************************************/
/* Just test we can set and clear the flag. We don't test its operation */
START_TEST(test_g_file_cloexec)
{
    int flag;
    int devzerofd = g_file_open_ro("/dev/zero");
    ck_assert(devzerofd >= 0);

    (void)g_file_set_cloexec(devzerofd, 1);
    flag = g_file_get_cloexec(devzerofd);
    ck_assert(flag != 0);

    (void)g_file_set_cloexec(devzerofd, 0);
    flag = g_file_get_cloexec(devzerofd);
    ck_assert(flag == 0);

    g_file_close(devzerofd);
}
END_TEST

/******************************************************************************/
START_TEST(test_g_file_get_open_fds)
{
    int fd_count = get_open_fd_count();
    int i;

    struct list *start_list = g_get_open_fds(0, -1);
    ck_assert_ptr_ne(start_list, NULL);
    ck_assert_int_eq(start_list->count, fd_count);

    // Open another file
    int devzerofd = g_file_open_ro("/dev/zero");
    ck_assert(devzerofd >= 0);

    // Have we now got one more open file?
    struct list *open_list = g_get_open_fds(0, -1);
    ck_assert_ptr_ne(open_list, NULL);
    ck_assert_int_eq(open_list->count, fd_count + 1);

    // Check the new file is not in the start list, but is in the open list
    ck_assert_int_lt(list_index_of(start_list, devzerofd), 0);
    ck_assert_int_ge(list_index_of(open_list, devzerofd), 0);

    g_file_close(devzerofd);

    struct list *finish_list = g_get_open_fds(0, -1);
    ck_assert_ptr_ne(finish_list, NULL);

    // start list same as finish list?
    ck_assert_int_eq(finish_list->count, fd_count);

    for (i = 0 ; i < start_list->count; ++i)
    {
        ck_assert_int_eq((int)finish_list->items[i],
                         (int)start_list->items[i]);
    }

    list_delete(start_list);
    list_delete(open_list);
    list_delete(finish_list);
}
END_TEST


/******************************************************************************/
START_TEST(test_g_file_is_open)
{
    int devzerofd = g_file_open_ro("/dev/zero");
    ck_assert(devzerofd >= 0);

    // Check open file comes up as open
    ck_assert_int_ne(g_file_is_open(devzerofd), 0);

    g_file_close(devzerofd);

    // Check the now-closed file no longer registers as open
    ck_assert_int_eq(g_file_is_open(devzerofd), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_g_sck_fd_passing)
{
    int sck[2];
    char buff[16];
    int istatus;
    unsigned int fdcount;

    int devzerofd = g_file_open_ro("/dev/zero");
    ck_assert(devzerofd >= 0);

    if (g_sck_local_socketpair(sck) != 0)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd);
        ck_abort_msg("Can't create socketpair [%s]", errstr);
    }

    // Pass the fd for /dev/zero to sck[0]...
    istatus = g_sck_send_fd_set(sck[0], "?", 1, &devzerofd, 1);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd);
        g_file_close(sck[0]);
        g_file_close(sck[1]);
        ck_abort_msg("Can't send fd set [%s]", errstr);
    }

    // We can now close the fd for /dev/zero, as it's "in flight"
    g_file_close(devzerofd);
    devzerofd = -1;

    // Read the fd for /dev/zero from sck[1]
    fdcount = -1;
    istatus = g_sck_recv_fd_set(sck[1], buff, sizeof(buff),
                                &devzerofd, 1, &fdcount);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(sck[0]);
        g_file_close(sck[1]);
        ck_abort_msg("Can't receive fd set [%s]", errstr);
    }

    // Don't need the socket pair any more
    g_file_close(sck[0]);
    g_file_close(sck[1]);

    // We should have got 1 fd back, and received a data byte of '?'
    if (fdcount != 1)
    {
        g_file_close(devzerofd);
        ck_abort_msg("Should have 1 fd, got %u", fdcount);
    }

    if (buff[0] != '?')
    {
        g_file_close(devzerofd);
        ck_abort_msg("Should have received '?' in buffer");
    }

    // Does the fd for /dev/zero work?
    istatus = g_file_read(devzerofd, buff, 1);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd);
        ck_abort_msg("Can't read from /dev/zero fd %d [%s]", devzerofd, errstr);
    }

    g_file_close(devzerofd);

    ck_assert_int_eq(buff[0], '\0');
}
END_TEST

START_TEST(test_g_sck_fd_overflow)
{
    int sck[2];
    char buff[16];
    int istatus;
    unsigned int fdcount;
    int devzerofd[2];

    // Count the number of file descriptors for the process
    unsigned int base_fd_count = get_open_fd_count();
    unsigned int proc_fd_count;

    // Open a couple of file descriptors to /dev/zero
    devzerofd[0] = g_file_open_ro("/dev/zero");
    devzerofd[1] = g_file_open_ro("/dev/zero");
    ck_assert(devzerofd[0] >= 0);
    ck_assert(devzerofd[1] >= 0);
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count + 2);

    if (g_sck_local_socketpair(sck) != 0)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd[0]);
        g_file_close(devzerofd[1]);
        ck_abort_msg("Can't create socketpair [%s]", errstr);
    }
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count + 4);

    // Pass the /dev/zero fds to sck[0]...
    istatus = g_sck_send_fd_set(sck[0], "?", 1, devzerofd, 2);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd[0]);
        g_file_close(devzerofd[1]);
        g_file_close(sck[0]);
        g_file_close(sck[1]);
        ck_abort_msg("Can't send fd set [%s]", errstr);
    }

    // We can now close fds for /dev/zero, as  they are "in flight"
    g_file_close(devzerofd[0]);
    g_file_close(devzerofd[1]);
    devzerofd[0] = -1;
    devzerofd[1] = -1;
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count + 2);

    // Read one fd for /dev/zero from sck[1]
    fdcount = -1;
    istatus = g_sck_recv_fd_set(sck[1], buff, sizeof(buff),
                                &devzerofd[0], 1, &fdcount);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(sck[0]);
        g_file_close(sck[1]);
        ck_abort_msg("Can't receive fd set [%s]", errstr);
    }

    // We should now have just ONE more file descriptor
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count + 3);

    // Don't need the socket pair any more
    g_file_close(sck[0]);
    g_file_close(sck[1]);
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count + 1);

    // Does the fd for /dev/zero work?
    istatus = g_file_read(devzerofd[0], buff, 1);
    if (istatus != 1)
    {
        const char *errstr = g_get_strerror();
        g_file_close(devzerofd[0]);
        ck_abort_msg("Can't read from /dev/zero fd %d [%s]",
                     devzerofd[0], errstr);
    }
    ck_assert_int_eq(buff[0], '\0');

    g_file_close(devzerofd[0]);
    proc_fd_count = get_open_fd_count();
    ck_assert_int_eq(proc_fd_count, base_fd_count);
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
    tcase_add_test(tc_os_calls, test_g_file_get_size__1GiB);
    tcase_add_test(tc_os_calls, test_g_file_get_size__just_less_than_2GiB);
#if 0
    tcase_add_test(tc_os_calls, test_g_file_get_size__2GiB);
    tcase_add_test(tc_os_calls, test_g_file_get_size__5GiB);
#endif
    tcase_add_test(tc_os_calls, test_g_file_rw);
    tcase_add_test(tc_os_calls, test_g_file_ro); // Must follow test_g_file_rw
    tcase_add_test(tc_os_calls, test_g_file_cloexec);
    tcase_add_test(tc_os_calls, test_g_file_get_open_fds);
    tcase_add_test(tc_os_calls, test_g_file_is_open);
    tcase_add_test(tc_os_calls, test_g_sck_fd_passing);
    tcase_add_test(tc_os_calls, test_g_sck_fd_overflow);

    // Add other test cases in other files
    suite_add_tcase(s, make_tcase_test_os_calls_signals());

    return s;
}
