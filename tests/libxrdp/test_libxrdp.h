#ifndef TEST_LIBXRDP_H
#define TEST_LIBXRDP_H

#include <check.h>

Suite *make_suite_test_xrdp_sec_process_mcs_data_monitors(void);
Suite *make_suite_test_monitor_processing(void);
#if defined(XRDP_NLA)
Suite *make_suite_test_xrdp_nla(void);
#endif

#endif /* TEST_LIBXRDP_H */
