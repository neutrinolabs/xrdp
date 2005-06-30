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
  self->rop = 0xcc; /* copy */
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
int APP_CC
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
  self->use_clip = 1;
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
int APP_CC
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
/* fill in an area of the screen with one color */
int APP_CC
xrdp_painter_fill_rect(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, int cx, int cy)
{
  int i;
  struct xrdp_region* region;
  struct xrdp_rect rect;

  if (!check_bounds(bitmap, &x, &y, &cx, &cy))
  {
    return 0;
  }
  if (!xrdp_painter_clip_adj(self, &x, &y, &cx, &cy))
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP) /* 0 */
  {
    return 0;
  }
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region,
                         self->clip_children);
  i = 0;
  while (xrdp_region_get_rect(region, i, &rect) == 0)
  {
    if (!ISRECTEMPTY(rect))
    {
      DEBUG(("sending rect order %d %d %d %d\n\r",
             rect.left, rect.top,
             rect.right, rect.bottom));
      libxrdp_orders_rect(self->session, rect.left, rect.top,
                          rect.right - rect.left,
                          rect.bottom - rect.top,
                          self->fg_color, 0);
    }
    i++;
  }
  xrdp_region_delete(region);
  return 0;
}

/*****************************************************************************/
/* fill in an area of the screen with opcodes and patterns */
/* todo, this needs work */
int APP_CC
xrdp_painter_fill_rect2(struct xrdp_painter* self,
                        struct xrdp_bitmap* bitmap,
                        int x, int y, int cx, int cy)
{
  int i;
  struct xrdp_region* region;
  struct xrdp_rect rect;

  if (!check_bounds(bitmap, &x, &y, &cx, &cy))
  {
    return 0;
  }
  if (!xrdp_painter_clip_adj(self, &x, &y, &cx, &cy))
  {
    return 0;
  }

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP) /* 0 */
  {
    return 0;
  }
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region,
                         self->clip_children);
  i = 0;
  while (xrdp_region_get_rect(region, i, &rect) == 0)
  {
    if (!ISRECTEMPTY(rect))
    {
      DEBUG(("sending rect2 order %d %d %d %d\n\r",
             rect.left, rect.top,
             rect.right, rect.bottom));
      libxrdp_orders_pat_blt(self->session, rect.left, rect.top,
                             rect.right - rect.left,
                             rect.bottom - rect.top,
                             self->rop, self->bg_color, self->fg_color,
                             &self->brush, 0);
    }
    i++;
  }
  xrdp_region_delete(region);
  return 0;
}

#define SSW 64
#define SSH 60

