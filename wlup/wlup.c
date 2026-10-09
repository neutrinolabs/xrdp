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
 * Frame path
 *   The output is captured with ext-image-copy-capture-v1 into a single
 *   wl_shm buffer which we keep for the whole session. The compositor
 *   reports which regions changed since the previous frame, and only
 *   those are painted to the RDP client. A capture request is only
 *   answered once the screen has changed, so an idle desktop costs
 *   nothing.
 *
 * Input path
 *   RDP scancodes are turned into evdev keycodes with the existing
 *   scancode tables, and sent through zwp_virtual_keyboard_v1 with an
 *   XKB keymap compiled by libxkbcommon. The pointer uses absolute
 *   motion through zwlr_virtual_pointer_v1.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include "wlup.h"
#include "log.h"
#include "string_calls.h"
#include "scancode.h"
#include "xrdp_constants.h"
#include "xrdp_scancode_defs.h"

/* Scroll distance for one wheel notch, matching libinput's default */
#define WLUP_WHEEL_STEP 15.0

static int start_capture(struct wlup *v);

/******************************************************************************/
static uint32_t
now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/******************************************************************************/
/* Shared memory                                                              */
/******************************************************************************/
static void
free_buffer(struct wlup *v)
{
    if (v->buffer != NULL)
    {
        wl_buffer_destroy(v->buffer);
        v->buffer = NULL;
    }
    if (v->pixels != NULL)
    {
        munmap(v->pixels, v->pixels_size);
        v->pixels = NULL;
        v->pixels_size = 0;
    }
    v->buffer_width = 0;
    v->buffer_height = 0;
}

/******************************************************************************/
/* return error */
static int
alloc_buffer(struct wlup *v, int width, int height, uint32_t format)
{
    struct wl_shm_pool *pool;
    size_t stride = (size_t)width * 4;
    size_t size = stride * height;
    int fd;

    free_buffer(v);

    fd = memfd_create("xrdp-wlup", MFD_CLOEXEC);
    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: memfd_create failed: %s", strerror(errno));
        return 1;
    }
    if (ftruncate(fd, size) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: ftruncate failed: %s", strerror(errno));
        close(fd);
        return 1;
    }
    v->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (v->pixels == MAP_FAILED)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: mmap failed: %s", strerror(errno));
        v->pixels = NULL;
        close(fd);
        return 1;
    }
    v->pixels_size = size;

    pool = wl_shm_create_pool(v->shm, fd, (int32_t)size);
    v->buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                          (int32_t)stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);

    v->buffer_width = width;
    v->buffer_height = height;
    v->buffer_format = (int)format;
    v->need_full_paint = 1;
    LOG(LOG_LEVEL_INFO, "wlup: capture buffer %dx%d format 0x%x",
        width, height, format);
    return 0;
}

/******************************************************************************/
/* Painting                                                                   */
/******************************************************************************/

/* Paints a rectangle of our copy of the desktop to the RDP client,
 * clipped to both the capture buffer and the RDP desktop */
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
    return v->server_paint_rect(v, x, y, right - x, bottom - y,
                                v->pixels, v->buffer_width, v->buffer_height,
                                x, y);
}

/******************************************************************************/
static int
paint_damage(struct wlup *v)
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
    return error;
}

/******************************************************************************/
/* Capture frame events                                                       */
/******************************************************************************/
static void
frame_transform(void *data, struct ext_image_copy_capture_frame_v1 *frame,
                uint32_t transform)
{
    if (transform != WL_OUTPUT_TRANSFORM_NORMAL)
    {
        LOG(LOG_LEVEL_WARNING, "wlup: output transform %u is not supported",
            transform);
    }
}

