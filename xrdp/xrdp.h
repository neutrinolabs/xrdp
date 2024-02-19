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
 * main include file
 */

#ifndef _XRDP_XRDP_H_
#define _XRDP_XRDP_H_

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "trans.h"
#include "list.h"
#include "list16.h"
#include "libxrdpinc.h"
#include "xrdp_constants.h"
#include "xrdp_types.h"
#include "defines.h"
#include "os_calls.h"
#include "ssl_calls.h"
#include "thread_calls.h"
#include "file.h"
#include "xrdp_client_info.h"
#include "log.h"

/* xrdp.c */
long
g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1,
            long sync_param2);
int
xrdp_child_fork(void);
long
g_get_sync_mutex(void);
void
g_set_sync_mutex(long mutex);
long
g_get_sync1_mutex(void);
void
g_set_sync1_mutex(long mutex);
void
g_set_term_event(tbus event);
void
g_set_sigchld_event(tbus event);
void
g_set_sync_event(tbus event);
long
g_get_threadid(void);
void
g_set_threadid(long id);
tbus
g_get_term(void);
tbus
g_get_sigchld(void);
int
g_is_term(void);
void
g_set_term(int in_val);
void
g_set_sigchld(int in_val);
tbus
g_get_sync_event(void);
void
g_process_waiting_function(void);

/* xrdp_cache.c */
struct xrdp_cache *
xrdp_cache_create(struct xrdp_wm *owner, struct xrdp_session *session,
                  struct xrdp_client_info *client_info);
void
xrdp_cache_delete(struct xrdp_cache *self);
int
xrdp_cache_reset(struct xrdp_cache *self,
                 struct xrdp_client_info *client_info);
int
xrdp_cache_add_bitmap(struct xrdp_cache *self, struct xrdp_bitmap *bitmap,
                      int hints);
int
xrdp_cache_add_palette(struct xrdp_cache *self, int *palette);
int
xrdp_cache_add_char(struct xrdp_cache *self,
                    struct xrdp_font_char *font_item);
int
xrdp_cache_add_pointer(struct xrdp_cache *self,
                       struct xrdp_pointer_item *pointer_item);
int
xrdp_cache_add_pointer_static(struct xrdp_cache *self,
                              struct xrdp_pointer_item *pointer_item,
                              int index);
int
xrdp_cache_add_brush(struct xrdp_cache *self,
                     char *brush_item_data);
int
xrdp_cache_add_os_bitmap(struct xrdp_cache *self, struct xrdp_bitmap *bitmap,
                         int rdpindex);
int
xrdp_cache_remove_os_bitmap(struct xrdp_cache *self, int rdpindex);
struct xrdp_os_bitmap_item *
xrdp_cache_get_os_bitmap(struct xrdp_cache *self, int rdpindex);

/* xrdp_wm.c */
struct xrdp_wm *
xrdp_wm_create(struct xrdp_process *owner,
               struct xrdp_client_info *client_info);
void
xrdp_wm_delete(struct xrdp_wm *self);
int
xrdp_wm_send_palette(struct xrdp_wm *self);
int
xrdp_wm_send_bell(struct xrdp_wm *self);
int
xrdp_wm_load_static_colors_plus(struct xrdp_wm *self, char *autorun_name);
int
xrdp_wm_load_static_pointers(struct xrdp_wm *self);
int
xrdp_wm_init(struct xrdp_wm *self);
int
xrdp_wm_send_bitmap(struct xrdp_wm *self, struct xrdp_bitmap *bitmap,
                    int x, int y, int cx, int cy);
int
xrdp_wm_set_pointer(struct xrdp_wm *self, int cache_idx);
unsigned int
xrdp_wm_htoi (const char *ptr);
int
xrdp_wm_set_focused(struct xrdp_wm *self, struct xrdp_bitmap *wnd);
int
xrdp_wm_get_vis_region(struct xrdp_wm *self, struct xrdp_bitmap *bitmap,
                       int x, int y, int cx, int cy,
                       struct xrdp_region *region, int clip_children);
