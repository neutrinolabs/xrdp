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

   libxrdp header

*/

#if !defined(LIBXRDP_H)
#define LIBXRDP_H

#include "arch.h"
#include "parse.h"
#include "xrdp_constants.h"
#include "defines.h"
#include "os_calls.h"
#include "ssl_calls.h"
#include "list.h"
#include "file.h"
#include "libxrdpinc.h"

/* tcp */
struct xrdp_tcp
{
  int sck;
  int sck_closed;
  struct xrdp_iso* iso_layer; /* owner */
};

/* iso */
struct xrdp_iso
{
  struct xrdp_mcs* mcs_layer; /* owner */
  struct xrdp_tcp* tcp_layer;
};

/* used in mcs */
struct mcs_channel_item
{
  char name[16];
  int flags;
  int chanid;
  struct stream* in_s;
};

/* mcs */
struct xrdp_mcs
{
  struct xrdp_sec* sec_layer; /* owner */
  struct xrdp_iso* iso_layer;
  int userid;
  int chanid;
  struct stream* client_mcs_data;
  struct stream* server_mcs_data;
  struct list* channel_list;
};

/* sec */
struct xrdp_sec
{
  struct xrdp_rdp* rdp_layer; /* owner */
  struct xrdp_mcs* mcs_layer;
  struct xrdp_channel* chan_layer;
  char server_random[32];
  char client_random[64];
  char client_crypt_random[72];
  struct stream client_mcs_data;
  struct stream server_mcs_data;
  int decrypt_use_count;
  int encrypt_use_count;
  char decrypt_key[16];
  char encrypt_key[16];
  char decrypt_update_key[16];
  char encrypt_update_key[16];
  int rc4_key_size; /* 1 = 40 bit, 2 = 128 bit */
  int rc4_key_len; /* 8 = 40 bit, 16 = 128 bit */
  int crypt_level; /* 1, 2, 3 = low, meduim, high */
  char sign_key[16];
  void* decrypt_rc4_info;
  void* encrypt_rc4_info;
  char pub_exp[4];
  char pub_mod[64];
  char pub_sig[64];
  char pri_exp[64];
  int channel_code;
};

/* channel */
struct xrdp_channel
{
  struct xrdp_sec* sec_layer;
  struct xrdp_mcs* mcs_layer;
};

/* rdp */
struct xrdp_rdp
{
  struct xrdp_session* session;
  struct xrdp_sec* sec_layer;
  int share_id;
  int mcs_channel;
  struct xrdp_client_info client_info;
};

/* state */
struct xrdp_orders_state
{
  int last_order; /* last order sent */

  int clip_left;  /* RDP_ORDER_BOUNDS, RDP_ORDER_LASTBOUNDS */
  int clip_top;
  int clip_right;
  int clip_bottom;

  int rect_x; /* RDP_ORDER_RECT */
  int rect_y;
  int rect_cx;
  int rect_cy;
  int rect_color;

  int scr_blt_x; /* RDP_ORDER_SCREENBLT */
  int scr_blt_y;
  int scr_blt_cx;
  int scr_blt_cy;
  int scr_blt_rop;
  int scr_blt_srcx;
  int scr_blt_srcy;

  int pat_blt_x; /* RDP_ORDER_PATBLT */
  int pat_blt_y;
  int pat_blt_cx;
  int pat_blt_cy;
  int pat_blt_rop;
  int pat_blt_bg_color;
  int pat_blt_fg_color;
  struct xrdp_brush pat_blt_brush;

  int dest_blt_x; /* RDP_ORDER_DESTBLT */
  int dest_blt_y;
  int dest_blt_cx;
  int dest_blt_cy;
  int dest_blt_rop;

  int line_mix_mode; /* RDP_ORDER_LINE */
  int line_startx;
  int line_starty;
  int line_endx;
  int line_endy;
  int line_bg_color;
  int line_rop;
  struct xrdp_pen line_pen;

  int mem_blt_color_table; /* RDP_ORDER_MEMBLT */
  int mem_blt_cache_id;
  int mem_blt_x;
  int mem_blt_y;
  int mem_blt_cx;
  int mem_blt_cy;
  int mem_blt_rop;
  int mem_blt_srcx;
  int mem_blt_srcy;
  int mem_blt_cache_idx;

