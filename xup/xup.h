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
   Copyright (C) Jay Sorg 2005-2006

   libxup main header file

*/

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"

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
  int width;
  int height;
  int bpp;
  int sck_closed;
  char username[256];
  char password[256];
  char ip[256];
  char port[256];
};
