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
 * wlup: experimental backend module connecting xrdp to a Wayland compositor
 *
 * This is the module core: what xrdp sees. Where frames and input come
 * from depends on the compositor, and is a backend (wlup_backend.h):
 * backends/wlroots for wlroots compositors, backends/mutter for GNOME.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <fcntl.h>
#include <unistd.h>

#include "wlup.h"
#include "wlup_backend.h"
#include "wlup_clip.h"
#include "wlup_gfx.h"
#include "log.h"
#include "string_calls.h"
#include "scancode.h"
#include "trans.h"
#include "xrdp_constants.h"
#include "xrdp_scancode_defs.h"

/* Output (bytes) xrdp may hold queued for the client while it waits for
 * the network, from what wlup painted, before wlup stops capturing, as
 * neutrinordp does. Frames that go through the H.264 encoder are paced
 * by frame acks instead */
#define MAX_QUEUED_MODULE_OUTPUT 50000

#if !defined(XRDP_WLUP_MUTTER)
/******************************************************************************/
const struct wlup_backend_ops *
wlup_backend_mutter(void)
{
    return NULL;
}
#endif

/******************************************************************************/
/* Painting                                                                   */
/******************************************************************************/

/* Paints a rectangle of our copy of the desktop to the RDP client,
 * clipped to both the capture buffer and the RDP desktop. With the GFX
 * H.264 encoder the rectangle goes into the next frame instead */
static int
paint_region(struct wlup *v, int x, int y, int cx, int cy)
{
    int right;
    int bottom;

    if (v->pixels == NULL || v->suppress_output)
    {
        return 0;
    }
    right = MIN(x + cx, MIN(v->buffer_width, v->server_width));
    bottom = MIN(y + cy, MIN(v->buffer_height, v->server_height));
    x = MAX(x, 0);
    y = MAX(y, 0);
    if (right <= x || bottom <= y)
    {
        return 0;
    }
    if (v->gfx_h264)
    {
        /* A backend whose compositor writes to the desktop image (wlroots:
         * between a capture request and ready) has it copied for the
         * frames while it is ours. During a capture the copy is already
         * up to date: every capture's damage is copied when it arrives,
         * and none arrives while output is suppressed, as the capture
         * stops then */
        if (v->ops != NULL && v->ops->pixels_busy != NULL &&
                !v->ops->pixels_busy(v))
        {
            wlup_gfx_copy_damage(v, x, y, right - x, bottom - y);
        }
        wlup_gfx_add_damage(v, x, y, right - x, bottom - y);
        return 0;
    }
    return v->server_paint_rect(v, x, y, right - x, bottom - y,
                                v->pixels, v->buffer_width, v->buffer_height,
                                x, y);
}

/******************************************************************************/
int
wlup_paint_region(struct wlup *v, int x, int y, int cx, int cy)
{
    return paint_region(v, x, y, cx, cy);
}

/******************************************************************************/
int
wlup_paint_damage(struct wlup *v)
{
    int error = 0;
    int i;

    if (v->suppress_output)
    {
        /* Everything will be repainted when output is resumed */
        return 0;
    }

    v->server_begin_update(v);
    if (v->need_full_paint)
    {
        error = paint_region(v, 0, 0, v->buffer_width, v->buffer_height);
        v->need_full_paint = 0;
    }
    else if (v->damage_overflow)
    {
        /* damage[0..3] holds the bounding box in this case */
        error = paint_region(v, v->damage[0], v->damage[1],
                             v->damage[2], v->damage[3]);
    }
    else
    {
        for (i = 0; i < v->num_damage && error == 0; ++i)
        {
            error = paint_region(v, v->damage[i * 4], v->damage[i * 4 + 1],
                                 v->damage[i * 4 + 2], v->damage[i * 4 + 3]);
        }
    }
    v->server_end_update(v);
    /* With the H.264 encoder, the damage goes now if the last frame has
     * been acknowledged */
    wlup_gfx_poll(v);
    return error;
}

/******************************************************************************/
/* Frames are not wanted:
 * - while output is suppressed. The compositor collects the damage
 *   meanwhile, and the first frame after the resume carries it
 * - while xrdp still holds more than MAX_QUEUED_MODULE_OUTPUT of what wlup
 *   painted. lib_mod_check_wait_objs() runs again as it is sent */
int
wlup_wants_frames(struct wlup *v)
{
    return !v->suppress_output &&
           (v->si == NULL ||
            v->si->source[XRDP_SOURCE_MOD] <= MAX_QUEUED_MODULE_OUTPUT);
}

/******************************************************************************/
/* The backend's frames_wanted(), if any */
static void
frames_wanted(struct wlup *v)
{
    if (v->ops != NULL && v->ops->frames_wanted != NULL)
    {
        v->ops->frames_wanted(v);
    }
}

/******************************************************************************/
/* Tells xrdp the resize it is waiting for is complete */
void
wlup_resize_finished(struct wlup *v)
{
    if (v->resize_pending)
    {
        v->resize_pending = 0;
        v->need_full_paint = 1;
        v->server_monitor_resize_done(v);
    }
}