/******************************************************************************/
static void
frame_damage(void *data, struct ext_image_copy_capture_frame_v1 *frame,
             int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct wlup *v = (struct wlup *)data;
    int i;

    if (!v->damage_overflow && v->num_damage < WLUP_MAX_DAMAGE)
    {
        i = v->num_damage++ * 4;
        v->damage[i] = x;
        v->damage[i + 1] = y;
        v->damage[i + 2] = width;
        v->damage[i + 3] = height;
        return;
    }

    if (!v->damage_overflow)
    {
        /* Collapse everything received so far into a bounding box */
        int left = v->damage[0];
        int top = v->damage[1];
        int right = left + v->damage[2];
        int bottom = top + v->damage[3];
        for (i = 1; i < v->num_damage; ++i)
        {
            left = MIN(left, v->damage[i * 4]);
            top = MIN(top, v->damage[i * 4 + 1]);
            right = MAX(right, v->damage[i * 4] + v->damage[i * 4 + 2]);
            bottom = MAX(bottom, v->damage[i * 4 + 1] + v->damage[i * 4 + 3]);
        }
        v->damage[0] = left;
        v->damage[1] = top;
        v->damage[2] = right - left;
        v->damage[3] = bottom - top;
        v->damage_overflow = 1;
    }
    {
        int right = MAX(v->damage[0] + v->damage[2], x + width);
        int bottom = MAX(v->damage[1] + v->damage[3], y + height);
        v->damage[0] = MIN(v->damage[0], x);
        v->damage[1] = MIN(v->damage[1], y);
        v->damage[2] = right - v->damage[0];
        v->damage[3] = bottom - v->damage[1];
    }
}

/******************************************************************************/
static void
frame_presentation_time(void *data,
                        struct ext_image_copy_capture_frame_v1 *frame,
                        uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                        uint32_t tv_nsec)
{
}

/******************************************************************************/
static void
frame_ready(void *data, struct ext_image_copy_capture_frame_v1 *frame)
{
    struct wlup *v = (struct wlup *)data;

    ext_image_copy_capture_frame_v1_destroy(v->frame);
    v->frame = NULL;
    paint_damage(v);
    /* Ask for the next frame straight away. The compositor holds it until
     * the screen changes */
    start_capture(v);
}

/******************************************************************************/
static void
frame_failed(void *data, struct ext_image_copy_capture_frame_v1 *frame,
             uint32_t reason)
{
    struct wlup *v = (struct wlup *)data;

    ext_image_copy_capture_frame_v1_destroy(v->frame);
    v->frame = NULL;

    switch (reason)
    {
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
            /* New constraints follow, ending with a 'done' event, which
             * reallocates the buffer and restarts the capture */
            LOG(LOG_LEVEL_INFO, "wlup: buffer constraints changed");
            break;
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
            LOG(LOG_LEVEL_ERROR, "wlup: capture session stopped");
            break;
        default:
            LOG(LOG_LEVEL_WARNING, "wlup: frame capture failed, retrying");
            start_capture(v);
            break;
    }
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener =
{
    .transform = frame_transform,
    .damage = frame_damage,
    .presentation_time = frame_presentation_time,
    .ready = frame_ready,
    .failed = frame_failed,
};

/******************************************************************************/
/* return error */
static int
start_capture(struct wlup *v)
{
    if (v->frame != NULL || v->buffer == NULL || v->session == NULL)
    {
        return 0;
    }
    v->num_damage = 0;
    v->damage_overflow = 0;
    v->frame = ext_image_copy_capture_session_v1_create_frame(v->session);
    ext_image_copy_capture_frame_v1_add_listener(v->frame, &frame_listener, v);
    ext_image_copy_capture_frame_v1_attach_buffer(v->frame, v->buffer);
    if (v->need_full_paint)
    {
        /* Fresh buffer: ask the compositor to fill all of it */
        ext_image_copy_capture_frame_v1_damage_buffer(v->frame, 0, 0,
                v->buffer_width, v->buffer_height);
    }
    ext_image_copy_capture_frame_v1_capture(v->frame);
    return 0;
}

/******************************************************************************/
/* Capture session events (buffer constraints)                                */
/******************************************************************************/
static void
session_buffer_size(void *data, struct ext_image_copy_capture_session_v1 *s,
                    uint32_t width, uint32_t height)
{
    struct wlup *v = (struct wlup *)data;
    v->constraint_width = width;
    v->constraint_height = height;
}

/******************************************************************************/
static void
session_shm_format(void *data, struct ext_image_copy_capture_session_v1 *s,
                   uint32_t format)
{
    struct wlup *v = (struct wlup *)data;

    /* Both have the 0x00RRGGBB little-endian layout xrdp uses at 24
     * and 32 bpp. Prefer XRGB as the alpha byte is meaningless here */
    if (format == WL_SHM_FORMAT_XRGB8888)
    {
        v->constraint_format = format;
        v->constraint_xrgb8888 = 1;
    }
    else if (format == WL_SHM_FORMAT_ARGB8888 && !v->constraint_xrgb8888)
    {
        v->constraint_format = format;
        v->constraint_xrgb8888 = 1;
    }
}

/******************************************************************************/
static void
session_dmabuf_device(void *data, struct ext_image_copy_capture_session_v1 *s,
                      struct wl_array *device)
{
}

/******************************************************************************/
static void
session_dmabuf_format(void *data, struct ext_image_copy_capture_session_v1 *s,
                      uint32_t format, struct wl_array *modifiers)
{
}

/******************************************************************************/
static void
session_done(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    struct wlup *v = (struct wlup *)data;

    if (!v->constraint_xrgb8888)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: compositor offers no XRGB8888/ARGB8888 "
            "shm format for capture");
        return;
    }
    if (v->buffer == NULL ||
            v->buffer_width != (int)v->constraint_width ||
            v->buffer_height != (int)v->constraint_height ||
            v->buffer_format != (int)v->constraint_format)
    {
        if (v->constraint_width != (uint32_t)v->server_width ||
                v->constraint_height != (uint32_t)v->server_height)
        {
            LOG(LOG_LEVEL_WARNING, "wlup: compositor output is %ux%u but the "
                "RDP desktop is %dx%d, the picture will be clipped or padded",
                v->constraint_width, v->constraint_height,
                v->server_width, v->server_height);
        }
        if (alloc_buffer(v, v->constraint_width, v->constraint_height,
                         v->constraint_format) != 0)
        {
            return;
        }
    }
    v->constraints_done = 1;
    /* The constraints are re-sent from scratch next time */
    v->constraint_xrgb8888 = 0;
    start_capture(v);
}

