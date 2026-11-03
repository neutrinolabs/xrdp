/**
 * xrdp: A Remote Desktop Protocol server.
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
 * Tests for xrdp_orders.c
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "os_calls.h"

#include "test_libxrdp.h"

/* An order larger than the order buffer must be refused even when an
 * order batch is already open (order_level >= 1). The caller (e.g. the
 * glyph cache) writes max_size bytes after a successful check. */
START_TEST(test_xrdp_orders_check__oversize_in_open_batch_is_refused)
{
    struct xrdp_rdp *rdp = g_new0(struct xrdp_rdp, 1);
    struct xrdp_orders *orders = xrdp_orders_create(NULL, rdp);
    int max_order_size;

    /* No client caps: the order buffer limit is 16K - 256 */
    max_order_size = 16 * 1024 - 256;
    orders->order_level = 1;
    orders->order_count_ptr = orders->out_s->data;
    orders->out_s->p = orders->out_s->data;

    ck_assert_int_ne(xrdp_orders_check(orders, max_order_size + 1), 0);

    xrdp_orders_delete(orders);
    g_free(rdp);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_xrdp_orders(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("test_xrdp_orders");

    tc = tcase_create("xrdp_orders_check");
    suite_add_tcase(s, tc);
    tcase_add_test(tc,
                   test_xrdp_orders_check__oversize_in_open_batch_is_refused);

    return s;
}
