
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <check.h>

char *
bin_to_hex(const char *input, int length);

Suite *make_suite_test_string(void);
Suite *make_suite_test_os_calls(void);
Suite *make_suite_test_ssl_calls(void);
Suite *make_suite_test_base64(void);

#endif /* TEST_COMMON_H */
