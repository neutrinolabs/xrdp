/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * media center
 */

#ifndef MC_H
#define MC_H

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"

#define CURRENT_MOD_VER 3

struct source_info;

/* Defined in xrdp_client_info.h */
struct monitor_info;

struct mod
{
    int size; /* size of this struct */
    int version; /* internal version */
    /* client functions */
    int (*mod_start)(struct mod *v, int w, int h, int bpp);
    int (*mod_connect)(struct mod *v);
    int (*mod_event)(struct mod *v, int msg, long param1, long param2,
                     long param3, long param4);
    int (*mod_signal)(struct mod *v);
    int (*mod_end)(struct mod *v);
    int (*mod_set_param)(struct mod *v, const char *name, const char *value);
    int (*mod_session_change)(struct mod *v, int, int);
    int (*mod_get_wait_objs)(struct mod *v, tbus *read_objs, int *rcount,
                             tbus *write_objs, int *wcount, int *timeout);
    int (*mod_check_wait_objs)(struct mod *v);
    tintptr mod_dumby[100 - 9]; /* align, 100 minus the number of mod
                                 functions above */
    /* server functions */
    int (*server_begin_update)(struct mod *v);
    int (*server_end_update)(struct mod *v);
    int (*server_fill_rect)(struct mod *v, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct mod *v, int x, int y, int cx, int cy,
                             int srcx, int srcy);
    int (*server_paint_rect)(struct mod *v, int x, int y, int cx, int cy,
                             char *data, int width, int height, int srcx, int srcy);
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
    tintptr server_dumby[100 - 28]; /* align, 100 minus the number of server
                                     functions above */
    /* common */
    tintptr handle; /* pointer to self as long */
    tintptr wm;
    tintptr painter;
    struct source_info *si;
    /* mod data */
    int sck;
    int width;
    int height;
    int bpp;
};

#endif // MC_H
