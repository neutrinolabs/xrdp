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
   Copyright (C) Jay Sorg 2005-2007

   librdp main header file

*/

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "ssl_calls.h"
#include "xrdp_constants.h"
#include "defines.h"

struct rdp_brush
{
  int xorigin;
  int yorigin;
  int style;
  char pattern[8];
};

struct rdp_pen
{
  int style;
  int width;
  int color;
};

struct rdp_colormap
{
  int ncolors;
  int colors[256];
};

struct rdp_bitmap
{
  int width;
  int height;
  int bpp;
  char* data;
};

struct rdp_cursor
{
  int x;
  int y;
  int width;
  int height;
  char mask[(32 * 32) / 8];
  char data[(32 * 32) * 3];
};

/* tcp */
struct rdp_tcp
{
  int sck;
  int sck_closed;
  struct rdp_iso* iso_layer; /* owner */
};

/* iso */
struct rdp_iso
{
  struct rdp_mcs* mcs_layer; /* owner */
  struct rdp_tcp* tcp_layer;
};

/* mcs */
struct rdp_mcs
{
  struct rdp_sec* sec_layer; /* owner */
  struct rdp_iso* iso_layer;
  int userid;
  struct stream* client_mcs_data;
  struct stream* server_mcs_data;
};

/* sec */
struct rdp_sec
{
  struct rdp_rdp* rdp_layer; /* owner */
  struct rdp_mcs* mcs_layer;
  struct rdp_lic* lic_layer;
  char server_random[32];
  char client_random[64];
  char client_crypt_random[72];
  struct stream* client_mcs_data;
  struct stream* server_mcs_data;
  int decrypt_use_count;
  int encrypt_use_count;
  char decrypt_key[16];
  char encrypt_key[16];
  char decrypt_update_key[16];
  char encrypt_update_key[16];
  int rc4_key_size; /* 1 = 40-bit, 2 = 128-bit */
  int rc4_key_len; /* 8 or 16 */
  int crypt_level; /* 1 = low, 2 = medium, 3 = high */
  char sign_key[16];
  void* decrypt_rc4_info;
  void* encrypt_rc4_info;
};

/* licence */
struct rdp_lic
{
  struct rdp_sec* sec_layer; /* owner */
  char licence_key[16];
  char licence_sign_key[16];
  int licence_issued;
};

/* rdp */
struct rdp_rdp
{
  struct mod* mod;
  struct rdp_sec* sec_layer;
  struct rdp_orders* orders;
  int share_id;
  int use_rdp5;
  int bitmap_compression;
  int bitmap_cache;
  int desktop_save;
  int polygon_ellipse_orders;
  int rec_mode;
  int rec_fd;
  /* cache */
  struct rdp_colormap colormap;
  struct rdp_cursor cursors[32];
};

struct rdp_orders_state
{
  /* order stuff */
  int order_type;
  /* clip state */
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;
  /* text order state */
  int text_font;
  int text_flags;
  int text_opcode;
  int text_mixmode;
  int text_fgcolor;
  int text_bgcolor;
  int text_clipleft;
  int text_cliptop;
  int text_clipright;
  int text_clipbottom;
  int text_boxleft;
  int text_boxtop;
  int text_boxright;
  int text_boxbottom;
  struct rdp_brush text_brush;
  int text_x;
  int text_y;
  int text_length;
  char text_text[256];
  /* destblt order state */
  int dest_x;
  int dest_y;
  int dest_cx;
  int dest_cy;
  int dest_opcode;
  /* patblt order state */
  int pat_x;
  int pat_y;
  int pat_cx;
  int pat_cy;
  int pat_opcode;
  int pat_bgcolor;
  int pat_fgcolor;
  struct rdp_brush pat_brush;
  /* screenblt order state */
  int screenblt_x;
  int screenblt_y;
  int screenblt_cx;
  int screenblt_cy;
  int screenblt_opcode;
  int screenblt_srcx;
  int screenblt_srcy;
  /* line order state */
  int line_mixmode;
  int line_startx;
  int line_starty;
  int line_endx;
  int line_endy;
  int line_bgcolor;
  int line_opcode;
  struct rdp_pen line_pen;
  /* rect order state */
  int rect_x;
  int rect_y;
  int rect_cx;
  int rect_cy;
  int rect_color;
  /* desksave order state */
  int desksave_offset;
  int desksave_left;
  int desksave_top;
  int desksave_right;
  int desksave_bottom;
  int desksave_action;
  /* memblt order state */
  int memblt_cache_id;
  int memblt_color_table;
  int memblt_x;
  int memblt_y;
  int memblt_cx;
  int memblt_cy;
  int memblt_opcode;
  int memblt_srcx;
  int memblt_srcy;
  int memblt_cache_idx;
  /* polyline order state */
  int polyline_x;
  int polyline_y;
  int polyline_opcode;
  int polyline_fgcolor;
  int polyline_lines;
  int polyline_datasize;
  char polyline_data[256];
};

