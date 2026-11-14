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
 * wlup unit tests: what the module core promises, seen from a backend
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "scancode.h"
#include "trans.h"
#include "wlup_backend.h"
#include "xrdp_constants.h"

#include "test_wlup.h"

static struct wlup *v;

/******************************************************************************/
static void
setup(void)
{
    v = mock_module_create(64, 48);
    ck_assert_int_eq(v->mod_connect(v, -1), 0);
}

static void
teardown(void)
{
    mock_module_destroy(v);
}

/******************************************************************************/
START_TEST(test_connect_uses_backend)
{
    ck_assert_int_eq(mock.connects, 1);
    ck_assert_ptr_nonnull(v->ops);
    ck_assert_str_eq(v->ops->name, "mock");
    v->mod_end(v);
    ck_assert_int_eq(mock.disconnects, 1);
}
END_TEST

/******************************************************************************/
/* A failed connect releases what it made, and no other op runs after it */
START_TEST(test_connect_failure)
{
    struct wlup *w = mock_module_create(64, 48);

    mock.connect_rv = 1;
    ck_assert_int_ne(w->mod_connect(w, -1), 0);
    ck_assert_int_eq(mock.disconnects, 1);
    ck_assert_ptr_null(w->ops);
    w->mod_event(w, WM_KEYDOWN, 0, 0, 0x1e, 0);
    w->mod_event(w, WM_MOUSEMOVE, 1, 2, 0, 0);
    ck_assert_int_eq(mock.keys, 0);
    ck_assert_int_eq(mock.mice, 0);
    ck_assert_int_eq(w->mod_check_wait_objs(w), 0);
    ck_assert_int_eq(mock.checks, 0);
    mock_module_destroy(w);
    ck_assert_int_eq(mock.disconnects, 1);
}
END_TEST

/******************************************************************************/
START_TEST(test_wants_frames)
{
    struct source_info si = { 0 };

    ck_assert_int_eq(wlup_wants_frames(v), 1);
    v->suppress_output = 1;
    ck_assert_int_eq(wlup_wants_frames(v), 0);
    v->suppress_output = 0;

    /* Output xrdp still holds for the client, from what wlup painted */
    v->si = &si;
    si.source[XRDP_SOURCE_MOD] = 50000;
    ck_assert_int_eq(wlup_wants_frames(v), 1);
    si.source[XRDP_SOURCE_MOD] = 50001;
    ck_assert_int_eq(wlup_wants_frames(v), 0);
    v->si = NULL;
}
END_TEST

/******************************************************************************/
/* The core asks the backend for frames again whenever that may change */
START_TEST(test_frames_wanted_after_ack)
{
    v->mod_frame_ack(v, 0, 1);
    ck_assert_int_eq(mock.frames_wanted, 1);
}
END_TEST

START_TEST(test_frames_wanted_after_events)
{
    ck_assert_int_eq(v->mod_check_wait_objs(v), 0);
    ck_assert_int_eq(mock.checks, 1);
    ck_assert_int_eq(mock.frames_wanted, 1);
}
END_TEST

START_TEST(test_backend_error_ends_session)
{
    mock.check_rv = 1;
    ck_assert_int_ne(v->mod_check_wait_objs(v), 0);
    ck_assert_int_eq(mock.frames_wanted, 0);
}
END_TEST

/******************************************************************************/
/* Nothing is painted while output is suppressed; the resume repaints all
 * of the desktop image and asks for frames again */
START_TEST(test_suppress_and_resume)
{
    v->mod_suppress_output(v, 1, 0, 0, 0, 0);
    ck_assert_int_eq(wlup_paint_region(v, 0, 0, 10, 10), 0);
    ck_assert_int_eq(server.paints, 0);

    v->mod_suppress_output(v, 0, 0, 0, 64, 48);
    ck_assert_int_eq(server.paints, 1);
    ck_assert_int_eq(server.last_paint_x, 0);
    ck_assert_int_eq(server.last_paint_y, 0);
    ck_assert_int_eq(server.last_paint_cx, 64);
    ck_assert_int_eq(server.last_paint_cy, 48);
    ck_assert_int_eq(mock.frames_wanted, 1);
}
END_TEST