/*****************************************************************************/
int APP_CC
xrdp_painter_draw_bitmap(struct xrdp_painter* self,
                         struct xrdp_bitmap* bitmap,
                         struct xrdp_bitmap* to_draw,
                         int x, int y, int cx, int cy)
{
  int i;
  int j;
  int k;
  int w;
  int h;
  int x1;
  int y1;
  int ok;
  int srcx;
  int srcy;
  int bitmap_id;
  int cache_id;
  int cache_idx;
  int palette_id;
  struct xrdp_region* region;
  struct xrdp_rect rect;
  struct xrdp_rect rect1;
  struct xrdp_rect rect2;
  struct xrdp_bitmap* b;

  /* todo data */

  if (bitmap->type == WND_TYPE_BITMAP)
  {
    return 0;
  }
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region,
                         self->clip_children);
  b = bitmap;
  while (b != 0)
  {
    x = x + b->left;
    y = y + b->top;
    b = b->parent;
  }
  if (self->wm->client_info->use_bitmap_cache)
  {
    /*palette_id = xrdp_cache_add_palette(self->wm->cache, self->wm->palette);*/
    palette_id = 0;
    j = 0;
    while (j < to_draw->height)
    {
      i = 0;
      while (i < to_draw->width)
      {
        x1 = x + i;
        y1 = y + j;
        w = MIN(SSW, to_draw->width - i);
        h = MIN(SSH, to_draw->height - j);
        b = xrdp_bitmap_create(w, h, self->wm->screen->bpp, 0, self->wm);
#ifdef USE_CRC
        xrdp_bitmap_copy_box_with_crc(to_draw, b, i, j, w, h);
#else
        xrdp_bitmap_copy_box(to_draw, b, i, j, w, h);
#endif
        bitmap_id = xrdp_cache_add_bitmap(self->wm->cache, b);
        cache_id = HIWORD(bitmap_id);
        cache_idx = LOWORD(bitmap_id);
        k = 0;
        while (xrdp_region_get_rect(region, k, &rect) == 0)
        {
          if (!ISRECTEMPTY(rect))
          {
            MAKERECT(rect1, x1, y1, w, h);
            if (rect_intersect(&rect, &rect1, &rect2))
            {
              ok = 1;
              if (self->use_clip)
              {
                rect = self->clip;
                RECTOFFSET(rect, x, y);
                if (!rect_intersect(&rect2, &rect, &rect1))
                {
                  ok = 0;
                }
              }
              else
              {
                rect1 = rect2;
              }
              if (ok)
              {
                rect1.right--;
                rect1.bottom--;
                /* check these so ms client don't crash */
                if (x1 + w >= self->wm->screen->width)
                {
                  w = self->wm->screen->width - x1;
                }
                if (y1 + h >= self->wm->screen->height)
                {
                  h = self->wm->screen->height - y1;
                }
                if (w > 0 && h > 0 && x1 + w > 0 && y1 + h > 0)
                {
                  srcx = 0;
                  srcy = 0;
                  if (x1 < 0)
                  {
                    w = w + x1;
                    srcx = srcx - x1;
                    x1 = 0;
                  }
                  if (y1 < 0)
                  {
                    h = h + y1;
                    srcy = srcy - y1;
                    y1 = 0;
                  }
                  //g_printf("%d %d %d %d %d %d\n", x1, y1, w, h, srcx, srcy);
                  DEBUG(("sending memblt order \n\r\
  cache_id %d\n\r\
  palette_id %d\n\r\
  x %d\n\r\
  y %d\n\r\
  cx %d\n\r\
  cy %d\n\r\
  rop %d\n\r\
  srcx %d\n\r\
  srcy %d\n\r\
  cache_idx %d\n\r",
                         cache_id, palette_id,
                         x1, y1, w, h, self->rop, srcx, srcy,
                         cache_idx));
                  libxrdp_orders_mem_blt(self->session, cache_id, palette_id,
                                         x1, y1, w, h, self->rop, srcx, srcy,
                                         cache_idx, &rect1);
                }
              }
            }
          }
          k++;
        }
        i += SSW;
      }
      j += SSH;
    }
  }
  else /* no bitmap cache */
  {
    /* make sure there is no waiting orders */
    libxrdp_orders_force_send(self->session);
    k = 0;
    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
      x1 = rect.left;
      y1 = rect.top;
      w = rect.right - rect.left;
      h = rect.bottom - rect.top;
      b = xrdp_bitmap_create(w, h, self->wm->screen->bpp, 0, self->wm);
      xrdp_bitmap_copy_box(to_draw, b, x1 - x, y1 - y, w, h);
      xrdp_wm_send_bitmap(self->wm, b, x1, y1, w, h);
      xrdp_bitmap_delete(b);
      k++;
    }
  }
  xrdp_region_delete(region);
  return 0;
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
int APP_CC
xrdp_painter_draw_text(struct xrdp_painter* self,
                       struct xrdp_bitmap* bitmap,
                       int x, int y, char* text)
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
  char* data;
  struct xrdp_region* region;
  struct xrdp_rect rect;
  struct xrdp_rect clip_rect;
  struct xrdp_rect draw_rect;
  struct xrdp_bitmap* b;
  struct xrdp_font* font;
  struct xrdp_font_char* font_item;

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
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, total_width, total_height,
                         region, self->clip_children);
  b = bitmap;
  while (b != 0)
  {
    x = x + b->left;
    y = y + b->top;
    b = b->parent;
  }
  if (self->use_clip)
  {
    clip_rect = self->clip;
  }
  else
  {
    MAKERECT(clip_rect, 0, 0, bitmap->width, bitmap->height);
  }
  b = bitmap;
  while (b != 0)
  {
    RECTOFFSET(clip_rect, b->left, b->top);
    b = b->parent;
  }
  k = 0;
  while (xrdp_region_get_rect(region, k, &rect) == 0)
  {
    if (!ISRECTEMPTY(rect))
    {
      if (rect_intersect(&rect, &clip_rect, &draw_rect))
      {
        x1 = x;
        y1 = y + total_height;
        draw_rect.right--;
        draw_rect.bottom--;
        flags = 0x03; /* 0x03 0x73; TEXT2_IMPLICIT_X and something else */
        DEBUG(("sending text order\n\r\
  font %d\n\r\
  flags %d\n\r\
  mixmode %d\n\r\
  color1 %d\n\r\
  color2 %d\n\r\
  clip box %d %d %d %d\n\r\
  box box %d %d %d %d\n\r\
  x %d\n\r\
  y %d\n\r\
  len %d\n\r\
  rect %d %d %d %d\n\r",
               f, flags, 0, font->color, 0, x, y,
               x + total_width, y + total_height,
               0, 0, 0, 0, x1, y1, len,
               draw_rect.left, draw_rect.top,
               draw_rect.right, draw_rect.bottom));
        libxrdp_orders_text(self->session, f, flags, 0,
                            font->color, 0,
                            x - 1, y - 1, x + total_width, y + total_height,
                            0, 0, 0, 0,
                            x1, y1, data, len * 2, &draw_rect);
      }
    }
    k++;
  }
  xrdp_region_delete(region);
  g_free(data);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_painter_copy(struct xrdp_painter* self,
                  struct xrdp_bitmap* src,
                  struct xrdp_bitmap* dst,
                  int x, int y, int cx, int cy,
                  int srcx, int srcy, int opcode)
{
  if (self == 0 || src == 0 || dst == 0)
  {
    return 0;
  }

  /* todo data */

  if (dst->type == WND_TYPE_BITMAP)
  {
    return 0;
  }
  if (src == dst && opcode == 12 && src->wm->screen == src)
  {
    libxrdp_orders_screen_blt(dst->wm->session, x, y, cx, cy,
                              srcx, srcy, 12, 0);
  }
  return 0;
}