/******************************************************************************/
/* Keyboard                                                                   */
/******************************************************************************/
/******************************************************************************/
static int
process_key(struct wlup *v, int key_code, int keyboard_flags, int down)
{
    int scancode = SCANCODE_FROM_KBD_EVENT(key_code, keyboard_flags);
    int x11_keycode = scancode_to_x11_keycode(scancode);

    if (x11_keycode < 8)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "wlup: no keycode for scancode 0x%x",
                  scancode);
        return 0;
    }
    if (v->ops == NULL)
    {
        return 0;
    }
    /* X11 keycodes are evdev codes plus 8 */
    v->ops->key(v, x11_keycode - 8, down);
    return 0;
}

/******************************************************************************/
/* Module entry points                                                        */
/******************************************************************************/

/* return error */
static int
lib_mod_event(struct wlup *v, int msg, long param1, long param2,
              long param3, long param4)
{
    int error = 0;

    switch (msg)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
            /* param3: RDP key code, param4: RDP keyboard flags */
            error = process_key(v, (int)param3, (int)param4,
                                msg == WM_KEYDOWN);
            break;
        case WM_CHANNEL_DATA:
            /* param1: channel id and flags, param2: size, param3: data,
             * param4: total size */
            if (LOWORD(param1) == v->clip_chanid && v->clip_chanid >= 0)
            {
                error = wlup_clip_process_channel_data(v, (char *)param3,
                                                       (int)param2, (int)param4,
                                                       HIWORD(param1));
            }
            break;
        case WM_INVALIDATE:
            v->server_begin_update(v);
            error = paint_region(v, (param1 >> 16) & 0xffff, param1 & 0xffff,
                                 (param2 >> 16) & 0xffff, param2 & 0xffff);
            v->server_end_update(v);
            break;
        default:
            if (msg >= WM_MOUSEMOVE && msg <= WM_BUTTON9DOWN &&
                    v->ops != NULL)
            {
                v->ops->mouse(v, msg, (int)param1, (int)param2);
            }
            break;
    }

    if (v->ops != NULL && v->ops->flush != NULL)
    {
        v->ops->flush(v);
    }
    return error;
}

