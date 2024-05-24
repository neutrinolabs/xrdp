
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <check.h>

char *
bin_to_hex(const char *input, int length);

Suite *make_suite_test_fifo(void);
Suite *make_suite_test_list(void);
Suite *make_suite_test_parse(void);
Suite *make_suite_test_string(void);
Suite *make_suite_test_string_unicode(void);
Suite *make_suite_test_os_calls(void);
Suite *make_suite_test_ssl_calls(void);
Suite *make_suite_test_base64(void);
Suite *make_suite_test_guid(void);
Suite *make_suite_test_scancode(void);

TCase *make_tcase_test_os_calls_signals(void);

void os_calls_signals_init(void);
void os_calls_signals_deinit(void);

#endif /* TEST_COMMON_H */
