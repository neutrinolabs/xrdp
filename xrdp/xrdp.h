/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2007

   main include file

*/

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "libxrdpinc.h"
#include "xrdp_types.h"
#include "xrdp_constants.h"
#include "defines.h"
#include "os_calls.h"
#include "ssl_calls.h"
#include "thread_calls.h"
#include "list.h"
#include "file.h"

/* xrdp.c */
long APP_CC
g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1,
            long sync_param2);
int APP_CC
g_is_term(void);
void APP_CC
g_set_term(int in_val);
void APP_CC
g_loop(void);

/* xrdp_cache.c */
struct xrdp_cache* APP_CC
xrdp_cache_create(struct xrdp_wm* owner, struct xrdp_session* session,
                  struct xrdp_client_info* client_info);
void APP_CC
xrdp_cache_delete(struct xrdp_cache* self);
int APP_CC
xrdp_cache_reset(struct xrdp_cache* self,
                 struct xrdp_client_info* client_info);
int APP_CC
xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap);
int APP_CC
xrdp_cache_add_palette(struct xrdp_cache* self, int* palette);
int APP_CC
xrdp_cache_add_char(struct xrdp_cache* self,
                    struct xrdp_font_char* font_item);
int APP_CC
xrdp_cache_add_pointer(struct xrdp_cache* self,
                       struct xrdp_pointer_item* pointer_item);
int APP_CC
xrdp_cache_add_pointer_static(struct xrdp_cache* self,
                              struct xrdp_pointer_item* pointer_item,
                              int index);

/* xrdp_wm.c */
struct xrdp_wm* APP_CC
xrdp_wm_create(struct xrdp_process* owner,
               struct xrdp_client_info* client_info);
void APP_CC
xrdp_wm_delete(struct xrdp_wm* self);
int APP_CC
xrdp_wm_send_palette(struct xrdp_wm* self);
int APP_CC
xrdp_wm_load_static_colors(struct xrdp_wm* self);
int APP_CC
xrdp_wm_load_static_pointers(struct xrdp_wm* self);
int APP_CC
xrdp_wm_init(struct xrdp_wm* self);
int APP_CC
xrdp_wm_send_bitmap(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                    int x, int y, int cx, int cy);
int APP_CC
xrdp_wm_set_pointer(struct xrdp_wm* self, int cache_idx);
int APP_CC
xrdp_wm_set_focused(struct xrdp_wm* self, struct xrdp_bitmap* wnd);
int APP_CC
xrdp_wm_get_vis_region(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                       int x, int y, int cx, int cy,
                       struct xrdp_region* region, int clip_children);
int APP_CC
xrdp_wm_mouse_move(struct xrdp_wm* self, int x, int y);
int APP_CC
xrdp_wm_mouse_click(struct xrdp_wm* self, int x, int y, int but, int down);
int APP_CC
xrdp_wm_key(struct xrdp_wm* self, int device_flags, int scan_code);
int APP_CC
xrdp_wm_key_sync(struct xrdp_wm* self, int device_flags, int key_flags);
int APP_CC
xrdp_wm_pu(struct xrdp_wm* self, struct xrdp_bitmap* control);
int APP_CC
xrdp_wm_send_pointer(struct xrdp_wm* self, int cache_idx,
                     char* data, char* mask, int x, int y);
int APP_CC
xrdp_wm_pointer(struct xrdp_wm* self, char* data, char* mask, int x, int y);
int
callback(long id, int msg, long param1, long param2, long param3, long param4);
int APP_CC
xrdp_wm_delete_all_childs(struct xrdp_wm* self);
int APP_CC
xrdp_wm_idle(struct xrdp_wm* self);
int APP_CC
xrdp_wm_app_sck_signal(struct xrdp_wm* self, int app_sck);
int APP_CC
xrdp_wm_log_msg(struct xrdp_wm* self, char* msg);

/* xrdp_process.c */
struct xrdp_process* APP_CC
xrdp_process_create(struct xrdp_listen* owner);
void APP_CC
xrdp_process_delete(struct xrdp_process* self);
int APP_CC
xrdp_process_main_loop(struct xrdp_process* self);

/* xrdp_listen.c */
struct xrdp_listen* APP_CC
xrdp_listen_create(void);
void APP_CC
xrdp_listen_delete(struct xrdp_listen* self);
int APP_CC
xrdp_listen_delete_pro(struct xrdp_listen* self, struct xrdp_process* pro);
int APP_CC
xrdp_listen_main_loop(struct xrdp_listen* self);

