
#ifndef TEST_LIBIPM__H
#define TEST_LIBIPM__H

#include <check.h>

/* Private libipm stuff. This is duplicated here, so if the libipm
 * value change, tests will fail! */
enum
{
    LIBIPM_VERSION = 2,
    LIBIPM_HEADER_SIZE = 12,
    LIBIPM_MAX_MESSAGE_SIZE = 8192,
    LIBIPM_MAX_FD_PER_MSG = 8,
    LIBIPM_MAX_PAYLOAD_SIZE = LIBIPM_MAX_MESSAGE_SIZE - LIBIPM_HEADER_SIZE
};

/* Globals */
extern struct trans *g_t_out;   /* Channel for outputting libipm messages */
extern struct trans *g_t_in;    /* Channel for inputting libipm messages */
extern struct trans *g_t_vanilla; /* Non-SCP channel */
extern int g_fd;                /* An open file descriptor (/dev/zero) */

extern const char *g_supported_types;  /* All recognised type codes */
extern const char *g_unimplemented_types; /* recognised, unimplemented codes */

#define TEST_MESSAGE_NO 66
/* Test message with no string translation */
#define TEST_MESSAGE_NO_STRING_NO 67

/**
 * Compares two binary blocks
 *
 * @param actual_data Actual data
 * @param actual_len Actual length
 * @param expected_data Expected data
 * @aram expected_len Expected data length
 *
 * Differences are raised using ck_* calls
 */
void
check_binary_data_eq(const void *actual_data,
                     unsigned int actual_len,
                     const void *expected_data,
                     unsigned int expected_len);

/**
 * Check the file descriptor specified is working as /dev/zero
 *
 * If it isn't, an exception is raised using ck_* calls
 */
void
check_fd_is_dev_zero(int fd);

/**
 * Looks for the specified string in the specified stream
 * @param s Stream to search
 * @param str Null-terminated string to look for
 * @return != 0 if string found in the buffer
 *
 * The whole buffer is searched for the string. not just the
 * used bit.
 */
int
does_stream_contain_string(const struct stream *s, const char *str);

/**
 * Returns the number of open file descriptors in the process */
unsigned int
get_open_fd_count(void);

Suite *make_suite_test_libipm_send_calls(void);
Suite *make_suite_test_libipm_recv_calls(void);

#endif /* TEST_LIBIPM__H */
