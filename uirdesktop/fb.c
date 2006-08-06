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
#include <fcntl.h>
#include <termios.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include "uimain.h"
#include "bsops.h"

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

extern int g_bs_bpp;
extern int g_bs_Bpp;
extern char * g_bs;


static int g_bpp = 8;
static int g_Bpp = 1;

/* keys */
struct key
{
 int scancode;
 int rdpcode;
 int ext;
};
static struct key g_keys[256];
static char g_keyfile[64] = "./default.key";

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
static int g_mouse_buttons = 0; /* mouse button states */
static int g_mousefd = 0; /* mouse fd */
static int g_mouse_state = 0; /* used when reading mouse device */

static int g_uts = 0; /* updates to skip */
static int g_alt_down = 0; /* used to disable control alt delete */
static int g_control_down = 0;
static int g_shift_down = 0;
static int g_disable_cad = 0; /* disable control alt delete */
                             /* and ctrl shift esc */
static int g_wfpx = 0; /* wait for pixel stuff */
static int g_wfpy = 0;
static int g_wfpv = 0;
static int g_show_wfp = 0;
static int g_no_draw = 0; /* this means don't draw the screen but draw on
                             backingstore */
/* for transparent colour */
static int g_use_trans = 0;
static int g_trans_colour = 0;


/* clip */
static int g_clip_left = 0;
static int g_clip_top = 0;
static int g_clip_right = 0;
static int g_clip_bottom = 0;

static int g_kbfd = 0; /* keyboard fd */

static int g_fbfd = 0; /* framebuffer fd */
static char * g_sdata = 0;
static struct fb_var_screeninfo g_vinfo;
static struct fb_fix_screeninfo g_finfo;

static short g_saved_red[256]; /* original hw palette */
static short g_saved_green[256];
static short g_saved_blue[256];

struct my_rect
{
  int x;
  int y;
  int w;
  int h;
};
static struct my_rect g_rect = {0, 0, 0, 0};

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
  int i;
  int j;
  int endi;
  int endj;
  int x;
  int y;
  int pixel;
  int r;
  int g;
  int b;

  endi = UI_MIN(g_rect.y + g_rect.h, g_clip_bottom);
  endj = UI_MIN(g_rect.x + g_rect.w, g_clip_right);
  x = UI_MAX(g_rect.x, g_clip_left);
  y = UI_MAX(g_rect.y, g_clip_top);
  //printf("hi %d %d %d %d\n", x, y, endi, endj);
  if (g_bpp == 16 && g_bs_bpp == 32)
  {
    for (i = y; i < endi; i++)
    {
      for (j = x; j < endj; j++)
      {
        pixel = ((unsigned int *) g_bs)[i * g_width + j];
        SPLIT_COLOUR32(pixel, b, g, r);
        MAKE_COLOUR16(pixel, r, g, b);
        ((unsigned short *) g_sdata)[i * g_width + j] = pixel;
      }
    }
  }
  g_rect.x = 0;
  g_rect.y = 0;
  g_rect.w = 0;
  g_rect.h = 0;
  //printf("bye\n");
}

/*****************************************************************************/
static void
process_keyboard(void)
{
  char buf[128];
  unsigned char ch;
  int count;
  int index;
  int keyup;
  int rdpkey;
  int ext;

  ext = 0;
  index = 0;
  count = read(g_kbfd, buf, 128);
  while (index < count)
  {
    ch = (unsigned char)buf[index];
    //printf("%2.2x\n", ch);
    keyup = ch & 0x80;
    rdpkey = ch & 0x7f;
    ext = g_keys[rdpkey].ext ? 0x100 : 0;
    rdpkey = g_keys[rdpkey].rdpcode;
    if (rdpkey == 0x1d) /* control */
    {
      g_control_down = !keyup;
    }
    if (rdpkey == 0x38) /* alt */
    {
      g_alt_down = !keyup;
    }
    if (rdpkey == 0x2a || rdpkey == 0x36) /* shift */
    {
      g_shift_down = !keyup;
    }
    if (g_disable_cad) /* diable control alt delete and control shift escape */
    {
      if (rdpkey == 0x53 && g_alt_down && g_control_down) /* delete */
      {
        rdpkey = 0;
      }
      if (rdpkey == 0x01 && g_shift_down && g_control_down) /* escape */
      {
        rdpkey = 0;
      }
    }
    if (rdpkey > 0 && g_mouse_buttons == 0)
    {
      if (!keyup)
      {
        ui_key_down(rdpkey, ext);
      }
      else
      {
        ui_key_up(rdpkey, ext);
      }
    }
    index++;
  }
}