  int text_font; /* RDP_ORDER_TEXT2 */
  int text_flags;
  int text_unknown;
  int text_mixmode;
  int text_fg_color;
  int text_bg_color;
  int text_clip_left;
  int text_clip_top;
  int text_clip_right;
  int text_clip_bottom;
  int text_box_left;
  int text_box_top;
  int text_box_right;
  int text_box_bottom;
  int text_x;
  int text_y;
  int text_len;
  char* text_data;
};

/* orders */
struct xrdp_orders
{
  struct stream* out_s;
  struct xrdp_rdp* rdp_layer;
  struct xrdp_session* session;
  struct xrdp_wm* wm;

  char* order_count_ptr; /* pointer to count, set when sending */
  int order_count;
  int order_level; /* inc for every call to xrdp_orders_init */
  struct xrdp_orders_state orders_state;
};

/* xrdp_tcp.c */
struct xrdp_tcp* APP_CC
xrdp_tcp_create(struct xrdp_iso* owner, int sck);
void APP_CC
xrdp_tcp_delete(struct xrdp_tcp* self);
int APP_CC
xrdp_tcp_init(struct xrdp_tcp* self, struct stream* s);
int APP_CC
xrdp_tcp_recv(struct xrdp_tcp* self, struct stream* s, int len);
int APP_CC
xrdp_tcp_send(struct xrdp_tcp* self, struct stream* s);

/* xrdp_iso.c */
struct xrdp_iso* APP_CC
xrdp_iso_create(struct xrdp_mcs* owner, int sck);
void APP_CC
xrdp_iso_delete(struct xrdp_iso* self);
int APP_CC
xrdp_iso_init(struct xrdp_iso* self, struct stream* s);
int APP_CC
xrdp_iso_recv(struct xrdp_iso* self, struct stream* s);
int APP_CC
xrdp_iso_send(struct xrdp_iso* self, struct stream* s);
int APP_CC
xrdp_iso_incoming(struct xrdp_iso* self);

/* xrdp_mcs.c */
struct xrdp_mcs* APP_CC
xrdp_mcs_create(struct xrdp_sec* owner, int sck,
                struct stream* client_mcs_data,
                struct stream* server_mcs_data);
void APP_CC
xrdp_mcs_delete(struct xrdp_mcs* self);
int APP_CC
xrdp_mcs_init(struct xrdp_mcs* self, struct stream* s);
int APP_CC
xrdp_mcs_recv(struct xrdp_mcs* self, struct stream* s, int* chan);
int APP_CC
xrdp_mcs_send(struct xrdp_mcs* self, struct stream* s, int chan);
int APP_CC
xrdp_mcs_incoming(struct xrdp_mcs* self);
int APP_CC
xrdp_mcs_disconnect(struct xrdp_mcs* self);

/* xrdp_sec.c */
struct xrdp_sec* APP_CC
xrdp_sec_create(struct xrdp_rdp* owner, int sck, int crypt_level,
                int channel_code);
void APP_CC
xrdp_sec_delete(struct xrdp_sec* self);
int APP_CC
xrdp_sec_init(struct xrdp_sec* self, struct stream* s);
int APP_CC
xrdp_sec_recv(struct xrdp_sec* self, struct stream* s, int* chan);
int APP_CC
xrdp_sec_send(struct xrdp_sec* self, struct stream* s, int chan);
int APP_CC
xrdp_sec_process_mcs_data(struct xrdp_sec* self);
int APP_CC
xrdp_sec_out_mcs_data(struct xrdp_sec* self);
int APP_CC
xrdp_sec_incoming(struct xrdp_sec* self);
int APP_CC
xrdp_sec_disconnect(struct xrdp_sec* self);

/* xrdp_rdp.c */
struct xrdp_rdp* APP_CC
xrdp_rdp_create(struct xrdp_session* session, int sck);
void APP_CC
xrdp_rdp_delete(struct xrdp_rdp* self);
int APP_CC
xrdp_rdp_init(struct xrdp_rdp* self, struct stream* s);
int APP_CC
xrdp_rdp_init_data(struct xrdp_rdp* self, struct stream* s);
int APP_CC
xrdp_rdp_recv(struct xrdp_rdp* self, struct stream* s, int* code);
int APP_CC
xrdp_rdp_send(struct xrdp_rdp* self, struct stream* s, int pdu_type);
int APP_CC
xrdp_rdp_send_data(struct xrdp_rdp* self, struct stream* s,
                   int data_pdu_type);
