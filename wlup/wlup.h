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
 * The state of the module core. Where frames and input come from is a
 * backend (wlup_backend.h), which keeps its own state in backend_data.
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
#define WLUP_MAX_MONITORS 16 /* GFX surfaces: 4 bits in xrdp's encoder */

/* Where frames and input come from */
enum wlup_backend
{
    WLUP_BACKEND_WLROOTS, /* Wayland protocols of wlroots compositors */
    WLUP_BACKEND_MUTTER   /* GNOME: Mutter D-Bus APIs, PipeWire, libei */
};


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
    tintptr server_unused1[48 - 29]; /* server functions 30 to 48 */
    int (*server_set_pointer_large)(struct wlup *v, int x, int y,
                                    char *data, char *mask, int bpp,
                                    int width, int height);
    tintptr server_unused2[50 - 49]; /* server function 50 */
    int (*server_egfx_cmd)(struct wlup *v, char *cmd, int cmd_bytes,
                           char *data, int data_bytes);
    int (*server_set_pointer_system)(struct wlup *v, int pointer_type);
    tintptr server_dumby[100 - 52]; /* align, 100 minus the number of server
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
    int large_pointer_flags; /* LARGE_POINTER_FLAG_* the client supports */

    /* frames to the GFX H.264 encoder (wlup_gfx.c) */
    int gfx_h264;            /* else server_paint_rect() */
    /* xrdp's, for the client's monitors at the time of each frame */
    const struct xrdp_client_info *client_info;
    int gfx_frame_id;        /* last frame sent */
    int gfx_frame_acked;     /* last frame xrdp acknowledged */
    unsigned int gfx_sent_time; /* g_get_elapsed_ms() at the last frame */
    short gfx_damage[WLUP_MAX_DAMAGE * 4]; /* for the next frame */
    int gfx_num_damage;
    int gfx_damage_overflow; /* gfx_damage[0..3] is a bounding box */
    /* frames are made from this copy of the desktop image, if the
     * compositor may write to the image (the backend's pixels_busy()) */
    char *gfx_shadow;
    size_t gfx_shadow_size;

    /* settings from xrdp.ini */
    char display_name[256]; /* WAYLAND_DISPLAY name or absolute path */
    char xkb_layout[64];
    char xkb_variant[64];
    enum wlup_backend backend;
    const struct wlup_backend_ops *ops; /* the backend, set on connect */
    char dbus_address[256]; /* Mutter backend: session bus, if not default */
    void *backend_data;     /* the backend's own state */
    int pipewire_fd;        /* Mutter backend: PipeWire connection from
                               sesman (display_aux_fd), or -1 */

    /* clipboard */
    void *clip;             /* RDP side (wlup_clip.c) */
    int clip_chanid;        /* clipboard channel, or -1 */

    /* resizing: while resize_pending, xrdp waits for
     * server_monitor_resize_done(), until the desktop image is
     * resize_width x resize_height */
    int resize_pending;
    int resize_width;
    int resize_height;

    /* The desktop image, XRGB8888, filled by the backend
     * (wlup_backend.h); also used to answer invalidate requests */
    char *pixels;
    size_t pixels_size;
    int buffer_width;
    int buffer_height;
    int need_full_paint;
    /* damage of the last frame, painted by wlup_paint_damage() */
    short damage[WLUP_MAX_DAMAGE * 4]; /* x, y, cx, cy per rect */
    int num_damage;
    int damage_overflow;
};

#include "wlup_backend.h"

#endif
