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
   Copyright (C) Jay Sorg 2004

   main include file

*/

/* check endianess */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define L_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
#define B_ENDIAN
#endif
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__)
#define NEED_ALIGN
#endif
/* include other h files */
#include "parse.h"
#include "xrdp_types.h"
#include "constants.h"
#include "xrdp_defines.h"

/* os_calls.c */
int g_init_system(void);
int g_exit_system(void);
void g_printf(char *format, ...);
void g_hexdump(char* p, int len);
void* g_malloc(int size, int zero);
void* g_malloc1(int size, int zero);
void g_free(void* ptr);
void g_free1(void* ptr);
void g_memset(void* ptr, int val, int size);
void g_memcpy(void* d_ptr, const void* s_ptr, int size);
int g_getchar(void);
int g_tcp_socket(void);
int g_tcp_local_socket(void);
void g_tcp_close(int sck);
int g_tcp_set_non_blocking(int sck);
int g_tcp_bind(int sck, char* port);
int g_tcp_local_bind(int sck, char* port);
int g_tcp_listen(int sck);
int g_tcp_accept(int sck);
int g_tcp_recv(int sck, void* ptr, int len, int flags);
int g_tcp_send(int sck, void* ptr, int len, int flags);
int g_tcp_last_error_would_block(int sck);
int g_tcp_select(int sck1, int sck2);
int g_is_term(void);
void g_set_term(int in_val);
void g_sleep(int msecs);
int g_thread_create(THREAD_RV (THREAD_CC * start_routine)(void*), void* arg);
void* g_rc4_info_create(void);
void g_rc4_info_delete(void* rc4_info);
void g_rc4_set_key(void* rc4_info, char* key, int len);
void g_rc4_crypt(void* rc4_info, char* data, int len);
void* g_sha1_info_create(void);
void g_sha1_info_delete(void* sha1_info);
void g_sha1_clear(void* sha1_info);
void g_sha1_transform(void* sha1_info, char* data, int len);
void g_sha1_complete(void* sha1_info, char* data);
void* g_md5_info_create(void);
void g_md5_info_delete(void* md5_info);
void g_md5_clear(void* md5_info);
void g_md5_transform(void* md5_info, char* data, int len);
void g_md5_complete(void* md5_info, char* data);
int g_mod_exp(char* out, char* in, char* mod, char* exp);
void g_random(char* data, int len);
int g_abs(int i);
int g_memcmp(void* s1, void* s2, int len);
int g_file_open(char* file_name);
int g_file_close(int fd);
int g_file_read(int fd, char* ptr, int len);
int g_file_write(int fd, char* ptr, int len);
int g_file_seek(int fd, int offset);
int g_file_lock(int fd, int start, int len);
int g_strlen(char* text);
char* g_strcpy(char* dest, char* src);
char* g_strncpy(char* dest, char* src, int len);
char* g_strcat(char* dest, char* src);
char* g_strdup(char* in);

/* xrdp_tcp.c */
struct xrdp_tcp* xrdp_tcp_create(struct xrdp_iso* owner);
void xrdp_tcp_delete(struct xrdp_tcp* self);
int xrdp_tcp_init(struct xrdp_tcp* self, struct stream* s);
int xrdp_tcp_recv(struct xrdp_tcp* self, struct stream* s, int len);
int xrdp_tcp_send(struct xrdp_tcp* self, struct stream* s);

/* xrdp_iso.c */
struct xrdp_iso* xrdp_iso_create(struct xrdp_mcs* owner);
void xrdp_iso_delete(struct xrdp_iso* self);
int xrdp_iso_init(struct xrdp_iso* self, struct stream* s);
int xrdp_iso_recv(struct xrdp_iso* self, struct stream* s);
int xrdp_iso_send(struct xrdp_iso* self, struct stream* s);
int xrdp_iso_incoming(struct xrdp_iso* self);

/* xrdp_mcs.c */
struct xrdp_mcs* xrdp_mcs_create(struct xrdp_sec* owner);
void xrdp_mcs_delete(struct xrdp_mcs* self);
int xrdp_mcs_init(struct xrdp_mcs* self, struct stream* s);
int xrdp_mcs_recv(struct xrdp_mcs* self, struct stream* s, int* chan);
int xrdp_mcs_send(struct xrdp_mcs* self, struct stream* s);
int xrdp_mcs_incoming(struct xrdp_mcs* self);
int xrdp_mcs_disconnect(struct xrdp_mcs* self);

/* xrdp_sec.c */
struct xrdp_sec* xrdp_sec_create(struct xrdp_rdp* owner);
void xrdp_sec_delete(struct xrdp_sec* self);
int xrdp_sec_init(struct xrdp_sec* self, struct stream* s);
int xrdp_sec_recv(struct xrdp_sec* self, struct stream* s, int* chan);
int xrdp_sec_send(struct xrdp_sec* self, struct stream* s, int flags);
int xrdp_sec_incoming(struct xrdp_sec* self);
int xrdp_sec_disconnect(struct xrdp_sec* self);

