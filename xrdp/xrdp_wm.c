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

   simple window manager

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_wm* xrdp_wm_create(struct xrdp_process* owner,
                               struct xrdp_client_info* client_info)
{
  struct xrdp_wm* self;

  self = (struct xrdp_wm*)g_malloc(sizeof(struct xrdp_wm), 1);
  self->client_info = client_info;
  self->screen = xrdp_bitmap_create(client_info->width,
                                    client_info->height,
                                    client_info->bpp,
                                    WND_TYPE_SCREEN, self);
  self->screen->wm = self;
  self->pro_layer = owner;
  self->orders = owner->orders;
  self->painter = xrdp_painter_create(self);
  self->rdp_layer = owner->rdp_layer;
  self->cache = xrdp_cache_create(self, self->orders, self->client_info);
  self->log = xrdp_list_create();
  self->log->auto_free = 1;
  return self;
}

/*****************************************************************************/
void xrdp_wm_delete(struct xrdp_wm* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_cache_delete(self->cache);
  xrdp_painter_delete(self->painter);
  xrdp_bitmap_delete(self->screen);
  /* free any modual stuff */
  if (self->mod != 0)
  {
    if (self->mod_exit != 0)
    {
      self->mod_exit(self->mod);
    }
  }
  if (self->mod_handle != 0)
  {
    g_free_library(self->mod_handle);
  }
  /* free the log */
  xrdp_list_delete(self->log);
  /* free self */
  g_free(self);
}

