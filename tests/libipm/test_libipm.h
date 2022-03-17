
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
    LIBIPM_MAX_PAYLOAD_SIZE = LIBIPM_MAX_MESSAGE_SIZE - LIBIPM_HEADER_SIZE
};

/* Globals */
extern struct trans *g_t_out;   /* Channel for outputting libipm messages */
extern struct trans *g_t_in;    /* Channel for inputting libipm messages */
extern struct trans *g_t_vanilla; /* Non-SCP channel */

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

Suite *make_suite_test_libipm_send_calls(void);
Suite *make_suite_test_libipm_recv_calls(void);

#endif /* TEST_LIBIPM__H */
