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
   Copyright (C) Jay Sorg 2004-2007

   painter, gc

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_painter* APP_CC
xrdp_painter_create(struct xrdp_wm* wm, struct xrdp_session* session)
{
  struct xrdp_painter* self;

  self = (struct xrdp_painter*)g_malloc(sizeof(struct xrdp_painter), 1);
  self->wm = wm;
  self->session = session;
  self->rop = 0xcc; /* copy gota use 0xcc*/
  self->clip_children = 1;
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_painter_delete(struct xrdp_painter* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_font_delete(self->font);
  g_free(self);
}

/*****************************************************************************/
int APP_CC
xrdp_painter_begin_update(struct xrdp_painter* self)
{
  libxrdp_orders_init(self->session);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_end_update(struct xrdp_painter* self)
{
  libxrdp_orders_send(self->session);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_font_needed(struct xrdp_painter* self)
{
  if (self->font == 0)
  {
    self->font = xrdp_font_create(self->wm);
  }
  return 0;
}

/*****************************************************************************/
/* returns boolean, true if there is something to draw */
static int APP_CC
xrdp_painter_clip_adj(struct xrdp_painter* self, int* x, int* y,
                      int* cx, int* cy)
{
  int dx;
  int dy;

  if (!self->use_clip)
  {
    return 1;
  }
  if (self->clip.left > *x)
  {
    dx = self->clip.left - *x;
  }
  else
  {
    dx = 0;
  }
  if (self->clip.top > *y)
  {
    dy = self->clip.top - *y;
  }
  else
  {
    dy = 0;
  }
  if (*x + *cx > self->clip.right)
  {
    *cx = *cx - ((*x + *cx) - self->clip.right);
  }
  if (*y + *cy > self->clip.bottom)
  {
    *cy = *cy - ((*y + *cy) - self->clip.bottom);
  }
  *cx = *cx - dx;
  *cy = *cy - dy;
  if (*cx <= 0)
  {
    return 0;
  }
  if (*cy <= 0)
  {
    return 0;
  }
  *x = *x + dx;
  *y = *y + dy;
  return 1;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_set_clip(struct xrdp_painter* self,
                      int x, int y, int cx, int cy)
{
  self->use_clip = &self->clip;
  self->clip.left = x;
  self->clip.top = y;
  self->clip.right = x + cx;
  self->clip.bottom = y + cy;
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_clr_clip(struct xrdp_painter* self)
{
  self->use_clip = 0;
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_painter_rop(int rop, int src, int dst)
{
  switch (rop & 0x0f)
  {
    case 0x0: return 0;
    case 0x1: return ~(src | dst);
    case 0x2: return (~src) & dst;
    case 0x3: return ~src;
    case 0x4: return src & (~dst);
    case 0x5: return ~(dst);
    case 0x6: return src ^ dst;
    case 0x7: return ~(src & dst);
    case 0x8: return src & dst;
    case 0x9: return ~(src) ^ dst;
    case 0xa: return dst;
    case 0xb: return (~src) | dst;
    case 0xc: return src;
    case 0xd: return src | (~dst);
    case 0xe: return src | dst;
    case 0xf: return ~0;
  }
  return dst;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_text_width(struct xrdp_painter* self, char* text)
{
  int index;
  int rv;
  int len;
  struct xrdp_font_char* font_item;

  xrdp_painter_font_needed(self);
  if (text == 0)
  {
    return 0;
  }
  rv = 0;
  len = g_strlen(text);
  for (index = 0; index < len; index++)
  {
    font_item = self->font->font_items + (unsigned char)text[index];
    rv = rv + font_item->incby;
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_text_height(struct xrdp_painter* self, char* text)
{
  int index;
  int rv;
  int len;
  struct xrdp_font_char* font_item;

  xrdp_painter_font_needed(self);
  if (text == 0)
  {
    return 0;
  }
  rv = 0;
  len = g_strlen(text);
  for (index = 0; index < len; index++)
  {
    font_item = self->font->font_items + (unsigned char)text[index];
    rv = MAX(rv, font_item->height);
  }
  return rv;
}

/*****************************************************************************/
/* fill in an area of the screen with one color */
int APP_CC
xrdp_painter_fill_rect(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, int cx, int cy)
{
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_rect rect;
  struct xrdp_region* region;
  int k;
  int dx;
  int dy;
  int rop;

  if (self == 0)
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP) /* 0 */
  {
    return 0;
  }
  xrdp_bitmap_get_screen_clip(bitmap, self, &clip_rect, &dx, &dy);
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region,
                         self->clip_children);
  x += dx;
  y += dy;
  if (self->mix_mode == 0 && self->rop == 0xcc)
  {
    k = 0;
    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
      if (rect_intersect(&rect, &clip_rect, &draw_rect))
      {
        libxrdp_orders_rect(self->session, x, y, cx, cy,
                            self->fg_color, &draw_rect);
      }
      k++;
    }
  }
  else if (self->mix_mode == 0 &&
             ((self->rop & 0xf) == 0x0 || /* black */
              (self->rop & 0xf) == 0xf || /* white */
              (self->rop & 0xf) == 0x5))  /* DSTINVERT */
  {
    k = 0;
    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
      if (rect_intersect(&rect, &clip_rect, &draw_rect))
      {
        libxrdp_orders_dest_blt(self->session, x, y, cx, cy,
                                self->rop, &draw_rect);
      }
      k++;
    }
  }
  else
  {
    k = 0;
    rop = self->rop;
    /* if opcode is in the form 0x00, 0x11, 0x22, ... convert it */
    if (((rop & 0xf0) >> 4) == (rop & 0xf))
    {
      switch (rop)
      {
        case 0x66: /* xor */
          rop = 0x5a;
          break;
        case 0xaa: /* noop */
          rop = 0xfb;
          break;
        case 0xcc: /* copy */
          rop = 0xf0;
          break;
        case 0x88: /* and */
          rop = 0xc0;
          break;
      }
    }
    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
      if (rect_intersect(&rect, &clip_rect, &draw_rect))
      {
        libxrdp_orders_pat_blt(self->session, x, y, cx, cy,
                               rop, self->bg_color, self->fg_color,
                               &self->brush, &draw_rect);
      }
      k++;
    }
  }
  xrdp_region_delete(region);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_draw_text(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, const char* text)
{
  int i;
  int f;
  int c;
  int k;
  int x1;
  int y1;
  int flags;
  int len;
  int index;
  int total_width;
  int total_height;
  int dx;
  int dy;
  char* data;
  struct xrdp_region* region;
  struct xrdp_rect rect;
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_font* font;
  struct xrdp_font_char* font_item;

  if (self == 0)
  {
    return 0;
  }
  len = g_strlen(text);
  if (len < 1)
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == 0)
  {
    return 0;
  }
  xrdp_painter_font_needed(self);
  font = self->font;
  f = 0;
  k = 0;
  total_width = 0;
  total_height = 0;
  data = (char*)g_malloc(len * 4, 1);
  for (index = 0; index < len; index++)
  {
    font_item = font->font_items + (unsigned char)text[index];
    i = xrdp_cache_add_char(self->wm->cache, font_item);
    f = HIWORD(i);
    c = LOWORD(i);
    data[index * 2] = c;
    data[index * 2 + 1] = k;
    k = font_item->incby;
    total_width += k;
    total_height = MAX(total_height, font_item->height);
  }
  xrdp_bitmap_get_screen_clip(bitmap, self, &clip_rect, &dx, &dy);
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, total_width, total_height,
                         region, self->clip_children);
  x += dx;
  y += dy;
  k = 0;
  while (xrdp_region_get_rect(region, k, &rect) == 0)
  {
    if (rect_intersect(&rect, &clip_rect, &draw_rect))
    {
      x1 = x;
      y1 = y + total_height;
      flags = 0x03; /* 0x03 0x73; TEXT2_IMPLICIT_X and something else */
      libxrdp_orders_text(self->session, f, flags, 0,
                          font->color, 0,
                          x - 1, y - 1, x + total_width, y + total_height,
                          0, 0, 0, 0,
                          x1, y1, data, len * 2, &draw_rect);
    }
    k++;
  }
  xrdp_region_delete(region);
  g_free(data);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_draw_text2(struct xrdp_painter* self,
                        struct xrdp_bitmap* bitmap,
                        int font, int flags, int mixmode,
                        int clip_left, int clip_top,
                        int clip_right, int clip_bottom,
                        int box_left, int box_top,
                        int box_right, int box_bottom,
                        int x, int y, char* data, int data_len)
{
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_rect rect;
  struct xrdp_region* region;
  int k;
  int dx;
  int dy;

  if (self == 0)
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP)
  {
    return 0;
  }
  xrdp_bitmap_get_screen_clip(bitmap, self, &clip_rect, &dx, &dy);
  region = xrdp_region_create(self->wm);
  if (box_right - box_left > 1)
  {
    xrdp_wm_get_vis_region(self->wm, bitmap, box_left, box_top,
                           box_right - box_left, box_bottom - box_top,
                           region, self->clip_children);
  }
  else
  {
    xrdp_wm_get_vis_region(self->wm, bitmap, clip_left, clip_top,
                           clip_right - clip_left, clip_bottom - clip_top,
                           region, self->clip_children);
  }
  clip_left += dx;
  clip_top += dy;
  clip_right += dx;
  clip_bottom += dy;
  box_left += dx;
  box_top += dy;
  box_right += dx;
  box_bottom += dy;
  x += dx;
  y += dy;
  k = 0;
  while (xrdp_region_get_rect(region, k, &rect) == 0)
  {
    if (rect_intersect(&rect, &clip_rect, &draw_rect))
    {
      libxrdp_orders_text(self->session, font, flags, mixmode,
                          self->fg_color, self->bg_color,
                          clip_left, clip_top, clip_right, clip_bottom,
                          box_left, box_top, box_right, box_bottom,
                          x, y, data, data_len, &draw_rect);
    }
    k++;
  }
  xrdp_region_delete(region);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_copy(struct xrdp_painter* self,
                  struct xrdp_bitmap* src,
                  struct xrdp_bitmap* dst,
                  int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_rect rect1;
  struct xrdp_rect rect2;
  struct xrdp_region* region;
  struct xrdp_bitmap* b;
  int i;
  int j;
  int k;
  int dx;
  int dy;
  int palette_id;
  int bitmap_id;
  int cache_id;
  int cache_idx;
  int dstx;
  int dsty;
  int w;
  int h;

  if (self == 0 || src == 0 || dst == 0)
  {
    return 0;
  }

  /* todo data */

  if (dst->type == WND_TYPE_BITMAP)
  {
    return 0;
  }
  if (src == dst && src->wm->screen == src)
  {
    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);
    xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                           region, self->clip_children);
    x += dx;
    y += dy;
    srcx += dx;
    srcy += dy;
    k = 0;
    while (xrdp_region_get_rect(region, k, &rect1) == 0)
    {
      if (rect_intersect(&rect1, &clip_rect, &draw_rect))
      {
        libxrdp_orders_screen_blt(self->session, x, y, cx, cy,
                                  srcx, srcy, self->rop, &draw_rect);
      }
      k++;
    }
    xrdp_region_delete(region);
  }
  else if (src->data != 0)
  /* todo, the non bitmap cache part is gone, it should be put back */
  {
    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);
    xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                           region, self->clip_children);
    x += dx;
    y += dy;
    palette_id = 0;
    j = srcy;
    while (j < src->height)
    {
      i = srcx;
      while (i < src->width)
      {
        w = MIN(64, src->width - i);
        h = MIN(64, src->height - j);
        b = xrdp_bitmap_create(w, h, self->wm->screen->bpp, 0, self->wm);
        xrdp_bitmap_copy_box_with_crc(src, b, i, j, w, h);
        bitmap_id = xrdp_cache_add_bitmap(self->wm->cache, b);
        cache_id = HIWORD(bitmap_id);
        cache_idx = LOWORD(bitmap_id);
        dstx = (x + i) - srcx;
        dsty = (y + j) - srcy;
        k = 0;
        while (xrdp_region_get_rect(region, k, &rect1) == 0)
        {
          if (rect_intersect(&rect1, &clip_rect, &rect2))
          {
            MAKERECT(rect1, dstx, dsty, w, h);
            if (rect_intersect(&rect2, &rect1, &draw_rect))
            {
              libxrdp_orders_mem_blt(self->session, cache_id, palette_id,
                                     dstx, dsty, w, h, self->rop, 0, 0,
                                     cache_idx, &draw_rect);
            }
          }
          k++;
        }
        i += 64;
      }
      j += 64;
    }
    xrdp_region_delete(region);
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_line(struct xrdp_painter* self,
                  struct xrdp_bitmap* bitmap,
                  int x1, int y1, int x2, int y2)
{
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_rect rect;
  struct xrdp_region* region;
  int k;
  int dx;
  int dy;
  int rop;

  if (self == 0)
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP)
  {
    return 0;
  }
  xrdp_bitmap_get_screen_clip(bitmap, self, &clip_rect, &dx, &dy);
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, MIN(x1, x2), MIN(y1, y2),
                         g_abs(x1 - x2) + 1, g_abs(y1 - y2) + 1,
                         region, self->clip_children);
  x1 += dx;
  y1 += dy;
  x2 += dx;
  y2 += dy;
  k = 0;
  rop = self->rop;
  if (rop < 0x01 || rop > 0x10)
  {
    rop = (rop & 0xf) + 1;
  }
  while (xrdp_region_get_rect(region, k, &rect) == 0)
  {
    if (rect_intersect(&rect, &clip_rect, &draw_rect))
    {
      libxrdp_orders_line(self->session, 0, x1, y1, x2, y2,
                          rop, self->bg_color,
                          &self->pen, &draw_rect);
    }
    k++;
  }
  xrdp_region_delete(region);
  return 0;
}
