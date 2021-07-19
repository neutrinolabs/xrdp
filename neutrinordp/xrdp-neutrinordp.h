/**
 * FreeRDP: A Remote Desktop Protocol Server
 * freerdp wrapper
 *
 * Copyright 2011-2013 Jay Sorg
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
 */

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"
#include "xrdp_rail.h"
#include "xrdp_client_info.h"
#include "xrdp_constants.h"

/* this is the freerdp main header */
#include <freerdp/freerdp.h>
#include <freerdp/rail.h>
#include <freerdp/rail/rail.h>
#include <freerdp/codec/bitmap.h>
//#include <freerdp/utils/memory.h>
//#include "/home/jay/git/jsorg71/staging/include/freerdp/freerdp.h"

struct bitmap_item
{
    int width;
    int height;
    char *data;
};

struct brush_item
{
    int bpp;
    int width;
    int height;
    char *data;
    char b8x8[8];
};

struct pointer_item
{
    int hotx;
    int hoty;
    char data[32 * 32 * 4];
    char mask[32 * 32 / 8];
    int bpp;
};

#define CURRENT_MOD_VER 4

struct source_info;

struct kbd_overrides
{
    int type;
    int subtype;
    int fn_keys;
    int layout;
    int layout_mask;
};

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
    int (*mod_frame_ack)(struct mod *mod, int flags, int frame_id);
    int (*mod_suppress_output)(struct mod *mod, int suppress,
                               int left, int top, int right, int bottom);
    int (*mod_server_monitor_resize)(struct mod *mod,
                                     int width, int height);
    int (*mod_server_monitor_full_invalidate)(struct mod *mod,
            int width, int height);
    int (*mod_server_version_message)(struct mod *mod);
    tintptr mod_dumby[100 - 14]; /* align, 100 minus the number of mod
                                 functions above */
    /* server functions */
    int (*server_begin_update)(struct mod *v);
    int (*server_end_update)(struct mod *v);
    int (*server_fill_rect)(struct mod *v, int x, int y, int cx, int cy);
    int (*server_screen_blt)(struct mod *v, int x, int y, int cx, int cy,
                             int srcx, int srcy);
    int (*server_paint_rect)(struct mod *v, int x, int y, int cx, int cy,
                             char *data, int width, int height, int srcx, int srcy);
    int (*server_set_pointer)(struct mod *v, int x, int y, char *data, char *mask);
    int (*server_palette)(struct mod *v, int *palette);
    int (*server_msg)(struct mod *v, char *msg, int code);
    int (*server_is_term)(struct mod *v);
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
    int (*server_reset)(struct mod *v, int width, int height, int bpp);
    int (*server_query_channel)(struct mod *v, int index,
                                char *channel_name,
                                int *channel_flags);
    int (*server_get_channel_id)(struct mod *v, const char *name);
    int (*server_send_to_channel)(struct mod *v, int channel_id,
                                  char *data, int data_len,
                                  int total_data_len, int flags);
    int (*server_bell_trigger)(struct mod *v);
    int (*server_chansrv_in_use)(struct mod *v);
    /* off screen bitmaps */
    int (*server_create_os_surface)(struct mod *v, int rdpindex,
                                    int width, int height);
    int (*server_switch_os_surface)(struct mod *v, int rdpindex);
    int (*server_delete_os_surface)(struct mod *v, int rdpindex);
    int (*server_paint_rect_os)(struct mod *mod, int x, int y,
                                int cx, int cy,
                                int rdpindex, int srcx, int srcy);
    int (*server_set_hints)(struct mod *mod, int hints, int mask);
    /* rail */
    int (*server_window_new_update)(struct mod *mod, int window_id,
                                    struct rail_window_state_order *window_state,
                                    int flags);
    int (*server_window_delete)(struct mod *mod, int window_id);
    int (*server_window_icon)(struct mod *mod,
                              int window_id, int cache_entry, int cache_id,
                              struct rail_icon_info *icon_info,
                              int flags);
    int (*server_window_cached_icon)(struct mod *mod,
                                     int window_id, int cache_entry,
                                     int cache_id, int flags);
    int (*server_notify_new_update)(struct mod *mod,
                                    int window_id, int notify_id,
                                    struct rail_notify_state_order *notify_state,
                                    int flags);
    int (*server_notify_delete)(struct mod *mod, int window_id,
                                int notify_id);
    int (*server_monitored_desktop)(struct mod *mod,
                                    struct rail_monitored_desktop_order *mdo,
                                    int flags);
    int (*server_set_pointer_ex)(struct mod *mod, int x, int y, char *data,
                                 char *mask, int bpp);
    int (*server_add_char_alpha)(struct mod *mod, int font, int character,
                                 int offset, int baseline,
                                 int width, int height, char *data);
    int (*server_create_os_surface_bpp)(struct mod *v, int rdpindex,
                                        int width, int height, int bpp);
    int (*server_paint_rect_bpp)(struct mod *v, int x, int y, int cx, int cy,
                                 char *data, int width, int height,
                                 int srcx, int srcy, int bpp);
    int (*server_composite)(struct mod *v, int srcidx, int srcformat,
                            int srcwidth, int srcrepeat, int *srctransform,
                            int mskflags, int mskidx, int mskformat,
                            int mskwidth, int mskrepeat, int op,
                            int srcx, int srcy, int mskx, int msky,
                            int dstx, int dsty, int width, int height,
                            int dstformat);
    int (*server_paint_rects)(struct mod *v,
                              int num_drects, short *drects,
                              int num_crects, short *crects,
                              char *data, int width, int height,
                              int flags, int frame_id);
    int (*server_session_info)(struct mod *v, const char *data,
                               int data_bytes);
    tintptr server_dumby[100 - 45]; /* align, 100 minus the number of server
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
    int colormap[256];
    char *chan_buf;
    int chan_buf_valid;
    int chan_buf_bytes;
    int vmaj;
    int vmin;
    int vrev;
    char username[INFO_CLIENT_MAX_CB_LEN];
    char password[INFO_CLIENT_MAX_CB_LEN];
    char domain[INFO_CLIENT_MAX_CB_LEN];
    int bool_keyBoardSynced ; /* Numlock can be out of sync, we hold state here to resolve */
    int keyBoardLockInfo ; /* Holds initial numlock capslock state */

    struct xrdp_client_info client_info;

    struct rdp_freerdp *inst;
    struct bitmap_item bitmap_cache[4][4096];
    struct brush_item brush_cache[64];
    struct pointer_item pointer_cache[32];
    char pamusername[255];

    int allow_client_experiencesettings;
    int perf_settings_override_mask; /* Performance bits overridden in ini file */
    int perf_settings_values_mask; /* Values of overridden performance bits */
    int allow_client_kbd_settings;
    struct kbd_overrides kbd_overrides; /* neutrinordp.overide_kbd_* values */
};
