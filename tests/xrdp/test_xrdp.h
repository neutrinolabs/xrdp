#ifndef TEST_XRDP_H
#define TEST_XRDP_H

#include <check.h>

Suite *make_suite_test_bitmap_load(void);
Suite *make_suite_test_keymap_load(void);
Suite *make_suite_egfx_base_functions(void);
Suite *make_suite_region(void);
Suite *make_suite_tconfig_load_gfx(void);

#endif /* TEST_XRDP_H */
