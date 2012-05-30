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
   Copyright (C) Jay Sorg 2004-2010

   types

*/
#define DEFAULT_STRING_LEN 255
#define LOG_WINDOW_CHAR_PER_LINE 60

/* lib */
struct xrdp_mod
{
  int size; /* size of this struct */
  int version; /* internal version */
  /* client functions */
  int (*mod_start)(struct xrdp_mod* v, int w, int h, int bpp);
  int (*mod_connect)(struct xrdp_mod* v);
  int (*mod_event)(struct xrdp_mod* v, int msg, long param1, long param2,
                   long param3, long param4);
  int (*mod_signal)(struct xrdp_mod* v);
  int (*mod_end)(struct xrdp_mod* v);
  int (*mod_set_param)(struct xrdp_mod* v, char* name, char* value);
  int (*mod_session_change)(struct xrdp_mod* v, int, int);
  int (*mod_get_wait_objs)(struct xrdp_mod* v, tbus* read_objs, int* rcount,
                           tbus* write_objs, int* wcount, int* timeout);
  int (*mod_check_wait_objs)(struct xrdp_mod* v);
  long mod_dumby[100 - 9]; /* align, 100 minus the number of mod 
                              functions above */
  /* server functions */
  int (*server_begin_update)(struct xrdp_mod* v);
  int (*server_end_update)(struct xrdp_mod* v);
  int (*server_fill_rect)(struct xrdp_mod* v, int x, int y, int cx, int cy);
  int (*server_screen_blt)(struct xrdp_mod* v, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(struct xrdp_mod* v, int x, int y, int cx, int cy,
                           char* data, int width, int height, int srcx, int srcy);
  int (*server_set_pointer)(struct xrdp_mod* v, int x, int y, char* data, char* mask);
  int (*server_palette)(struct xrdp_mod* v, int* palette);
  int (*server_msg)(struct xrdp_mod* v, char* msg, int code);
  int (*server_is_term)(struct xrdp_mod* v);
  int (*server_set_clip)(struct xrdp_mod* v, int x, int y, int cx, int cy);
  int (*server_reset_clip)(struct xrdp_mod* v);
  int (*server_set_fgcolor)(struct xrdp_mod* v, int fgcolor);
  int (*server_set_bgcolor)(struct xrdp_mod* v, int bgcolor);
  int (*server_set_opcode)(struct xrdp_mod* v, int opcode);
  int (*server_set_mixmode)(struct xrdp_mod* v, int mixmode);
  int (*server_set_brush)(struct xrdp_mod* v, int x_orgin, int y_orgin,
                          int style, char* pattern);
  int (*server_set_pen)(struct xrdp_mod* v, int style,
                        int width);
  int (*server_draw_line)(struct xrdp_mod* v, int x1, int y1, int x2, int y2);
  int (*server_add_char)(struct xrdp_mod* v, int font, int charactor,
                         int offset, int baseline,
                         int width, int height, char* data);
  int (*server_draw_text)(struct xrdp_mod* v, int font,
                          int flags, int mixmode, int clip_left, int clip_top,
                          int clip_right, int clip_bottom,
                          int box_left, int box_top,
                          int box_right, int box_bottom,
                          int x, int y, char* data, int data_len);
  int (*server_reset)(struct xrdp_mod* v, int width, int height, int bpp);
  int (*server_query_channel)(struct xrdp_mod* v, int index,
                              char* channel_name,
                              int* channel_flags);
  int (*server_get_channel_id)(struct xrdp_mod* v, char* name);
  int (*server_send_to_channel)(struct xrdp_mod* v, int channel_id,
                                char* data, int data_len,
                                int total_data_len, int flags);
  int (*server_bell_trigger)(struct xrdp_mod* v);
  int (*server_create_os_surface)(struct xrdp_mod* v, int rdpindex,
                                  int width, int height);
  int (*server_switch_os_surface)(struct xrdp_mod* v, int rdpindex);
  int (*server_delete_os_surface)(struct xrdp_mod* v, int rdpindex);
  int (*server_paint_rect_os)(struct xrdp_mod* mod, int x, int y,
                              int cx, int cy,
                              int rdpindex, int srcx, int srcy);
  int (*server_set_hints)(struct xrdp_mod* mod, int hints, int mask);
  long server_dumby[100 - 30]; /* align, 100 minus the number of server
                                  functions above */
  /* common */
  long handle; /* pointer to self as int */
  long wm; /* struct xrdp_wm* */
  long painter;
  int sck;
};

/* header for bmp file */
struct xrdp_bmp_header
{
  int size;
  int image_width;
  int image_height;
  short planes;
  short bit_count;
  int compression;
  int image_size;
  int x_pels_per_meter;
  int y_pels_per_meter;
  int clr_used;
  int clr_important;
};

struct xrdp_palette_item
{
  int stamp;
  int palette[256];
};

struct xrdp_bitmap_item
{
  int stamp;
  struct xrdp_bitmap* bitmap;
};

struct xrdp_os_bitmap_item
{
  int id;
  struct xrdp_bitmap* bitmap;
};

struct xrdp_char_item
{
  int stamp;
  struct xrdp_font_char font_item;
};

struct xrdp_pointer_item
{
  int stamp;
  int x; /* hotspot */
  int y;
  char data[32 * 32 * 3];
  char mask[32 * 32 / 8];
};

struct xrdp_brush_item
{
  int stamp;
  /* expand this to a structure to handle more complicated brushes
     for now its 8x8 1bpp brushes only */
  char pattern[8];
};

/* differnce caches */
struct xrdp_cache
{
  struct xrdp_wm* wm; /* owner */
  struct xrdp_session* session;
  /* palette */
  int palette_stamp;
  struct xrdp_palette_item palette_items[6];
  /* bitmap */
  int bitmap_stamp;
  struct xrdp_bitmap_item bitmap_items[3][2000];
  int use_bitmap_comp;
  int cache1_entries;
  int cache1_size;
  int cache2_entries;
  int cache2_size;
  int cache3_entries;
  int cache3_size;
  int bitmap_cache_persist_enable;
  int bitmap_cache_version;
  /* font */
  int char_stamp;
  struct xrdp_char_item char_items[12][256];
  /* pointer */
  int pointer_stamp;
  struct xrdp_pointer_item pointer_items[32];
  int pointer_cache_entries;
  int brush_stamp;
  struct xrdp_brush_item brush_items[64];
  struct xrdp_os_bitmap_item os_bitmap_items[2000];
  struct list* xrdp_os_del_list;
};

struct xrdp_mm
{
  struct xrdp_wm* wm; /* owner */
  int connected_state; /* true if connected to sesman else false */
  struct trans* sesman_trans; /* connection to sesman */
  int sesman_trans_up; /* true once connected to sesman */
  int delete_sesman_trans; /* boolean set when done with sesman connection */
  struct list* login_names;
  struct list* login_values;
  /* mod vars */
  long mod_handle; /* returned from g_load_library */
  struct xrdp_mod* (*mod_init)(void);
  int (*mod_exit)(struct xrdp_mod*);
  struct xrdp_mod* mod; /* module interface */
  int display; /* 10 for :10.0, 11 for :11.0, etc */
  int code; /* 0 Xvnc session 10 X11rdp session */
  int sesman_controlled; /* true if this is a sesman session */
  struct trans* chan_trans; /* connection to chansrv */
  int chan_trans_up; /* true once connected to chansrv */
  int delete_chan_trans; /* boolean set when done with channel connection */
  int usechansrv; /* true if chansrvport is set in xrdp.ini or using sesman */
};

struct xrdp_key_info
{
  int sym;
  int chr;
};

struct xrdp_keymap
{
  struct xrdp_key_info keys_noshift[256];
  struct xrdp_key_info keys_shift[256];
  struct xrdp_key_info keys_altgr[256];
  struct xrdp_key_info keys_capslock[256];
  struct xrdp_key_info keys_shiftcapslock[256];
};

/* the window manager */
struct xrdp_wm
{
  struct xrdp_process* pro_layer; /* owner */
  struct xrdp_bitmap* screen;
  struct xrdp_session* session;
  struct xrdp_painter* painter;
  struct xrdp_cache* cache;
  int palette[256];
  struct xrdp_bitmap* login_window;
  /* generic colors */
  int black;
  int grey;
  int dark_grey;
  int blue;
  int dark_blue;
  int white;
  int red;
  int green;
  int background;
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
  /* popup for combo box */
  struct xrdp_bitmap* popup_wnd;
  /* focused window */
  struct xrdp_bitmap* focused_window;
  /* pointer */
  int current_pointer;
  int mouse_x;
  int mouse_y;
  /* keyboard info */
  int keys[256]; /* key states 0 up 1 down*/
  int caps_lock;
  int scroll_lock;
  int num_lock;
  /* client info */
  struct xrdp_client_info* client_info;
  /* session log */
  struct list* log;
  struct xrdp_bitmap* log_wnd;
  int login_mode;
  tbus login_mode_event;
  struct xrdp_mm* mm;
  struct xrdp_font* default_font;
  struct xrdp_keymap keymap;
  int hide_log_window;
  struct xrdp_bitmap* target_surface; /* either screen or os surface */
  int current_surface_index;
  int hints;
};

/* rdp process */
struct xrdp_process
{
  int status;
  struct trans* server_trans; /* in tcp server mode */
  tbus self_term_event;
  struct xrdp_listen* lis_layer; /* owner */
  struct xrdp_session* session;
  /* create these when up and running */
  struct xrdp_wm* wm;
  //int app_sck;
  tbus done_event;
  int session_id;
};

/* rdp listener */
struct xrdp_listen
{
  int status;
  struct trans* listen_trans; /* in tcp listen mode */
  struct list* process_list;
  tbus pro_done_event;
  struct xrdp_startup_params* startup_params;
};

/* region */
struct xrdp_region
{
  struct xrdp_wm* wm; /* owner */
  struct list* rects;
};

/* painter */
struct xrdp_painter
{
  int rop;
  struct xrdp_rect* use_clip; /* nil if not using clip */
  struct xrdp_rect clip;
  int clip_children;
  int bg_color;
  int fg_color;
  int mix_mode;
  struct xrdp_brush brush;
  struct xrdp_pen pen;
  struct xrdp_session* session;
  struct xrdp_wm* wm; /* owner */
  struct xrdp_font* font;
};

/* window or bitmap */
struct xrdp_bitmap
{
  /* 0 = bitmap 1 = window 2 = screen 3 = button 4 = image 5 = edit
     6 = label 7 = combo 8 = special */
  int type;
  int width;
  int height;
  struct xrdp_wm* wm;
  /* msg 1 = click 2 = mouse move 3 = paint 100 = modal result */
  /* see messages in constants.h */
  int (*notify)(struct xrdp_bitmap* wnd, struct xrdp_bitmap* sender,
                int msg, long param1, long param2);
  /* for bitmap */
  int bpp;
  int line_size; /* in bytes */
  int do_not_free_data;
  char* data;
  /* for all but bitmap */
  int left;
  int top;
  int pointer;
  int bg_color;
  int tab_stop;
  int id;
  char* caption1;
  /* for window or screen */
  struct xrdp_bitmap* modal_dialog;
  struct xrdp_bitmap* focused_control;
  struct xrdp_bitmap* owner; /* window that created us */
  struct xrdp_bitmap* parent; /* window contained in */
  /* for modal dialog */
  struct xrdp_bitmap* default_button; /* button when enter is pressed */
  struct xrdp_bitmap* esc_button; /* button when esc is pressed */
  /* list of child windows */
  struct list* child_list;
  /* for edit */
  int edit_pos;
  twchar password_char;
  /* for button or combo */
  int state; /* for button 0 = normal 1 = down */
  /* for combo */
  struct list* string_list;
  struct list* data_list;
  /* for combo or popup */
  int item_index;
  /* for popup */
  struct xrdp_bitmap* popped_from;
  int item_height;
  /* crc */
  int crc;
};

#define NUM_FONTS 0x4e00
#define DEFAULT_FONT_NAME "sans-10.fv1"

#define DEFAULT_ELEMENT_TOP   35
#define DEFAULT_BUTTON_W      60
#define DEFAULT_BUTTON_H      23
#define DEFAULT_COMBO_W       210
#define DEFAULT_COMBO_H       21
#define DEFAULT_EDIT_W        210
#define DEFAULT_EDIT_H        21
#define DEFAULT_WND_LOGIN_W   500
#define DEFAULT_WND_LOGIN_H   250
#define DEFAULT_WND_HELP_W    340
#define DEFAULT_WND_HELP_H    300
#define DEFAULT_WND_LOG_W     400
#define DEFAULT_WND_LOG_H     400
#define DEFAULT_WND_SPECIAL_H 100

/* font */
struct xrdp_font
{
  struct xrdp_wm* wm;
  struct xrdp_font_char font_items[NUM_FONTS];
  char name[32];
  int size;
  int style;
};

/* module */
struct xrdp_mod_data
{
  struct list* names;
  struct list* values;
};

struct xrdp_startup_params
{
  char port[128];
  int kill;
  int no_daemon;
  int help;
  int version;
  int fork;
};
