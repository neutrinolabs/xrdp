/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   fb calls
   Copyright (C) Jay Sorg 2006

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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "uimain.h"

extern char g_username[];
extern char g_hostname[];
extern char g_servername[];
extern char g_password[];
extern char g_shell[];
extern char g_directory[];
extern char g_domain[];
extern int g_width;
extern int g_height;
extern int g_tcp_sck;
extern int g_server_depth;
extern int g_tcp_port_rdp; /* in tcp.c */
extern int g_bytes_in;
extern int pal_entries[];

extern char * g_bs;

struct cursor
{
  unsigned char andmask[32 * 32];
  unsigned char xormask[32 * 32];
  int x;
  int y;
  int w;
  int h;
};
static struct cursor g_mcursor; /* current mouse */
static int g_mouse_x = 0;
static int g_mouse_y = 0;

static int g_wfpx = 0; /* wait for pixel stuff */
static int g_wfpy = 0;
static int g_wfpv = 0;
static int g_show_wfp = 0;
static int g_no_draw = 0; /* this means don't draw the screen but draw on
                             backingstore */

/* for transparent colour */
static int g_use_trans = 0;
static int g_trans_colour = 0;

/*****************************************************************************/
void
mi_error(char * msg)
{
  printf(msg);
}

/*****************************************************************************/
void
mi_warning(char * msg)
{
  printf(msg);
}

/*****************************************************************************/
int
mi_read_keyboard_state(void)
{
  return 0;
}

/*****************************************************************************/
/* returns non zero if ok */
int
mi_create_window(void)
{
  return 1;
}

/*****************************************************************************/
void
mi_update_screen(void)
{
}

/*****************************************************************************/
int
mi_main_loop(void)
{
  return 0;
}

/*****************************************************************************/
void
mi_add_to(int x, int y, int cx, int cy)
{
/*
  int right;
  int bottom;

  if (g_rect.h == 0 && g_rect.w == 0)
  {
    g_rect.x = x;
    g_rect.y = y;
    g_rect.w = cx;
    g_rect.h = cy;
  }
  else
  {
    right = g_rect.x + g_rect.w;
    bottom = g_rect.y + g_rect.h;
    if (x + cx > right)
    {
      right = x + cx;
    }
    if (y + cy > bottom)
    {
      bottom = y + cy;
    }
    if (x < g_rect.x)
    {
      g_rect.x = x;
    }
    if (y < g_rect.y)
    {
      g_rect.y = y;
    }
    g_rect.w = right - g_rect.x;
    g_rect.h = bottom - g_rect.y;
  }
*/
}

/*****************************************************************************/
void
mi_invalidate(int x, int y, int cx, int cy)
{
  mi_add_to(x, y, cx, cy);
  mi_update_screen();
}

/*****************************************************************************/
/* return boolean */
int
mi_create_bs(void)
{
  return 1;
}

/*****************************************************************************/
void
mi_begin_update(void)
{
}

/*****************************************************************************/
void
mi_end_update(void)
{
}

/*****************************************************************************/
void
mi_fill_rect(int x, int y, int cx, int cy, int colour)
{
  if (g_no_draw)
  {
    return;
  }
}

/*****************************************************************************/
void
mi_line(int x1, int y1, int x2, int y2, int colour)
{
  if (g_no_draw)
  {
    return;
  }
}

/*****************************************************************************/
void
mi_screen_copy(int x, int y, int cx, int cy, int srcx, int srcy)
{
  if (g_no_draw)
  {
    return;
  }
}

/*****************************************************************************/
void
mi_set_clip(int x, int y, int cx, int cy)
{
}

/*****************************************************************************/
void
mi_reset_clip(void)
{
}

/*****************************************************************************/
void *
mi_create_cursor(unsigned int x, unsigned int y,
                 int width, int height,
                 unsigned char * andmask, unsigned char * xormask)
{
  return (void *) 1;
}

/*****************************************************************************/
void
mi_destroy_cursor(void * cursor)
{
}

/*****************************************************************************/
void
mi_set_cursor(void * cursor)
{
}

/*****************************************************************************/
void
mi_set_null_cursor(void)
{
}

