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
extern int pal_entries[];

extern char * g_bs;

static pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;

/* direct frame buffer stuff */
static IDirectFB * g_dfb = 0;
static IDirectFBSurface * g_primary = 0;
static int g_event_fd = 0;
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
  //g_provider->RenderTo(g_provider, g_primary, 0);
  if (g_rect.w > 0 && g_rect.h > 0)
  {
    g_primary->Blit(g_primary, g_s, &g_rect, g_rect.x, g_rect.y);

    //g_primary->Blit(g_primary, g_s, 0, 0, 0);

    //g_primary->Blit(g_primary, g_primary, 0, 0, 0);
    //g_primary->Blit(g_primary, g_s, &g_rect, g_rect.x, g_rect.y);
    //g_primary->Flip(g_primary, 0, 0);
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
  int count;
  DFBEvent event[10];
  struct timeval tv;

  fd = g_tcp_sck;
  if (g_event_fd > fd)
  {
    fd = g_event_fd;
  }
  FD_ZERO(&rfds);
  FD_SET(g_tcp_sck, &rfds);
  FD_SET(g_event_fd, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  rv = select(fd + 1, &rfds, 0, 0, &tv);
  while (rv > -1)
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
      if (FD_ISSET(g_event_fd, &rfds))
      {
        count = read(g_event_fd, &(event[0]), sizeof(DFBEvent));
        if (count == sizeof(DFBEvent))
        {
          //printf("got event\n");
          //hexdump(&event, sizeof(DFBEvent));
          process_event(&(event[0]));
        }
        else
        {
          printf("error reading g_event_fd in main_loop\n");
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
    FD_SET(g_event_fd, &rfds);
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
}

/*****************************************************************************/
static void
render_callback(DFBRectangle * rect, void * ctx)
{
  printf("hi11\n");
/*     int               width;
     int               height;
     IDirectFBSurface *image = (IDirectFBSurface*) ctx;

     image->GetSize( image, &width, &height );

     primary->Blit( primary, image, rect,
                    (screen_width - width) / 2 + rect->x,
                    (screen_height - height) / 2 + rect->y); */
}

/*****************************************************************************/
void *
update_thread(void * arg)
{
  while (g_bs != 0)
  {
    //usleep(1000000 / 24); /* 24 times per second */
    usleep(1000000 / 12); /* 12 times per second */
    pthread_mutex_lock(&g_mutex1);
    mi_update_screen();

    //g_primary->Blit(g_primary, g_s, &g_rect, g_rect.x, g_rect.y);
    //g_primary->Flip(g_primary, 0, 0);

    pthread_mutex_unlock(&g_mutex1);
  }
  return 0;
}

/*****************************************************************************/
void
mi_begin_update(void)
{
#if 0
  //DFBSurfaceDescription dsc;
  DFBDataBufferDescription ddsc;
  pthread_t thread;
  //pthread_attr_t tt;
  //struct sched_param param;
  //char * priority;
  //int policy;
  void * data;
  int fd;
  IDirectFBDataBuffer * g_buffer1 = 0;

  pthread_mutex_lock(&g_mutex1);

  if (g_provider == 0)
  {

    ddsc.flags         = DBDESC_FILE;
    ddsc.file   = strdup("/media/xrdp256.bmp");
    g_dfb->CreateDataBuffer(g_dfb, &ddsc, &g_buffer1);

    //data = g_bs;
    data = malloc(50230 * 2);
    g_buffer1->GetData(g_buffer1, 50230 * 2, data, &fd);
    printf("here %d\n", fd);

    //fd = open("/media/xrdp256.bmp", O_RDONLY);
    //data = mmap( NULL, 50230, PROT_READ, MAP_SHARED, fd, 0 );
    //if (data == MAP_FAILED)
    //{
    //  printf("error\n");
    //}
    //printf("%d\n", read(fd, data, 50231));
    //close(fd);
    /*mmap(g_bs, g_width * g_height * 4, PROT_READ, MAP_SHARED, 0, 0);
    if (data == MAP_FAILED)
    {
      printf("error calling mmap in mi_begin_update\n");
      data = 0;
    }*/
    /* create a data buffer for memory */
    ddsc.flags         = DBDESC_MEMORY;
    //ddsc.flags         = DBDESC_FILE;
    ddsc.file         = 0;
    ddsc.memory.data   = data;
    //ddsc.file   = strdup("/media/xrdp256.bmp");
    ddsc.memory.length = 50230;
    //ddsc.memory.length = g_width * g_height * 4;
    if (g_buffer == 0)
    {
      //if (g_dfb->CreateDataBuffer(g_dfb, &ddsc, &g_buffer) != 0)
      if (g_dfb->CreateDataBuffer(g_dfb, 0, &g_buffer) != 0)
      {
        printf("error calling CreateDataBuffer in mi_begin_update\n");
        g_buffer = 0;
      }
    }
    if (g_buffer != 0)
    {
      g_buffer->PutData(g_buffer, data, 50230);
      usleep(1000000);
      g_buffer->SeekTo(g_buffer, 0);
      fd = g_buffer->CreateImageProvider(g_buffer, &g_provider);
      printf("%d %d\n", fd, DFB_NOIMPL);
      if (fd != 0)
      {
        printf("error calling CreateImageProvider in mi_begin_update\n");
        g_provider = 0;
      }
    }
    if (g_provider != 0)
    {
      //g_provider->SetRenderCallback(g_provider, render_callback, 0);
      pthread_create(&thread, 0, update_thread, 0);
      pthread_detach(thread);
    }
  }
#endif
  DFBSurfaceDescription dsc;
  pthread_t thread;

  pthread_mutex_lock(&g_mutex1);
  if (g_s == 0)
  {

    dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT |
                DSDESC_PREALLOCATED | DSDESC_PIXELFORMAT;
    dsc.caps = DSCAPS_SYSTEMONLY;
    dsc.width = g_width;
    dsc.height = g_height;
    dsc.pixelformat = DSPF_AiRGB;
    //dsc.pixelformat = DSPF_RGB32;
    dsc.preallocated[0].data = g_bs;
    dsc.preallocated[0].pitch = g_width * 4;
    if (g_dfb->CreateSurface(g_dfb, &dsc, &g_s) == 0)
    {
      //policy = SCHED_RR;
      //pthread_attr_init(&tt);
      //pthread_attr_setschedpolicy(&tt, policy);
      //param.sched_priority = 10;
      //pthread_attr_setschedparam(&tt, &param);
      //priority = strdup("Low");
      //pthread_create(&thread, &tt, update_thread, (void *) priority);
      pthread_create(&thread, 0, update_thread, 0);
      pthread_detach(thread);
    }
    else
    {
      g_s = 0;
    }
  }
}

/*****************************************************************************/
void
mi_end_update(void)
{
  pthread_mutex_unlock(&g_mutex1);
}

/*****************************************************************************/
void
mi_fill_rect(int x, int y, int cx, int cy, int red, int green, int blue)
{
  mi_add_to(x, y, cx, cy);
}

/*****************************************************************************/
void
mi_line(int x1, int y1, int x2, int y2, int red, int green, int blue)
{
  int x;
  int y;
  int cx;
  int cy;

  x = UI_MIN(x1, x2);
  y = UI_MIN(y1, y2);
  cx = (UI_MAX(x1, x2) + 1) - x;
  cy = (UI_MAX(y1, y2) + 1) - y;
  mi_add_to(x, y, cx, cy);
}

/*****************************************************************************/
void
mi_screen_copy(int x, int y, int cx, int cy, int srcx, int srcy)
{
  mi_add_to(x, y, cx, cy);
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
  }
  return 1;
}

/*****************************************************************************/
int
main(int argc, char ** argv)
{
  int rv;
  int i;
  DFBSurfaceDescription dsc;
  DFBResult err;
  IDirectFBScreen * pScreen = NULL;
  DFBScreenEncoderConfig encConfig;
  DFBScreenEncoderConfigFlags encFlags;
  IDirectFBDisplayLayer * layer;

  //strcpy(g_servername, "205.5.61.3");
  //strcpy(g_servername, "127.0.0.1");
  //strcpy(g_servername, "24.158.1.82");
  strcpy(g_hostname, "test");
  //strcpy(g_username, "d");
  //strcpy(g_password, "tucker");
  g_server_depth = 24;

  if (!parse_parameters(argc, argv))
  {
    return 0;
  }

  printf("DirectFBInit\n");
  err = DirectFBInit(&argc, &argv);
  if (err == 0)
  {
    printf("DirectFBCreate\n");
    err = DirectFBCreate(&g_dfb);
  }
  if (err == 0)
  {
    printf("SetCooperativeLevel\n");
    err = g_dfb->SetCooperativeLevel(g_dfb, DFSCL_FULLSCREEN);
  }
  if (err == 0)
  {
    /*g_dfb->GetScreen(g_dfb, 0, &pScreen);
    pScreen->GetEncoderConfiguration(pScreen, 0, &encConfig);
    if (encConfig.tv_standard != DSETV_NTSC)
    {
      encConfig.tv_standard = DSETV_NTSC;
      //encConfig.tv_standard = DSETV_SECAM;
      encConfig.flags = DSECONF_TV_STANDARD;
      g_dfb->SetVideoMode(g_dfb, 720, 480, 32);
      if (pScreen->TestEncoderConfiguration(pScreen, 0, &encConfig,
                                            &encFlags) == DFB_UNSUPPORTED)
      {
        printf("error\n");
      }
      pScreen->SetEncoderConfiguration(pScreen, 0, &encConfig);
      for (i = 0; i < 5; i++)
      {
        g_dfb->GetDisplayLayer(g_dfb, i, &layer);
        layer->SetCooperativeLevel(layer, DLSCL_EXCLUSIVE);
      }
    } */
    err = g_dfb->GetScreen(g_dfb, 1, &pScreen);
  }
  if (err == 0)
  {
    dsc.flags = DSDESC_CAPS;
    dsc.caps  = DSCAPS_PRIMARY; // | DSCAPS_DOUBLE;
    //dsc.caps  = DSCAPS_PRIMARY | DSCAPS_DOUBLE;
    err = g_dfb->CreateSurface(g_dfb, &dsc, &g_primary);
  }
  if (err == 0)
  {
    g_dfb->CreateInputEventBuffer(g_dfb, DICAPS_AXES | DICAPS_BUTTONS |
                                  DICAPS_KEYS, 0, &g_event);
    g_event->CreateFileDescriptor(g_event, &g_event_fd);
  }
  if (err == 0)
  {
    err = g_primary->GetSize(g_primary, &g_width, &g_height);
  }
  printf("%d %d\n", g_width, g_height);
  if (err != 0)
  {
    printf("error in main\n");
    return 1;
  }
  rv = ui_main();
  g_primary->Release(g_primary);
  g_dfb->Release(g_dfb);
  return rv;
}
