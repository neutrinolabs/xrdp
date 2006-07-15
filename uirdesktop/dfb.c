/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   directfb calls
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

//#define USE_FLIPPING
#define USE_ORDERS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <directfb.h>
#include <pthread.h>
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

static DFBRegion g_reg;

static pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;

/* direct frame buffer stuff */
static IDirectFB * g_dfb = 0;
static IDirectFBSurface * g_primary = 0;
static IDirectFBEventBuffer * g_event = 0;
static DFBRectangle g_rect = {0, 0, 0, 0};

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

static IDirectFBSurface * g_s = 0;

//static IDirectFBDataBuffer * g_buffer = 0;
//static IDirectFBImageProvider * g_provider = 0;

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
  g_primary->SetColor(g_primary, 0, 0, 0, 0xff);
  g_primary->FillRectangle(g_primary, 0, 0, g_width, g_height);
  return 1;
}

/*****************************************************************************/
void
mi_update_screen(void)
{
  if (g_rect.w > 0 && g_rect.h > 0)
  {
#ifdef USE_ORDERS_NOT
    DFBRegion reg;
    reg.x1 = 0;
    reg.y1 = 0;
    reg.x2 = g_width;
    reg.y2 = g_height;
    g_primary->SetClip(g_primary, &reg);
#endif
#ifdef USE_FLIPPING
    g_primary->Blit(g_primary, g_primary, 0, 0, 0);
    g_primary->Blit(g_primary, g_s, &g_rect, g_rect.x, g_rect.y);
    g_primary->Flip(g_primary, 0, 0);
#else
    g_primary->Blit(g_primary, g_s, &g_rect, g_rect.x, g_rect.y);
#endif
#ifdef USE_ORDERS_NOT
    g_primary->SetClip(g_primary, &g_reg);
#endif
  }
  g_rect.x = 0;
  g_rect.y = 0;
  g_rect.w = 0;
  g_rect.h = 0;
}

/*****************************************************************************/
void process_event(DFBEvent * event)
{
  DFBInputEvent * input_event;
  int mouse_x;
  int mouse_y;

  mouse_x = g_mouse_x + g_mcursor.x;
  mouse_y = g_mouse_y + g_mcursor.y;
  if (event->clazz == DFEC_INPUT)
  {
    input_event = (DFBInputEvent *) event;
    if (input_event->type == DIET_AXISMOTION)
    {
      if (input_event->flags & DIEF_AXISABS)
      {
        if (input_event->axis == DIAI_X)
        {
          mouse_x = input_event->axisabs;
        }
        else if (input_event->axis == DIAI_Y)
        {
          mouse_y = input_event->axisabs;
        }
      }
      if (input_event->flags & DIEF_AXISREL)
      {
        if (input_event->axis == DIAI_X)
        {
          mouse_x += input_event->axisrel;
        }
        else if (input_event->axis == DIAI_Y)
        {
          mouse_y += input_event->axisrel;
        }
      }
      mouse_x = UI_MAX(mouse_x, 0);
      mouse_x = UI_MIN(mouse_x, g_width - 1);
      mouse_y = UI_MAX(mouse_y, 0);
      mouse_y = UI_MIN(mouse_y, g_height - 1);
      ui_mouse_move(mouse_x, mouse_y);
    }
    else if (input_event->type == DIET_BUTTONPRESS)
    {
      if (input_event->button == DIBI_LEFT)
      {
        ui_mouse_button(1, mouse_x, mouse_y, 1);
        //rdp_send_input(0, RDP_INPUT_MOUSE,
        //               MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON1,
        //               mouse_x, mouse_y);
      }
      else if (input_event->button == DIBI_RIGHT)
      {
        //mi_update_screen();
        ui_mouse_button(2, mouse_x, mouse_y, 1);
//        invalidate(0, 0, g_width, g_height);
        //rdp_send_input(0, RDP_INPUT_MOUSE,
        //               MOUSE_FLAG_DOWN | MOUSE_FLAG_BUTTON2,
        //               mouse_x, mouse_y);
      }
      else if (input_event->button == DIBI_MIDDLE)
      {
        ui_mouse_button(3, mouse_x, mouse_y, 1);
      }
    }
    else if (input_event->type == DIET_BUTTONRELEASE)
    {
      if (input_event->button == DIBI_LEFT)
      {
        ui_mouse_button(1, mouse_x, mouse_y, 0);
        //rdp_send_input(0, RDP_INPUT_MOUSE,
        //               MOUSE_FLAG_BUTTON1,
        //               mouse_x, mouse_y);
      }
      else if (input_event->button == DIBI_RIGHT)
      {
        ui_mouse_button(2, mouse_x, mouse_y, 0);
        //rdp_send_input(0, RDP_INPUT_MOUSE,
        //               MOUSE_FLAG_BUTTON2,
        //               mouse_x, mouse_y);
      }
      else if (input_event->button == DIBI_MIDDLE)
      {
        ui_mouse_button(3, mouse_x, mouse_y, 0);
      }
    }
    else if (input_event->type == DIET_KEYPRESS)
    {
      //printf("hi1 %d\n", input_event->key_id);
      return;
    }
    else if (input_event->type == DIET_KEYRELEASE)
    {
      //printf("hi2 %d\n", input_event->key_id);
      return;
    }
  }
  g_mouse_x = mouse_x - g_mcursor.x;
  g_mouse_y = mouse_y - g_mcursor.y;
//  printf("%d %d\n", g_mouse_x, g_mouse_y);
  g_primary->SetColor(g_primary, 0, 0, 0, 0xff);
  g_primary->FillRectangle(g_primary, g_mouse_x, g_mouse_y, 5, 5);
//  draw_mouse();
}

