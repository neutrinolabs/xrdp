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
   Copyright (C) Jay Sorg 2004-2005

   libvnc

*/

/* include other h files */
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "d3des.h"

struct vnc
{
  int size; /* size of this struct */
  /* client functions */
  int (*mod_start)(struct vnc* v, int w, int h, int bpp);
  int (*mod_connect)(struct vnc* v);
  int (*mod_event)(struct vnc* v, int msg, long param1, long param2,
                   long param3, long param4);
  int (*mod_signal)(struct vnc* v);
  int (*mod_end)(struct vnc* v);
  int (*mod_set_param)(struct vnc* v, char* name, char* value);
  /* server functions */
  int (*server_begin_update)(struct vnc* v);
  int (*server_end_update)(struct vnc* v);
  int (*server_fill_rect)(struct vnc* v, int x, int y, int cx, int cy,
                          int color);
  int (*server_screen_blt)(struct vnc* v, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(struct vnc* v, int x, int y, int cx, int cy,
                           char* data);
  int (*server_set_cursor)(struct vnc* v, int x, int y, char* data, char* mask);
  int (*server_palette)(struct vnc* v, int* palette);
  int (*server_msg)(struct vnc* v, char* msg);
  int (*server_is_term)(struct vnc* v);
  /* common */
  long handle; /* pointer to self as long */
  long wm;
  long painter;
  int sck;
  /* mod data */
  int server_width;
  int server_height;
  int server_bpp;
  int mod_width;
  int mod_height;
  int mod_bpp;
  char mod_name[256];
  int mod_mouse_state;
  int palette[256];
  int vnc_desktop;
  char username[256];
  char password[256];
  char ip[256];
  char port[256];
  int sck_closed;
  int shift_state; /* 0 up, 1 down */
};