/*****************************************************************************/
int xrdp_wm_send_palette(struct xrdp_wm* self)
{
  int i;
  int color;
  struct stream* s;

  if (self->client_info->bpp > 8)
  {
    return 0;
  }
  xrdp_orders_force_send(self->orders);
  make_stream(s);
  init_stream(s, 8192);
  xrdp_rdp_init_data(self->rdp_layer, s);
  out_uint16_le(s, RDP_UPDATE_PALETTE);
  out_uint16_le(s, 0);
  out_uint16_le(s, 256); /* # of colors */
  out_uint16_le(s, 0);
  for (i = 0; i < 256; i++)
  {
    color = self->palette[i];
    out_uint8(s, color >> 16);
    out_uint8(s, color >> 8);
    out_uint8(s, color);
  }
  s_mark_end(s);
  xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_UPDATE);
  free_stream(s);
  xrdp_orders_init(self->orders);
  xrdp_orders_send_palette(self->orders, self->palette, 0);
  xrdp_orders_send(self->orders);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_send_bitmap(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                        int x, int y, int cx, int cy)
{
  int data_size;
  int line_size;
  int i;
  int j;
  int total_lines;
  int lines_sending;
  int Bpp;
  int e;
  int bufsize;
  int total_bufsize;
  int num_updates;
  char* p_num_updates;
  char* p;
  char* q;
  struct stream* s;
  struct stream* temp_s;

  Bpp = (bitmap->bpp + 7) / 8;
  e = bitmap->width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  line_size = bitmap->width * Bpp;
  make_stream(s);
  init_stream(s, 8192);
  if (self->client_info->use_bitmap_comp)
  {
    make_stream(temp_s);
    init_stream(temp_s, 65536);
    i = 0;
    if (cy <= bitmap->height)
    {
      i = cy;
    }
    while (i > 0)
    {
      total_bufsize = 0;
      num_updates = 0;
      xrdp_rdp_init_data(self->rdp_layer, s);
      out_uint16_le(s, RDP_UPDATE_BITMAP);
      p_num_updates = s->p;
      out_uint8s(s, 2); /* num_updates set later */
      do
      {
        if (self->client_info->op1)
        {
          s_push_layer(s, channel_hdr, 18);
        }
        else
        {
          s_push_layer(s, channel_hdr, 26);
        }
        p = s->p;
        lines_sending = xrdp_bitmap_compress(bitmap->data, bitmap->width,
                                             bitmap->height,
                                             s, bitmap->bpp,
                                             4096 - total_bufsize,
                                             i - 1, temp_s, e);
        if (lines_sending == 0)
        {
          break;
        }
        num_updates++;
        bufsize = s->p - p;
        total_bufsize += bufsize;
        i = i - lines_sending;
        s_mark_end(s);
        s_pop_layer(s, channel_hdr);
        out_uint16_le(s, x); /* left */
        out_uint16_le(s, y + i); /* top */
        out_uint16_le(s, (x + cx) - 1); /* right */
        out_uint16_le(s, (y + i + lines_sending) - 1); /* bottom */
        out_uint16_le(s, bitmap->width + e); /* width */
        out_uint16_le(s, lines_sending); /* height */
        out_uint16_le(s, bitmap->bpp); /* bpp */
        if (self->client_info->op1)
        {
          out_uint16_le(s, 0x401); /* compress */
          out_uint16_le(s, bufsize); /* compressed size */
          j = (bitmap->width + e) * Bpp;
          j = j * lines_sending;
        }
        else
        {
          out_uint16_le(s, 0x1); /* compress */
          out_uint16_le(s, bufsize + 8);
          out_uint8s(s, 2); /* pad */
          out_uint16_le(s, bufsize); /* compressed size */
          j = (bitmap->width + e) * Bpp;
          out_uint16_le(s, j); /* line size */
          j = j * lines_sending;
          out_uint16_le(s, j); /* final size */
        }
        if (j > 32768)
        {
          g_printf("error, decompressed size too big, its %d\n\r", j);
        }
        if (bufsize > 8192)
        {
          g_printf("error, compressed size too big, its %d\n\r", bufsize);
        }
        s->p = s->end;
      } while (total_bufsize < 4096 && i > 0);
      p_num_updates[0] = num_updates;
      p_num_updates[1] = num_updates >> 8;
      xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_UPDATE);
      if (total_bufsize > 8192)
      {
        g_printf("error, total compressed size too big, its %d\n\r",
                 total_bufsize);
      }
    }
    free_stream(temp_s);
  }
  else
  {
    lines_sending = 0;
    data_size = bitmap->width * bitmap->height * Bpp;
    total_lines = bitmap->height;
    i = 0;
    p = bitmap->data;
    if (line_size > 0 && total_lines > 0)
    {
      while (i < total_lines)
      {
        lines_sending = 4096 / (line_size + e * Bpp);
        if (i + lines_sending > total_lines)
        {
          lines_sending = total_lines - i;
        }
        p = p + line_size * lines_sending;
        xrdp_rdp_init_data(self->rdp_layer, s);
        out_uint16_le(s, RDP_UPDATE_BITMAP);
        out_uint16_le(s, 1); /* num updates */
        out_uint16_le(s, x);
        out_uint16_le(s, y + i);
        out_uint16_le(s, (x + cx) - 1);
        out_uint16_le(s, (y + i + lines_sending) - 1);
        out_uint16_le(s, bitmap->width + e);
        out_uint16_le(s, lines_sending);
        out_uint16_le(s, bitmap->bpp); /* bpp */
        out_uint16_le(s, 0); /* compress */
        out_uint16_le(s, (line_size + e * Bpp) * lines_sending); /* bufsize */
        q = p;
        for (j = 0; j < lines_sending; j++)
        {
          q = q - line_size;
          out_uint8a(s, q, line_size)
          out_uint8s(s, e * Bpp);
        }
        s_mark_end(s);
        xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_UPDATE);
        i = i + lines_sending;
      }
    }
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_set_focused(struct xrdp_wm* self, struct xrdp_bitmap* wnd)
{
  struct xrdp_bitmap* focus_out_control;
  struct xrdp_bitmap* focus_in_control;

  if (self == 0)
  {
    return 0;
  }
  if (self->focused_window == wnd)
  {
    return 0;
  }
  focus_out_control = 0;
  focus_in_control = 0;
  if (self->focused_window != 0)
  {
    xrdp_bitmap_set_focus(self->focused_window, 0);
    focus_out_control = self->focused_window->focused_control;
  }
  self->focused_window = wnd;
  if (self->focused_window != 0)
  {
    xrdp_bitmap_set_focus(self->focused_window, 1);
    focus_in_control = self->focused_window->focused_control;
  }
  xrdp_bitmap_invalidate(focus_out_control, 0);
  xrdp_bitmap_invalidate(focus_in_control, 0);
  return 0;
}