/*****************************************************************************/
static int
process_mouse(void)
{
  char d[128];
  int c;
  int i;
  int b;
  int old_x;
  int old_y;
  int old_but1;
  int old_but2;
  int mouse_x; /* hot spot */
  int mouse_y; /* hot spot */

  mouse_x = g_mouse_x + g_mcursor.x;
  mouse_y = g_mouse_y + g_mcursor.y;
  old_x = mouse_x;
  old_y = mouse_y;
  old_but1 = g_mouse_buttons & 1;
  old_but2 = g_mouse_buttons & 2;
  c = read(g_mousefd, &d, 128);
  for (i = 0; i < c; i++)
  {
    b = (unsigned char)d[i];
    switch (g_mouse_state)
    {
      case 0:
        if (b & 0x08) /* PS2_CTRL_BYTE */
        {
          g_mouse_buttons = b & (1 | 2);
          g_mouse_state = 1;
        }
        break;
      case 1: /* x */
        if (b > 127)
        {
          b -= 256;
        }
        mouse_x += b;
        if (mouse_x < 0)
        {
          mouse_x = 0;
        }
        else if (mouse_x >= g_width)
        {
          mouse_x = g_width - 1;
        }
        g_mouse_state = 2;
        break;
      case 2: /* y */
        if (b > 127)
        {
          b -= 256;
        }
        mouse_y += -b;
        if (mouse_y < 0)
        {
          mouse_y = 0;
        }
        else if (mouse_y >= g_height)
        {
          mouse_y = g_height - 1;
        }
        g_mouse_state = 0;
        break;
    }
  }
  if (old_x != mouse_x || old_y != mouse_y) /* mouse pos changed */
  {
    ui_mouse_move(mouse_x, mouse_y);
  }
  if (old_but1 != (g_mouse_buttons & 1)) /* left button changed */
  {
    if (g_mouse_buttons & 1)
    {
      ui_mouse_button(1, mouse_x, mouse_y, 1);
    }
    else
    {
      ui_mouse_button(1, mouse_x, mouse_y, 0);
    }
  }
  if (old_but2 != (g_mouse_buttons & 2)) /* right button changed */
  {
    if (g_mouse_buttons & 2)
    {
      ui_mouse_button(2, mouse_x, mouse_y, 1);
    }
    else
    {
      ui_mouse_button(2, mouse_x, mouse_y, 0);
    }
  }
  //undraw_mouse();
  g_mouse_x = mouse_x - g_mcursor.x;
  g_mouse_y = mouse_y - g_mcursor.y;
  //draw_mouse();
  return 0;
}

