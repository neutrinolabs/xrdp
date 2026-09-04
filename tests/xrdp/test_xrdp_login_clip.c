/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2026
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
 * Tests for the login screen clipboard module
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "xrdp.h"
#include "xrdp_login_clip.h"
#include "scancode.h"

#include "test_xrdp.h"

#define XK_v 0x0076

/* Key state array as held by struct xrdp_wm */
static int g_keys[SCANCODE_MAX_INDEX + 1];

static void
clear_keys(void)
{
    g_memset(g_keys, 0, sizeof(g_keys));
}

/******************************************************************************/
START_TEST(test_is_paste_key__ctrl_v)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 1);

    clear_keys();
    g_keys[SCANCODE_INDEX_RCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 1);
}
END_TEST

/******************************************************************************/
/* Windows clients send AltGr as LCTRL + RALT. AltGr+V must not paste */
START_TEST(test_is_paste_key__altgr_v_is_not_paste)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    g_keys[SCANCODE_INDEX_RALT_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__plain_v_is_not_paste)
{
    struct xrdp_key_info ki = { XK_v, 'v' };

    clear_keys();
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__ctrl_other_key_is_not_paste)
{
    struct xrdp_key_info ki = { 0x0063, 'c' };

    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2e, &ki), 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_is_paste_key__shift_insert)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LSHIFT_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_INSERT_KEY,
                     NULL), 1);

    clear_keys();
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, SCANCODE_INSERT_KEY,
                     NULL), 0);
}
END_TEST

/******************************************************************************/
/* A NULL key info must never be dereferenced */
START_TEST(test_is_paste_key__null_key_info)
{
    clear_keys();
    g_keys[SCANCODE_INDEX_LCTRL_KEY] = 1;
    ck_assert_int_eq(xrdp_login_clip_is_paste_key(g_keys, 0x2f, NULL), 0);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_login_clip(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("LoginClip");

    tc = tcase_create("xrdp_login_clip");
    tcase_add_test(tc, test_is_paste_key__ctrl_v);
    tcase_add_test(tc, test_is_paste_key__altgr_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__plain_v_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__ctrl_other_key_is_not_paste);
    tcase_add_test(tc, test_is_paste_key__shift_insert);
    tcase_add_test(tc, test_is_paste_key__null_key_info);

    suite_add_tcase(s, tc);

    return s;
}
