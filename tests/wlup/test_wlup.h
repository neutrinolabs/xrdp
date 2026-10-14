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
 * wlup unit tests: the module core, with a mock backend
 */

#ifndef TEST_WLUP_H
#define TEST_WLUP_H

#include <check.h>

#include "wlup.h"

/* What the mock backend saw, and what it answers */
struct mock_backend
{
    int connect_rv;      /* connect() returns this */
    int check_rv;        /* check_wait_objs() returns this */
    int resize_rv;       /* resize() returns this */
    int pixels_busy;     /* pixels_busy() returns this */
    int connects;
    int disconnects;
    int checks;
    int frames_wanted;
    int keys;
    int last_key;
    int last_key_down;
    int mice;
    int last_mouse_msg;
    int last_mouse_x;
    int last_mouse_y;
    int resizes;
    int last_resize_width;
    int last_resize_height;
};

extern struct mock_backend mock;

/* What the mock xrdp side saw */
struct mock_server
{
    int begin_updates;
    int end_updates;
    int paints;
    int last_paint_x;
    int last_paint_y;
    int last_paint_cx;
    int last_paint_cy;
    int resize_dones;
};

extern struct mock_server server;

/* A module with the mock xrdp callbacks, started at width x height */
struct wlup *
mock_module_create(int width, int height);
void
mock_module_destroy(struct wlup *v);

Suite *
make_suite_wlup_core(void);
Suite *
make_suite_wlup_gfx(void);

#endif
