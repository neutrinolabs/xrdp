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

   types

*/

/* for memory debugging */
struct xrdp_mem
{
  int size;
  int id;
};

/* header for bmp file */
struct xrdp_bmp_header
{
  long size;
  long image_width;
  long image_height;
  short planes;
  short bit_count;
  long compression;
  long image_size;
  long x_pels_per_meter;
  long y_pels_per_meter;
  long clr_used;
  long clr_important;
};

/* list */
struct xrdp_list
{
  int* items;
  int count;
  int alloc_size;
  int grow_by;
  int auto_free;
};

/* rect */
struct xrdp_rect
{
  int left;
  int top;
  int right;
  int bottom;
};

/* bounds */
struct xrdp_bounds
{
  int x;
  int y;
  int cx;
  int cy;
};

/* brush */
struct xrdp_brush
{
  int x_orgin;
  int y_orgin;
  int style;
  char pattern[8];
};

/* pen */
struct xrdp_pen
{
  int style;
  int width;
  int color;
};

/* tcp */
struct xrdp_tcp
{
  int sck;
  struct xrdp_iso* iso_layer; /* owner */
};

/* iso */
struct xrdp_iso
{
  struct xrdp_mcs* mcs_layer; /* owner */
  struct xrdp_tcp* tcp_layer;
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
};

/* sec */
struct xrdp_sec
{
  struct xrdp_rdp* rdp_layer; /* owner */
  struct xrdp_mcs* mcs_layer;
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
  int rc4_key_size;
  int rc4_key_len;
  char sign_key[16];
  void* decrypt_rc4_info;
  void* encrypt_rc4_info;
};

/* rdp */
struct xrdp_rdp
{
  struct xrdp_process* pro_layer; /* owner */
  struct xrdp_sec* sec_layer;
  int share_id;
  int mcs_channel;
  int bpp;
  int width;
  int height;
  int up_and_running;
};

/* orders */
struct xrdp_orders
{
  struct stream* out_s;
  struct xrdp_rdp* rdp_layer;
  struct xrdp_process* pro_layer; /* owner */

  char* order_count_ptr; /* pointer to count, set when sending */
  int order_count;
  int order_level; /* inc for every call to xrdp_orders_init */

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

struct xrdp_palette_item
{
  int use_count;
  int palette[256];
};

struct xrdp_bitmap_item
{
  int use_count;
  struct xrdp_bitmap* bitmap;
};

struct xrdp_font_item
{
  int offset;
  int baseline;
  int width;
  int height;
  int incby;
  char* data;
};

struct xrdp_char_item
{
  int use_count;
  struct xrdp_font_item font_item;
};

/* differnce caches */
struct xrdp_cache
{
  struct xrdp_wm* wm; /* owner */
  struct xrdp_orders* orders;
  struct xrdp_palette_item palette_items[6];
  struct xrdp_bitmap_item bitmap_items[3][600];
  struct xrdp_char_item char_items[12][256];
};

/* the window manager */
struct xrdp_wm
{
  struct xrdp_process* pro_layer; /* owner */
  struct xrdp_bitmap* screen;
  struct xrdp_orders* orders;
  struct xrdp_painter* painter;
  struct xrdp_rdp* rdp_layer;
  struct xrdp_cache* cache;
  int palette[256];
  struct xrdp_bitmap* login_window;
  /* generic colors */
  int black;
  int grey;
  int dark_grey;
  int blue;
  int white;
  int red;
  int green;
  /* dragging info */
  int dragging;
  int draggingx;
  int draggingy;
  int draggingcx;
  int draggingcy;
  int draggingdx;
  int draggingdy;
  int draggingorgx;
  int draggingorgy;
  int draggingxorstate;
  struct xrdp_bitmap* dragging_window;
  /* the down(clicked) button */
  struct xrdp_bitmap* button_down;
  /* focused window */
  struct xrdp_bitmap* focused_window;
  /* cursor */
  int current_cursor;
};

/* rdp process */
struct xrdp_process
{
  int status;
  int sck;
  int term;
  struct xrdp_listen* lis_layer; /* owner */
  struct xrdp_rdp* rdp_layer;
  /* create these when up and running */
  struct xrdp_orders* orders;
  struct xrdp_wm* wm;
};

/* rdp listener */
struct xrdp_listen
{
  int status;
  int sck;
  int term;
  struct xrdp_process* process_list[100]; /* 100 processes possible */
  int process_list_count;
  int process_list_max;
};

/* region */
struct xrdp_region
{
  struct xrdp_wm* wm; /* owner */
  struct xrdp_list* rects;
};

/* painter */
struct xrdp_painter
{
  int rop;
  int use_clip;
  struct xrdp_rect clip;
  int bg_color;
  int fg_color;
  struct xrdp_brush brush;
  struct xrdp_orders* orders;
  struct xrdp_wm* wm; /* owner */
  struct xrdp_font* font;
};

/* window or bitmap */
struct xrdp_bitmap
{
  int type; /* 0 = bitmap 1 = window 2 = screen 3 = button 4 = image 5 = edit 6 = label */
  int state; /* for button 0 = normal 1 = down */
  int id;
  char* data;
  int width;
  int height;
  int bpp;
  int left;
  int top;
  int bg_color;
  int line_size; /* in bytes */
  int focused;
  char title[256];
  struct xrdp_bitmap* modal_dialog;
  struct xrdp_bitmap* owner; /* window that created us */
  struct xrdp_bitmap* parent; /* window contained in */
  struct xrdp_list* child_list;
  struct xrdp_wm* wm;
  int cursor;
  /* msg 1 = click 2 = mouse move 3 = paint 100 = modal result */
  int (*notify)(struct xrdp_bitmap* wnd, struct xrdp_bitmap* sender,
                int msg, int param1, int param2);
};

/* font */
struct xrdp_font
{
  struct xrdp_wm* wm;
  struct xrdp_font_item font_items[256];
  int color;
  char name[32];
  int size;
  int style;
};