/******************************************************************************/
static void
session_stopped(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    struct wlup *v = (struct wlup *)data;

    LOG(LOG_LEVEL_ERROR, "wlup: capture session stopped by the compositor");
    ext_image_copy_capture_session_v1_destroy(v->session);
    v->session = NULL;
}

static const struct ext_image_copy_capture_session_v1_listener
    session_listener =
{
    .buffer_size = session_buffer_size,
    .shm_format = session_shm_format,
    .dmabuf_device = session_dmabuf_device,
    .dmabuf_format = session_dmabuf_format,
    .done = session_done,
    .stopped = session_stopped,
};

/******************************************************************************/
/* Registry                                                                   */
/******************************************************************************/
static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
    struct wlup *v = (struct wlup *)data;

    if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        v->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0 && v->seat == NULL)
    {
        v->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
    else if (strcmp(interface, wl_output_interface.name) == 0 &&
             v->output == NULL)
    {
        /* Prototype: the first output is the RDP desktop */
        v->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
    }
    else if (strcmp(interface,
                    ext_output_image_capture_source_manager_v1_interface.name)
             == 0)
    {
        v->source_manager = wl_registry_bind(registry, name,
                                             &ext_output_image_capture_source_manager_v1_interface, 1);
    }
    else if (strcmp(interface,
                    ext_image_copy_capture_manager_v1_interface.name) == 0)
    {
        v->copy_manager = wl_registry_bind(registry, name,
                                           &ext_image_copy_capture_manager_v1_interface, 1);
    }
    else if (strcmp(interface,
                    zwlr_virtual_pointer_manager_v1_interface.name) == 0)
    {
        v->pointer_manager_version = MIN(version, 2);
        v->pointer_manager = wl_registry_bind(registry, name,
                                              &zwlr_virtual_pointer_manager_v1_interface,
                                              v->pointer_manager_version);
    }
    else if (strcmp(interface,
                    zwp_virtual_keyboard_manager_v1_interface.name) == 0)
    {
        v->keyboard_manager = wl_registry_bind(registry, name,
                                               &zwp_virtual_keyboard_manager_v1_interface, 1);
    }
}

/******************************************************************************/
static void
registry_global_remove(void *data, struct wl_registry *registry,
                       uint32_t name)
{
}

