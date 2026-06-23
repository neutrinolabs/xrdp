/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2024
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <limits.h>

#include "xrdp_client_info.h"

#include "test_common.h"

/******************************************************************************/
/* DPI = (height_pixels * 127) / (height_mm * 5), integer truncated */

START_TEST(test_calc_dpi__hidpi)
{
    /* 2160 px over 392 mm = 139.95 -> 139 (truncated) */
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(2160, 392), 139);
}
END_TEST

START_TEST(test_calc_dpi__midpi)
{
    /* 1440 px over 392 mm = 93.30 -> 93 */
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(1440, 392), 93);
}
END_TEST

START_TEST(test_calc_dpi__nominal_96)
{
    /* 1080 px over 286 mm = 95.91; integer truncation yields 95
       (~96 nominal). This documents the truncation behavior. */
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(1080, 286), 95);
}
END_TEST

START_TEST(test_calc_dpi__zero_mm_is_invalid)
{
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(2160, 0), 0);
}
END_TEST

START_TEST(test_calc_dpi__zero_pixels_is_invalid)
{
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(0, 392), 0);
}
END_TEST

START_TEST(test_calc_dpi__pixel_overflow_is_invalid)
{
    /* A huge pixel height (e.g. from an unsigned underflow of
       bottom - top + 1) must not overflow the arithmetic */
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(UINT_MAX, 392), 0);
}
END_TEST

START_TEST(test_calc_dpi__mm_overflow_is_invalid)
{
    ck_assert_uint_eq(xrdp_client_info_calculate_dpi(2160, UINT_MAX), 0);
}
END_TEST

/******************************************************************************/
/* Session DPI range check: XRDP_SESSION_DPI_MIN..XRDP_SESSION_DPI_MAX */

START_TEST(test_valid__below_min)
{
    ck_assert_int_eq(xrdp_client_info_dpi_valid_for_session(49), 0);
}
END_TEST

START_TEST(test_valid__at_min)
{
    ck_assert_int_ne(xrdp_client_info_dpi_valid_for_session(50), 0);
}
END_TEST

START_TEST(test_valid__in_range)
{
    ck_assert_int_ne(xrdp_client_info_dpi_valid_for_session(139), 0);
}
END_TEST

START_TEST(test_valid__at_max)
{
    ck_assert_int_ne(xrdp_client_info_dpi_valid_for_session(400), 0);
}
END_TEST

START_TEST(test_valid__above_max)
{
    ck_assert_int_eq(xrdp_client_info_dpi_valid_for_session(401), 0);
}
END_TEST

START_TEST(test_valid__zero)
{
    ck_assert_int_eq(xrdp_client_info_dpi_valid_for_session(0), 0);
}
END_TEST

START_TEST(test_valid__computed_low_dpi_rejected)
{
    /* 640 px over 400 mm = 40 DPI -> below range -> rejected */
    unsigned int dpi = xrdp_client_info_calculate_dpi(640, 400);
    ck_assert_uint_eq(dpi, 40);
    ck_assert_int_eq(xrdp_client_info_dpi_valid_for_session(dpi), 0);
}
END_TEST

START_TEST(test_valid__computed_high_dpi_rejected)
{
    /* 3840 px over 100 mm = 975 DPI -> above range -> rejected */
    unsigned int dpi = xrdp_client_info_calculate_dpi(3840, 100);
    ck_assert_uint_eq(dpi, 975);
    ck_assert_int_eq(xrdp_client_info_dpi_valid_for_session(dpi), 0);
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_xrdp_client_info(void)
{
    Suite *s;
    TCase *tc_calc;
    TCase *tc_valid;

    s = suite_create("XrdpClientInfo");

    tc_calc = tcase_create("calculate_dpi");
    suite_add_tcase(s, tc_calc);
    tcase_add_test(tc_calc, test_calc_dpi__hidpi);
    tcase_add_test(tc_calc, test_calc_dpi__midpi);
    tcase_add_test(tc_calc, test_calc_dpi__nominal_96);
    tcase_add_test(tc_calc, test_calc_dpi__zero_mm_is_invalid);
    tcase_add_test(tc_calc, test_calc_dpi__zero_pixels_is_invalid);
    tcase_add_test(tc_calc, test_calc_dpi__pixel_overflow_is_invalid);
    tcase_add_test(tc_calc, test_calc_dpi__mm_overflow_is_invalid);

    tc_valid = tcase_create("dpi_valid_for_session");
    suite_add_tcase(s, tc_valid);
    tcase_add_test(tc_valid, test_valid__below_min);
    tcase_add_test(tc_valid, test_valid__at_min);
    tcase_add_test(tc_valid, test_valid__in_range);
    tcase_add_test(tc_valid, test_valid__at_max);
    tcase_add_test(tc_valid, test_valid__above_max);
    tcase_add_test(tc_valid, test_valid__zero);
    tcase_add_test(tc_valid, test_valid__computed_low_dpi_rejected);
    tcase_add_test(tc_valid, test_valid__computed_high_dpi_rejected);

    return s;
}
