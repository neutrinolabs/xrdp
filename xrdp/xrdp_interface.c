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

/*****************************************************************************/
/* this is the log windows nofity function */
int DEFAULT_CC
xrdp_wm_log_wnd_notify(struct xrdp_bitmap* wnd,
                       struct xrdp_bitmap* sender,
                       int msg, long param1, long param2)
{
  struct xrdp_painter* painter;
  struct xrdp_wm* wm;
  struct xrdp_rect rect;
  int index;
  char* text;

  if (wnd == 0)
  {
    return 0;
  }
  if (sender == 0)
  {
    return 0;
  }
  if (wnd->owner == 0)
  {
    return 0;
  }
  wm = wnd->wm;
  if (msg == 1) /* click */
  {
    if (sender->id == 1) /* ok button */
    {
      /* close the log window */
      MAKERECT(rect, wnd->left, wnd->top, wnd->width, wnd->height);
      xrdp_bitmap_delete(wnd);
      xrdp_bitmap_invalidate(wm->screen, &rect);
      if (wm->mod_handle == 0)
      {
        wm->pro_layer->term = 1; /* kill session */
      }
    }
  }
  else if (msg == WM_PAINT) /* 3 */
  {
    painter = (struct xrdp_painter*)param1;
    if (painter != 0)
    {
      painter->font->color = wnd->wm->black;
      for (index = 0; index < wnd->wm->log->count; index++)
      {
        text = (char*)list_get_item(wnd->wm->log, index);
        xrdp_painter_draw_text(painter, wnd, 10, 30 + index * 15, text);
      }
    }
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_begin_update(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = xrdp_painter_create(wm, wm->session);
  xrdp_painter_begin_update(p);
  mod->painter = (long)p;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_end_update(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  mod->painter = 0;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_fill_rect(p, wm->screen, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  p->rop = 0xcc;
  xrdp_painter_copy(p, wm->screen, wm->screen, x, y, cx, cy, srcx, srcy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  b = xrdp_bitmap_create_with_data(width, height, wm->screen->bpp, data, wm);
  xrdp_painter_copy(p, b, wm->screen, x, y, cx, cy, srcx, srcy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pointer(struct xrdp_mod* mod, int x, int y,
                   char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_wm_pointer(wm, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_palette(struct xrdp_mod* mod, int* palette)
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
int DEFAULT_CC
server_msg(struct xrdp_mod* mod, char* msg, int code)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* but;

  if (code == 1)
  {
    g_printf(msg);
    g_printf("\n\r");
    return 0;
  }
  wm = (struct xrdp_wm*)mod->wm;
  list_add_item(wm->log, (long)g_strdup(msg));
  if (wm->log_wnd == 0)
  {
    /* log window */
    wm->log_wnd = xrdp_bitmap_create(400, 400, wm->screen->bpp,
                                     WND_TYPE_WND, wm);
    list_add_item(wm->screen->child_list, (long)wm->log_wnd);
    wm->log_wnd->parent = wm->screen;
    wm->log_wnd->owner = wm->screen;
    wm->log_wnd->bg_color = wm->grey;
    wm->log_wnd->left = 10;
    wm->log_wnd->top = 10;
    set_string(&wm->log_wnd->caption1, "Connection Log");
    /* ok button */
    but = xrdp_bitmap_create(60, 25, wm->screen->bpp, WND_TYPE_BUTTON, wm);
    list_insert_item(wm->log_wnd->child_list, 0, (long)but);
    but->parent = wm->log_wnd;
    but->owner = wm->log_wnd;
    but->left = (400 - 60) - 10;
    but->top = (400 - 25) - 10;
    but->id = 1;
    but->tab_stop = 1;
    set_string(&but->caption1, "OK");
    wm->log_wnd->focused_control = but;
    /* set notify function */
    wm->log_wnd->notify = xrdp_wm_log_wnd_notify;
  }
  xrdp_bitmap_invalidate(wm->log_wnd, 0);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_is_term(struct xrdp_mod* mod)
{
  return g_is_term();
}

/*****************************************************************************/
int DEFAULT_CC
server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_set_clip(p, x, y, cx, cy);
}

/*****************************************************************************/
int DEFAULT_CC
server_reset_clip(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_clr_clip(p);
}

/*****************************************************************************/
int DEFAULT_CC
server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->fg_color = fgcolor;
  p->pen.color = p->fg_color;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->bg_color = bgcolor;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_opcode(struct xrdp_mod* mod, int opcode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->rop = opcode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_mixmode(struct xrdp_mod* mod, int mixmode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->mix_mode = mixmode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->brush.x_orgin = x_orgin;
  p->brush.y_orgin = y_orgin;
  p->brush.style = style;
  g_memcpy(p->brush.pattern, pattern, 8);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->pen.style = style;
  p->pen.width = width;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_line(p, wm->screen, x1, y1, x2, y2);
}

/*****************************************************************************/
int DEFAULT_CC
server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  struct xrdp_font_char fi;

  fi.offset = offset;
  fi.baseline = baseline;
  fi.width = width;
  fi.height = height;
  fi.incby = 0;
  fi.data = data;
  return libxrdp_orders_send_font(((struct xrdp_wm*)mod->wm)->session,
                                  &fi, font, charactor);
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_text(struct xrdp_mod* mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_draw_text2(p, wm->screen, font, flags,
                                 mixmode, clip_left, clip_top,
                                 clip_right, clip_bottom,
                                 box_left, box_top,
                                 box_right, box_bottom,
                                 x, y, data, data_len);
}