/* xrdp_rdp.c */
struct xrdp_rdp* xrdp_rdp_create(struct xrdp_process* owner);
void xrdp_rdp_delete(struct xrdp_rdp* self);
int xrdp_rdp_init(struct xrdp_rdp* self, struct stream* s);
int xrdp_rdp_init_data(struct xrdp_rdp* self, struct stream* s);
int xrdp_rdp_recv(struct xrdp_rdp* self, struct stream* s, int* code);
int xrdp_rdp_send(struct xrdp_rdp* self, struct stream* s, int pdu_type);
int xrdp_rdp_send_data(struct xrdp_rdp* self, struct stream* s,
                       int data_pdu_type);
int xrdp_rdp_incoming(struct xrdp_rdp* self);
int xrdp_rdp_send_demand_active(struct xrdp_rdp* self);
int xrdp_rdp_process_confirm_active(struct xrdp_rdp* self, struct stream* s);
int xrdp_rdp_process_data(struct xrdp_rdp* self, struct stream* s);
int xrdp_rdp_disconnect(struct xrdp_rdp* self);

/* xrdp_orders.c */
struct xrdp_orders* xrdp_orders_create(struct xrdp_process* owner);
void xrdp_orders_delete(struct xrdp_orders* self);
int xrdp_orders_init(struct xrdp_orders* self);
int xrdp_orders_send(struct xrdp_orders* self);
int xrdp_orders_rect(struct xrdp_orders* self, int x, int y, int cx, int cy,
                     int color, struct xrdp_rect* rect);
int xrdp_orders_screen_blt(struct xrdp_orders* self, int x, int y,
                           int cx, int cy, int srcx, int srcy,
                           int rop, struct xrdp_rect* rect);
int xrdp_orders_pat_blt(struct xrdp_orders* self, int x, int y,
                        int cx, int cy, int rop, int bg_color,
                        int fg_color, struct xrdp_brush* brush,
                        struct xrdp_rect* rect);
int xrdp_orders_dest_blt(struct xrdp_orders* self, int x, int y,
                         int cx, int cy, int rop,
                         struct xrdp_rect* rect);
int xrdp_orders_line(struct xrdp_orders* self, int mix_mode,
                     int startx, int starty,
                     int endx, int endy, int rop, int bg_color,
                     struct xrdp_pen* pen,
                     struct xrdp_rect* rect);
int xrdp_orders_mem_blt(struct xrdp_orders* self, int cache_id,
                        int color_table, int x, int y, int cx, int cy,
                        int rop, int srcx, int srcy,
                        int cache_idx, struct xrdp_rect* rect);
int xrdp_orders_text(struct xrdp_orders* self,
                     int font, int flags, int mixmode,
                     int fg_color, int bg_color,
                     int clip_left, int clip_top,
                     int clip_right, int clip_bottom,
                     int box_left, int box_top,
                     int box_right, int box_bottom,
                     int x, int y, char* data, int data_len,
                     struct xrdp_rect* rect);
int xrdp_orders_send_palette(struct xrdp_orders* self, int* palette,
                             int cache_id);
int xrdp_orders_send_raw_bitmap(struct xrdp_orders* self,
                                struct xrdp_bitmap* bitmap,
                                int cache_id, int cache_idx);
int xrdp_orders_send_font(struct xrdp_orders* self,
                          struct xrdp_font_item* font_item,
                          int font_index, int char_index);

/* xrdp_cache.c */
struct xrdp_cache* xrdp_cache_create(struct xrdp_wm* owner,
                                     struct xrdp_orders* orders);
void xrdp_cache_delete(struct xrdp_cache* self);
int xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap);
int xrdp_cache_add_palette(struct xrdp_cache* self, int* palette);
int xrdp_cache_add_char(struct xrdp_cache* self,
                        struct xrdp_font_item* font_item);

/* xrdp_wm.c */
struct xrdp_wm* xrdp_wm_create(struct xrdp_process* owner);
void xrdp_wm_delete(struct xrdp_wm* self);
int xrdp_wm_send_palette(struct xrdp_wm* self);
int xrdp_wm_init(struct xrdp_wm* self);
int xrdp_wm_send_bitmap(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                        int x, int y, int cx, int cy);
int xrdp_wm_get_vis_region(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                           int x, int y, int cx, int cy,
                           struct xrdp_region* region);
int xrdp_wm_mouse_move(struct xrdp_wm* self, int x, int y);
int xrdp_wm_mouse_click(struct xrdp_wm* self, int x, int y, int but, int down);
int xrdp_wm_key(struct xrdp_wm* self, int device_flags, int scan_code);
int xrdp_wm_key_sync(struct xrdp_wm* self, int device_flags, int key_flags);
int xrdp_wm_pu(struct xrdp_wm* self, struct xrdp_bitmap* control);

/* xrdp_process.c */
struct xrdp_process* xrdp_process_create(struct xrdp_listen* owner);
void xrdp_process_delete(struct xrdp_process* self);
int xrdp_process_main_loop(struct xrdp_process* self);