int
xrdp_wm_mouse_move(struct xrdp_wm *self, int x, int y);
int
xrdp_wm_mouse_touch(struct xrdp_wm *self, int gesture, int param);
int
xrdp_wm_mouse_click(struct xrdp_wm *self, int x, int y, int but, int down);
int
xrdp_wm_key(struct xrdp_wm *self, int device_flags, int scan_code);
int
xrdp_wm_key_sync(struct xrdp_wm *self, int device_flags, int key_flags);
int
xrdp_wm_pu(struct xrdp_wm *self, struct xrdp_bitmap *control);
int
xrdp_wm_send_pointer(struct xrdp_wm *self, int cache_idx,
                     char *data, char *mask, int x, int y, int bpp,
                     int width, int height);
int
xrdp_wm_pointer(struct xrdp_wm *self, char *data, char *mask, int x, int y,
                int bpp, int width, int height);
int
callback(intptr_t id, int msg, intptr_t param1, intptr_t param2,
         intptr_t param3, intptr_t param4);
int
xrdp_wm_delete_all_children(struct xrdp_wm *self);
int
xrdp_wm_show_log(struct xrdp_wm *self);
int
xrdp_wm_log_msg(struct xrdp_wm *self, enum logLevels loglevel,
                const char *fmt, ...) printflike(3, 4);
int
xrdp_wm_get_wait_objs(struct xrdp_wm *self, tbus *robjs, int *rc,
                      tbus *wobjs, int *wc, int *timeout);
int
xrdp_wm_check_wait_objs(struct xrdp_wm *self);
const char *
xrdp_wm_login_state_to_str(enum wm_login_state login_state);
int
xrdp_wm_set_login_state(struct xrdp_wm *self, enum wm_login_state login_state);
int
xrdp_wm_can_resize(struct xrdp_wm *self);
void
xrdp_wm_mod_connect_done(struct xrdp_wm *self, int status);

/* xrdp_process.c */
struct xrdp_process *
xrdp_process_create(struct xrdp_listen *owner, tbus done_event);
void
xrdp_process_delete(struct xrdp_process *self);
int
xrdp_process_main_loop(struct xrdp_process *self);

/* xrdp_listen.c */
struct xrdp_listen *
xrdp_listen_create(void);
void
xrdp_listen_delete(struct xrdp_listen *self);
int
xrdp_listen_main_loop(struct xrdp_listen *self);
int
xrdp_listen_test(struct xrdp_startup_params *startup_params);

/* xrdp_region.c */
struct xrdp_region *
xrdp_region_create(struct xrdp_wm *wm);
void
xrdp_region_delete(struct xrdp_region *self);
int
xrdp_region_add_rect(struct xrdp_region *self, struct xrdp_rect *rect);
int
xrdp_region_subtract_rect(struct xrdp_region *self, struct xrdp_rect *rect);
int
xrdp_region_intersect_rect(struct xrdp_region *self, struct xrdp_rect *rect);
int
xrdp_region_get_rect(struct xrdp_region *self, int index,
                     struct xrdp_rect *rect);
int
xrdp_region_get_bounds(struct xrdp_region *self, struct xrdp_rect *rect);
int
xrdp_region_not_empty(struct xrdp_region *self);

/* xrdp_bitmap_common.c */
struct xrdp_bitmap *
xrdp_bitmap_create(int width, int height, int bpp,
                   int type, struct xrdp_wm *wm);
struct xrdp_bitmap *
xrdp_bitmap_create_with_data(int width, int height,
                             int bpp, char *data,
                             struct xrdp_wm *wm);