static const struct wl_registry_listener registry_listener =
{
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/******************************************************************************/
/* Keyboard                                                                   */
/******************************************************************************/

/* Compiles an XKB keymap and hands it to the virtual keyboard.
 * return error */
static int
send_keymap(struct wlup *v)
{
    struct xkb_context *ctx;
    struct xkb_rule_names names = {0};
    char *text;
    size_t size;
    int fd;
    int rv = 1;

    ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: xkb_context_new failed");
        return 1;
    }
    names.rules = "evdev";
    names.layout = v->xkb_layout[0] != '\0' ? v->xkb_layout : "us";
    names.variant = v->xkb_variant[0] != '\0' ? v->xkb_variant : NULL;
    v->xkb_keymap = xkb_keymap_new_from_names(ctx, &names,
                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    xkb_context_unref(ctx);
    if (v->xkb_keymap == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: cannot compile XKB keymap layout=%s "
            "variant=%s", names.layout,
            names.variant != NULL ? names.variant : "");
        return 1;
    }
    v->xkb_state = xkb_state_new(v->xkb_keymap);

    text = xkb_keymap_get_as_string(v->xkb_keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    size = strlen(text) + 1;
    fd = memfd_create("xrdp-wlup-keymap", MFD_CLOEXEC);
    if (fd >= 0 && write(fd, text, size) == (ssize_t)size)
    {
        zwp_virtual_keyboard_v1_keymap(v->keyboard,
                                       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                       fd, (uint32_t)size);
        rv = 0;
        LOG(LOG_LEVEL_INFO, "wlup: keyboard layout %s", names.layout);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "wlup: cannot write keymap: %s", strerror(errno));
    }
    if (fd >= 0)
    {
        close(fd);
    }
    free(text);
    return rv;
}

/******************************************************************************/
static int
process_key(struct wlup *v, int key_code, int keyboard_flags, int down)
{
    int scancode = SCANCODE_FROM_KBD_EVENT(key_code, keyboard_flags);
    int x11_keycode = scancode_to_x11_keycode(scancode);

    if (v->keyboard == NULL || v->xkb_state == NULL)
    {
        return 0;
    }
    if (x11_keycode < 8)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "wlup: no keycode for scancode 0x%x",
                  scancode);
        return 0;
    }
    /* X11 keycodes are evdev codes plus 8 */
    zwp_virtual_keyboard_v1_key(v->keyboard, now_ms(), x11_keycode - 8,
                                down ? WL_KEYBOARD_KEY_STATE_PRESSED
                                : WL_KEYBOARD_KEY_STATE_RELEASED);

    /* The compositor does not derive modifiers from the key events of a
     * virtual keyboard, the client has to send them, as a real
     * wl_keyboard does after a modifier key changes */
    if (xkb_state_update_key(v->xkb_state, x11_keycode,
                             down ? XKB_KEY_DOWN : XKB_KEY_UP) != 0)
    {
        zwp_virtual_keyboard_v1_modifiers(v->keyboard,
            xkb_state_serialize_mods(v->xkb_state, XKB_STATE_MODS_DEPRESSED),
            xkb_state_serialize_mods(v->xkb_state, XKB_STATE_MODS_LATCHED),
            xkb_state_serialize_mods(v->xkb_state, XKB_STATE_MODS_LOCKED),
            xkb_state_serialize_layout(v->xkb_state,
                                       XKB_STATE_LAYOUT_EFFECTIVE));
    }
    return 0;
}