/*****************************************************************************/
static void
out_params(void)
{
  fprintf(stderr, "rdesktop: A Remote Desktop Protocol client.\n");
  fprintf(stderr, "Version 1.4.1. Copyright (C) 1999-2006 Matt Chapman.\n");
  fprintf(stderr, "direct framebuffer uiport by Jay Sorg\n");
  fprintf(stderr, "See http://www.rdesktop.org/ for more information.\n\n");
  fprintf(stderr, "Usage: dfbrdesktop [options] server\n");
  fprintf(stderr, "   -u: user name\n");
  fprintf(stderr, "   -n: client hostname\n");
  fprintf(stderr, "   -s: shell\n");
  fprintf(stderr, "   -p: password\n");
  fprintf(stderr, "   -d: domain\n");
  fprintf(stderr, "   -c: working directory\n");
  fprintf(stderr, "   -a: colour depth\n");
  fprintf(stderr, "   -wfp x y pixel: skip screen updates till x, y pixel is \
this colour\n");
  fprintf(stderr, "   -trans pixel: transparent colour\n");
  fprintf(stderr, "\n");
}

/*****************************************************************************/
static int
parse_parameters(int in_argc, char ** in_argv)
{
  int i;

  if (in_argc <= 1)
  {
    out_params();
    return 0;
  }
  for (i = 1; i < in_argc; i++)
  {
    strcpy(g_servername, in_argv[i]);
    if (strcmp(in_argv[i], "-h") == 0)
    {
      out_params();
      return 0;
    }
    else if (strcmp(in_argv[i], "-u") == 0)
    {
      strcpy(g_username, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-n") == 0)
    {
      strcpy(g_hostname, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-s") == 0)
    {
      strcpy(g_shell, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-p") == 0)
    {
      strcpy(g_password, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-d") == 0)
    {
      strcpy(g_domain, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-c") == 0)
    {
      strcpy(g_directory, in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-a") == 0)
    {
      g_server_depth = atoi(in_argv[i + 1]);
    }
    else if (strcmp(in_argv[i], "-wfp") == 0)
    {
      g_wfpx = atoi(in_argv[i + 1]);
      g_wfpy = atoi(in_argv[i + 2]);
      g_wfpv = atoi(in_argv[i + 3]);
      if (g_wfpv == 0)
      {
        g_show_wfp = 1;
      }
      else
      {
        g_no_draw = 1;
      }
    }
    else if (strcmp(in_argv[i], "-trans") == 0)
    {
      g_use_trans = 1;
      g_trans_colour = atoi(in_argv[i + 1]);
    }
  }
  return 1;
}

/*****************************************************************************/
int
main(int argc, char ** argv)
{
  int rv;

  rv = 0;
  return rv;
}

/*****************************************************************************/
/* returns non zero ok */
int
librdesktop_init(long obj1, long obj2, long obj3, int in_argc, char ** in_argv)
{
  return 1;
}

/*****************************************************************************/
/* returns non zero ok */
int
librdesktop_connect(void)
{
  return ui_lib_main();
}

/*****************************************************************************/
/* returns non zero ok */
int
librdesktop_check_wire(void)
{
  fd_set rfds;
  int rv;
  int fd;
  struct timeval tv;

  fd = g_tcp_sck;
  FD_ZERO(&rfds);
  FD_SET(g_tcp_sck, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  rv = select(fd + 1, &rfds, 0, 0, &tv);
  if (rv > -1)
  {
    if (rv > 0)
    {
      if (FD_ISSET(g_tcp_sck, &rfds))
      {
        if (!ui_read_wire())
        {
          return 0;
        }
      }
    }
  }
  return 1;
}

/*****************************************************************************/
int
librdesktop_mouse_move(int x, int y)
{
  ui_mouse_move(x, y);
  return 0;
}

/*****************************************************************************/
int
librdesktop_mouse_button(int button, int x, int y, int down)
{
  ui_mouse_button(button, x, y, down);
  return 0;
}

/*****************************************************************************/
int
librdesktop_key_down(int key, int ext)
{
  ui_key_down(key, ext);
  return 0;
}

/*****************************************************************************/
int
librdesktop_key_up(int key, int ext)
{
  ui_key_up(key, ext);
  return 0;
}

/*****************************************************************************/
int
librdesktop_quit(void)
{
  return 1;
}
