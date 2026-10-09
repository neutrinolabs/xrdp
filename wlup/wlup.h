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
 * The module is a plain Wayland client of the compositor:
 *   frames : ext-image-copy-capture-v1 into a wl_shm buffer
 *   input  : zwlr_virtual_pointer_v1 and zwp_virtual_keyboard_v1
 * This is the set of protocols offered by wlroots-based compositors
 * (sway, labwc, ...).
 */

#ifndef WLUP_H
#define WLUP_H

#include <stdint.h>

#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"
#include "xrdp_client_info.h"

#define CURRENT_MOD_VER 4

/* Largest number of damage rectangles kept per frame. If a frame has
 * more, the bounding box of all of them is painted instead */
#define WLUP_MAX_DAMAGE 64

struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_seat;
struct wl_output;
struct wl_buffer;
struct ext_output_image_capture_source_manager_v1;
struct ext_image_copy_capture_manager_v1;
struct ext_image_capture_source_v1;
struct ext_image_copy_capture_session_v1;
struct ext_image_copy_capture_frame_v1;
struct zwlr_virtual_pointer_manager_v1;
struct zwlr_virtual_pointer_v1;
struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;
struct xkb_keymap;
struct xkb_state;

struct wlup
{
    int size; /* size of this struct */
    int version; /* internal version */
    /* client functions */
    int (*mod_start)(struct wlup *v, int w, int h, int bpp);
    int (*mod_connect)(struct wlup *v, int fd);
    int (*mod_event)(struct wlup *v, int msg, long param1, long param2,
                     long param3, long param4);
    int (*mod_signal)(struct wlup *v);
    int (*mod_end)(struct wlup *v);
    int (*mod_set_param)(struct wlup *v, const char *name, const char *value);
    int (*mod_session_change)(struct wlup *v, int, int);
    int (*mod_get_wait_objs)(struct wlup *v, tbus *read_objs, int *rcount,
                             tbus *write_objs, int *wcount, int *timeout);
    int (*mod_check_wait_objs)(struct wlup *v);
    int (*mod_frame_ack)(struct wlup *v, int flags, int frame_id);
    int (*mod_suppress_output)(struct wlup *v, int suppress,
                               int left, int top, int right, int bottom);
    int (*mod_server_monitor_resize)(struct wlup *v,
                                     int width, int height,
                                     int num_monitors,
                                     const struct monitor_info *monitors,
                                     int *in_progress);
    int (*mod_server_monitor_full_invalidate)(struct wlup *v,
            int width, int height);
    int (*mod_server_version_message)(struct wlup *v);
    tintptr mod_dumby[100 - 14]; /* align, 100 minus the number of mod
                                  functions above */
    /* server functions */
    int (*server_begin_update)(struct wlup *v);
    int (*server_end_update)(struct wlup *v);
    int (*server_fill_rect)(struct wlup *v, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct wlup *v, int x, int y, int cx, int cy,
                             int srcx, int srcy);
    int (*server_paint_rect)(struct wlup *v, int x, int y, int cx, int cy,
                             char *data, int width, int height, int srcx, int srcy);
    int (*server_set_cursor)(struct wlup *v, int x, int y, char *data, char *mask);
    int (*server_palette)(struct wlup *v, int *palette);
    int (*server_msg)(struct wlup *v, const char *msg, int code);
    int (*server_is_term)(void);
    int (*server_set_clip)(struct wlup *v, int x, int y, int cx, int cy);
    int (*server_reset_clip)(struct wlup *v);
    int (*server_set_fgcolor)(struct wlup *v, int fgcolor);
    int (*server_set_bgcolor)(struct wlup *v, int bgcolor);
    int (*server_set_opcode)(struct wlup *v, int opcode);
    int (*server_set_mixmode)(struct wlup *v, int mixmode);
    int (*server_set_brush)(struct wlup *v, int x_origin, int y_origin,
                            int style, char *pattern);
    int (*server_set_pen)(struct wlup *v, int style,
                          int width);
    int (*server_draw_line)(struct wlup *v, int x1, int y1, int x2, int y2);
    int (*server_add_char)(struct wlup *v, int font, int character,
                           int offset, int baseline,
                           int width, int height, char *data);
    int (*server_draw_text)(struct wlup *v, int font,
                            int flags, int mixmode, int clip_left, int clip_top,
                            int clip_right, int clip_bottom,
                            int box_left, int box_top,
                            int box_right, int box_bottom,
                            int x, int y, char *data, int data_len);
    int (*client_monitor_resize)(struct wlup *v, int width, int height,
                                 int num_monitors,
                                 const struct monitor_info *monitors);
    int (*server_monitor_resize_done)(struct wlup *v);
    int (*server_get_channel_count)(struct wlup *v);
    int (*server_query_channel)(struct wlup *v, int index,
                                char *channel_name,
                                int *channel_flags);
    int (*server_get_channel_id)(struct wlup *v, const char *name);
    int (*server_send_to_channel)(struct wlup *v, int channel_id,
                                  char *data, int data_len,
                                  int total_data_len, int flags);
    int (*server_bell_trigger)(struct wlup *v);
    int (*server_chansrv_in_use)(struct wlup *v);
    void (*server_init_xkb_layout)(struct wlup *v,
                                   struct xrdp_client_info *client_info);
    tintptr server_dumby[100 - 29]; /* align, 100 minus the number of server
                                     functions above */
    /* common */
    tintptr handle; /* pointer to self as long */
    tintptr wm;
    tintptr painter;
    struct source_info *si;

    /* mod data */
    int server_bpp;
    int server_width;  /* size of the RDP desktop */
    int server_height;
    int suppress_output;

    /* settings from xrdp.ini */
    char display_name[256]; /* WAYLAND_DISPLAY name or absolute path */
    char xkb_layout[64];
    char xkb_variant[64];

    /* Wayland globals */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_output *output;
    struct ext_output_image_capture_source_manager_v1 *source_manager;
    struct ext_image_copy_capture_manager_v1 *copy_manager;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    uint32_t pointer_manager_version;
    struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;

    /* capture */
    struct ext_image_capture_source_v1 *source;
    struct ext_image_copy_capture_session_v1 *session;
    struct ext_image_copy_capture_frame_v1 *frame;
    uint32_t constraint_width; /* buffer constraints being received */
    uint32_t constraint_height;
    int constraint_xrgb8888; /* compositor offers XRGB8888 or ARGB8888 */
    uint32_t constraint_format;
    int constraints_done;

    struct wl_buffer *buffer;
    char *pixels;      /* mapping of the shm buffer, also our copy of the */
    size_t pixels_size; /* desktop used to answer invalidate requests */
    int buffer_width;
    int buffer_height;
    int buffer_format;
    int need_full_paint;

    short damage[WLUP_MAX_DAMAGE * 4]; /* x, y, cx, cy per rect */
    int num_damage;
    int damage_overflow;

    /* input */
    struct zwlr_virtual_pointer_v1 *pointer;
    struct zwp_virtual_keyboard_v1 *keyboard;
    struct xkb_keymap *xkb_keymap; /* the keymap the compositor was given */
    struct xkb_state *xkb_state;   /* to compute the modifier state */
};

#endif