//******************************************************************************
int xrdp_wm_get_pixel(char* data, int x, int y, int width, int bpp)
{
  int start;
  int shift;

  if (bpp == 1)
  {
    width = (width + 7) / 8;
    start = (y * width) + x / 8;
    shift = x % 8;
    return (data[start] & (0x80 >> shift)) != 0;
  }
  else if (bpp == 4)
  {
    width = (width + 1) / 2;
    start = y * width + x / 2;
    shift = x % 2;
    if (shift == 0)
    {
      return (data[start] & 0xf0) >> 4;
    }
    else
    {
      return data[start] & 0x0f;
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_pointer(struct xrdp_wm* self, char* data, char* mask, int x, int y)
{

  struct xrdp_pointer_item pointer_item;

  g_memset(&pointer_item, 0, sizeof(struct xrdp_pointer_item));
  pointer_item.x = x;
  pointer_item.y = y;
  g_memcpy(pointer_item.data, data, 32 * 32 * 3);
  g_memcpy(pointer_item.mask, mask, 32 * 32 / 8);
  self->screen->pointer = xrdp_cache_add_pointer(self->cache, &pointer_item);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_load_pointer(struct xrdp_wm* self, char* file_name, char* data,
                        char* mask, int* x, int* y)
{
  int fd;
  int bpp;
  int w;
  int h;
  int i;
  int j;
  int pixel;
  int palette[16];
  struct stream* fs;

  make_stream(fs);
  init_stream(fs, 8192);
  fd = g_file_open(file_name);
  g_file_read(fd, fs->data, 8192);
  g_file_close(fd);
  in_uint8s(fs, 6);
  in_uint8(fs, w);
  in_uint8(fs, h);
  in_uint8s(fs, 2);
  in_uint16_le(fs, *x);
  in_uint16_le(fs, *y);
  in_uint8s(fs, 22);
  in_uint8(fs, bpp);
  in_uint8s(fs, 25);
  if (w == 32 && h == 32)
  {
    if (bpp == 1)
    {
      in_uint8a(fs, palette, 8);
      for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 32; j++)
        {
          pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 1)];
          *data = pixel;
          data++;
          *data = pixel >> 8;
          data++;
          *data = pixel >> 16;
          data++;
        }
      }
      in_uint8s(fs, 128);
    }
    else if (bpp == 4)
    {
      in_uint8a(fs, palette, 64);
      for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 32; j++)
        {
          pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 1)];
          *data = pixel;
          data++;
          *data = pixel >> 8;
          data++;
          *data = pixel >> 16;
          data++;
        }
      }
      in_uint8s(fs, 512);
    }
    g_memcpy(mask, fs->p, 128); /* mask */
  }
  free_stream(fs);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_send_pointer(struct xrdp_wm* self, int cache_idx,
                        char* data, char* mask, int x, int y)
{
  struct stream* s;
  char* p;
  int i;
  int j;

  make_stream(s);
  init_stream(s, 8192);
  xrdp_rdp_init_data(self->rdp_layer, s);
  out_uint16_le(s, RDP_POINTER_COLOR);
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, cache_idx); /* cache_idx */
  out_uint16_le(s, x);
  out_uint16_le(s, y);
  out_uint16_le(s, 32);
  out_uint16_le(s, 32);
  out_uint16_le(s, 128);
  out_uint16_le(s, 3072);
  p = data;
  for (i = 0; i < 32; i++)
  {
    for (j = 0; j < 32; j++)
    {
      out_uint8(s, *p);
      p++;
      out_uint8(s, *p);
      p++;
      out_uint8(s, *p);
      p++;
    }
  }
  out_uint8a(s, mask, 128); /* mask */
  s_mark_end(s);
  xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_POINTER);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_set_pointer(struct xrdp_wm* self, int cache_idx)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  xrdp_rdp_init_data(self->rdp_layer, s);
  out_uint16_le(s, RDP_POINTER_CACHED);
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, cache_idx); /* cache_idx */
  s_mark_end(s);
  xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_POINTER);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_init(struct xrdp_wm* self)
{
#ifndef XRDP_LIB /* if lib, dodn't create login screen */
  int bindex;
  int gindex;
  int rindex;
  struct xrdp_pointer_item pointer_item;

  if (self->screen->bpp == 8)
  {
    /* rgb332 */
    for (bindex = 0; bindex < 4; bindex++)
    {
      for (gindex = 0; gindex < 8; gindex++)
      {
        for (rindex = 0; rindex < 8; rindex++)
        {
          self->palette[(bindex << 6) | (gindex << 3) | rindex] =
                (((rindex << 5) | (rindex << 2) | (rindex >> 1)) << 16) |
                (((gindex << 5) | (gindex << 2) | (gindex >> 1)) << 8) |
                ((bindex << 6) | (bindex << 4) | (bindex << 2) | (bindex));
        }
      }
    }
    self->black     = COLOR8(0, 0, 0);
    self->grey      = COLOR8(0xc0, 0xc0, 0xc0);
    self->dark_grey = COLOR8(0x80, 0x80, 0x80);
    self->blue      = COLOR8(0x00, 0x00, 0xff);
    self->dark_blue = COLOR8(0x00, 0x00, 0x7f);
    self->white     = COLOR8(0xff, 0xff, 0xff);
    self->red       = COLOR8(0xff, 0x00, 0x00);
    self->green     = COLOR8(0x00, 0xff, 0x00);
    xrdp_wm_send_palette(self);
  }
  else if (self->screen->bpp == 15)
  {
    self->black     = COLOR15(0, 0, 0);
    self->grey      = COLOR15(0xc0, 0xc0, 0xc0);
    self->dark_grey = COLOR15(0x80, 0x80, 0x80);
    self->blue      = COLOR15(0x00, 0x00, 0xff);
    self->dark_blue = COLOR15(0x00, 0x00, 0x7f);
    self->white     = COLOR15(0xff, 0xff, 0xff);
    self->red       = COLOR15(0xff, 0x00, 0x00);
    self->green     = COLOR15(0x00, 0xff, 0x00);
  }
  else if (self->screen->bpp == 16)
  {
    self->black     = COLOR16(0, 0, 0);
    self->grey      = COLOR16(0xc0, 0xc0, 0xc0);
    self->dark_grey = COLOR16(0x80, 0x80, 0x80);
    self->blue      = COLOR16(0x00, 0x00, 0xff);
    self->dark_blue = COLOR16(0x00, 0x00, 0x7f);
    self->white     = COLOR16(0xff, 0xff, 0xff);
    self->red       = COLOR16(0xff, 0x00, 0x00);
    self->green     = COLOR16(0x00, 0xff, 0x00);
  }
  else if (self->screen->bpp == 24)
  {
    self->black     = COLOR24(0, 0, 0);
    self->grey      = COLOR24(0xc0, 0xc0, 0xc0);
    self->dark_grey = COLOR24(0x80, 0x80, 0x80);
    self->blue      = COLOR24(0x00, 0x00, 0xff);
    self->dark_blue = COLOR24(0x00, 0x00, 0x7f);
    self->white     = COLOR24(0xff, 0xff, 0xff);
    self->red       = COLOR24(0xff, 0x00, 0x00);
    self->green     = COLOR24(0x00, 0xff, 0x00);
  }
  DEBUG(("sending cursor\n\r"));
  xrdp_wm_load_pointer(self, "cursor1.cur", pointer_item.data,
                       pointer_item.mask, &pointer_item.x, &pointer_item.y);
  xrdp_cache_add_pointer_static(self->cache, &pointer_item, 1);
  DEBUG(("sending cursor\n\r"));
  xrdp_wm_load_pointer(self, "cursor0.cur", pointer_item.data,
                       pointer_item.mask, &pointer_item.x, &pointer_item.y);
  xrdp_cache_add_pointer_static(self->cache, &pointer_item, 0);
  xrdp_login_wnd_create(self);
  /* clear screen */
  self->screen->bg_color = self->black;
  xrdp_bitmap_invalidate(self->screen, 0);

  xrdp_wm_set_focused(self, self->login_window);

#endif
  return 0;
}

