#ifndef TEST_XRDP_H
#define TEST_XRDP_H

#include <check.h>

Suite *make_suite_test_bitmap_load(void);
Suite *make_suite_test_keymap_load(void);
Suite *make_suite_egfx_base_functions(void);
Suite *make_suite_region(void);
Suite *make_suite_tconfig_load_gfx(void);
#if defined(XRDP_OPENH264) || defined(XRDP_X264)
Suite *make_suite_encoder(void);
#endif
#if defined(XRDP_OPENH264)
Suite *make_suite_encoder_openh264(void);
#endif

Suite *make_suite_login(void);
#endif /* TEST_XRDP_H */