/* xrdp_region.c */
struct xrdp_region* APP_CC
xrdp_region_create(struct xrdp_wm* wm);
void APP_CC
xrdp_region_delete(struct xrdp_region* self);
int APP_CC
xrdp_region_add_rect(struct xrdp_region* self, struct xrdp_rect* rect);
int APP_CC
xrdp_region_insert_rect(struct xrdp_region* self, int i, int left,
                        int top, int right, int bottom);
int APP_CC
xrdp_region_subtract_rect(struct xrdp_region* self,
                          struct xrdp_rect* rect);
int APP_CC
xrdp_region_get_rect(struct xrdp_region* self, int index,
                     struct xrdp_rect* rect);

/* xrdp_bitmap.c */
struct xrdp_bitmap* APP_CC
xrdp_bitmap_create(int width, int height, int bpp,
                   int type, struct xrdp_wm* wm);
struct xrdp_bitmap* APP_CC
xrdp_bitmap_create_with_data(int width, int height,
                             int bpp, char* data,
                             struct xrdp_wm* wm);
void APP_CC
xrdp_bitmap_delete(struct xrdp_bitmap* self);
struct xrdp_bitmap* APP_CC
xrdp_bitmap_get_child_by_id(struct xrdp_bitmap* self, int id);
int APP_CC
xrdp_bitmap_set_focus(struct xrdp_bitmap* self, int focused);
int APP_CC
xrdp_bitmap_resize(struct xrdp_bitmap* self, int width, int height);
int APP_CC
xrdp_bitmap_load(struct xrdp_bitmap* self, const char* filename, int* palette);
int APP_CC
xrdp_bitmap_get_pixel(struct xrdp_bitmap* self, int x, int y);
int APP_CC
xrdp_bitmap_set_pixel(struct xrdp_bitmap* self, int x, int y, int pixel);
int APP_CC
xrdp_bitmap_copy_box(struct xrdp_bitmap* self,
                     struct xrdp_bitmap* dest,
                     int x, int y, int cx, int cy);
int APP_CC
xrdp_bitmap_copy_box_with_crc(struct xrdp_bitmap* self,
                              struct xrdp_bitmap* dest,
                              int x, int y, int cx, int cy);
int APP_CC
xrdp_bitmap_compare(struct xrdp_bitmap* self,
                    struct xrdp_bitmap* b);
int APP_CC
xrdp_bitmap_compare_with_crc(struct xrdp_bitmap* self,
                             struct xrdp_bitmap* b);
int APP_CC
xrdp_bitmap_invalidate(struct xrdp_bitmap* self, struct xrdp_rect* rect);
int APP_CC
xrdp_bitmap_def_proc(struct xrdp_bitmap* self, int msg,
                     int param1, int param2);
int APP_CC
xrdp_bitmap_to_screenx(struct xrdp_bitmap* self, int x);
int APP_CC
xrdp_bitmap_to_screeny(struct xrdp_bitmap* self, int y);
int APP_CC
xrdp_bitmap_from_screenx(struct xrdp_bitmap* self, int x);
int APP_CC
xrdp_bitmap_from_screeny(struct xrdp_bitmap* self, int y);
int APP_CC
xrdp_bitmap_get_screen_clip(struct xrdp_bitmap* self,
                            struct xrdp_painter* painter,
                            struct xrdp_rect* rect,
                            int* dx, int* dy);

/* xrdp_painter.c */
struct xrdp_painter* APP_CC
xrdp_painter_create(struct xrdp_wm* wm, struct xrdp_session* session);
void APP_CC
xrdp_painter_delete(struct xrdp_painter* self);
int APP_CC
xrdp_painter_begin_update(struct xrdp_painter* self);
int APP_CC
xrdp_painter_end_update(struct xrdp_painter* self);
int APP_CC
xrdp_painter_font_needed(struct xrdp_painter* self);
int APP_CC
xrdp_painter_set_clip(struct xrdp_painter* self,
                      int x, int y, int cx, int cy);
int APP_CC
xrdp_painter_clr_clip(struct xrdp_painter* self);
int APP_CC
xrdp_painter_fill_rect(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, int cx, int cy);
int APP_CC
xrdp_painter_draw_bitmap(struct xrdp_painter* self,
                         struct xrdp_bitmap* bitmap,
                         struct xrdp_bitmap* to_draw,
                         int x, int y, int cx, int cy);
