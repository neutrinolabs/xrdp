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

struct list;
struct monitor_info;

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
    int check_for_app_input;
    void *rdp;
    void *orders;
    struct xrdp_client_info *client_info;
    int up_and_running;
    int (*is_term)(void);
    int in_process_data; /* inc / dec libxrdp_process_data calls */

    struct source_info si;
    char *xrdp_ini; /* path to xrdp.ini */
};

struct xrdp_drdynvc_procs
{
    int (*open_response)(intptr_t id, int chan_id, int creation_status);
    int (*close_response)(intptr_t id, int chan_id);
    int (*data_first)(intptr_t id, int chan_id, char *data, int bytes, int total_bytes);
    int (*data)(intptr_t id, int chan_id, char *data, int bytes);
};

/* Defined in xrdp_client_info.h */
struct display_size_description;

/***
 * Initialise the XRDP library
 *
 * @param id Channel ID (xrdp_process* as integer type)
 * @param trans Transport object to use for this instance
 * @param xrdp_ini Path to xrdp.ini config file, or NULL for default
 * @return an allocated xrdp_session object
 */
struct xrdp_session *
libxrdp_init(tbus id, struct trans *trans, const char *xrdp_ini);
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
                     char *data, char *mask, int x, int y, int bpp,
                     int width, int height);
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
libxrdp_reset(struct xrdp_session *session);
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
/**
 * Returns the number of channels in the session
 *
 * This value is one more than the last valid channel ID
 *
 * @param session RDP session
 * @return Number of available channels
 */
int
libxrdp_get_channel_count(const struct xrdp_session *session);
int
libxrdp_query_channel(struct xrdp_session *session, int channel_id,
                      char *channel_name, int *channel_flags);
/**
 * Gets the channel ID for the named channel
 *
 * Channel IDs are in the range 0..(channel_count-1)
 *
 * @param session RDP session
 * @param name name of channel
 * @return channel ID, or -1 if the channel cannot be found
 */
int
libxrdp_get_channel_id(struct xrdp_session *session, const char *name);
int
libxrdp_send_to_channel(struct xrdp_session *session, int channel_id,
                        char *data, int data_len,
                        int total_data_len, int flags);
int
libxrdp_disable_channel(struct xrdp_session *session, int channel_id,
                        int is_disabled);
int
libxrdp_drdynvc_open(struct xrdp_session *session, const char *name,
                     int flags, struct xrdp_drdynvc_procs *procs,
                     int *chan_id);
int
libxrdp_drdynvc_close(struct xrdp_session *session, int chan_id);
int
libxrdp_drdynvc_data_first(struct xrdp_session *session, int chan_id,
                           const char *data, int data_bytes,
                           int total_data_bytes);
int
libxrdp_drdynvc_data(struct xrdp_session *session, int chan_id,
                     const char *data, int data_bytes);
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
int EXPORT_CC
libxrdp_planar_compress(char *in_data, int width, int height,
                        struct stream *s, int bpp, int byte_limit,
                        int start_line, struct stream *temp_s,
                        int e, int flags);
/**
 * Processes a stream that is based on either
 *  2.2.1.3.6 Client Monitor Data (TS_UD_CS_MONITOR) or 2.2.2.2 DISPLAYCONTROL_MONITOR_LAYOUT_PDU
 *  and then stores the processed monitor data into the description parameter.
 * @param s
 *      The stream to process.
 * @param description
 *      Must be pre-allocated. Monitor data is filled in as part of processing the stream.
 * @param full_parameters
 *      0 if the monitor stream is from 2.2.1.3.6 Client Monitor Data (TS_UD_CS_MONITOR)
 *      1 if the monitor stream is from 2.2.2.2 DISPLAYCONTROL_MONITOR_LAYOUT_PDU
 * @return 0 if the data is processed, non-zero if there is an error.
 */
int EXPORT_CC
libxrdp_process_monitor_stream(struct stream *s, struct display_size_description *description,
                               int full_parameters);

/**
 * Processes a stream that is based on [MS-RDPBCGR] 2.2.1.3.9 Client
 * Monitor Extended Data (TS_UD_CS_MONITOR_EX)
 *
 * Data is stored in a struct which has already been read by
 * libxrdp_process_monitor_stream() withj full_parameters set to 0.
 *
 * @param s Stream to read data from. Stream is read up to monitorAttributeSize
 * @param description Result of reading TS_UD_CS_MONITOR PDU
 * @return 0 if the data is processed, non-zero if there is an error.
 */
int EXPORT_CC
libxrdp_process_monitor_ex_stream(struct stream *s,
                                  struct display_size_description *description);

/**
 * Convert a list of monitors into a full description
 *
 * Monitor data is sanitised during the conversion
 *
 * @param num_monitor Monitor count (> 0)
 * @param monitors List of monitors
 * @param[out] description Display size description
 *
 * @return 0 if the data is processed, non-zero if there is an error.
 */
int
libxrdp_init_display_size_description(
    unsigned int num_monitor,
    const struct monitor_info *monitors,
    struct display_size_description *description);

#endif