/*****************************************************************************/
int
mi_main_loop(void)
{
  fd_set rfds;
  int rv;
  int fd;
  DFBEvent event[10];
  struct timeval tv;

  fd = g_tcp_sck;
  FD_ZERO(&rfds);
  FD_SET(g_tcp_sck, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  rv = select(fd + 1, &rfds, 0, 0, &tv);
  while (rv > -1)
  {
    if (g_event->HasEvent(g_event) == DFB_OK)
    {
      if (g_event->GetEvent(g_event, &event[0]) == 0)
      {
        process_event(&event[0]);
      }
    }
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
    else
    {
      //usleep(1000000 / 60);
      usleep(0);
    }
    FD_ZERO(&rfds);
    FD_SET(g_tcp_sck, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    rv = select(fd + 1, &rfds, 0, 0, &tv);
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
void *
update_thread(void * arg)
{
  struct timeval ltime;
  struct timeval ntime;
  int nsecs;

  gettimeofday(&ltime, 0);
  while (g_bs != 0)
  {
    gettimeofday(&ntime, 0);
    nsecs = (ntime.tv_sec - ltime.tv_sec) * 1000000 + (ntime.tv_usec - ltime.tv_usec);
    nsecs = (1000000 / 12) - nsecs;
    if (nsecs < 0)
    {
      nsecs = 0;
    }
    usleep(nsecs);
    gettimeofday(&ltime, 0);
    pthread_mutex_lock(&g_mutex1);
    mi_update_screen();
    pthread_mutex_unlock(&g_mutex1);
  }
  return 0;
}

/*****************************************************************************/
int
mi_create_bs(void)
{
  //pthread_t thread;
  DFBSurfaceDescription dsc;

  g_bs = malloc(g_width * g_height * 4);
  dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT |
              DSDESC_PREALLOCATED | DSDESC_PIXELFORMAT;
  dsc.caps = DSCAPS_SYSTEMONLY;
  dsc.width = g_width;
  dsc.height = g_height;
  dsc.pixelformat = DSPF_AiRGB;
  dsc.preallocated[0].data = g_bs;
  dsc.preallocated[0].pitch = g_width * 4;
  if (g_dfb->CreateSurface(g_dfb, &dsc, &g_s) == 0)
  {
    //pthread_create(&thread, 0, update_thread, 0);
    //pthread_detach(thread);
  }
  else
  {
    g_s = 0;
    free(g_bs);
    g_bs = 0;
    return 0;
  }
  return 1;
}

/*****************************************************************************/
void
mi_begin_update(void)
{
  pthread_mutex_lock(&g_mutex1);
}

/*****************************************************************************/
void
mi_end_update(void)
{
  pthread_mutex_unlock(&g_mutex1);
}

/*****************************************************************************/
void
mi_fill_rect(int x, int y, int cx, int cy, int colour)
{
#ifdef USE_ORDERS
  int red;
  int green;
  int blue;

  mi_update_screen();
  red = (colour & 0xff0000) >> 16;
  green = (colour & 0xff00) >> 8;
  blue = colour & 0xff;
  g_primary->SetColor(g_primary, red, green, blue, 0xff);
  g_primary->FillRectangle(g_primary, x, y, cx, cy);
#else
  mi_add_to(x, y, cx, cy);
#endif
}

/*****************************************************************************/
void
mi_line(int x1, int y1, int x2, int y2, int colour)
{
#ifdef USE_ORDERS_TOO_SLOW
  int r;
  int g;
  int b;

  mi_update_screen();
  r = (colour >> 16) & 0xff;
  g = (colour >> 8) & 0xff;
  b = colour & 0xff;
  g_primary->SetColor(g_primary, r, g, b, 0xff);
  g_primary->DrawLine(g_primary, x1, y1, x2, y2);
#else
  int x;
  int y;
  int cx;
  int cy;

  x = UI_MIN(x1, x2);
  y = UI_MIN(y1, y2);
  cx = (UI_MAX(x1, x2) + 1) - x;
  cy = (UI_MAX(y1, y2) + 1) - y;
  mi_add_to(x, y, cx, cy);
#endif
}

/*****************************************************************************/
void
mi_screen_copy(int x, int y, int cx, int cy, int srcx, int srcy)
{
#ifdef USE_ORDERS
  DFBRectangle rect;
  DFBSurfaceDescription dsc;
  IDirectFBSurface * surface;

  mi_update_screen();
  //if (srcy < y)
  {
    dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT;
    dsc.caps = DSCAPS_VIDEOONLY;
    dsc.width = cx;
    dsc.height = cy;
    if (g_dfb->CreateSurface(g_dfb, &dsc, &surface) == 0)
    {
      rect.x = srcx;
      rect.y = srcy;
      rect.w = cx;
      rect.h = cy;
      surface->Blit(surface, g_primary, &rect, 0, 0);
      g_primary->Blit(g_primary, surface, 0, x, y);
      surface->Release(surface);
    }
  }
  //else
  //{
  //  rect.x = srcx;
  //  rect.y = srcy;
  //  rect.w = cx;
  //  rect.h = cy;
  //  g_primary->Blit(g_primary, g_primary, &rect, x, y);
  //}
#else
  mi_add_to(x, y, cx, cy);
#endif
}

/*****************************************************************************/
void
mi_set_clip(int x, int y, int cx, int cy)
{
#ifdef USE_ORDERS
  g_reg.x1 = x;
  g_reg.y1 = y;
  g_reg.x2 = (x + cx) - 1;
  g_reg.y2 = (y + cy) - 1;
  g_primary->SetClip(g_primary, &g_reg);
#endif
}

/*****************************************************************************/
void
mi_reset_clip(void)
{
#ifdef USE_ORDERS
// this dosen't work, directb bug?
//  g_primary->SetClip(g_primary, 0);
  g_reg.x1 = 0;
  g_reg.y1 = 0;
  g_reg.x2 = g_width;
  g_reg.y2 = g_height;
  g_primary->SetClip(g_primary, &g_reg);
#endif
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
  }
  return 1;
}

/*****************************************************************************/
int
main(int argc, char ** argv)
{
  int rv;
  DFBSurfaceDescription dsc;
  DFBResult err;

  strcpy(g_hostname, "test");
  g_server_depth = 24;
  if (!parse_parameters(argc, argv))
  {
    return 0;
  }
  err = DirectFBInit(&argc, &argv);
  if (err == 0)
  {
    err = DirectFBCreate(&g_dfb);
  }
  if (err == 0)
  {
    err = g_dfb->SetCooperativeLevel(g_dfb, DFSCL_FULLSCREEN);
  }
  if (err == 0)
  {
    dsc.flags = DSDESC_CAPS;
#ifdef USE_FLIPPING
    dsc.caps  = DSCAPS_PRIMARY | DSCAPS_DOUBLE | DSCAPS_FLIPPING;
#else
    dsc.caps  = DSCAPS_PRIMARY;
#endif
    err = g_dfb->CreateSurface(g_dfb, &dsc, &g_primary);
  }
  if (err == 0)
  {
    g_dfb->CreateInputEventBuffer(g_dfb, DICAPS_AXES | DICAPS_BUTTONS |
                                  DICAPS_KEYS, 0, &g_event);
  }
  if (err == 0)
  {
    err = g_primary->GetSize(g_primary, &g_width, &g_height);
  }
  if (err != 0)
  {
    printf("error in main\n");
    return 1;
  }
  rv = ui_main();
  g_s->Release(g_s);
  g_primary->Release(g_primary);
  g_dfb->Release(g_dfb);
  return rv;
}

/*****************************************************************************/
/* returns non zero ok */
int
librdesktop_init(long obj1, long obj2, long obj3, int in_argc, char ** in_argv)
{
  strcpy(g_hostname, "test");
  g_dfb = (IDirectFB *) obj1;
  g_primary = (IDirectFBSurface *) obj2;
  g_primary->GetSize(g_primary, &g_width, &g_height);
  g_server_depth = 24;
  if (!parse_parameters(in_argc, in_argv))
  {
    return 0;
  }
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