/******************************************************************************/
START_TEST(test_paint_is_clipped)
{
    ck_assert_int_eq(wlup_paint_region(v, -10, -10, 1000, 1000), 0);
    ck_assert_int_eq(server.paints, 1);
    ck_assert_int_eq(server.last_paint_x, 0);
    ck_assert_int_eq(server.last_paint_y, 0);
    ck_assert_int_eq(server.last_paint_cx, 64);
    ck_assert_int_eq(server.last_paint_cy, 48);

    /* Entirely outside: nothing */
    ck_assert_int_eq(wlup_paint_region(v, 100, 100, 10, 10), 0);
    ck_assert_int_eq(server.paints, 1);
}
END_TEST

/******************************************************************************/
/* The backend gets Linux evdev codes, whatever the RDP scancode set */
START_TEST(test_key_is_evdev)
{
    scancode_set_keycode_set("evdev");
    /* RDP scancode 0x1e is the A key, evdev KEY_A is 30 */
    v->mod_event(v, WM_KEYDOWN, 0, 0, 0x1e, 0);
    ck_assert_int_eq(mock.keys, 1);
    ck_assert_int_eq(mock.last_key, 30);
    ck_assert_int_eq(mock.last_key_down, 1);
    v->mod_event(v, WM_KEYUP, 0, 0, 0x1e, 0);
    ck_assert_int_eq(mock.last_key_down, 0);
}
END_TEST

START_TEST(test_mouse_is_forwarded)
{
    v->mod_event(v, WM_MOUSEMOVE, 10, 20, 0, 0);
    ck_assert_int_eq(mock.mice, 1);
    ck_assert_int_eq(mock.last_mouse_msg, WM_MOUSEMOVE);
    ck_assert_int_eq(mock.last_mouse_x, 10);
    ck_assert_int_eq(mock.last_mouse_y, 20);
}
END_TEST

/******************************************************************************/
/* xrdp waits for a resize under way until the backend says it is done */
START_TEST(test_resize_in_progress)
{
    int in_progress = -1;

    mock.resize_rv = 1;
    v->mod_server_monitor_resize(v, 80, 60, 1, NULL, &in_progress);
    ck_assert_int_eq(mock.resizes, 1);
    ck_assert_int_eq(mock.last_resize_width, 80);
    ck_assert_int_eq(mock.last_resize_height, 60);
    ck_assert_int_eq(in_progress, 1);
    ck_assert_int_eq(v->resize_pending, 1);

    wlup_resize_finished(v);
    ck_assert_int_eq(server.resize_dones, 1);
    ck_assert_int_eq(v->resize_pending, 0);
    /* Only once */
    wlup_resize_finished(v);
    ck_assert_int_eq(server.resize_dones, 1);
}
END_TEST

START_TEST(test_resize_unsupported)
{
    int in_progress = -1;

    mock.resize_rv = 0;
    v->mod_server_monitor_resize(v, 80, 60, 1, NULL, &in_progress);
    ck_assert_int_eq(in_progress, 0);
    ck_assert_int_eq(v->resize_pending, 0);
}
END_TEST

START_TEST(test_resize_to_same_size)
{
    int in_progress = -1;

    v->mod_server_monitor_resize(v, 64, 48, 1, NULL, &in_progress);
    ck_assert_int_eq(mock.resizes, 0);
    ck_assert_int_eq(in_progress, 0);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_wlup_core(void)
{
    Suite *s = suite_create("wlup core");
    TCase *tc = tcase_create("with a mock backend");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_connect_uses_backend);
    tcase_add_test(tc, test_connect_failure);
    tcase_add_test(tc, test_wants_frames);
    tcase_add_test(tc, test_frames_wanted_after_ack);
    tcase_add_test(tc, test_frames_wanted_after_events);
    tcase_add_test(tc, test_backend_error_ends_session);
    tcase_add_test(tc, test_suppress_and_resume);
    tcase_add_test(tc, test_paint_is_clipped);
    tcase_add_test(tc, test_key_is_evdev);
    tcase_add_test(tc, test_mouse_is_forwarded);
    tcase_add_test(tc, test_resize_in_progress);
    tcase_add_test(tc, test_resize_unsupported);
    tcase_add_test(tc, test_resize_to_same_size);
    suite_add_tcase(s, tc);
    return s;
}