/* orders */
struct rdp_orders
{
  struct rdp_rdp* rdp_layer;
  /* order state */
  struct rdp_orders_state state;
  /* cache */
  struct rdp_colormap* cache_colormap[6];
  struct rdp_bitmap* cache_bitmap[3][600];
};

struct mod
{
  int size; /* size of this struct */
  int version; /* internal version */
  /* client functions */
  int (*mod_start)(struct mod* v, int w, int h, int bpp);
  int (*mod_connect)(struct mod* v);
  int (*mod_event)(struct mod* v, int msg, long param1, long param2,
                   long param3, long param4);
  int (*mod_signal)(struct mod* v);
  int (*mod_end)(struct mod* v);
  int (*mod_set_param)(struct mod* v, char* name, char* value);
  long mod_dumby[100 - 6]; /* align, 100 minus the number of mod 
                              functions above */
  /* server functions */
  int (*server_begin_update)(struct mod* v);
  int (*server_end_update)(struct mod* v);
  int (*server_fill_rect)(struct mod* v, int x, int y, int cx, int cy);
  int (*server_screen_blt)(struct mod* v, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(struct mod* v, int x, int y, int cx, int cy,
                           char* data, int width, int height, int srcx, int srcy);
  int (*server_set_cursor)(struct mod* v, int x, int y, char* data, char* mask);
  int (*server_palette)(struct mod* v, int* palette);
  int (*server_msg)(struct mod* v, char* msg, int code);
  int (*server_is_term)(struct mod* v);
  int (*server_set_clip)(struct mod* v, int x, int y, int cx, int cy);
  int (*server_reset_clip)(struct mod* v);
  int (*server_set_fgcolor)(struct mod* v, int fgcolor);
  int (*server_set_bgcolor)(struct mod* v, int bgcolor);
  int (*server_set_opcode)(struct mod* v, int opcode);
  int (*server_set_mixmode)(struct mod* v, int mixmode);
  int (*server_set_brush)(struct mod* v, int x_orgin, int y_orgin,
                          int style, char* pattern);
  int (*server_set_pen)(struct mod* v, int style,
                        int width);
  int (*server_draw_line)(struct mod* v, int x1, int y1, int x2, int y2);
  int (*server_add_char)(struct mod* v, int font, int charactor,
                         int offset, int baseline,
                         int width, int height, char* data);
  int (*server_draw_text)(struct mod* v, int font,
                          int flags, int mixmode, int clip_left, int clip_top,
                          int clip_right, int clip_bottom,
                          int box_left, int box_top,
                          int box_right, int box_bottom,
                          int x, int y, char* data, int data_len);
  int (*server_reset)(struct mod* v, int width, int height, int bpp);
  int (*server_query_channel)(struct mod* v, int index,
                              char* channel_name,
                              int* channel_flags);
  int (*server_get_channel_id)(struct mod* v, char* name);
  int (*server_send_to_channel)(struct mod* v, int channel_id,
                                char* data, int data_len);
  long server_dumby[100 - 24]; /* align, 100 minus the number of server 
                                  functions above */
  /* common */
  long handle; /* pointer to self as long */
  long wm;
  long painter;
  int sck;
  /* mod data */
  struct rdp_rdp* rdp_layer;
  int width;
  int height;
  int rdp_bpp;
  int xrdp_bpp;
  char ip[256];
  char port[256];
  char username[256];
  char password[256];
  char hostname[256];
  char domain[256];
  char program[256];
  char directory[256];
  int keylayout;
  int up_and_running;
  struct stream* in_s;
};

/* rdp_tcp.c */
struct rdp_tcp* APP_CC
rdp_tcp_create(struct rdp_iso* owner);
void APP_CC
rdp_tcp_delete(struct rdp_tcp* self);
int APP_CC
rdp_tcp_init(struct rdp_tcp* self, struct stream* s);
int APP_CC
rdp_tcp_recv(struct rdp_tcp* self, struct stream* s, int len);
int APP_CC
rdp_tcp_send(struct rdp_tcp* self, struct stream* s);
int APP_CC
rdp_tcp_connect(struct rdp_tcp* self, char* ip, char* port);
int APP_CC
rdp_tcp_disconnect(struct rdp_tcp* self);

/* rdp_ico.c */
struct rdp_iso* APP_CC
rdp_iso_create(struct rdp_mcs* owner);
void APP_CC
rdp_iso_delete(struct rdp_iso* self);
int APP_CC
rdp_iso_recv(struct rdp_iso* self, struct stream* s);
int APP_CC
rdp_iso_init(struct rdp_iso* self, struct stream* s);
int APP_CC
rdp_iso_send(struct rdp_iso* self, struct stream* s);
int APP_CC
rdp_iso_connect(struct rdp_iso* self, char* ip, char* port);
int APP_CC
rdp_iso_disconnect(struct rdp_iso* self);

/* rdp_mcs.c */
struct rdp_mcs* APP_CC
rdp_mcs_create(struct rdp_sec* owner,
               struct stream* client_mcs_data,
               struct stream* server_mcs_data);
void APP_CC
rdp_mcs_delete(struct rdp_mcs* self);
int APP_CC
rdp_mcs_init(struct rdp_mcs* self, struct stream* s);
int APP_CC
rdp_mcs_send(struct rdp_mcs* self, struct stream* s);
int APP_CC
rdp_mcs_connect(struct rdp_mcs* self, char* ip, char* port);
int APP_CC
rdp_mcs_recv(struct rdp_mcs* self, struct stream* s, int* chan);

/* rdp_sec.c */
struct rdp_sec* APP_CC
rdp_sec_create(struct rdp_rdp* owner);
void APP_CC
rdp_sec_delete(struct rdp_sec* self);
int APP_CC
rdp_sec_init(struct rdp_sec* self, struct stream* s, int flags);
int APP_CC
rdp_sec_send(struct rdp_sec* self, struct stream* s, int flags);
int APP_CC
rdp_sec_recv(struct rdp_sec* self, struct stream* s, int* chan);
int APP_CC
rdp_sec_connect(struct rdp_sec* self, char* ip, char* port);
void APP_CC
rdp_sec_buf_out_uint32(char* buffer, int value);
void APP_CC
rdp_sec_hash_16(char* out, char* in, char* salt1, char* salt2);
void APP_CC
rdp_sec_hash_48(char* out, char* in, char* salt1, char* salt2, int salt);
void APP_CC
rdp_sec_sign(char* signature, int siglen, char* session_key, int keylen,
             char* data, int datalen);

/* rdp_rdp.c */
struct rdp_rdp* APP_CC
rdp_rdp_create(struct mod* owner);
void APP_CC
rdp_rdp_delete(struct rdp_rdp* self);
int APP_CC
rdp_rdp_connect(struct rdp_rdp* self, char* ip, char* port);
int APP_CC
rdp_rdp_send_input(struct rdp_rdp* self, struct stream* s,
                   int time, int message_type,
                   int device_flags, int param1, int param2);
int APP_CC
rdp_rdp_send_invalidate(struct rdp_rdp* self, struct stream* s,
                        int left, int top, int width, int height);
int APP_CC
rdp_rdp_recv(struct rdp_rdp* self, struct stream* s, int* type);
int APP_CC
rdp_rdp_process_data_pdu(struct rdp_rdp* self, struct stream* s);
int APP_CC
rdp_rdp_process_demand_active(struct rdp_rdp* self, struct stream* s);
void APP_CC
rdp_rdp_out_unistr(struct stream* s, char* text);
int APP_CC
rdp_rec_check_file(struct rdp_rdp* self);
int APP_CC
rdp_rec_write_item(struct rdp_rdp* self, struct stream* s);

/* rdp_bitmap.c */
int APP_CC
rdp_bitmap_decompress(char* output, int width, int height, char* input,
                      int size, int Bpp);

/* rdp_orders.c */
struct rdp_orders* APP_CC
rdp_orders_create(struct rdp_rdp* owner);
void APP_CC
rdp_orders_delete(struct rdp_orders* self);
void APP_CC
rdp_orders_reset_state(struct rdp_orders* self);
int APP_CC
rdp_orders_process_orders(struct rdp_orders* self, struct stream* s,
                          int num_orders);
char* APP_CC
rdp_orders_convert_bitmap(int in_bpp, int out_bpp, char* bmpdata,
                          int width, int height, int* palette);
int APP_CC
rdp_orders_convert_color(int in_bpp, int out_bpp, int in_color, int* palette);

/* rdp_lic.c */
struct rdp_lic* APP_CC
rdp_lic_create(struct rdp_sec* owner);
void APP_CC
rdp_lic_delete(struct rdp_lic* self);
void APP_CC
rdp_lic_process(struct rdp_lic* self, struct stream* s);