void
xrdp_bitmap_delete(struct xrdp_bitmap *self);
int
xrdp_bitmap_resize(struct xrdp_bitmap *self, int width, int height);
int
xrdp_bitmap_get_pixel(struct xrdp_bitmap *self, int x, int y);
int
xrdp_bitmap_set_pixel(struct xrdp_bitmap *self, int x, int y, int pixel);
int
xrdp_bitmap_copy_box(struct xrdp_bitmap *self,
                     struct xrdp_bitmap *dest,
                     int x, int y, int cx, int cy);

/* xrdp_bitmap.c */
struct xrdp_bitmap *
xrdp_bitmap_get_child_by_id(struct xrdp_bitmap *self, int id);
int
xrdp_bitmap_set_focus(struct xrdp_bitmap *self, int focused);
int
xrdp_bitmap_hash_crc(struct xrdp_bitmap *self);
int
xrdp_bitmap_copy_box_with_crc(struct xrdp_bitmap *self,
                              struct xrdp_bitmap *dest,
                              int x, int y, int cx, int cy);
int
xrdp_bitmap_compare(struct xrdp_bitmap *self,
                    struct xrdp_bitmap *b);
int
xrdp_bitmap_invalidate(struct xrdp_bitmap *self, struct xrdp_rect *rect);
int
xrdp_bitmap_def_proc(struct xrdp_bitmap *self, int msg,
                     int param1, int param2);
int
xrdp_bitmap_to_screenx(struct xrdp_bitmap *self, int x);
int
xrdp_bitmap_to_screeny(struct xrdp_bitmap *self, int y);
int
xrdp_bitmap_from_screenx(struct xrdp_bitmap *self, int x);
int
xrdp_bitmap_from_screeny(struct xrdp_bitmap *self, int y);
int
xrdp_bitmap_get_screen_clip(struct xrdp_bitmap *self,
                            struct xrdp_painter *painter,
                            struct xrdp_rect *rect,
                            int *dx, int *dy);

/* xrdp_bitmap_load.c */
/**
 * Loads a bitmap from a file and (optionally) transforms it
 *
 * @param self from rdp_bitmap_create()
 * @param filename Filename to load
 * @param[in] palette For 8-bit conversions. Currently unused
 * @param background Background color for alpha-blending
 * @param transform Transform to apply to the image after loading
 * @param twidth target width if transform != XBLT_NONE
 * @param theight target height if transform != XBLT_NONE
 * @return 0 for success.
 *
 * The background color is only used if the specified image contains
 * an alpha layer. It is in HCOLOR format, and the bpp must correspond to
 * the bpp used to create 'self'.
 *
 * After a successful call, the bitmap is resized to the image file size.
 *
 * If the call is not successful, the bitmap will be in an indeterminate
 * state and should not be used.
 */
int
xrdp_bitmap_load(struct xrdp_bitmap *self, const char *filename,
                 const int *palette,
                 int background,
                 enum xrdp_bitmap_load_transform transform,
                 int twidth,
                 int theight);
/* xrdp_painter.c */
struct xrdp_painter *
xrdp_painter_create(struct xrdp_wm *wm, struct xrdp_session *session);
void
xrdp_painter_delete(struct xrdp_painter *self);
int
wm_painter_set_target(struct xrdp_painter *self);
int
xrdp_painter_begin_update(struct xrdp_painter *self);
int
xrdp_painter_end_update(struct xrdp_painter *self);
int
xrdp_painter_font_needed(struct xrdp_painter *self);
int
xrdp_painter_set_clip(struct xrdp_painter *self,
                      int x, int y, int cx, int cy);
int
xrdp_painter_clr_clip(struct xrdp_painter *self);
int
xrdp_painter_fill_rect(struct xrdp_painter *self,
                       struct xrdp_bitmap *bitmap,
                       int x, int y, int cx, int cy);
int
xrdp_painter_draw_bitmap(struct xrdp_painter *self,
                         struct xrdp_bitmap *bitmap,
                         struct xrdp_bitmap *to_draw,
                         int x, int y, int cx, int cy);
int
xrdp_painter_text_width(struct xrdp_painter *self, const char *text);

