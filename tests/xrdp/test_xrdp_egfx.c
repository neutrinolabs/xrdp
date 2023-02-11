/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2021
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
#include "xrdp_egfx.h"
#include "test_xrdp.h"

START_TEST(test_xrdp_egfx_send_create_surface__happy_path)
{
    struct xrdp_egfx_bulk *bulk = g_new0(struct xrdp_egfx_bulk, 1);

    const int surface_id = 0xFF;
    const int width = 640;
    const int height = 480;
    const int pixel_format = 32;

    struct stream *s = xrdp_egfx_create_surface(
                           bulk, surface_id, width, height, pixel_format);
    s->p = s->data;

    unsigned char descriptor;
    in_uint8(s, descriptor);
    ck_assert_int_eq(0xE0, descriptor);

    g_free(bulk);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_egfx_base_functions(void)
{
    Suite *s;
    TCase *tc_process_monitors;

    s = suite_create("test_xrdp_egfx_base_functions");

    tc_process_monitors = tcase_create("xrdp_egfx_base_functions");
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_send_create_surface__happy_path);

    suite_add_tcase(s, tc_process_monitors);

    return s;
}