/*****************************************************************************/
/* returns the number for rects visible for an area relative to a drawable */
/* putting the rects in region */
int xrdp_wm_get_vis_region(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                           int x, int y, int cx, int cy,
                           struct xrdp_region* region, int clip_children)
{
  int i;
  struct xrdp_bitmap* p;
  struct xrdp_rect a;
  struct xrdp_rect b;

  /* area we are drawing */
  MAKERECT(a, bitmap->left + x, bitmap->top + y, cx, cy);
  p = bitmap->parent;
  while (p != 0)
  {
    RECTOFFSET(a, p->left, p->top);
    p = p->parent;
  }
  a.left = MAX(self->screen->left, a.left);
  a.top = MAX(self->screen->top, a.top);
  a.right = MIN(self->screen->left + self->screen->width, a.right);
  a.bottom = MIN(self->screen->top + self->screen->height, a.bottom);
  xrdp_region_add_rect(region, &a);
  if (clip_children)
  {
    /* loop through all windows in z order */
    for (i = 0; i < self->screen->child_list->count; i++)
    {
      p = (struct xrdp_bitmap*)xrdp_list_get_item(self->screen->child_list, i);
      if (p == bitmap || p == bitmap->parent)
      {
        return 0;
      }
      MAKERECT(b, p->left, p->top, p->width, p->height);
      xrdp_region_subtract_rect(region, &b);
    }
  }
  return 0;
}

/*****************************************************************************/
/* return the window at x, y on the screen */
struct xrdp_bitmap* xrdp_wm_at_pos(struct xrdp_bitmap* wnd, int x, int y,
                                   struct xrdp_bitmap** wnd1)
{
  int i;
  struct xrdp_bitmap* p;
  struct xrdp_bitmap* q;

