/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * header file for use with libxrdp.so / xrdp.dll
 */

#ifndef LIBXRDPINC_H
#define LIBXRDPINC_H

#include "xrdp_rail.h"

/* struct xrdp_client_info moved to xrdp_client_info.h */

struct xrdp_brush
{
    int x_origin;
    int y_origin;
    int style;
    char pattern[8];
};

struct xrdp_pen
{
    int style;
    int width;
    int color;
};

/* 2.2.2.2.1.2.5.1 Cache Glyph Data (TS_CACHE_GLYPH_DATA) */
struct xrdp_font_char
{
    int offset;    /* x */
    int baseline;  /* y */
    int width;     /* cx */
    int height;    /* cy */
    int incby;
    int bpp;
    char *data;
};

struct xrdp_rect
{
    int left;
    int top;
    int right;
    int bottom;
};

struct xrdp_session
{
    tintptr id;
    struct trans *trans;
    int (*callback)(intptr_t id, int msg, intptr_t param1, intptr_t param2,
                    intptr_t param3, intptr_t param4);
    void *rdp;
    void *orders;
    struct xrdp_client_info *client_info;
    int up_and_running;
    int (*is_term)(void);
    int in_process_data; /* inc / dec libxrdp_process_data calls */

    struct source_info si;
};

struct xrdp_session *
libxrdp_init(tbus id, struct trans *trans);
int
libxrdp_exit(struct xrdp_session *session);
int
libxrdp_disconnect(struct xrdp_session *session);
int
libxrdp_process_incoming(struct xrdp_session *session);
int EXPORT_CC
libxrdp_get_pdu_bytes(const char *aheader);
struct stream *
libxrdp_force_read(struct trans *trans);
int
libxrdp_process_data(struct xrdp_session *session, struct stream *s);
int
libxrdp_send_palette(struct xrdp_session *session, int *palette);
int
libxrdp_send_bell(struct xrdp_session *session);
int
libxrdp_send_bitmap(struct xrdp_session *session, int width, int height,
                    int bpp, char *data, int x, int y, int cx, int cy);
int
libxrdp_send_pointer(struct xrdp_session *session, int cache_idx,
                     char *data, char *mask, int x, int y, int bpp);
int
libxrdp_set_pointer(struct xrdp_session *session, int cache_idx);
int
libxrdp_orders_init(struct xrdp_session *session);
int
libxrdp_orders_send(struct xrdp_session *session);
int
libxrdp_orders_force_send(struct xrdp_session *session);
int
libxrdp_orders_rect(struct xrdp_session *session, int x, int y,
                    int cx, int cy, int color, struct xrdp_rect *rect);
int
libxrdp_orders_screen_blt(struct xrdp_session *session, int x, int y,
                          int cx, int cy, int srcx, int srcy,
                          int rop, struct xrdp_rect *rect);
int
libxrdp_orders_pat_blt(struct xrdp_session *session, int x, int y,
                       int cx, int cy, int rop, int bg_color,
                       int fg_color, struct xrdp_brush *brush,
                       struct xrdp_rect *rect);
int
libxrdp_orders_dest_blt(struct xrdp_session *session, int x, int y,
                        int cx, int cy, int rop,
                        struct xrdp_rect *rect);
int
libxrdp_orders_line(struct xrdp_session *session, int mix_mode,
                    int startx, int starty,
                    int endx, int endy, int rop, int bg_color,
                    struct xrdp_pen *pen,
                    struct xrdp_rect *rect);
int
libxrdp_orders_mem_blt(struct xrdp_session *session, int cache_id,
                       int color_table, int x, int y, int cx, int cy,
                       int rop, int srcx, int srcy,
                       int cache_idx, struct xrdp_rect *rect);
int
libxrdp_orders_composite_blt(struct xrdp_session *session, int srcidx,
                             int srcformat, int srcwidth, int srcrepeat,
                             int *srctransform, int mskflags,
                             int mskidx, int mskformat, int mskwidth,
                             int mskrepeat, int op, int srcx, int srcy,
                             int mskx, int msky, int dstx, int dsty,
                             int width, int height, int dstformat,
                             struct xrdp_rect *rect);