/******************************************************************************/
/* Pointer                                                                    */
/******************************************************************************/
static void
pointer_button(struct wlup *v, uint32_t button, int down)
{
    zwlr_virtual_pointer_v1_button(v->pointer, now_ms(), button,
                                   down ? WL_POINTER_BUTTON_STATE_PRESSED
                                   : WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(v->pointer);
}

/******************************************************************************/
static void
pointer_wheel(struct wlup *v, uint32_t axis, int steps)
{
    uint32_t t = now_ms();
    zwlr_virtual_pointer_v1_axis_source(v->pointer,
                                        WL_POINTER_AXIS_SOURCE_WHEEL);
    zwlr_virtual_pointer_v1_axis_discrete(v->pointer, t, axis,
                                          wl_fixed_from_double(steps * WLUP_WHEEL_STEP),
                                          steps);
    zwlr_virtual_pointer_v1_frame(v->pointer);
}

/******************************************************************************/
static int
process_mouse(struct wlup *v, int msg, int x, int y)
{
    int width = v->buffer_width > 0 ? v->buffer_width : v->server_width;
    int height = v->buffer_height > 0 ? v->buffer_height : v->server_height;

    if (v->pointer == NULL)
    {
        return 0;
    }
    /* Scroll goes to the surface under the pointer, so wheel events must
     * not move it. xrdp passes its last stored position with them, which
     * is only updated by move events, not by clicks, and can be stale */
    if (msg < WM_BUTTON4UP || msg > WM_BUTTON7DOWN)
    {
        x = MAX(0, MIN(x, width - 1));
        y = MAX(0, MIN(y, height - 1));
        zwlr_virtual_pointer_v1_motion_absolute(v->pointer, now_ms(), x, y,
                                                width, height);
        zwlr_virtual_pointer_v1_frame(v->pointer);
    }

    switch (msg)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            pointer_button(v, BTN_LEFT, msg == WM_LBUTTONDOWN);
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            pointer_button(v, BTN_RIGHT, msg == WM_RBUTTONDOWN);
            break;
        case WM_BUTTON3DOWN:
        case WM_BUTTON3UP:
            pointer_button(v, BTN_MIDDLE, msg == WM_BUTTON3DOWN);
            break;
        case WM_BUTTON8DOWN:
        case WM_BUTTON8UP:
            pointer_button(v, BTN_SIDE, msg == WM_BUTTON8DOWN);
            break;
        case WM_BUTTON9DOWN:
        case WM_BUTTON9UP:
            pointer_button(v, BTN_EXTRA, msg == WM_BUTTON9DOWN);
            break;
        /* Wheel notches arrive as X11-style button 4-7 down/up pairs.
         * Act on the 'down' half only */
        case WM_BUTTON4DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
            break;
        case WM_BUTTON5DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_VERTICAL_SCROLL, 1);
            break;
        case WM_BUTTON6DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_HORIZONTAL_SCROLL, -1);
            break;
        case WM_BUTTON7DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_HORIZONTAL_SCROLL, 1);
            break;
        default:
            break;
    }
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
        case WM_INVALIDATE:
            v->server_begin_update(v);
            error = paint_region(v, (param1 >> 16) & 0xffff, param1 & 0xffff,
                                 (param2 >> 16) & 0xffff, param2 & 0xffff);
            v->server_end_update(v);
            break;
        default:
            if (msg >= WM_MOUSEMOVE && msg <= WM_BUTTON9DOWN)
            {
                error = process_mouse(v, msg, (int)param1, (int)param2);
            }
            break;
    }

    if (v->display != NULL)
    {
        wl_display_flush(v->display);
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
    char text[512];
    const char *name = v->display_name[0] != '\0' ? v->display_name : NULL;

    v->display = wl_display_connect(name);
    if (v->display == NULL)
    {
        g_snprintf(text, sizeof(text), "wlup error - cannot connect to the "
                   "Wayland compositor at '%s'",
                   name != NULL ? name : "$WAYLAND_DISPLAY");
        v->server_msg(v, text, 0);
        return 1;
    }

    v->registry = wl_display_get_registry(v->display);
    wl_registry_add_listener(v->registry, &registry_listener, v);
    wl_display_roundtrip(v->display);

    if (v->shm == NULL || v->output == NULL || v->source_manager == NULL ||
            v->copy_manager == NULL)
    {
        v->server_msg(v, "wlup error - the compositor does not offer "
                      "ext-image-copy-capture-v1 for an output", 0);
        return 1;
    }

    /* Input is optional, the desktop is shown read-only without it */
    if (v->pointer_manager != NULL)
    {
        if (v->pointer_manager_version >= 2)
        {
            v->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer_with_output(
                             v->pointer_manager, v->seat, v->output);
        }
        else
        {
            v->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
                             v->pointer_manager, v->seat);
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "wlup: no zwlr_virtual_pointer_manager_v1, "
            "the mouse will not work");
    }
    if (v->keyboard_manager != NULL && v->seat != NULL)
    {
        v->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
                          v->keyboard_manager, v->seat);
        scancode_set_keycode_set("evdev");
        send_keymap(v);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "wlup: no zwp_virtual_keyboard_manager_v1, "
            "the keyboard will not work");
    }

    /* Cursors are painted into the frames for now. A separate cursor
     * session would let the RDP client draw the cursor locally */
    v->source = ext_output_image_capture_source_manager_v1_create_source(
                    v->source_manager, v->output);
    v->session = ext_image_copy_capture_manager_v1_create_session(
                     v->copy_manager, v->source,
                     EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);
    ext_image_copy_capture_session_v1_add_listener(v->session,
            &session_listener, v);

    /* Receive the buffer constraints. session_done() starts the capture */
    wl_display_roundtrip(v->display);
    if (!v->constraints_done)
    {
        v->server_msg(v, "wlup error - no usable capture buffer format", 0);
        return 1;
    }

    g_snprintf(text, sizeof(text), "wlup: connected to Wayland display %s",
               name != NULL ? name : "$WAYLAND_DISPLAY");
    v->server_msg(v, text, 0);
    LOG(LOG_LEVEL_INFO, "%s", text);
    return 0;
}

