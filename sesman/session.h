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
   Copyright (C) Jay Sorg 2005

   session manager
   linux only

*/

#ifndef SESSION_H
#define SESSION_H

struct session_item
{
  char name[256];
  int pid; /* pid of sesman waiting for wm to end */
  int display;
  int width;
  int height;
  int bpp;
  long data;
};

/******************************************************************************/
struct session_item* DEFAULT_CC
session_find_item(char* name, int width, int height, int bpp);

/******************************************************************************/
/* returns 0 if error else the display number the session was started on */
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data);

#endif

