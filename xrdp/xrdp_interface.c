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

   interface

*/

#include "xrdp.h"

#ifdef XRDP_LIB

struct xrdp_mod g_mod;
struct xrdp_process* g_rdp_process;
struct stream* g_s;
int (* g_callback)(int, int, int);

/*****************************************************************************/
int mod_event(struct xrdp_mod* v, int msg, int param1, int param2)
{
  if (g_callback != 0)
  {
    return g_callback(msg, param1, param2);
  }
  else
  {
    return 0;
  }
}

/*****************************************************************************/
int server_init(void)
{
  g_init_system();
  g_memset(&g_mod, 0, sizeof(struct xrdp_mod));
  g_rdp_process = 0;
  make_stream(g_s);
  init_stream(g_s, 8192);
  g_mod.handle = (int)(&g_mod);
  g_mod.mod_event = mod_event;
  return 0;
}

/*****************************************************************************/
int server_exit(void)
{
  xrdp_process_delete(g_rdp_process);
  free_stream(g_s);
  g_exit_system();
  return 0;
}

/*****************************************************************************/
int server_connect(int sck, int* width, int* height, int* bpp)
{
  g_rdp_process = xrdp_process_create(0);
  g_rdp_process->sck = sck;
  if (xrdp_rdp_incoming(g_rdp_process->rdp_layer) != 0)
  {
    return 1;
  }
  if (width != 0)
  {
    *width = g_rdp_process->rdp_layer->client_info.width;
  }
  if (height != 0)
  {
    *height = g_rdp_process->rdp_layer->client_info.height;
  }
  if (bpp != 0)
  {
    *bpp = g_rdp_process->rdp_layer->client_info.bpp;
  }
  return 0;
}

/*****************************************************************************/
int server_loop(int sck)
{
  if (g_rdp_process == 0)
  {
    return 1;
  }
  if (g_rdp_process->term)
  {
    return 1;
  }
  if (g_rdp_process->wm != 0)
  {
    if (g_rdp_process->wm->mod == 0)
    {
      g_rdp_process->wm->mod = &g_mod;
    }
    if (g_mod.wm == 0)
    {
      g_mod.wm = (int)(g_rdp_process->wm);
    }
  }
  init_stream(g_s, 8192);
  if (xrdp_process_loop(g_rdp_process, g_s) != 0)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
int server_set_callback(int (* callback)(int, int, int))
{
  g_callback = callback;
  return 0;
}

/*****************************************************************************/
int server_begin_update(void)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)g_mod.wm;
  p = xrdp_painter_create(wm);
  xrdp_painter_begin_update(p);
  g_mod.painter = (int)p;
  return 0;
}

/*****************************************************************************/
int server_end_update(void)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)g_mod.painter;
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  g_mod.painter = 0;
  return 0;
}

/*****************************************************************************/
int server_fill_rect(int x, int y, int cx, int cy, int color)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)g_mod.wm;
  p = (struct xrdp_painter*)g_mod.painter;
  p->fg_color = color;
  xrdp_painter_fill_rect(p, wm->screen, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int server_screen_blt(int x, int y, int cx, int cy, int srcx, int srcy)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)g_mod.wm;
  xrdp_orders_init(wm->orders);
  xrdp_orders_screen_blt(wm->orders, x, y, cx, cy, srcx, srcy, 0xcc, 0);
  xrdp_orders_send(wm->orders);
  return 0;
}

/*****************************************************************************/
int server_paint_rect(int x, int y, int cx, int cy, char* data)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;

  wm = (struct xrdp_wm*)g_mod.wm;
  b = xrdp_bitmap_create_with_data(cx, cy, wm->screen->bpp, data, wm);
  xrdp_painter_draw_bitmap((struct xrdp_painter*)g_mod.painter,
                           wm->screen, b, x, y, cx, cy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int server_set_cursor(int x, int y, char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)g_mod.wm;
  xrdp_wm_send_cursor(wm, 2, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int server_palette(int* palette)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)g_mod.wm;
  if (g_memcmp(wm->palette, palette, 255 * sizeof(int)) != 0)
  {
    g_memcpy(wm->palette, palette, 256 * sizeof(int));
    xrdp_wm_send_palette(wm);
  }
  return 0;
}

#else

/*****************************************************************************/
int server_begin_update(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = xrdp_painter_create(wm);
  xrdp_painter_begin_update(p);
  mod->painter = (int)p;
  return 0;
}

/*****************************************************************************/
int server_end_update(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  mod->painter = 0;
  return 0;
}

/*****************************************************************************/
int server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                     int color)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  p->fg_color = color;
  xrdp_painter_fill_rect(p, wm->screen, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                      int srcx, int srcy)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_orders_init(wm->orders);
  xrdp_orders_screen_blt(wm->orders, x, y, cx, cy, srcx, srcy, 0xcc, 0);
  xrdp_orders_send(wm->orders);
  return 0;
}

/*****************************************************************************/
int server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                      char* data)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;

  wm = (struct xrdp_wm*)mod->wm;
  b = xrdp_bitmap_create_with_data(cx, cy, wm->screen->bpp, data, wm);
  xrdp_painter_draw_bitmap((struct xrdp_painter*)mod->painter,
                           wm->screen, b, x, y, cx, cy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int server_set_cursor(struct xrdp_mod* mod, int x, int y,
                      char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_wm_send_cursor(wm, 2, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int server_palette(struct xrdp_mod* mod, int* palette)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  if (g_memcmp(wm->palette, palette, 255 * sizeof(int)) != 0)
  {
    g_memcpy(wm->palette, palette, 256 * sizeof(int));
    xrdp_wm_send_palette(wm);
  }
  return 0;
}

/*****************************************************************************/
int server_error_popup(struct xrdp_mod* mod, char* error, char* caption)
{
#if 0
  struct xrdp_wm* wm;
  struct xrdp_bitmap* wnd;
  struct xrdp_bitmap* but;

  wm = (struct xrdp_wm*)mod->wm;
  wnd = xrdp_bitmap_create(400, 200, wm->screen->bpp, WND_TYPE_WND, wm);
  xrdp_list_add_item(wm->screen->child_list, (int)wnd);
  wnd->parent = wm->screen;
  wnd->owner = wm->screen;
  wnd->bg_color = wm->grey;
  wnd->left = wm->screen->width / 2 - wnd->width / 2;
  wnd->top = wm->screen->height / 2 - wnd->height / 2;
  wnd->notify = xrdp_wm_popup_notify;
  g_strcpy(wnd->caption, caption);

  /* button */
  but = xrdp_bitmap_create(60, 25, wm->screen->bpp, WND_TYPE_BUTTON, wm);
  xrdp_list_add_item(wnd->child_list, (int)but);
  but->parent = wnd;
  but->owner = wnd;
  but->left = 180;
  but->top = 160;
  but->id = 1;
  g_strcpy(but->caption, "OK");
  but->tab_stop = 1;

  xrdp_bitmap_invalidate(wm->screen, 0);
  //xrdp_bitmap_invalidate(wnd, 0);
  g_sleep(2000);
#endif
  return 0;
}

#endif /* XRDP_LIB */