int APP_CC
xrdp_rdp_send_data_update_sync(struct xrdp_rdp* self);
int APP_CC
xrdp_rdp_incoming(struct xrdp_rdp* self);
int APP_CC
xrdp_rdp_send_demand_active(struct xrdp_rdp* self);
int APP_CC
xrdp_rdp_process_confirm_active(struct xrdp_rdp* self, struct stream* s);
int APP_CC
xrdp_rdp_process_data(struct xrdp_rdp* self, struct stream* s);
int APP_CC
xrdp_rdp_disconnect(struct xrdp_rdp* self);
int APP_CC
xrdp_rdp_send_deactive(struct xrdp_rdp* self);

/* xrdp_orders.c */
struct xrdp_orders* APP_CC
xrdp_orders_create(struct xrdp_session* session,
                   struct xrdp_rdp* rdp_layer);
void APP_CC
xrdp_orders_delete(struct xrdp_orders* self);
int APP_CC
xrdp_orders_reset(struct xrdp_orders* self);
int APP_CC
xrdp_orders_init(struct xrdp_orders* self);
int APP_CC
xrdp_orders_send(struct xrdp_orders* self);
int APP_CC
xrdp_orders_force_send(struct xrdp_orders* self);
int APP_CC
xrdp_orders_rect(struct xrdp_orders* self, int x, int y, int cx, int cy,
                 int color, struct xrdp_rect* rect);
int APP_CC
xrdp_orders_screen_blt(struct xrdp_orders* self, int x, int y,
                       int cx, int cy, int srcx, int srcy,
                       int rop, struct xrdp_rect* rect);
int APP_CC
xrdp_orders_pat_blt(struct xrdp_orders* self, int x, int y,
                    int cx, int cy, int rop, int bg_color,
                    int fg_color, struct xrdp_brush* brush,
                    struct xrdp_rect* rect);
int APP_CC
xrdp_orders_dest_blt(struct xrdp_orders* self, int x, int y,
                     int cx, int cy, int rop,
                     struct xrdp_rect* rect);
int APP_CC
xrdp_orders_line(struct xrdp_orders* self, int mix_mode,
                 int startx, int starty,
                 int endx, int endy, int rop, int bg_color,
                 struct xrdp_pen* pen,
                 struct xrdp_rect* rect);
int APP_CC
xrdp_orders_mem_blt(struct xrdp_orders* self, int cache_id,
                    int color_table, int x, int y, int cx, int cy,
                    int rop, int srcx, int srcy,
                    int cache_idx, struct xrdp_rect* rect);
int APP_CC
xrdp_orders_text(struct xrdp_orders* self,
                 int font, int flags, int mixmode,
                 int fg_color, int bg_color,
                 int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len,
                 struct xrdp_rect* rect);
int APP_CC
xrdp_orders_send_palette(struct xrdp_orders* self, int* palette,
                         int cache_id);
int APP_CC
xrdp_orders_send_raw_bitmap(struct xrdp_orders* self,
                            int width, int height, int bpp, char* data,
                            int cache_id, int cache_idx);
int APP_CC
xrdp_orders_send_bitmap(struct xrdp_orders* self,
                        int width, int height, int bpp, char* data,
                        int cache_id, int cache_idx);
int APP_CC
xrdp_orders_send_font(struct xrdp_orders* self,
                      struct xrdp_font_char* font_char,
                      int font_index, int char_index);
int APP_CC
xrdp_orders_send_raw_bitmap2(struct xrdp_orders* self,
                             int width, int height, int bpp, char* data,
                             int cache_id, int cache_idx);
int APP_CC
xrdp_orders_send_bitmap2(struct xrdp_orders* self,
                         int width, int height, int bpp, char* data,
                         int cache_id, int cache_idx);

/* xrdp_bitmap_compress.c */
int APP_CC
xrdp_bitmap_compress(char* in_data, int width, int height,
                     struct stream* s, int bpp, int byte_limit,
                     int start_line, struct stream* temp,
                     int e);

/* xrdp_channel.c */
struct xrdp_channel* APP_CC
xrdp_channel_create(struct xrdp_sec* owner, struct xrdp_mcs* mcs_layer);
void APP_CC
xrdp_channel_delete(struct xrdp_channel* self);
int APP_CC
xrdp_channel_init(struct xrdp_channel* self, struct stream* s);
int APP_CC
xrdp_channel_send(struct xrdp_channel* self, struct stream* s, int channel_id);
int APP_CC
xrdp_channel_process(struct xrdp_channel* self, struct stream* s,
                     int chanid);

#endif