  /* loop through all windows in z order */
  for (i = 0; i < wnd->child_list->count; i++)
  {
    p = (struct xrdp_bitmap*)xrdp_list_get_item(wnd->child_list, i);
    if (x >= p->left && y >= p->top && x < p->left + p->width &&
        y < p->top + p->height)
    {
      if (wnd1 != 0)
      {
        *wnd1 = p;
      }
      q = xrdp_wm_at_pos(p, x - p->left, y - p->top, 0);
      if (q == 0)
      {
        return p;
      }
      else
      {
        return q;
      }
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_xor_pat(struct xrdp_wm* self, int x, int y, int cx, int cy)
{
  self->painter->clip_children = 0;
  self->painter->rop = 0x5a;
  xrdp_painter_begin_update(self->painter);
  self->painter->use_clip = 0;
  self->painter->brush.pattern[0] = 0xaa;
  self->painter->brush.pattern[1] = 0x55;
  self->painter->brush.pattern[2] = 0xaa;
  self->painter->brush.pattern[3] = 0x55;
  self->painter->brush.pattern[4] = 0xaa;
  self->painter->brush.pattern[5] = 0x55;
  self->painter->brush.pattern[6] = 0xaa;
  self->painter->brush.pattern[7] = 0x55;
  self->painter->brush.x_orgin = 0;
  self->painter->brush.x_orgin = 0;
  self->painter->brush.style = 3;
  self->painter->bg_color = self->black;
  self->painter->fg_color = self->white;
  /* top */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y, cx, 5);
  /* bottom */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y + (cy - 5), cx, 5);
  /* left */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y + 5, 5, cy - 10);
  /* right */
  xrdp_painter_fill_rect2(self->painter, self->screen, x + (cx - 5), y + 5, 5,
                          cy - 10);
  xrdp_painter_end_update(self->painter);
  self->painter->rop = 0xcc;
  self->painter->clip_children = 1;
  return 0;
}

/*****************************************************************************/
/* this don't are about nothing, just copy the bits */
/* no clipping rects, no windows in the way, nothing */
int xrdp_wm_bitblt(struct xrdp_wm* self,
                   struct xrdp_bitmap* dst, int dx, int dy,
                   struct xrdp_bitmap* src, int sx, int sy,
                   int sw, int sh, int rop)
{
//  int i;
//  int line_size;
//  int Bpp;
//  char* s;
//  char* d;

//  if (sw <= 0 || sh <= 0)
//    return 0;
  if (self->screen == dst && self->screen == src)
  { /* send a screen blt */
//    Bpp = (dst->bpp + 7) / 8;
//    line_size = sw * Bpp;
//    s = src->data + (sy * src->width + sx) * Bpp;
//    d = dst->data + (dy * dst->width + dx) * Bpp;
//    for (i = 0; i < sh; i++)
//    {
//      //g_memcpy(d, s, line_size);
//      s += src->width * Bpp;
//      d += dst->width * Bpp;
//    }
    xrdp_orders_init(self->orders);
    xrdp_orders_screen_blt(self->orders, dx, dy, sw, sh, sx, sy, rop, 0);
    xrdp_orders_send(self->orders);
  }
  return 0;
}

/*****************************************************************************/
/* return true is rect is totaly exposed going in reverse z order */
/* from wnd up */
int xrdp_wm_is_rect_vis(struct xrdp_wm* self, struct xrdp_bitmap* wnd,
                        struct xrdp_rect* rect)
{
  struct xrdp_rect wnd_rect;
  struct xrdp_bitmap* b;
  int i;;

  /* if rect is part off screen */
  if (rect->left < 0)
  {
    return 0;
  }
  if (rect->top < 0)
  {
    return 0;
  }
  if (rect->right >= self->screen->width)
  {
    return 0;
  }
  if (rect->bottom >= self->screen->height)
  {
    return 0;
  }

  i = xrdp_list_index_of(self->screen->child_list, (long)wnd);
  i--;
  while (i >= 0)
  {
    b = (struct xrdp_bitmap*)xrdp_list_get_item(self->screen->child_list, i);
    MAKERECT(wnd_rect, b->left, b->top, b->width, b->height);
    if (rect_intersect(rect, &wnd_rect, 0))
    {
      return 0;
    }
    i--;
  }
  return 1;
}

