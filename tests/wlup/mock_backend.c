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
 * wlup unit tests: a backend and an xrdp side that only record calls
 *
 * The module core picks this backend up as wlup_backend_wlroots, the
 * default, as the real wlroots backend is not linked into the tests.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <string.h>

#include "os_calls.h"
#include "wlup_backend.h"

#include "test_wlup.h"

#define MOCK_WIDTH 64
#define MOCK_HEIGHT 48

struct mock_backend mock;
struct mock_server server;

/******************************************************************************/
/* Backend                                                                    */
/******************************************************************************/
static int
mock_connect(struct wlup *v, int fd)
{
    mock.connects++;
    if (mock.connect_rv != 0)
    {
        return mock.connect_rv;
    }
    /* The desktop image a real backend would fill */
    v->pixels_size = MOCK_WIDTH * MOCK_HEIGHT * 4;
    v->pixels = (char *)g_malloc(v->pixels_size, 1);
    v->buffer_width = MOCK_WIDTH;
    v->buffer_height = MOCK_HEIGHT;
    return 0;
}

static void
mock_disconnect(struct wlup *v)
{
    mock.disconnects++;
    g_free(v->pixels);
    v->pixels = NULL;
}

static void
mock_get_wait_objs(struct wlup *v, tbus *read_objs, int *rcount,
                   tbus *write_objs, int *wcount, int *timeout)
{
}

static int
mock_check_wait_objs(struct wlup *v)
{
    mock.checks++;
    return mock.check_rv;
}

static void
mock_key(struct wlup *v, int evdev_code, int down)
{
    mock.keys++;
    mock.last_key = evdev_code;
    mock.last_key_down = down;
}

static void
mock_mouse(struct wlup *v, int msg, int x, int y)
{
    mock.mice++;
    mock.last_mouse_msg = msg;
    mock.last_mouse_x = x;
    mock.last_mouse_y = y;
}

static int
mock_resize(struct wlup *v, int width, int height)
{
    mock.resizes++;
    mock.last_resize_width = width;
    mock.last_resize_height = height;
    return mock.resize_rv;
}

static void
mock_frames_wanted(struct wlup *v)
{
    mock.frames_wanted++;
}

static int
mock_pixels_busy(struct wlup *v)
{
    return mock.pixels_busy;
}

static void
mock_set_clipboard_text(struct wlup *v, const char *text, int len)
{
    len = MIN(len, (int)sizeof(mock.clip_text) - 1);
    memcpy(mock.clip_text, text, len);
    mock.clip_text[len] = '\0';
    mock.clip_text_len = len;
}

const struct wlup_backend_ops wlup_backend_wlroots =
{
    .name = "mock",
    .connect = mock_connect,
    .disconnect = mock_disconnect,
    .get_wait_objs = mock_get_wait_objs,
    .check_wait_objs = mock_check_wait_objs,
    .key = mock_key,
    .mouse = mock_mouse,
    .resize = mock_resize,
    .frames_wanted = mock_frames_wanted,
    .pixels_busy = mock_pixels_busy,
    .set_clipboard_text = mock_set_clipboard_text
};

#if defined(XRDP_WLUP_MUTTER)
/* The core only has this stub when built without the Mutter backend */
const struct wlup_backend_ops *
wlup_backend_mutter(void)
{
    return NULL;
}
#endif

/******************************************************************************/
/* xrdp side                                                                  */
/******************************************************************************/
static int
server_begin_update(struct wlup *v)
{
    server.begin_updates++;
    return 0;
}

static int
server_end_update(struct wlup *v)
{
    server.end_updates++;
    return 0;
}

static int
server_paint_rect(struct wlup *v, int x, int y, int cx, int cy,
                  char *data, int width, int height, int srcx, int srcy)
{
    server.paints++;
    server.last_paint_x = x;
    server.last_paint_y = y;
    server.last_paint_cx = cx;
    server.last_paint_cy = cy;
    return 0;
}

static int
server_set_fgcolor(struct wlup *v, int fgcolor)
{
    return 0;
}

static int
server_fill_rect(struct wlup *v, int x, int y, int cx, int cy)
{
    return 0;
}

static int
server_msg(struct wlup *v, const char *msg, int code)
{
    return 0;
}

static int
server_monitor_resize_done(struct wlup *v)
{
    server.resize_dones++;
    return 0;
}

static int
server_get_channel_id(struct wlup *v, const char *name)
{
    return server.channel_id;
}

static int
server_send_to_channel(struct wlup *v, int channel_id, char *data,
                       int data_len, int total_data_len, int flags)
{
    if (server.partial_len + data_len > (int)sizeof(server.partial))
    {
        return 1;
    }
    memcpy(server.partial + server.partial_len, data, data_len);
    server.partial_len += data_len;
    if (server.partial_len < total_data_len)
    {
        return 0;
    }
    memcpy(server.last_pdu, server.partial, server.partial_len);
    server.last_pdu_len = server.partial_len;
    if (server.pdus < 16)
    {
        /* msgType, little endian */
        server.pdu_types[server.pdus] = (unsigned char)server.partial[0] |
                                        (unsigned char)server.partial[1] << 8;
    }
    server.pdus++;
    server.partial_len = 0;
    return 0;
}

/******************************************************************************/
struct wlup *
mock_module_create(int width, int height)
{
    struct wlup *v;

    memset(&mock, 0, sizeof(mock));
    memset(&server, 0, sizeof(server));
    v = (struct wlup *)mod_init();
    v->server_begin_update = server_begin_update;
    v->server_end_update = server_end_update;
    v->server_paint_rect = server_paint_rect;
    v->server_set_fgcolor = server_set_fgcolor;
    v->server_fill_rect = server_fill_rect;
    v->server_msg = server_msg;
    v->server_monitor_resize_done = server_monitor_resize_done;
    v->server_get_channel_id = server_get_channel_id;
    v->server_send_to_channel = server_send_to_channel;
    v->mod_start(v, width, height, 32);
    /* mod_start() painted the empty desktop */
    memset(&server, 0, sizeof(server));
    server.channel_id = -1; /* no clipboard channel, unless a test sets one */
    return v;
}

/******************************************************************************/
void
mock_module_destroy(struct wlup *v)
{
    v->mod_end(v);
    mod_exit((tintptr)v);
}