int
libxrdp_orders_text(struct xrdp_session *session,
                    int font, int flags, int mixmode,
                    int fg_color, int bg_color,
                    int clip_left, int clip_top,
                    int clip_right, int clip_bottom,
                    int box_left, int box_top,
                    int box_right, int box_bottom,
                    int x, int y, char *data, int data_len,
                    struct xrdp_rect *rect);
int
libxrdp_orders_send_palette(struct xrdp_session *session, int *palette,
                            int cache_id);
int
libxrdp_orders_send_raw_bitmap(struct xrdp_session *session,
                               int width, int height, int bpp, char *data,
                               int cache_id, int cache_idx);
int
libxrdp_orders_send_bitmap(struct xrdp_session *session,
                           int width, int height, int bpp, char *data,
                           int cache_id, int cache_idx);
int
libxrdp_orders_send_font(struct xrdp_session *session,
                         struct xrdp_font_char *font_char,
                         int font_index, int char_index);
int
libxrdp_reset(struct xrdp_session *session,
              int width, int height, int bpp);
int
libxrdp_orders_send_raw_bitmap2(struct xrdp_session *session,
                                int width, int height, int bpp, char *data,
                                int cache_id, int cache_idx);
int
libxrdp_orders_send_bitmap2(struct xrdp_session *session,
                            int width, int height, int bpp, char *data,
                            int cache_id, int cache_idx, int hints);
int
libxrdp_orders_send_bitmap3(struct xrdp_session *session,
                            int width, int height, int bpp, char *data,
                            int cache_id, int cache_idx, int hints);
int
libxrdp_query_channel(struct xrdp_session *session, int index,
                      char *channel_name, int *channel_flags);
int
libxrdp_get_channel_id(struct xrdp_session *session, const char *name);
int
libxrdp_send_to_channel(struct xrdp_session *session, int channel_id,
                        char *data, int data_len,
                        int total_data_len, int flags);
int
libxrdp_orders_send_brush(struct xrdp_session *session,
                          int width, int height, int bpp, int type,
                          int size, char *data, int cache_id);
int
libxrdp_orders_send_create_os_surface(struct xrdp_session *session, int id,
                                      int width, int height,
                                      struct list *del_list);
int
libxrdp_orders_send_switch_os_surface(struct xrdp_session *session, int id);
int
libxrdp_window_new_update(struct xrdp_session *session, int window_id,
                          struct rail_window_state_order *window_state,
                          int flags);
int
libxrdp_window_delete(struct xrdp_session *session, int window_id);
int
libxrdp_window_icon(struct xrdp_session *session, int window_id,
                    int cache_entry, int cache_id,
                    struct rail_icon_info *icon_info, int flags);
int
libxrdp_window_cached_icon(struct xrdp_session *session, int window_id,
                           int cache_entry, int cache_id,
                           int flags);
int
libxrdp_notify_new_update(struct xrdp_session *session,
                          int window_id, int notify_id,
                          struct rail_notify_state_order *notify_state,
                          int flags);
int
libxrdp_notify_delete(struct xrdp_session *session,
                      int window_id, int notify_id);
int
libxrdp_monitored_desktop(struct xrdp_session *session,
                          struct rail_monitored_desktop_order *mdo,
                          int flags);
int
libxrdp_codec_jpeg_compress(struct xrdp_session *session,
                            int format, char *inp_data,
                            int width, int height,
                            int stride, int x, int y,
                            int cx, int cy, int quality,
                            char *out_data, int *io_len);
int
libxrdp_fastpath_send_surface(struct xrdp_session *session,
                              char *data_pad, int pad_bytes,
                              int data_bytes,
                              int destLeft, int dst_Top,
                              int destRight, int destBottom, int bpp,
                              int codecID, int width, int height);
int EXPORT_CC
libxrdp_fastpath_send_frame_marker(struct xrdp_session *session,
                                   int frame_action, int frame_id);
int EXPORT_CC
libxrdp_send_session_info(struct xrdp_session *session, const char *data,
                          int data_bytes);

#endif
