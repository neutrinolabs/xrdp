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

/* The last frames sent: their commands and NV12 data */
#define MAX_KEPT 4
static struct kept_frame
{
    char cmd[512];
    int cmd_bytes;
    uint8_t data[4096];
    int data_bytes;
} kept[MAX_KEPT];

/******************************************************************************/
/* xrdp takes the frame's mapping and unmaps it once it is encoded */
static int
egfx_cmd(struct wlup *w, char *cmd, int cmd_bytes, char *data, int data_bytes)
{
    if (frames_sent < MAX_KEPT)
    {
        struct kept_frame *k = &kept[frames_sent];

        k->cmd_bytes = MIN(cmd_bytes, (int)sizeof(k->cmd));
        memcpy(k->cmd, cmd, k->cmd_bytes);
        k->data_bytes = MIN(data_bytes, (int)sizeof(k->data));
        memcpy(k->data, data, k->data_bytes);
    }
    frames_sent++;
    munmap(data, data_bytes);
    return 0;
}

/* Little-endian fields of a kept command */
static int
u16_at(int frame, int offset)
{
    const uint8_t *p = (const uint8_t *)kept[frame].cmd + offset;

    return p[0] | (p[1] << 8);
}

static unsigned int
u32_at(int frame, int offset)
{
    return u16_at(frame, offset) | ((unsigned int)u16_at(frame, offset + 2) << 16);
}

/* Offsets in a frame: STARTFRAME is 16 bytes, then WIRETOSURFACE_1 */
#define W2S_SURFACE 24
#define W2S_FLAGS 29
#define W2S_NUM_D 33
#define W2S_RECT_D 35

/* The monitor rect at the end of WIRETOSURFACE_1: left, top, cx, cy */
static int
w2s_monitor(int frame, int field)
{
    int num_d = u16_at(frame, W2S_NUM_D);
    int num_c = u16_at(frame, W2S_RECT_D + num_d * 8);

    return u16_at(frame, W2S_RECT_D + num_d * 8 + 2 + num_c * 8 + field * 2);
}

/* Two client monitors: left one 0..31, right one 32..63 (or from 40
 * with a gap), both the full height of the 64x48 desktop */
static struct xrdp_client_info two_monitors;

static void
use_two_monitors(int right_from)
{
    struct display_size_description *ds = &two_monitors.display_sizes;

    memset(&two_monitors, 0, sizeof(two_monitors));
    ds->monitorCount = 2;
    ds->minfo_wm[0].left = 0;
    ds->minfo_wm[0].top = 0;
    ds->minfo_wm[0].right = 31;
    ds->minfo_wm[0].bottom = 47;
    ds->minfo_wm[1].left = right_from;
    ds->minfo_wm[1].top = 0;
    ds->minfo_wm[1].right = 63;
    ds->minfo_wm[1].bottom = 47;
    v->client_info = &two_monitors;
}

static void
setup(void)
{
    v = mock_module_create(64, 48);
    ck_assert_int_eq(v->mod_connect(v, -1), 0);
    v->gfx_h264 = 1;
    v->server_egfx_cmd = egfx_cmd;
    frames_sent = 0;
    memset(kept, 0, sizeof(kept));
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
/* Damage across two monitors: a frame for each, on its own surface,
 * with the damage relative to the monitor */
START_TEST(test_frame_per_monitor)
{
    uint32_t *px = (uint32_t *)v->pixels;

    use_two_monitors(32);
    px[10 * 64 + 40] = 0xffffff; /* white at 40,10: 8,10 on the right one */
    wlup_gfx_add_damage(v, 20, 4, 24, 10);
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 2);
    ck_assert_int_eq(v->gfx_frame_id, 2);
    ck_assert_int_eq(v->gfx_num_damage, 0);

    /* left monitor: surface 0, x 20..31 */
    ck_assert_int_eq(u32_at(0, 8), 1); /* frame id */
    ck_assert_int_eq(u16_at(0, W2S_SURFACE), 0);
    ck_assert_uint_eq(u32_at(0, W2S_FLAGS), 0);
    ck_assert_int_eq(u16_at(0, W2S_NUM_D), 1);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D), 20);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D + 2), 4);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D + 4), 12);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D + 6), 10);
    ck_assert_int_eq(w2s_monitor(0, 0), 0);
    ck_assert_int_eq(w2s_monitor(0, 2), 32);
    ck_assert_int_eq(w2s_monitor(0, 3), 48);
    ck_assert_int_eq(kept[0].data_bytes, 32 * 48 * 3 / 2);

    /* right monitor: surface 1, monitor 1 in the flags, x 0..11 */
    ck_assert_int_eq(u32_at(1, 8), 2);
    ck_assert_int_eq(u16_at(1, W2S_SURFACE), 1);
    ck_assert_uint_eq(u32_at(1, W2S_FLAGS), 1u << 28);
    ck_assert_int_eq(u16_at(1, W2S_RECT_D), 0);
    ck_assert_int_eq(u16_at(1, W2S_RECT_D + 4), 12);
    ck_assert_int_eq(w2s_monitor(1, 0), 32);
    ck_assert_int_eq(w2s_monitor(1, 1), 0);
    ck_assert_int_eq(w2s_monitor(1, 2), 32);

    /* the white pixel is in the right monitor's picture, at 8,10 */
    /* (BT.709 rounding makes white 254) */
    ck_assert_int_ge(kept[1].data[10 * 32 + 8], 250);
    ck_assert_int_eq(kept[1].data[10 * 32 + 9], 0);
}
END_TEST

/******************************************************************************/
/* The next update waits for the last frame of the one before */
START_TEST(test_monitors_wait_for_last_ack)
{
    use_two_monitors(32);
    wlup_gfx_add_damage(v, 0, 0, 64, 10);
    wlup_gfx_poll(v);
    ck_assert_int_eq(frames_sent, 2);

    wlup_gfx_add_damage(v, 0, 20, 64, 10);
    wlup_gfx_frame_ack(v, 1);
    ck_assert_int_eq(frames_sent, 2);
    wlup_gfx_frame_ack(v, 2);
    ck_assert_int_eq(frames_sent, 4);
}
END_TEST

/******************************************************************************/
/* Damage on one monitor: only its surface gets a frame */
START_TEST(test_only_damaged_monitor)
{
    use_two_monitors(32);
    wlup_gfx_add_damage(v, 50, 30, 4, 4);
    wlup_gfx_poll(v);
    ck_assert_int_eq(frames_sent, 1);
    ck_assert_int_eq(u16_at(0, W2S_SURFACE), 1);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D), 18);
    ck_assert_int_eq(u16_at(0, W2S_RECT_D + 2), 30);
}
END_TEST

/******************************************************************************/
/* Damage between the monitors shows nowhere: no frame, and it does not
 * stay behind to be tried again */
START_TEST(test_damage_between_monitors)
{
    use_two_monitors(40);
    wlup_gfx_add_damage(v, 33, 0, 6, 48);
    ck_assert_int_eq(wlup_gfx_poll(v), 0);
    ck_assert_int_eq(frames_sent, 0);
    ck_assert_int_eq(v->gfx_frame_id, 0);
    ck_assert_int_eq(v->gfx_num_damage, 0);
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
    tcase_add_test(tc, test_frame_per_monitor);
    tcase_add_test(tc, test_monitors_wait_for_last_ack);
    tcase_add_test(tc, test_only_damaged_monitor);
    tcase_add_test(tc, test_damage_between_monitors);
    suite_add_tcase(s, tc);
    return s;
}