int APP_CC
xrdp_painter_text_width(struct xrdp_painter* self, char* text);
int APP_CC
xrdp_painter_text_height(struct xrdp_painter* self, char* text);
int APP_CC
xrdp_painter_draw_text(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, const char* text);
int APP_CC
xrdp_painter_draw_text2(struct xrdp_painter* self,
                        struct xrdp_bitmap* bitmap,
                        int font, int flags, int mixmode,
                        int clip_left, int clip_top,
                        int clip_right, int clip_bottom,
                        int box_left, int box_top,
                        int box_right, int box_bottom,
                        int x, int y, char* data, int data_len);
int APP_CC
xrdp_painter_copy(struct xrdp_painter* self,
                  struct xrdp_bitmap* src,
                  struct xrdp_bitmap* dst,
                  int x, int y, int cx, int cy,
                  int srcx, int srcy);
int APP_CC
xrdp_painter_line(struct xrdp_painter* self,
                  struct xrdp_bitmap* bitmap,
                  int x1, int y1, int x2, int y2);

/* xrdp_font.c */
struct xrdp_font* APP_CC
xrdp_font_create(struct xrdp_wm* wm);
void APP_CC
xrdp_font_delete(struct xrdp_font* self);
int APP_CC
xrdp_font_item_compare(struct xrdp_font_char* font1,
                       struct xrdp_font_char* font2);

/* funcs.c */
int APP_CC
rect_contains_pt(struct xrdp_rect* in, int x, int y);
int APP_CC
rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2,
               struct xrdp_rect* out);
int APP_CC
rect_contained_by(struct xrdp_rect* in1, int left, int top,
                  int right, int bottom);
int APP_CC
check_bounds(struct xrdp_bitmap* b, int* x, int* y, int* cx, int* cy);
int APP_CC
add_char_at(char* text, char ch, int index);
int APP_CC
remove_char_at(char* text, int index);
int APP_CC
set_string(char** in_str, const char* in);

/* in lang.c */
char APP_CC
get_char_from_scan_code(int device_flags, int scan_code, int* keys,
                        int caps_lock, int num_lock, int scroll_lock,
                        int keylayout);

/* xrdp_login_wnd.c */
int APP_CC
xrdp_login_wnd_create(struct xrdp_wm* self);

/* xrdp_bitmap_compress.c */
int APP_CC
xrdp_bitmap_compress(char* in_data, int width, int height,
                     struct stream* s, int bpp, int byte_limit,
                     int start_line, struct stream* temp,
                     int e);

/* xrdp_mm.c */
struct xrdp_mm* APP_CC
xrdp_mm_create(struct xrdp_wm* owner);
void APP_CC
xrdp_mm_delete(struct xrdp_mm* self);
int APP_CC
xrdp_mm_connect(struct xrdp_mm* self);
int APP_CC
xrdp_mm_signal(struct xrdp_mm* self);

int DEFAULT_CC
server_begin_update(struct xrdp_mod* mod);
int DEFAULT_CC
server_end_update(struct xrdp_mod* mod);
int DEFAULT_CC
server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy);
int DEFAULT_CC
server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy);
int DEFAULT_CC
server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy);
int DEFAULT_CC
server_set_pointer(struct xrdp_mod* mod, int x, int y,
                   char* data, char* mask);
int DEFAULT_CC
server_palette(struct xrdp_mod* mod, int* palette);
int DEFAULT_CC
server_msg(struct xrdp_mod* mod, char* msg, int code);
int DEFAULT_CC
server_is_term(struct xrdp_mod* mod);
int DEFAULT_CC
server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy);
int DEFAULT_CC
server_reset_clip(struct xrdp_mod* mod);
int DEFAULT_CC
server_set_fgcolor(struct xrdp_mod* mod, int fgcolor);
int DEFAULT_CC
server_set_bgcolor(struct xrdp_mod* mod, int bgcolor);
int DEFAULT_CC
server_set_opcode(struct xrdp_mod* mod, int opcode);
int DEFAULT_CC
server_set_mixmode(struct xrdp_mod* mod, int mixmode);
int DEFAULT_CC
server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern);
int DEFAULT_CC
server_set_pen(struct xrdp_mod* mod, int style, int width);
int DEFAULT_CC
server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2);
int DEFAULT_CC
server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data);
int DEFAULT_CC
server_draw_text(struct xrdp_mod* mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len);
int DEFAULT_CC
server_reset(struct xrdp_mod* mod, int width, int height, int bpp);
int DEFAULT_CC
server_query_channel(struct xrdp_mod* mod, int index, char* channel_name,
                     int* channel_flags);
int DEFAULT_CC
server_get_channel_id(struct xrdp_mod* mod, char* name);
int DEFAULT_CC
server_send_to_channel(struct xrdp_mod* mod, int channel_id,
                       char* data, int data_len);