/*****************************************************************************/
int xrdp_wm_move_window(struct xrdp_wm* self, struct xrdp_bitmap* wnd,
                        int dx, int dy)
{
  struct xrdp_rect rect1;
  struct xrdp_rect rect2;
  struct xrdp_region* r;
  int i;

  MAKERECT(rect1, wnd->left, wnd->top, wnd->width, wnd->height);
  if (xrdp_wm_is_rect_vis(self, wnd, &rect1))
  {
    rect2 = rect1;
    RECTOFFSET(rect2, dx, dy);
    if (xrdp_wm_is_rect_vis(self, wnd, &rect2))
    { /* if both src and dst are unobscured, we can do a bitblt move */
      xrdp_wm_bitblt(self, self->screen, wnd->left + dx, wnd->top + dy,
                     self->screen, wnd->left, wnd->top,
                     wnd->width, wnd->height, 0xcc);
      wnd->left += dx;
      wnd->top += dy;
      r = xrdp_region_create(self);
      xrdp_region_add_rect(r, &rect1);
      xrdp_region_subtract_rect(r, &rect2);
      i = 0;
      while (xrdp_region_get_rect(r, i, &rect1) == 0)
      {
        xrdp_bitmap_invalidate(self->screen, &rect1);
        i++;
      }
      xrdp_region_delete(r);
      return 0;
    }
  }
  wnd->left += dx;
  wnd->top += dy;
  xrdp_bitmap_invalidate(self->screen, &rect1);
  xrdp_bitmap_invalidate(wnd, 0);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_mouse_move(struct xrdp_wm* self, int x, int y)
{
  struct xrdp_bitmap* b;
  int boxx;
  int boxy;

  if (self == 0)
  {
    return 0;
  }
  if (x < 0)
  {
    x = 0;
  }
  if (y < 0)
  {
    y = 0;
  }
  if (x >= self->screen->width)
  {
    x = self->screen->width;
  }
  if (y >= self->screen->height)
  {
    y = self->screen->height;
  }
  self->mouse_x = x;
  self->mouse_y = y;
  if (self->dragging)
  {
    xrdp_painter_begin_update(self->painter);
    boxx = self->draggingx - self->draggingdx;
    boxy = self->draggingy - self->draggingdy;
    if (self->draggingxorstate)
    {
      xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
    }
    self->draggingx = x;
    self->draggingy = y;
    boxx = self->draggingx - self->draggingdx;
    boxy = self->draggingy - self->draggingdy;
    xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
    self->draggingxorstate = 1;
    xrdp_painter_end_update(self->painter);
    return 0;
  }
  b = xrdp_wm_at_pos(self->screen, x, y, 0);
  if (b == 0) /* if b is null, the movment must be over the screen */
  {
    if (self->screen->pointer != self->current_pointer)
    {
      xrdp_wm_set_pointer(self, self->screen->pointer);
      self->current_pointer = self->screen->pointer;
    }
    if (self->mod != 0) /* if screen is mod controled */
    {
      if (self->mod->mod_event != 0)
      {
        self->mod->mod_event(self->mod, WM_MOUSEMOVE, x, y);
      }
    }
  }
  if (self->button_down != 0)
  {
    if (b == self->button_down && self->button_down->state == 0)
    {
      self->button_down->state = 1;
      xrdp_bitmap_invalidate(self->button_down, 0);
    }
    else if (b != self->button_down)
    {
      self->button_down->state = 0;
      xrdp_bitmap_invalidate(self->button_down, 0);
    }
  }
  if (b != 0)
  {
    if (!self->dragging)
    {
      if (b->pointer != self->current_pointer)
      {
        xrdp_wm_set_pointer(self, b->pointer);
        self->current_pointer = b->pointer;
      }
      xrdp_bitmap_def_proc(b, WM_MOUSEMOVE,
                           xrdp_bitmap_from_screenx(b, x),
                           xrdp_bitmap_from_screeny(b, y));
      if (self->button_down == 0)
      {
        if (b->notify != 0)
        {
          b->notify(b->owner, b, 2, x, y);
        }
      }
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_clear_popup(struct xrdp_wm* self)
{
  int i;
  struct xrdp_rect rect;
  //struct xrdp_bitmap* b;

  //b = 0;
  if (self->popup_wnd != 0)
  {
    //b = self->popup_wnd->popped_from;
    i = xrdp_list_index_of(self->screen->child_list, (long)self->popup_wnd);
    xrdp_list_remove_item(self->screen->child_list, i);
    MAKERECT(rect, self->popup_wnd->left, self->popup_wnd->top,
             self->popup_wnd->width, self->popup_wnd->height);
    xrdp_bitmap_invalidate(self->screen, &rect);
    xrdp_bitmap_delete(self->popup_wnd);
  }
  //xrdp_wm_set_focused(self, b->parent);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_mouse_click(struct xrdp_wm* self, int x, int y, int but, int down)
{
  struct xrdp_bitmap* control;
  struct xrdp_bitmap* focus_out_control;
  struct xrdp_bitmap* wnd;
  int newx;
  int newy;
  int oldx;
  int oldy;

  if (self == 0)
  {
    return 0;
  }
  if (x < 0)
  {
    x = 0;
  }
  if (y < 0)
  {
    y = 0;
  }
  if (x >= self->screen->width)
  {
    x = self->screen->width;
  }
  if (y >= self->screen->height)
  {
    y = self->screen->height;
  }
  if (self->dragging && but == 1 && !down && self->dragging_window != 0)
  { /* if done dragging */
    self->draggingx = x;
    self->draggingy = y;
    newx = self->draggingx - self->draggingdx;
    newy = self->draggingy - self->draggingdy;
    oldx = self->dragging_window->left;
    oldy = self->dragging_window->top;
    /* draw xor box one more time */
    if (self->draggingxorstate)
    {
      xrdp_wm_xor_pat(self, newx, newy, self->draggingcx, self->draggingcy);
    }
    self->draggingxorstate = 0;
    /* move screen to new location */
    xrdp_wm_move_window(self, self->dragging_window, newx - oldx, newy - oldy);
    self->dragging_window = 0;
    self->dragging = 0;
  }
  wnd = 0;
  control = xrdp_wm_at_pos(self->screen, x, y, &wnd);
  if (control == 0)
  {
    if (self->mod != 0) /* if screen is mod controled */
    {
      if (self->mod->mod_event != 0)
      {
        if (but == 1 && down)
        {
          self->mod->mod_event(self->mod, WM_LBUTTONDOWN, x, y);
        }
        else if (but == 1 && !down)
        {
          self->mod->mod_event(self->mod, WM_LBUTTONUP, x, y);
        }
        if (but == 2 && down)
        {
          self->mod->mod_event(self->mod, WM_RBUTTONDOWN, x, y);
        }
        else if (but == 2 && !down)
        {
          self->mod->mod_event(self->mod, WM_RBUTTONUP, x, y);
        }
        if (but == 3 && down)
        {
          self->mod->mod_event(self->mod, WM_BUTTON3DOWN, x, y);
        }
        else if (but == 3 && !down)
        {
          self->mod->mod_event(self->mod, WM_BUTTON3UP, x, y);
        }
        if (but == 4)
        {
          self->mod->mod_event(self->mod, WM_BUTTON4DOWN,
                               self->mouse_x, self->mouse_y);
          self->mod->mod_event(self->mod, WM_BUTTON4UP,
                               self->mouse_x, self->mouse_y);
        }
        if (but == 5)
        {
          self->mod->mod_event(self->mod, WM_BUTTON5DOWN,
                               self->mouse_x, self->mouse_y);
          self->mod->mod_event(self->mod, WM_BUTTON5UP,
                               self->mouse_x, self->mouse_y);
        }
      }
    }
  }
  if (self->popup_wnd != 0)
  {
    if (self->popup_wnd == control && !down)
    {
      xrdp_bitmap_def_proc(self->popup_wnd, WM_LBUTTONUP, x, y);
      xrdp_wm_clear_popup(self);
      self->button_down = 0;
      return 0;
    }
    else if (self->popup_wnd != control && down)
    {
      xrdp_wm_clear_popup(self);
      self->button_down = 0;
      return 0;
    }
  }
  if (control != 0)
  {
    if (wnd != 0)
    {
      if (wnd->modal_dialog != 0) /* if window has a modal dialog */
      {
        return 0;
      }
      if (control == wnd)
      {
      }
      else if (control->tab_stop)
      {
        focus_out_control = wnd->focused_control;
        wnd->focused_control = control;
        xrdp_bitmap_invalidate(focus_out_control, 0);
        xrdp_bitmap_invalidate(control, 0);
      }
    }
    if ((control->type == WND_TYPE_BUTTON ||
         control->type == WND_TYPE_COMBO) &&
        but == 1 && !down && self->button_down == control)
    { /* if clicking up on a button that was clicked down */
      self->button_down = 0;
      control->state = 0;
      xrdp_bitmap_invalidate(control, 0);
      if (control->parent != 0)
      {
        if (control->parent->notify != 0)
        {
          /* control can be invalid after this */
          control->parent->notify(control->owner, control, 1, x, y);
        }
      }
    }
    else if ((control->type == WND_TYPE_BUTTON ||
              control->type == WND_TYPE_COMBO) &&
             but == 1 && down)
    { /* if clicking down on a button or combo */
      self->button_down = control;
      control->state = 1;
      xrdp_bitmap_invalidate(control, 0);
      if (control->type == WND_TYPE_COMBO)
      {
        xrdp_wm_pu(self, control);
      }
    }
    else if (but == 1 && down)
    {
      if (self->popup_wnd == 0)
      {
        xrdp_wm_set_focused(self, wnd);
        if (control->type == WND_TYPE_WND && y < (control->top + 21))
        { /* if dragging */
          if (self->dragging) /* rarely happens */
          {
            newx = self->draggingx - self->draggingdx;
            newy = self->draggingy - self->draggingdy;
            if (self->draggingxorstate)
            {
              xrdp_wm_xor_pat(self, newx, newy,
                              self->draggingcx, self->draggingcy);
            }
            self->draggingxorstate = 0;
          }
          self->dragging = 1;
          self->dragging_window = control;
          self->draggingorgx = control->left;
          self->draggingorgy = control->top;
          self->draggingx = x;
          self->draggingy = y;
          self->draggingdx = x - control->left;
          self->draggingdy = y - control->top;
          self->draggingcx = control->width;
          self->draggingcy = control->height;
        }
      }
    }
  }
  else
  {
    xrdp_wm_set_focused(self, 0);
  }
  /* no matter what, mouse is up, reset button_down */
  if (but == 1 && !down && self->button_down != 0)
  {
    self->button_down = 0;
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_key(struct xrdp_wm* self, int device_flags, int scan_code)
{
  int msg;
  char c;

  scan_code = scan_code % 128;
  if (self->popup_wnd != 0)
  {
    xrdp_wm_clear_popup(self);
    return 0;
  }
  if (device_flags & 0x8000) /* key up */
  {
    self->keys[scan_code] = 0;
    msg = WM_KEYUP;
  }
  else /* key down */
  {
    self->keys[scan_code] = 1;
    msg = WM_KEYDOWN;
    switch (scan_code)
    {
      case 58: self->caps_lock = !self->caps_lock; break; /* caps lock */
      case 69: self->num_lock = !self->num_lock; break; /* num lock */
      case 70: self->scroll_lock = !self->scroll_lock; break; /* scroll lock */
    }
  }
  if (self->mod != 0)
  {
    if (self->mod->mod_event != 0)
    {
      c = get_char_from_scan_code(device_flags, scan_code, self->keys,
                                  self->caps_lock,
                                  self->num_lock,
                                  self->scroll_lock);
      if (c != 0)
      {
        self->mod->mod_event(self->mod, msg, c, 0xffff);
      }
      else
      {
        self->mod->mod_event(self->mod, msg, scan_code, device_flags);
      }
    }
  }
  else if (self->focused_window != 0)
  {
    xrdp_bitmap_def_proc(self->focused_window,
                         msg, scan_code, device_flags);
  }
  return 0;
}

/*****************************************************************************/
/* happens when client gets focus and sends key modifier info */
int xrdp_wm_key_sync(struct xrdp_wm* self, int device_flags, int key_flags)
{
  self->num_lock = 0;
  self->scroll_lock = 0;
  self->caps_lock = 0;
  if (key_flags & 1)
  {
    self->scroll_lock = 1;
  }
  if (key_flags & 2)
  {
    self->num_lock = 1;
  }
  if (key_flags & 4)
  {
    self->caps_lock = 1;
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_pu(struct xrdp_wm* self, struct xrdp_bitmap* control)
{
  int x;
  int y;

  if (self == 0)
  {
    return 0;
  }
  if (control == 0)
  {
    return 0;
  }
  self->popup_wnd = xrdp_bitmap_create(control->width, 100,
                                       self->screen->bpp,
                                       WND_TYPE_SPECIAL, self);
  self->popup_wnd->popped_from = control;
  self->popup_wnd->parent = self->screen;
  self->popup_wnd->owner = self->screen;
  x = xrdp_bitmap_to_screenx(control, 0);
  y = xrdp_bitmap_to_screeny(control, 0);
  self->popup_wnd->left = x;
  self->popup_wnd->top = y + control->height;
  self->popup_wnd->item_index = control->item_index;
  xrdp_list_insert_item(self->screen->child_list, 0, (long)self->popup_wnd);
  xrdp_bitmap_invalidate(self->popup_wnd, 0);
  return 0;
}
