
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

   Copyright (C) Jay Sorg 2004

   painter, gc

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_painter* xrdp_painter_create(struct xrdp_wm* wm)
{
  struct xrdp_painter* self;

  self = (struct xrdp_painter*)g_malloc(sizeof(struct xrdp_painter), 1);
  self->wm = wm;
  self->orders = wm->orders;
  self->rop = 0xcc; /* copy */
  return self;
}

/*****************************************************************************/
void xrdp_painter_delete(struct xrdp_painter* self)
{
  if (self == 0)
    return;
  g_free(self);
}

/*****************************************************************************/
int xrdp_painter_begin_update(struct xrdp_painter* self)
{
  xrdp_orders_init(self->orders);
  return 0;
}

/*****************************************************************************/
int xrdp_painter_end_update(struct xrdp_painter* self)
{
  xrdp_orders_send(self->orders);
  return 0;
}

/*****************************************************************************/
int xrdp_painter_clip_adj(struct xrdp_painter* self, int* x, int* y,
                          int* cx, int* cy)
{
  int dx;
  int dy;

  if (!self->use_clip)
    return 1;
  if (self->clip.left > *x)
    dx = self->clip.left - *x;
  else
    dx = 0;
  if (self->clip.top > *y)
    dy = self->clip.top - *y;
  else
    dy = 0;
  if (*x + *cx > self->clip.right)
    *cx = *cx - ((*x + *cx) - self->clip.right);
  if (*y + *cy > self->clip.bottom)
    *cy = *cy - ((*y + *cy) - self->clip.bottom);
  *cx = *cx - dx;
  *cy = *cy - dy;
  if (*cx <= 0)
    return 0;
  if (*cy <= 0)
    return 0;
  *x = *x + dx;
  *y = *y + dy;
  return 1;
}

/*****************************************************************************/
int xrdp_painter_set_clip(struct xrdp_painter* self,
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
int xrdp_painter_clr_clip(struct xrdp_painter* self)
{
  self->use_clip = 0;
  return 0;
}

/*****************************************************************************/
int xrdp_painter_rop(int rop, int src, int dst)
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
int xrdp_painter_fill_rect(struct xrdp_painter* self,
                           struct xrdp_bitmap* bitmap,
                           int x, int y, int cx, int cy)
{
  int i;
  struct xrdp_region* region;
  struct xrdp_rect rect;

  if (x >= bitmap->width) return 0;
  if (y >= bitmap->height) return 0;
  if (x < 0) { cx += x; x = 0; }
  if (y < 0) { cy += y; y = 0; }
  if (cx <= 0) return 0;
  if (cy <= 0) return 0;
  if (x + cx > bitmap->width) cx = bitmap->width - x;
  if (y + cy > bitmap->height) cy = bitmap->height - y;

  if (!xrdp_painter_clip_adj(self, &x, &y, &cx, &cy))
    return 0;

  if (bitmap->type == 0)
    return 0;
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region);
  i = 0;
  while (xrdp_region_get_rect(region, i, &rect) == 0)
  {
    DEBUG(("sending rect order %d %d %d %d\n\r", rect.left, rect.top,
           rect.right, rect.bottom));
    xrdp_orders_rect(self->orders, rect.left, rect.top,
                     rect.right - rect.left,
                     rect.bottom - rect.top,
                     self->fg_color, 0);
    i++;
  }
  xrdp_region_delete(region);
  return 0;
}

/*****************************************************************************/
/* fill in an area of the screen with opcodes and patterns */
/* todo, this needs work */
int xrdp_painter_fill_rect2(struct xrdp_painter* self,
                            struct xrdp_bitmap* bitmap,
                            int x, int y, int cx, int cy)
{
  int i;
  struct xrdp_region* region;
  struct xrdp_rect rect;

  if (x >= bitmap->width) return 0;
  if (y >= bitmap->height) return 0;
  if (x < 0) { cx += x; x = 0; }
  if (y < 0) { cy += y; y = 0; }
  if (cx <= 0) return 0;
  if (cy <= 0) return 0;
  if (x + cx > bitmap->width) cx = bitmap->width - x;
  if (y + cy > bitmap->height) cy = bitmap->height - y;

  if (!xrdp_painter_clip_adj(self, &x, &y, &cx, &cy))
    return 0;

  if (bitmap->type == 0) /* bitmap */
    return 0;
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region);
  i = 0;
  while (xrdp_region_get_rect(region, i, &rect) == 0)
  {
    DEBUG(("sending rect order %d %d %d %d\n\r", rect.left, rect.top,
           rect.right, rect.bottom));
    xrdp_orders_pat_blt(self->orders, rect.left, rect.top,
                        rect.right - rect.left,
                        rect.bottom - rect.top,
                        self->rop, self->bg_color, self->fg_color,
                        &self->brush, 0);
    i++;
  }
  xrdp_region_delete(region);

  return 0;
}

#define SS 16

/*****************************************************************************/
int xrdp_painter_draw_bitmap(struct xrdp_painter* self,
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

  if (bitmap->type == 0)
    return 0;
  region = xrdp_region_create(self->wm);
  xrdp_wm_get_vis_region(self->wm, bitmap, x, y, cx, cy, region);
  b = bitmap;
  while (b != 0)
  {
    x = x + b->left;
    y = y + b->top;
    b = b->parent;
  }
  palette_id = xrdp_cache_add_palette(self->wm->cache, self->wm->palette);
  i = 0;
  while (i < to_draw->width)
  {
    j = 0;
    while (j < to_draw->height)
    {
      x1 = x + i;
      y1 = y + j;
      MIN(w, SS, to_draw->width - i);
      MIN(h, SS, to_draw->height - j);
      b = xrdp_bitmap_create(w, h, self->wm->screen->bpp, 0);
      xrdp_bitmap_copy_box(to_draw, b, i, j, w, h);
      bitmap_id = xrdp_cache_add_bitmap(self->wm->cache, b);
      HIWORD(cache_id, bitmap_id);
      LOWORD(cache_idx, bitmap_id);
      k = 0;
      while (xrdp_region_get_rect(region, k, &rect) == 0)
      {
        xrdp_wm_rect(&rect1, x1, y1, w, h);
        if (xrdp_wm_rect_intersect(&rect, &rect1, &rect2))
        {
          ok = 1;
          if (self->use_clip)
          {
            rect = self->clip;
            xrdp_wm_rect_offset(&rect, x, y);
            if (!xrdp_wm_rect_intersect(&rect2, &rect, &rect1))
              ok = 0;
          }
          else
            rect1 = rect2;
          if (ok)
          {
            rect1.right--;
            rect1.bottom--;
            /* check these so ms client don't crash */
            if (x1 + w >= self->wm->screen->width)
              w = self->wm->screen->width - x1;
            if (y1 + h >= self->wm->screen->height)
              h = self->wm->screen->height - y1;
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
              xrdp_orders_mem_blt(self->orders, cache_id, palette_id,
                                  x1, y1, w, h, self->rop, srcx, srcy,
                                  cache_idx, &rect1);
            }
          }
        }
        k++;
      }
      xrdp_bitmap_delete(b);
      j += SS;
    }
    i += SS;
  }
  xrdp_region_delete(region);
  return 0;
}
