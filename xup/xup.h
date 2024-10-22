/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * libxup main header file
 */

#ifndef XUP_H
#define XUP_H

/**
 * Enum for the states used to process a
 * capabilities message from the Xorg module
 */
enum caps_processing_status
{
    E_CAPS_NOT_PROCESSED, ///< Capabilities mesage from module not processed
    E_CAPS_OK,            ///< Capabilities are OK
    E_CAPS_NOT_OK         ///< Capabilities are not OK
};

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"
#include "log.h"
#include "xrdp_client_info.h"
#include "xrdp_constants.h"
#include "xrdp_rail.h"

#define CURRENT_MOD_VER 4

struct source_info;
struct xrdp_client_info;

struct mod
{
    int size; /* size of this struct */
    int version; /* internal version */
    /* client functions */
    int (*mod_start)(struct mod *v, int w, int h, int bpp);
    int (*mod_connect)(struct mod *v);
    int (*mod_event)(struct mod *v, int msg, tbus param1, tbus param2,
                     tbus param3, tbus param4);
    int (*mod_signal)(struct mod *v);
    int (*mod_end)(struct mod *v);
    int (*mod_set_param)(struct mod *v, const char *name, const char *value);
    int (*mod_session_change)(struct mod *v, int, int);
    int (*mod_get_wait_objs)(struct mod *v, tbus *read_objs, int *rcount,
                             tbus *write_objs, int *wcount, int *timeout);
    int (*mod_check_wait_objs)(struct mod *v);
    int (*mod_frame_ack)(struct mod *v, int flags, int frame_id);
    int (*mod_suppress_output)(struct mod *v, int suppress,
                               int left, int top, int right, int bottom);
    int (*mod_server_monitor_resize)(struct mod *v,
                                     int width, int height,
                                     int num_monitors,
                                     const struct monitor_info *monitors,
                                     int *in_progress);
    int (*mod_server_monitor_full_invalidate)(struct mod *v,
            int width, int height);
    int (*mod_server_version_message)(struct mod *v);
    tintptr mod_dumby[100 - 14]; /* align, 100 minus the number of mod
                                 functions above */
    /* server functions */
    int (*server_begin_update)(struct mod *v);
    int (*server_end_update)(struct mod *v);
    int (*server_fill_rect)(struct mod *v, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct mod *v, int x, int y, int cx, int cy,
                             int srcx, int srcy);
    int (*server_paint_rect)(struct mod *v, int x, int y, int cx, int cy,
                             char *data, int width, int height,
                             int srcx, int srcy);
    int (*server_set_cursor)(struct mod *v, int x, int y, char *data, char *mask);
    int (*server_palette)(struct mod *v, int *palette);
    int (*server_msg)(struct mod *v, const char *msg, int code);
    int (*server_is_term)(void);
    int (*server_set_clip)(struct mod *v, int x, int y, int cx, int cy);
    int (*server_reset_clip)(struct mod *v);
    int (*server_set_fgcolor)(struct mod *v, int fgcolor);
    int (*server_set_bgcolor)(struct mod *v, int bgcolor);
    int (*server_set_opcode)(struct mod *v, int opcode);
    int (*server_set_mixmode)(struct mod *v, int mixmode);
    int (*server_set_brush)(struct mod *v, int x_origin, int y_origin,
                            int style, char *pattern);
    int (*server_set_pen)(struct mod *v, int style,
                          int width);
    int (*server_draw_line)(struct mod *v, int x1, int y1, int x2, int y2);
    int (*server_add_char)(struct mod *v, int font, int character,
                           int offset, int baseline,
                           int width, int height, char *data);
    int (*server_draw_text)(struct mod *v, int font,
                            int flags, int mixmode, int clip_left, int clip_top,
                            int clip_right, int clip_bottom,
                            int box_left, int box_top,
                            int box_right, int box_bottom,
                            int x, int y, char *data, int data_len);
    int (*client_monitor_resize)(struct mod *v, int width, int height,
                                 int num_monitors,
                                 const struct monitor_info *monitors);
    int (*server_monitor_resize_done)(struct mod *v);
    int (*server_get_channel_count)(struct mod *v);
    int (*server_query_channel)(struct mod *v, int index,
                                char *channel_name,
                                int *channel_flags);
    int (*server_get_channel_id)(struct mod *v, const char *name);
    int (*server_send_to_channel)(struct mod *v, int channel_id,
                                  char *data, int data_len,
                                  int total_data_len, int flags);
    int (*server_bell_trigger)(struct mod *v);
    int (*server_chansrv_in_use)(struct mod *v);
    void (*server_init_xkb_layout)(struct mod *v,
                                   struct xrdp_client_info *client_info);
    /* off screen bitmaps */
    int (*server_create_os_surface)(struct mod *v, int rdpindex,
                                    int width, int height);
    int (*server_switch_os_surface)(struct mod *v, int rdpindex);
    int (*server_delete_os_surface)(struct mod *v, int rdpindex);
    int (*server_paint_rect_os)(struct mod *v, int x, int y,
                                int cx, int cy,
                                int rdpindex, int srcx, int srcy);
    int (*server_set_hints)(struct mod *v, int hints, int mask);
    /* rail */
    int (*server_window_new_update)(struct mod *v, int window_id,
                                    struct rail_window_state_order *window_state,
                                    int flags);
    int (*server_window_delete)(struct mod *v, int window_id);
    int (*server_window_icon)(struct mod *v,
                              int window_id, int cache_entry, int cache_id,
                              struct rail_icon_info *icon_info,
                              int flags);
    int (*server_window_cached_icon)(struct mod *v,
                                     int window_id, int cache_entry,
                                     int cache_id, int flags);
    int (*server_notify_new_update)(struct mod *v,
                                    int window_id, int notify_id,
                                    struct rail_notify_state_order *notify_state,
                                    int flags);
    int (*server_notify_delete)(struct mod *v, int window_id,
                                int notify_id);
    int (*server_monitored_desktop)(struct mod *v,
                                    struct rail_monitored_desktop_order *mdo,
                                    int flags);
    int (*server_set_cursor_ex)(struct mod *v, int x, int y, char *data,
                                char *mask, int bpp);
    int (*server_add_char_alpha)(struct mod *v, int font, int character,
                                 int offset, int baseline,
                                 int width, int height, char *data);
    int (*server_create_os_surface_bpp)(struct mod *v, int rdpindex,
                                        int width, int height, int bpp);
    int (*server_paint_rect_bpp)(struct mod *v, int x, int y, int cx, int cy,
                                 char *data, int width, int height,
                                 int srcx, int srcy, int bpp);
    int (*server_composite)(struct mod *v, int srcidx, int srcformat, int srcwidth,
                            int srcrepeat, int *srctransform, int mskflags, int mskidx,
                            int mskformat, int mskwidth, int mskrepeat, int op,
                            int srcx, int srcy, int mskx, int msky,
                            int dstx, int dsty, int width, int height, int dstformat);
    int (*server_paint_rects)(struct mod *v,
                              int num_drects, short *drects,
                              int num_crects, short *crects,
                              char *data, int width, int height,
                              int flags, int frame_id);
    int (*server_session_info)(struct mod *v, const char *data,
                               int data_bytes);
    int (*server_set_pointer_large)(struct mod *v, int x, int y,
                                    char *data, char *mask, int bpp,
                                    int width, int height);
    int (*server_paint_rects_ex)(struct mod *v,
                                 int num_drects, short *drects,
                                 int num_crects, short *crects,
                                 char *data, int left, int top,
                                 int width, int height,
                                 int flags, int frame_id,
                                 void *shmem_ptr, int shmem_bytes);
    int (*server_egfx_cmd)(struct mod *v,
                           char *cmd, int cmd_bytes,
                           char *data, int data_bytes);
    tintptr server_dumby[100 - 51]; /* align, 100 minus the number of server
                                     functions above */
    /* common */
    tintptr handle; /* pointer to self as long */
    tintptr wm;
    tintptr painter;
    struct source_info *si;
    /* mod data */
    int width;
    int height;
    int bpp;
    int sck_closed;
    char username[INFO_CLIENT_MAX_CB_LEN];
    char password[INFO_CLIENT_MAX_CB_LEN];
    char ip[256];
    char port[256];
    int shift_state;
    struct xrdp_client_info client_info;
    int screen_shmem_id;
    int screen_shmem_id_mapped; /* boolean */
    char *screen_shmem_pixels;
    struct trans *trans;
    char keycode_set[32];
    enum caps_processing_status caps_processing_status;
};

#endif // XUP_H