/*****************************************************************************/
int
mi_main_loop(void)
{
  fd_set rfds;
//  fd_set wfds;
  int rv;
  int fd;
  struct timeval tv;

  fd = UI_MAX(g_tcp_sck, UI_MAX(g_mousefd, g_kbfd));
  FD_ZERO(&rfds);
  FD_SET(g_tcp_sck, &rfds);
  FD_SET(g_mousefd, &rfds);
  FD_SET(g_kbfd, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  rv = select(fd + 1, &rfds, 0, 0, &tv);
  while (rv > -1)
  {
    if (rv == 0)
    {
      usleep(0);
    }
    if (FD_ISSET(g_kbfd, &rfds))
    {
      process_keyboard();
    }
    if (FD_ISSET(g_mousefd, &rfds))
    {
      process_mouse();
    }
    if (FD_ISSET(g_tcp_sck, &rfds))
    {
      if (!ui_read_wire())
      {
        return 0;
      }
    }
    fd = UI_MAX(g_tcp_sck, UI_MAX(g_mousefd, g_kbfd));
    FD_ZERO(&rfds);
    FD_SET(g_tcp_sck, &rfds);
    FD_SET(g_mousefd, &rfds);
    FD_SET(g_kbfd, &rfds);
#ifdef WITH_RDPSND
//    if (g_rdpsnd && g_dsp_busy)
//    {
//      fd = MAX(fd, g_dsp_fd);
//      FD_ZERO(&wfds);
//      FD_SET(g_dsp_fd, &wfds);
//      rv = select(fd + 1, &rfds, &wfds, 0, 0);
//      if (rv > 0 && FD_ISSET(g_dsp_fd, &wfds))
//      {
//        wave_out_play();
//      }
//    }
//    else
//    {
//      rv = select(fd + 1, &rfds, 0, 0, 0);
//    }
#else
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    rv = select(fd + 1, &rfds, 0, 0, &tv);
#endif
  }
  return 0;
}

/*****************************************************************************/
void
mi_add_to(int x, int y, int cx, int cy)
{
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
  mi_add_to(x, y, cx, cy);
  mi_update_screen();
}

/*****************************************************************************/
void
mi_line(int x1, int y1, int x2, int y2, int colour)
{
  if (g_no_draw)
  {
    return;
  }
  int x;
  int y;
  int cx;
  int cy;

  x = UI_MIN(x1, x2);
  y = UI_MIN(y1, y2);
  cx = (UI_MAX(x1, x2) + 1) - x;
  cy = (UI_MAX(y1, y2) + 1) - y;
  mi_add_to(x, y, cx, cy);
  mi_update_screen();
}

/*****************************************************************************/
void
mi_screen_copy(int x, int y, int cx, int cy, int srcx, int srcy)
{
  if (g_no_draw)
  {
    return;
  }
  mi_add_to(x, y, cx, cy);
  mi_update_screen();
}

/*****************************************************************************/
void
mi_set_clip(int x, int y, int cx, int cy)
{
  g_clip_left = x;
  g_clip_top = y;
  g_clip_right = x + cx;
  g_clip_bottom = y + cy;
  g_clip_left = UI_MAX(g_clip_left, 0);
  g_clip_top = UI_MAX(g_clip_top, 0);
  g_clip_right = UI_MIN(g_clip_right, g_width);
  g_clip_bottom = UI_MIN(g_clip_bottom, g_height);
}

/*****************************************************************************/
void
mi_reset_clip(void)
{
  g_clip_left = 0;
  g_clip_top = 0;
  g_clip_right = g_width;
  g_clip_bottom = g_height;
}

/*****************************************************************************/
void *
mi_create_cursor(unsigned int x, unsigned int y,
                 int width, int height,
                 unsigned char * andmask, unsigned char * xormask)
{
  struct cursor * c;
  int i;
  int j;

  c = (struct cursor *) malloc(sizeof(struct cursor));
  memset(c, 0, sizeof(struct cursor));
  c->w = width;
  c->h = height;
  c->x = x;
  c->y = y;
  for (i = 0; i < 32; i++)
  {
    for (j = 0; j < 32; j++)
    {
      if (bs_is_pixel_on(andmask, j, i, 32, 1))
      {
        bs_set_pixel_on(c->andmask, j, 31 - i, 32, 8, 255);
      }
      if (bs_is_pixel_on(xormask, j, i, 32, 1))
      {
        bs_set_pixel_on(c->xormask, j, 31 - i, 32, 8, 255);
      }
    }
  }
  return c;
}

/*****************************************************************************/
void
mi_destroy_cursor(void * cursor)
{
   struct cursor * c;

  c = (struct cursor *) cursor;
  free(c);
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
static void
get_username_and_hostname(void)
{
  char fullhostname[64];
  char * p;
  struct passwd * pw;

  strncpy(g_username, "unknown", 255);
  strncpy(g_hostname, "unknown", 255);
  pw = getpwuid(getuid());
  if (pw != 0)
  {
    if (pw->pw_name != 0)
    {
      strncpy(g_username, pw->pw_name, 255);
    }
  }
  if (gethostname(fullhostname, sizeof(fullhostname)) != -1)
  {
    p = strchr(fullhostname, '.');
    if (p != 0)
    {
      *p = 0;
    }
    strncpy(g_hostname, fullhostname, 255);
  }
}

/*****************************************************************************/
static void
save_palette(void)
{
  struct fb_cmap cmap;

  cmap.start = 0;
  if (g_bpp == 15)
  {
    cmap.len = 16;
  }
  else
  {
    cmap.len = 256;
  }
  cmap.red = (unsigned short *) g_saved_red;
  cmap.green = (unsigned short *) g_saved_green;
  cmap.blue = (unsigned short *) g_saved_blue;
  cmap.transp = 0;
  ioctl(g_fbfd, FBIOGETCMAP, &cmap);
}

/*****************************************************************************/
static void
restore_palette(void)
{
  struct fb_cmap cmap;

  cmap.start = 0;
  if (g_bpp == 15)
  {
    cmap.len = 16;
  }
  else
  {
    cmap.len = 256;
  }
  cmap.red = (unsigned short *) g_saved_red;
  cmap.green = (unsigned short *) g_saved_green;
  cmap.blue = (unsigned short *) g_saved_blue;
  cmap.transp = 0;
  ioctl(g_fbfd, FBIOPUTCMAP, &cmap);
}

/*****************************************************************************/
static void
set_directcolor_palette(void)
{
  short r[256];
  int i;
  struct fb_cmap cmap;

  if (g_bpp == 15)
  {
    for (i = 0; i < 32; i++)
    {
      r[i] = i << 11;
    }
    cmap.len = 32;
  }
  else
  {
    for (i = 0; i < 256; i++)
    {
      r[i] = i << 8;
    }
    cmap.len = 256;
  }
  cmap.start = 0;
  cmap.red = (unsigned short *) r;
  cmap.green = (unsigned short *) r;
  cmap.blue = (unsigned short *) r;
  cmap.transp = 0;
  ioctl(g_fbfd, FBIOPUTCMAP, &cmap);
}

/*****************************************************************************/
static int
htoi(char * val)
{
  int rv;

  rv = 0;
  switch (val[0])
  {
    case '1': rv = 16; break;
    case '2': rv = 16 * 2; break;
    case '3': rv = 16 * 3; break;
    case '4': rv = 16 * 4; break;
    case '5': rv = 16 * 5; break;
    case '6': rv = 16 * 6; break;
    case '7': rv = 16 * 7; break;
    case '8': rv = 16 * 8; break;
    case '9': rv = 16 * 9; break;
    case 'a': rv = 16 * 10; break;
    case 'b': rv = 16 * 11; break;
    case 'c': rv = 16 * 12; break;
    case 'd': rv = 16 * 13; break;
    case 'e': rv = 16 * 14; break;
    case 'f': rv = 16 * 15; break;
  }
  switch (val[1])
  {
    case '1': rv += 1; break;
    case '2': rv += 2; break;
    case '3': rv += 3; break;
    case '4': rv += 4; break;
    case '5': rv += 5; break;
    case '6': rv += 6; break;
    case '7': rv += 7; break;
    case '8': rv += 8; break;
    case '9': rv += 9; break;
    case 'a': rv += 10; break;
    case 'b': rv += 11; break;
    case 'c': rv += 12; break;
    case 'd': rv += 13; break;
    case 'e': rv += 14; break;
    case 'f': rv += 15; break;
  }
  return rv;
}


/*****************************************************************************/
static int
load_keys(void)
{
  int fd;
  int len;
  int index;
  int i1;
  int comment;
  int val1;
  int val2;
  int val3;
  char all_lines[8192];
  char line[256];
  char val[4];

  memset(g_keys, 0, sizeof(g_keys));
  fd = open(g_keyfile, O_RDWR);
  if (fd > 0)
  {
    i1 = 0;
    line[0] = 0;
    comment = 0;
    len = read(fd, all_lines, 8192);
    for (index = 0; index < len ; index++)
    {
      if (all_lines[index] == '#')
      {
        comment = 1;
      }
      else if (all_lines[index] == 13 || all_lines[index] == 10)
      {
        if (strlen(line) > 7)
        {
          val[0] = line[0];
          val[1] = line[1];
          val[2] = 0;
          val1 = htoi(val);
          val[0] = line[3];
          val[1] = line[4];
          val[2] = 0;
          val2 = htoi(val);
          val[0] = line[6];
          val[1] = line[7];
          val[2] = 0;
          val3 = htoi(val);
          g_keys[val1].scancode = val1;
          g_keys[val1].rdpcode = val2;
          g_keys[val1].ext = val3;
        }
        line[0] = 0;
        i1 = 0;
        comment = 0;
      }
      else if (!comment)
      {
        line[i1] = all_lines[index];
        i1++;
        line[i1] = 0;
      }
    }
    close(fd);
  }
  return 0;
}

/*****************************************************************************/
int
main(int in_argc, char ** in_argv)
{
  int rv;
  int screensize;
  struct termios new_termios;

  rv = 0;
  g_server_depth = 24;
  memset(&g_mcursor, 0, sizeof(struct cursor));
  get_username_and_hostname();
  /* read command line options */
  if (!parse_parameters(in_argc, in_argv))
  {
    exit(0);
  }
  /* Open the file for reading and writing */
  g_fbfd = open("/dev/fb0", O_RDWR);
  if (g_fbfd == -1)
  {
    printf("Error: cannot open framebuffer device.\n");
    exit(101);
  }
  printf("The framebuffer device was opened successfully.\n");
  /* Get fixed screen information */
  if (ioctl(g_fbfd, FBIOGET_FSCREENINFO, &g_finfo))
  {
    printf("Error reading fixed information.\n");
    exit(102);
  }
  /* Get variable screen information */
  if (ioctl(g_fbfd, FBIOGET_VSCREENINFO, &g_vinfo))
  {
    printf("Error reading variable information.\n");
    exit(103);
  }
  g_bpp = g_vinfo.bits_per_pixel;
  g_Bpp = (g_bpp + 7) / 8;
  g_width = g_vinfo.xres;
  g_height = g_vinfo.yres;
  g_clip_right = g_width;
  g_clip_bottom = g_height;
  printf("%dx%d, %dbpp\n", g_vinfo.xres, g_vinfo.yres, g_vinfo.bits_per_pixel);
  /* Figure out the size of the screen in bytes */
  screensize = g_vinfo.xres * g_vinfo.yres * g_Bpp;
  /* Map the device to memory */
  g_sdata = (char *) mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
                          g_fbfd, 0);
  g_bs = malloc(screensize);
  if ((int) g_sdata == -1)
  {
    printf("Error: failed to map framebuffer device to memory.\n");
    exit(104);
  }
  printf("The framebuffer device was mapped to memory successfully.\n");
  /* open mouse */
  g_mousefd = open("/dev/mouse", O_RDWR);
  if (g_mousefd == -1)
  {
    g_mousefd = open("/dev/psaux", O_RDWR);
  }
  if (g_mousefd == -1)
  {
    printf("Error: failed to open /dev/mouse or /dev/psaux\n");
    exit(105);
  }
  g_kbfd = open("/dev/tty0", O_RDWR);
  if (g_kbfd == -1)
  {
    printf("Error: failed to open /dev/tty0\n");
    exit(106);
  }
  /* check fb type */
  if (g_finfo.visual != FB_VISUAL_DIRECTCOLOR &&
      g_finfo.visual != FB_VISUAL_TRUECOLOR)
  {
    printf("unsupports fb\n");
    exit(107);
  }
  if (g_finfo.visual == FB_VISUAL_DIRECTCOLOR)
  {
    /* save hardware palette */
    save_palette();
    /* set palette to match truecolor */
    set_directcolor_palette();
  }
  /* clear the screen */
  mi_fill_rect(0, 0, g_width, g_height, 0);
  /* connect */
#ifdef WITH_RDPSND
  /* init sound */
//  if (g_rdpsnd)
//  {
//    rdpsnd_init();
//  }
#endif
#if 0
 /* setup keyboard */
 ioctl(g_kbfd, KDGKBMODE, &g_org_kbmode); /* save this */
 tcgetattr(g_kbfd, &org_termios); /* save this */
 new_termios = org_termios;
 new_termios.c_lflag &= ~(ICANON | ECHO | ISIG);
 new_termios.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
 new_termios.c_cc[VMIN] = 0;
 new_termios.c_cc[VTIME] = 0;
 tcsetattr(g_kbfd, TCSAFLUSH, &new_termios);
 ioctl(g_kbfd, KDSKBMODE, K_MEDIUMRAW);
#endif
  load_keys();
  /* do it all here */
  rv = ui_main();
  /* clear the screen when done */
  mi_fill_rect(0, 0, g_width, g_height, 0);
  /* restore some stuff */
  if (g_finfo.visual == FB_VISUAL_DIRECTCOLOR)
  {
    restore_palette();
  }
  munmap(g_sdata, screensize);
  close(g_fbfd);
  close(g_mousefd);
#if 0
  ioctl(g_kbfd, KDSKBMODE, g_org_kbmode);
  tcsetattr(g_kbfd, TCSANOW, &org_termios);
#endif
  close(g_kbfd);
  free(g_bs);
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