/* As above, but have a maximum Unicode character count for the string */
int
xrdp_painter_text_width_count(struct xrdp_painter *self,
                              const char *text, unsigned int c32_count);

/* Size of a string composed of a repeated number of Unicode characters */
int
xrdp_painter_repeated_char_width(struct xrdp_painter *self,
                                 char32_t c32, unsigned int repeat_count);

unsigned int
xrdp_painter_font_body_height(const struct xrdp_painter *self);
int
xrdp_painter_draw_text(struct xrdp_painter *self,
                       struct xrdp_bitmap *bitmap,
                       int x, int y, const char *text);
int
xrdp_painter_draw_text2(struct xrdp_painter *self,
                        struct xrdp_bitmap *bitmap,
                        int font, int flags, int mixmode,
                        int clip_left, int clip_top,
                        int clip_right, int clip_bottom,
                        int box_left, int box_top,
                        int box_right, int box_bottom,
                        int x, int y, char *data, int data_len);
int
xrdp_painter_draw_char(struct xrdp_painter *self,
                       struct xrdp_bitmap *bitmap,
                       int x, int y, char32_t chr,
                       unsigned int repeat_count);
int
xrdp_painter_copy(struct xrdp_painter *self,
                  struct xrdp_bitmap *src,
                  struct xrdp_bitmap *dst,
                  int x, int y, int cx, int cy,
                  int srcx, int srcy);
int
xrdp_painter_composite(struct xrdp_painter *self,
                       struct xrdp_bitmap *src,
                       int srcformat,
                       int srcwidth,
                       int srcrepeat,
                       struct xrdp_bitmap *dst,
                       int *srctransform,
                       int mskflags,
                       struct xrdp_bitmap *msk,
                       int mskformat, int mskwidth, int mskrepeat, int op,
                       int srcx, int srcy, int mskx, int msky,
                       int dstx, int dsty, int width, int height,
                       int dstformat);
int
xrdp_painter_line(struct xrdp_painter *self,
                  struct xrdp_bitmap *bitmap,
                  int x1, int y1, int x2, int y2);

/* xrdp_font.c */
struct xrdp_font *
xrdp_font_create(struct xrdp_wm *wm, unsigned int dpi);
void
xrdp_font_delete(struct xrdp_font *self);
int
xrdp_font_item_compare(struct xrdp_font_char *font1,
                       struct xrdp_font_char *font2);
/**
 * Gets a checked xrdp_font_char from a font
 * @param f Font
 * @param c32 Unicode codepoint
 */
#define XRDP_FONT_GET_CHAR(f, c32) \
    (((unsigned int)(c32) >= ' ') && ((unsigned int)(c32) < (f)->char_count) \
     ? ((f)->chars + (unsigned int)(c32)) \
     : (f)->default_char)

/* funcs.c */
int
rect_contains_pt(struct xrdp_rect *in, int x, int y);
int
rect_intersect(struct xrdp_rect *in1, struct xrdp_rect *in2,
               struct xrdp_rect *out);
int
rect_contained_by(struct xrdp_rect *in1, int left, int top,
                  int right, int bottom);
int
check_bounds(struct xrdp_bitmap *b, int *x, int *y, int *cx, int *cy);
int
set_string(char **in_str, const char *in);

/* in lang.c */
struct xrdp_key_info *
get_key_info_from_scan_code(int device_flags, int scan_code, int *keys,
                            int caps_lock, int num_lock, int scroll_lock,
                            struct xrdp_keymap *keymap);
int
get_keysym_from_scan_code(int device_flags, int scan_code, int *keys,
                          int caps_lock, int num_lock, int scroll_lock,
                          struct xrdp_keymap *keymap);
char32_t
get_char_from_scan_code(int device_flags, int scan_code, int *keys,
                        int caps_lock, int num_lock, int scroll_lock,
                        struct xrdp_keymap *keymap);
int
get_keymaps(int keylayout, struct xrdp_keymap *keymap);