/* xrdp_listen.c */
struct xrdp_listen* xrdp_listen_create(void);
void xrdp_listen_delete(struct xrdp_listen* self);
int xrdp_listen_delete_pro(struct xrdp_listen* self, struct xrdp_process* pro);
int xrdp_listen_main_loop(struct xrdp_listen* self);

/* xrdp_region.c */
struct xrdp_region* xrdp_region_create(struct xrdp_wm* wm);
void xrdp_region_delete(struct xrdp_region* self);
int xrdp_region_add_rect(struct xrdp_region* self, struct xrdp_rect* rect);
int xrdp_region_insert_rect(struct xrdp_region* self, int i, int left,
                            int top, int right, int bottom);
int xrdp_region_subtract_rect(struct xrdp_region* self,
                              struct xrdp_rect* rect);
int xrdp_region_get_rect(struct xrdp_region* self, int index,
                         struct xrdp_rect* rect);

/* xrdp_bitmap.c */
struct xrdp_bitmap* xrdp_bitmap_create(int width, int height, int bpp,
                                       int type);
void xrdp_bitmap_delete(struct xrdp_bitmap* self);
int xrdp_bitmap_set_focus(struct xrdp_bitmap* self, int focused);
int xrdp_bitmap_load(struct xrdp_bitmap* self, char* filename, int* palette);
int xrdp_bitmap_get_pixel(struct xrdp_bitmap* self, int x, int y);
int xrdp_bitmap_set_pixel(struct xrdp_bitmap* self, int x, int y, int pixel);
int xrdp_bitmap_copy_box(struct xrdp_bitmap* self, struct xrdp_bitmap* dest,
                         int x, int y, int cx, int cy);
int xrdp_bitmap_compare(struct xrdp_bitmap* self, struct xrdp_bitmap* b);
int xrdp_bitmap_invalidate(struct xrdp_bitmap* self, struct xrdp_rect* rect);
int xrdp_bitmap_def_proc(struct xrdp_bitmap* self, int msg,
                         int param1, int param2);
int xrdp_bitmap_to_screenx(struct xrdp_bitmap* self, int x);
int xrdp_bitmap_to_screeny(struct xrdp_bitmap* self, int y);
int xrdp_bitmap_from_screenx(struct xrdp_bitmap* self, int x);
int xrdp_bitmap_from_screeny(struct xrdp_bitmap* self, int y);

/* xrdp_painter.c */
struct xrdp_painter* xrdp_painter_create(struct xrdp_wm* wn);
void xrdp_painter_delete(struct xrdp_painter* self);
int xrdp_painter_begin_update(struct xrdp_painter* self);
int xrdp_painter_end_update(struct xrdp_painter* self);
int xrdp_painter_set_clip(struct xrdp_painter* self,
                          int x, int y, int cx, int cy);
int xrdp_painter_clr_clip(struct xrdp_painter* self);
int xrdp_painter_fill_rect(struct xrdp_painter* self,
                           struct xrdp_bitmap* bitmap,
                           int x, int y, int cx, int cy);
int xrdp_painter_fill_rect2(struct xrdp_painter* self,
                            struct xrdp_bitmap* bitmap,
                            int x, int y, int cx, int cy);
int xrdp_painter_draw_bitmap(struct xrdp_painter* self,
                             struct xrdp_bitmap* bitmap,
                             struct xrdp_bitmap* to_draw,
                             int x, int y, int cx, int cy);
int xrdp_painter_text_width(struct xrdp_painter* self, char* text);
int xrdp_painter_text_height(struct xrdp_painter* self, char* text);
int xrdp_painter_draw_text(struct xrdp_painter* self,
                           struct xrdp_bitmap* bitmap,
                           int x, int y, char* text);

/* xrdp_list.c */
struct xrdp_list* xrdp_list_create(void);
void xrdp_list_delete(struct xrdp_list* self);
void xrdp_list_add_item(struct xrdp_list* self, int item);
int xrdp_list_get_item(struct xrdp_list* self, int index);
void xrdp_list_clear(struct xrdp_list* self);
int xrdp_list_index_of(struct xrdp_list* self, int item);
void xrdp_list_remove_item(struct xrdp_list* self, int index);
void xrdp_list_insert_item(struct xrdp_list* self, int index, int item);

/* xrdp_font.c */
struct xrdp_font* xrdp_font_create(struct xrdp_wm* wm);
void xrdp_font_delete(struct xrdp_font* self);
int xrdp_font_item_compare(struct xrdp_font_item* font1,
                           struct xrdp_font_item* font2);

/* funcs.c */
int rect_contains_pt(struct xrdp_rect* in, int x, int y);
int rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2,
                   struct xrdp_rect* out);
int check_bounds(struct xrdp_bitmap* b, int* x, int* y, int* cx, int* cy);
char get_char_from_scan_code(int device_flags, int scan_code, int* keys,
                             int caps_lock, int num_lock, int scroll_lock);
int add_char_at(char* text, char ch, int index);
int remove_char_at(char* text, int index);
int set_string(char** in_str, char* in);