/******************************************************************************/
/* return error */
static int
lib_mod_start(struct wlup *v, int w, int h, int bpp)
{
    if (bpp != 24 && bpp != 32)
    {
        v->server_msg(v, "wlup error - only 24 and 32 bpp RDP connections "
                      "are supported", 0);
        return 1;
    }
    v->server_bpp = bpp;
    v->server_width = w;
    v->server_height = h;

    v->server_begin_update(v);
    v->server_set_fgcolor(v, 0);
    v->server_fill_rect(v, 0, 0, w, h);
    v->server_end_update(v);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_connect(struct wlup *v, int fd)
{
    v->ops = v->backend == WLUP_BACKEND_MUTTER ? wlup_backend_mutter()
             : &wlup_backend_wlroots;
    if (v->ops == NULL)
    {
        v->server_msg(v, "wlup error - built without the Mutter backend "
                      "(--enable-wlup-mutter)", 0);
        return 1;
    }
    if (v->ops->connect(v, fd) != 0)
    {
        /* Releases what was made; no other op runs on a failed backend */
        v->ops->disconnect(v);
        v->ops = NULL;
        return 1;
    }
    /* Text clipboard: the RDP channel */
    return wlup_clip_open_channel(v);
}

/******************************************************************************/
static void
disconnect(struct wlup *v)
{
    if (v->ops != NULL)
    {
        v->ops->disconnect(v);
    }
    if (v->pipewire_fd >= 0)
    {
        close(v->pipewire_fd);
        v->pipewire_fd = -1;
    }
}

/******************************************************************************/
/* return error */
static int
lib_mod_end(struct wlup *v)
{
    disconnect(v);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_set_param(struct wlup *v, const char *name, const char *value)
{
    if (g_strcasecmp(name, "wayland_display") == 0)
    {
        g_strncpy(v->display_name, value, sizeof(v->display_name) - 1);
    }
    else if (g_strcasecmp(name, "xkb_layout") == 0)
    {
        g_strncpy(v->xkb_layout, value, sizeof(v->xkb_layout) - 1);
    }
    else if (g_strcasecmp(name, "xkb_variant") == 0)
    {
        g_strncpy(v->xkb_variant, value, sizeof(v->xkb_variant) - 1);
    }
    else if (g_strcasecmp(name, "backend") == 0)
    {
        v->backend = g_strcasecmp(value, "mutter") == 0 ?
                     WLUP_BACKEND_MUTTER : WLUP_BACKEND_WLROOTS;
    }
    else if (g_strcasecmp(name, "dbus_address") == 0)
    {
        g_strncpy(v->dbus_address, value, sizeof(v->dbus_address) - 1);
    }
    else if (g_strcasecmp(name, "display_aux_fd") == 0)
    {
        /* A second connection to the session from sesman; xrdp keeps
         * ownership of value, so take a copy */
        if (v->pipewire_fd >= 0)
        {
            close(v->pipewire_fd);
        }
        v->pipewire_fd = fcntl(g_atoi(value), F_DUPFD_CLOEXEC, 3);
    }
    else if (g_strcasecmp(name, "client_info") == 0)
    {
        const struct xrdp_client_info *ci =
            (const struct xrdp_client_info *)value;
        v->large_pointer_flags = ci->large_pointer_support_flags;
        wlup_gfx_init(v, ci);
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_signal(struct wlup *v)
{
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_get_wait_objs(struct wlup *v, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    if (v != NULL && v->ops != NULL)
    {
        v->ops->get_wait_objs(v, read_objs, rcount, write_objs, wcount,
                              timeout);
    }
    if (v != NULL)
    {
        wlup_gfx_get_timeout(v, timeout);
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
check_wait_objs(struct wlup *v)
{
    if (v == NULL || v->ops == NULL)
    {
        return 0;
    }
    if (v->ops->check_wait_objs(v) != 0)
    {
        return 1;
    }
    /* With the H.264 encoder, sends the damage if the last frame has
     * been acknowledged, or its ack has timed out */
    if (wlup_gfx_poll(v) != 0)
    {
        return 1;
    }
    frames_wanted(v);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_check_wait_objs(struct wlup *v)
{
    int rv;

    /* What xrdp has to queue for the client from here on counts as
     * wlup's, for wlup_wants_frames() */
    if (v != NULL && v->si != NULL)
    {
        v->si->cur_source = XRDP_SOURCE_MOD;
    }
    rv = check_wait_objs(v);
    if (v != NULL && v->si != NULL)
    {
        v->si->cur_source = XRDP_SOURCE_NONE;
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_frame_ack(struct wlup *v, int flags, int frame_id)
{
    wlup_gfx_frame_ack(v, frame_id);
    frames_wanted(v);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_suppress_output(struct wlup *v, int suppress,
                        int left, int top, int right, int bottom)
{
    LOG(LOG_LEVEL_INFO, "wlup: output %s",
        suppress ? "suppressed" : "resumed");
    v->suppress_output = suppress;
    if (!suppress)
    {
        v->server_begin_update(v);
        paint_region(v, 0, 0, v->buffer_width, v->buffer_height);
        v->server_end_update(v);
        wlup_gfx_poll(v);
        frames_wanted(v);
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_version_message(struct wlup *v)
{
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_monitor_resize(struct wlup *v, int width, int height,
                              int num_monitors,
                              const struct monitor_info *monitors,
                              int *in_progress)
{
    /* Prototype: all monitors become one output of the total size */
    v->server_width = width;
    v->server_height = height;
    v->need_full_paint = 1;
    wlup_gfx_reset(v);
    *in_progress = 0;
    if ((width != v->buffer_width || height != v->buffer_height) &&
            v->ops != NULL && v->ops->resize(v, width, height))
    {
        /* xrdp waits for server_monitor_resize_done() */
        v->resize_width = width;
        v->resize_height = height;
        v->resize_pending = 1;
        *in_progress = 1;
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_monitor_full_invalidate(struct wlup *v, int width, int height)
{
    /* xrdp has a new encoder after a resize: start over with a full frame */
    wlup_gfx_reset(v);
    v->server_begin_update(v);
    paint_region(v, 0, 0, v->buffer_width, v->buffer_height);
    v->server_end_update(v);
    wlup_gfx_poll(v);
    return 0;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct wlup *v;

    v = (struct wlup *)g_malloc(sizeof(struct wlup), 1);
    /* set client functions */
    v->size = sizeof(struct wlup);
    v->version = CURRENT_MOD_VER;
    v->handle = (tintptr) v;
    v->mod_connect = lib_mod_connect;
    v->mod_start = lib_mod_start;
    v->mod_event = lib_mod_event;
    v->mod_signal = lib_mod_signal;
    v->mod_end = lib_mod_end;
    v->mod_set_param = lib_mod_set_param;
    v->mod_get_wait_objs = lib_mod_get_wait_objs;
    v->mod_check_wait_objs = lib_mod_check_wait_objs;
    v->mod_frame_ack = lib_mod_frame_ack;
    v->mod_suppress_output = lib_mod_suppress_output;
    v->mod_server_monitor_resize = lib_mod_server_monitor_resize;
    v->mod_server_monitor_full_invalidate =
        lib_mod_server_monitor_full_invalidate;
    v->mod_server_version_message = lib_mod_server_version_message;
    v->pipewire_fd = -1;
    wlup_clip_init(v);
    return (tintptr) v;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct wlup *v = (struct wlup *) handle;

    if (v == NULL)
    {
        return 0;
    }
    disconnect(v);
    wlup_clip_exit(v);
    g_free(v);
    return 0;
}
