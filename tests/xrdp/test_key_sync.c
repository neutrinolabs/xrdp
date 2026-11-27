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
 * Tests for keyboard toggle-key (CapsLock / NumLock / ScrollLock) state
 * handling in the window manager. libxrdp reports a client TS_SYNC_EVENT
 * ([MS-RDPBCGR] 2.2.8.1.1.3.1.1.5) as RDP_INPUT_SYNCHRONIZE with the
 * toggleFlags in param1. The wm must mirror the flags for its own key
 * mapping and hand them to the backend module unchanged as WM_KEYBRD_SYNC.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "test_xrdp.h"

#include "xrdp.h"
#include "ms-rdpbcgr.h"
#include <string.h>

static int g_calls;
static int g_msg;
static long g_param1;
static long g_param3;

static int
stub_mod_event(struct xrdp_mod *v, int msg, long param1, long param2,
               long param3, long param4)
{
    g_calls++;
    g_msg = msg;
    g_param1 = param1;
    g_param3 = param3;
    return 0;
}

struct fixture
{
    struct xrdp_process process;
    struct xrdp_wm wm;
    struct xrdp_mm mm;
    struct xrdp_mod mod;
};

static void
init_fixture(struct fixture *f, int with_mod)
{
    memset(f, 0, sizeof(*f));
    f->process.wm = &f->wm;
    f->wm.mm = &f->mm;
    f->mod.mod_event = stub_mod_event;
    if (with_mod)
    {
        f->mm.mod = &f->mod;
    }
    g_calls = 0;
    g_msg = -1;
    g_param1 = -1;
    g_param3 = -1;
}

START_TEST(test_key_sync__mirrors_flags_and_forwards_to_module)
{
    struct fixture f;
    int flags;

    for (flags = 0; flags <= 0xf; ++flags)
    {
        init_fixture(&f, 1);
        ck_assert_int_eq(callback(&f.process, RDP_INPUT_SYNCHRONIZE,
                                  flags, 0, 0, 0), 0);
        ck_assert_int_eq(f.wm.scroll_lock, (flags & TS_SYNC_SCROLL_LOCK) != 0);
        ck_assert_int_eq(f.wm.num_lock, (flags & TS_SYNC_NUM_LOCK) != 0);
        ck_assert_int_eq(f.wm.caps_lock, (flags & TS_SYNC_CAPS_LOCK) != 0);
        ck_assert_int_eq(g_calls, 1);
        ck_assert_int_eq(g_msg, WM_KEYBRD_SYNC);
        ck_assert_int_eq(g_param1, flags);
        ck_assert_int_eq(g_param3, flags);
    }
}
END_TEST

/* A sync event replaces the state; it does not accumulate */
START_TEST(test_key_sync__replaces_previous_state)
{
    struct fixture f;

    init_fixture(&f, 1);
    f.wm.caps_lock = 1;
    f.wm.num_lock = 1;
    f.wm.scroll_lock = 1;
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SYNCHRONIZE,
                              TS_SYNC_NUM_LOCK, 0, 0, 0), 0);
    ck_assert_int_eq(f.wm.caps_lock, 0);
    ck_assert_int_eq(f.wm.num_lock, 1);
    ck_assert_int_eq(f.wm.scroll_lock, 0);
}
END_TEST

/* Before the backend is connected (login screen) the event is saved so
 * xrdp_mm_connect() can replay it once the module is up */
START_TEST(test_key_sync__saved_when_no_module)
{
    struct fixture f;
    int flags = TS_SYNC_NUM_LOCK | TS_SYNC_CAPS_LOCK;

    init_fixture(&f, 0);
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SYNCHRONIZE,
                              flags, 0, 0, 0), 0);
    ck_assert_int_eq(g_calls, 0);
    ck_assert_int_eq(f.mm.last_sync_saved, 1);
    ck_assert_int_eq(f.mm.last_sync_key_flags, flags);
    ck_assert_int_eq(f.wm.num_lock, 1);
    ck_assert_int_eq(f.wm.caps_lock, 1);
    ck_assert_int_eq(f.wm.scroll_lock, 0);
}
END_TEST

/* The wm tracks lock keys pressed on the login screen itself. Windows
 * clients auto-repeat held keys, so a held CapsLock arrives as a stream
 * of key-down events; only the first one toggles on the client */
START_TEST(test_key_press__held_caps_lock_toggles_once)
{
    struct fixture f;

    init_fixture(&f, 0);
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SCANCODE,
                              SCANCODE_CAPS_KEY, 0, 0, 0), 0);
    ck_assert_int_eq(f.wm.caps_lock, 1);
    /* auto-repeat: another key-down with no key-up in between */
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SCANCODE,
                              SCANCODE_CAPS_KEY, 0, 0, 0), 0);
    ck_assert_int_eq(f.wm.caps_lock, 1);
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SCANCODE,
                              SCANCODE_CAPS_KEY, 0, KBDFLAGS_RELEASE, 0), 0);
    ck_assert_int_eq(f.wm.caps_lock, 1);
    /* a fresh press toggles it off */
    ck_assert_int_eq(callback(&f.process, RDP_INPUT_SCANCODE,
                              SCANCODE_CAPS_KEY, 0, 0, 0), 0);
    ck_assert_int_eq(f.wm.caps_lock, 0);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_key_sync(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("KeySync");

    tc = tcase_create("key_sync");
    tcase_add_test(tc, test_key_sync__mirrors_flags_and_forwards_to_module);
    tcase_add_test(tc, test_key_sync__replaces_previous_state);
    tcase_add_test(tc, test_key_sync__saved_when_no_module);
    tcase_add_test(tc, test_key_press__held_caps_lock_toggles_once);
    suite_add_tcase(s, tc);

    return s;
}