int
km_load_file(const char *filename, struct xrdp_keymap *keymap);

/* xrdp_login_wnd.c */
/**
 * Gets the DPI of the login (primary) monitor
 *
 * @param self xrdp_wm instance
 * @return DPI of primary monitor, or 0 if unavailable.
 */
unsigned int
xrdp_login_wnd_get_monitor_dpi(struct xrdp_wm *self);
int
xrdp_login_wnd_create(struct xrdp_wm *self);
int
load_xrdp_config(struct xrdp_config *config, const char *xrdp_ini, int bpp);
void
xrdp_login_wnd_scale_config_values(struct xrdp_wm *self);

/* xrdp_bitmap_compress.c */
int
xrdp_bitmap_compress(char *in_data, int width, int height,
                     struct stream *s, int bpp, int byte_limit,
                     int start_line, struct stream *temp,
                     int e);

/* xrdp_mm.c */

struct display_control_monitor_layout_data
{
    struct display_size_description description;
    enum display_resize_state state;
    int last_state_update_timestamp;
    int start_time;
    /// This flag is set if the state machine needs to
    /// shutdown/startup EGFX
    int using_egfx;
};

int
xrdp_mm_drdynvc_up(struct xrdp_mm *self);
int
xrdp_mm_suppress_output(struct xrdp_mm *self, int suppress,
                        int left, int top, int right, int bottom);
int
xrdp_mm_up_and_running(struct xrdp_mm *self);
struct xrdp_mm *
xrdp_mm_create(struct xrdp_wm *owner);
void
xrdp_mm_delete(struct xrdp_mm *self);
void
xrdp_mm_connect(struct xrdp_mm *self);
int
xrdp_mm_process_channel_data(struct xrdp_mm *self, tbus param1, tbus param2,
                             tbus param3, tbus param4);
int
xrdp_mm_get_wait_objs(struct xrdp_mm *self,
                      tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout);
int
xrdp_mm_check_chan(struct xrdp_mm *self);
int
xrdp_mm_check_wait_objs(struct xrdp_mm *self);
int
xrdp_mm_frame_ack(struct xrdp_mm *self, int frame_id);
void
xrdp_mm_efgx_add_dirty_region_to_planar_list(struct xrdp_mm *self,
        struct xrdp_region *dirty_region);
int
xrdp_mm_egfx_send_planar_bitmap(struct xrdp_mm *self,
                                struct xrdp_bitmap *bitmap,
                                struct xrdp_rect *rect,
                                int surface_id, int x, int y);
int
server_begin_update(struct xrdp_mod *mod);
int
server_end_update(struct xrdp_mod *mod);
int
server_bell_trigger(struct xrdp_mod *mod);
int
server_chansrv_in_use(struct xrdp_mod *mod);
int
server_fill_rect(struct xrdp_mod *mod, int x, int y, int cx, int cy);
int
server_screen_blt(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                  int srcx, int srcy);
int
server_paint_rect(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                  char *data, int width, int height, int srcx, int srcy);
int
server_paint_rect_bpp(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                      char *data, int width, int height, int srcx, int srcy,
                      int bpp);
int
server_composite(struct xrdp_mod *mod, int srcidx, int srcformat, int srcwidth,
                 int srcrepeat, int *srctransform, int mskflags, int mskidx,
                 int mskformat, int mskwidth, int mskrepeat, int op,
                 int srcx, int srcy, int mskx, int msky,
                 int dstx, int dsty, int width, int height, int dstformat);
int
server_paint_rects(struct xrdp_mod *mod, int num_drects, short *drects,
                   int num_crects, short *crects,
                   char *data, int width, int height,
                   int flags, int frame_id);
int
server_set_pointer(struct xrdp_mod *mod, int x, int y,
                   char *data, char *mask);
int
server_set_pointer_ex(struct xrdp_mod *mod, int x, int y,
                      char *data, char *mask, int bpp);