/******************************************************************************/
static void
disconnect(struct wlup *v)
{
    if (v->frame != NULL)
    {
        ext_image_copy_capture_frame_v1_destroy(v->frame);
        v->frame = NULL;
    }
    if (v->session != NULL)
    {
        ext_image_copy_capture_session_v1_destroy(v->session);
        v->session = NULL;
    }
    if (v->source != NULL)
    {
        ext_image_capture_source_v1_destroy(v->source);
        v->source = NULL;
    }
    free_buffer(v);
    if (v->pointer != NULL)
    {
        zwlr_virtual_pointer_v1_destroy(v->pointer);
        v->pointer = NULL;
    }
    if (v->keyboard != NULL)
    {
        zwp_virtual_keyboard_v1_destroy(v->keyboard);
        v->keyboard = NULL;
    }
    if (v->xkb_state != NULL)
    {
        xkb_state_unref(v->xkb_state);
        v->xkb_state = NULL;
    }
    if (v->xkb_keymap != NULL)
    {
        xkb_keymap_unref(v->xkb_keymap);
        v->xkb_keymap = NULL;
    }
    if (v->display != NULL)
    {
        /* Destroying the connection releases all remaining proxies on the
         * compositor side */
        wl_display_disconnect(v->display);
        v->display = NULL;
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
    if (v != NULL && v->display != NULL)
    {
        wl_display_flush(v->display);
        read_objs[(*rcount)++] = wl_display_get_fd(v->display);
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_check_wait_objs(struct wlup *v)
{
    struct pollfd pfd;

    if (v == NULL || v->display == NULL)
    {
        return 0;
    }

    /* Standard libwayland pattern for an external event loop */
    while (wl_display_prepare_read(v->display) != 0)
    {
        if (wl_display_dispatch_pending(v->display) < 0)
        {
            break;
        }
    }
    wl_display_flush(v->display);

    pfd.fd = wl_display_get_fd(v->display);
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
    {
        wl_display_read_events(v->display);
    }
    else
    {
        wl_display_cancel_read(v->display);
    }
    wl_display_dispatch_pending(v->display);
    wl_display_flush(v->display);

    if (wl_display_get_error(v->display) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: Wayland connection error %d",
            wl_display_get_error(v->display));
        return 1;
    }
    if (v->session == NULL)
    {
        /* The compositor stopped the capture, e.g. the output went away */
        return 1;
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_frame_ack(struct wlup *v, int flags, int frame_id)
{
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_suppress_output(struct wlup *v, int suppress,
                        int left, int top, int right, int bottom)
{
    v->suppress_output = suppress;
    if (!suppress)
    {
        v->server_begin_update(v);
        paint_region(v, 0, 0, v->buffer_width, v->buffer_height);
        v->server_end_update(v);
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
    /* Prototype: the compositor output keeps its size. Resizing it needs
     * a compositor-specific call (e.g. 'swaymsg output ... mode') */
    v->server_width = width;
    v->server_height = height;
    *in_progress = 0;
    v->need_full_paint = 1;
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_monitor_full_invalidate(struct wlup *v, int width, int height)
{
    v->server_begin_update(v);
    paint_region(v, 0, 0, v->buffer_width, v->buffer_height);
    v->server_end_update(v);
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
    g_free(v);
    return 0;
}
