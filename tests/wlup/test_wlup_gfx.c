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
 * wlup unit tests: frames for the GFX H.264 encoder (wlup_gfx.c)
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <limits.h>
#include <string.h>
#include <sys/mman.h>

#include "os_calls.h"
#include "wlup_gfx.h"

#include "test_wlup.h"

static struct wlup *v;
static int frames_sent;

/******************************************************************************/
/* xrdp takes the frame's mapping and unmaps it once it is encoded */
static int
egfx_cmd(struct wlup *w, char *cmd, int cmd_bytes, char *data, int data_bytes)
{
    frames_sent++;
    munmap(data, data_bytes);
    return 0;
}

static void
setup(void)
{
    v = mock_module_create(64, 48);
    ck_assert_int_eq(v->mod_connect(v, -1), 0);
    v->gfx_h264 = 1;
    v->server_egfx_cmd = egfx_cmd;
    frames_sent = 0;
}

static void
teardown(void)
{
    mock_module_destroy(v);
}

/******************************************************************************/
START_TEST(test_damage_kept_then_merged)
{
    short *d = v->gfx_damage;
    int i;

    for (i = 0; i < WLUP_MAX_DAMAGE; i++)
    {
        wlup_gfx_add_damage(v, i % 8 * 8, i / 8 * 6, 2, 2);
    }
    ck_assert_int_eq(v->gfx_num_damage, WLUP_MAX_DAMAGE);
    ck_assert_int_eq(v->gfx_damage_overflow, 0);

    /* One more: all of them become their bounding box */
    wlup_gfx_add_damage(v, 60, 46, 4, 2);
    ck_assert_int_eq(v->gfx_num_damage, 1);
    ck_assert_int_eq(v->gfx_damage_overflow, 1);
    ck_assert_int_eq(d[0], 0);
    ck_assert_int_eq(d[1], 0);
    ck_assert_int_eq(d[0] + d[2], 64);
    ck_assert_int_eq(d[1] + d[3], 48);
}
END_TEST

/******************************************************************************/
/* A frame goes once the one before has been acknowledged */
START_TEST(test_frames_wait_for_ack)
{
    wlup_gfx_add_damage(v, 0, 0, 10, 10);
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 1);
    ck_assert_int_eq(v->gfx_frame_id, 1);
    ck_assert_int_eq(v->gfx_num_damage, 0);

    /* No ack yet: the damage waits */
    wlup_gfx_add_damage(v, 20, 20, 10, 10);
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 1);
    ck_assert_int_eq(v->gfx_num_damage, 1);

    /* The ack sends it straight away */
    wlup_gfx_frame_ack(v, 1);
    ck_assert_int_eq(frames_sent, 2);
    ck_assert_int_eq(v->gfx_frame_id, 2);
    ck_assert_int_eq(v->gfx_num_damage, 0);
}
END_TEST

START_TEST(test_nothing_to_send)
{
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 0);
}
END_TEST

/******************************************************************************/
/* A frame whose ack never comes does not stop the frames for good */
START_TEST(test_lost_ack)
{
    int timeout = -1;

    wlup_gfx_add_damage(v, 0, 0, 10, 10);
    wlup_gfx_poll(v);
    wlup_gfx_add_damage(v, 20, 20, 10, 10);

    /* Waiting: xrdp is asked to wake wlup in time to notice */
    wlup_gfx_get_timeout(v, &timeout);
    ck_assert_int_ge(timeout, 0);
    ck_assert_int_le(timeout, 2000);

    v->gfx_sent_time = g_get_elapsed_ms() - 2500;
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 2);
}
END_TEST

START_TEST(test_no_timeout_when_acked)
{
    int timeout = -1;

    wlup_gfx_add_damage(v, 0, 0, 10, 10);
    wlup_gfx_get_timeout(v, &timeout);
    ck_assert_int_eq(timeout, -1);
}
END_TEST

/******************************************************************************/
/* xrdp acknowledges INT_MAX when it has no encoder */
START_TEST(test_ack_beyond_last_frame)
{
    wlup_gfx_add_damage(v, 0, 0, 10, 10);
    wlup_gfx_poll(v);
    wlup_gfx_frame_ack(v, INT_MAX);
    ck_assert_int_eq(v->gfx_frame_acked, v->gfx_frame_id);
}
END_TEST

START_TEST(test_reset_forgets_frame)
{
    wlup_gfx_add_damage(v, 0, 0, 10, 10);
    wlup_gfx_poll(v);
    wlup_gfx_add_damage(v, 20, 20, 10, 10);
    wlup_gfx_reset(v);
    ck_assert_int_eq(v->gfx_frame_acked, v->gfx_frame_id);
    ck_assert_int_eq(v->gfx_num_damage, 0);
}
END_TEST

/******************************************************************************/
/* The copy for the frames is widened to even coordinates, as the NV12
 * conversion works on 2x2 blocks */
START_TEST(test_shadow_copy_widened)
{
    char *sh;

    /* The first copy takes all of the image */
    memset(v->pixels, 1, v->pixels_size);
    wlup_gfx_copy_damage(v, 0, 0, 1, 1);
    ck_assert_ptr_nonnull(v->gfx_shadow);
    ck_assert_int_eq(v->gfx_shadow[(47 * 64 + 63) * 4], 1);

    memset(v->pixels, 2, v->pixels_size);
    wlup_gfx_copy_damage(v, 3, 3, 2, 2);
    sh = v->gfx_shadow;
    /* x and y 3..4 widen to 2..5 */
    ck_assert_int_eq(sh[(2 * 64 + 2) * 4], 2);
    ck_assert_int_eq(sh[(5 * 64 + 5) * 4], 2);
    ck_assert_int_eq(sh[(1 * 64 + 1) * 4], 1);
    ck_assert_int_eq(sh[(6 * 64 + 6) * 4], 1);
    wlup_gfx_free_shadow(v);
    ck_assert_ptr_null(v->gfx_shadow);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_wlup_gfx(void)
{
    Suite *s = suite_create("wlup gfx");
    TCase *tc = tcase_create("H.264 frames");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_damage_kept_then_merged);
    tcase_add_test(tc, test_frames_wait_for_ack);
    tcase_add_test(tc, test_nothing_to_send);
    tcase_add_test(tc, test_lost_ack);
    tcase_add_test(tc, test_no_timeout_when_acked);
    tcase_add_test(tc, test_ack_beyond_last_frame);
    tcase_add_test(tc, test_reset_forgets_frame);
    tcase_add_test(tc, test_shadow_copy_widened);
    suite_add_tcase(s, tc);
    return s;
}