int
server_set_pointer_large(struct xrdp_mod *mod, int x, int y,
                         char *data, char *mask, int bpp,
                         int width, int height);
int
server_paint_rects_ex(struct xrdp_mod *mod,
                      int num_drects, short *drects,
                      int num_crects, short *crects,
                      char *data, int left, int top,
                      int width, int height,
                      int flags, int frame_id,
                      void *shmem_ptr, int shmem_bytes);
int
server_palette(struct xrdp_mod *mod, int *palette);
int
server_msg(struct xrdp_mod *mod, const char *msg, int code);
int
server_set_clip(struct xrdp_mod *mod, int x, int y, int cx, int cy);
int
server_reset_clip(struct xrdp_mod *mod);
int
server_set_fgcolor(struct xrdp_mod *mod, int fgcolor);
int
server_set_bgcolor(struct xrdp_mod *mod, int bgcolor);
int
server_set_opcode(struct xrdp_mod *mod, int opcode);
int
server_set_mixmode(struct xrdp_mod *mod, int mixmode);
int
server_set_brush(struct xrdp_mod *mod, int x_origin, int y_origin,
                 int style, char *pattern);
int
server_set_pen(struct xrdp_mod *mod, int style, int width);
int
server_draw_line(struct xrdp_mod *mod, int x1, int y1, int x2, int y2);
int
server_add_char(struct xrdp_mod *mod, int font, int character,
                int offset, int baseline,
                int width, int height, char *data);
int
server_draw_text(struct xrdp_mod *mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char *data, int data_len);
int
client_monitor_resize(struct xrdp_mod *mod, int width, int height,
                      int num_monitors, const struct monitor_info *monitors);
int
server_monitor_resize_done(struct xrdp_mod *mod);
int
is_channel_allowed(struct xrdp_wm *wm, int channel_id);
int
server_get_channel_count(struct xrdp_mod *mod);
int
server_query_channel(struct xrdp_mod *mod, int index, char *channel_name,
                     int *channel_flags);
int
server_get_channel_id(struct xrdp_mod *mod, const char *name);
int
server_send_to_channel(struct xrdp_mod *mod, int channel_id,
                       char *data, int data_len,
                       int total_data_len, int flags);
int
server_create_os_surface(struct xrdp_mod *mod, int id,
                         int width, int height);
int
server_create_os_surface_bpp(struct xrdp_mod *mod, int id,
                             int width, int height, int bpp);
int
server_switch_os_surface(struct xrdp_mod *mod, int id);
int
server_delete_os_surface(struct xrdp_mod *mod, int id);
int
server_paint_rect_os(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                     int id, int srcx, int srcy);
int
server_set_hints(struct xrdp_mod *mod, int hints, int mask);
int
server_window_new_update(struct xrdp_mod *mod, int window_id,
                         struct rail_window_state_order *window_state,
                         int flags);
int
server_window_delete(struct xrdp_mod *mod, int window_id);
int
server_window_icon(struct xrdp_mod *mod, int window_id, int cache_entry,
                   int cache_id, struct rail_icon_info *icon_info,
                   int flags);
int
server_window_cached_icon(struct xrdp_mod *mod,
                          int window_id, int cache_entry,
                          int cache_id, int flags);
int
server_notify_new_update(struct xrdp_mod *mod,
                         int window_id, int notify_id,
                         struct rail_notify_state_order *notify_state,
                         int flags);
int
server_notify_delete(struct xrdp_mod *mod, int window_id,
                     int notify_id);
int
server_monitored_desktop(struct xrdp_mod *mod,
                         struct rail_monitored_desktop_order *mdo,
                         int flags);
int
server_add_char_alpha(struct xrdp_mod *mod, int font, int character,
                      int offset, int baseline,
                      int width, int height, char *data);
int
server_session_info(struct xrdp_mod *mod, const char *data, int data_bytes);
int
server_egfx_cmd(struct xrdp_mod *v,
                char *cmd, int cmd_bytes,
                char *data, int data_bytes);

#endif
