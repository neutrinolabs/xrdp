/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg, Christopher Pitstick 2004-2023
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
 *
 * Test driver for XRDP routines
 *
 * If you want to run this driver under valgrind to check for memory leaks,
 * use the following command line:-
 *
 * CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all \
 *     .libs/test_xrdp
 *
 * without the 'CK_FORK=no', memory still allocated by the test driver will
 * be logged
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "log.h"
#include "os_calls.h"
#include <stdlib.h>

#if defined(XRDP_PIXMAN)
#include <pixman.h>
#else
#include "pixman-region.h"
#endif

#include "test_xrdp.h"
#include "xrdp.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#ifdef __cplusplus
#error "cmocka is not supported with C++"
#else
#include <cmocka.h>
#endif

#define UNUSED(x) (void)(x)

// Mock functions
static pixman_box16_t test_box =
{
    0, 0, 100, 100
};

pixman_box16_t *__wrap_pixman_region_extents(pixman_region16_t *region)
{
    check_expected_ptr(region);
    return mock_ptr_type(pixman_box16_t *);
}

static void test_xrdp_region_get_bounds__negligent_path(void **state)
{
    struct xrdp_region *region = g_new0(struct xrdp_region, 1);
    struct xrdp_rect *rect = g_new0(struct xrdp_rect, 1);

    // Cmocka boilerplate
    UNUSED(state);
    expect_any(__wrap_pixman_region_extents, region);
    will_return(__wrap_pixman_region_extents, NULL);

    const int ret = xrdp_region_get_bounds(region, rect);

    assert_int_equal(1, ret);

    g_free(region);
    g_free(rect);
}

static void test_xrdp_region_get_bounds__happy_path(void **state)
{
    struct xrdp_region *region = g_new0(struct xrdp_region, 1);
    struct xrdp_rect *rect = g_new0(struct xrdp_rect, 1);

    // Cmocka boilerplate
    UNUSED(state);
    expect_any(__wrap_pixman_region_extents, region);
    will_return(__wrap_pixman_region_extents, &test_box);

    const int ret = xrdp_region_get_bounds(region, rect);

    assert_int_equal(0, ret);
    assert_int_equal(0, rect->top);
    assert_int_equal(0, rect->left);
    assert_int_equal(100, rect->bottom);
    assert_int_equal(100, rect->right);

    g_free(region);
    g_free(rect);
}

pixman_bool_t __wrap_pixman_region_not_empty(pixman_region16_t *region)
{
    check_expected_ptr(region);
    return mock_type(pixman_bool_t);
}

static void test_xrdp_region_not_empty__happy_path(void **state)
{
    struct xrdp_region *region = g_new0(struct xrdp_region, 1);

    // Cmocka boilerplate
    UNUSED(state);
    expect_any(__wrap_pixman_region_not_empty, region);
    will_return(__wrap_pixman_region_not_empty, (pixman_bool_t)0);

    const int ret = xrdp_region_not_empty(region);

    assert_int_equal(0, ret);

    g_free(region);
}

START_TEST(execute_suite)
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(test_xrdp_region_get_bounds__negligent_path),
        cmocka_unit_test(test_xrdp_region_get_bounds__happy_path),
        cmocka_unit_test(test_xrdp_region_not_empty__happy_path)
    };

    ck_assert_int_eq(cmocka_run_group_tests(tests, NULL, NULL), 0);
}
END_TEST

/******************************************************************************/
/*
For an example of how to use cmocka, see the following:
https://gitlab.com/cmocka/cmocka/-/blob/master/example/mock/uptime/test_uptime.c
*/
Suite *
make_suite_region(void)
{
    Suite *s;
    TCase *tc_region;

    s = suite_create("test_xrdp_region");

    tc_region = tcase_create("test_xrdp_region");
    tcase_add_test(tc_region, execute_suite);

    suite_add_tcase(s, tc_region);

    return s;
}
