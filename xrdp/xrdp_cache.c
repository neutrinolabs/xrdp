
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

   cache

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_cache* xrdp_cache_create(struct xrdp_wm* owner,
                                     struct xrdp_orders* orders)
{
  struct xrdp_cache* self;

  self = (struct xrdp_cache*)g_malloc(sizeof(struct xrdp_cache), 1);
  self->wm = owner;
  self->orders = orders;
  return self;
}

/*****************************************************************************/
void xrdp_cache_delete(struct xrdp_cache* self)
{
  int i;
  int j;

  if (self == 0)
    return;
  /* free all the cached bitmaps */
  for (i = 0; i < 3; i++)
    for (j = 0; j < 600; j++)
      xrdp_bitmap_delete(self->bitmap_items[i][j].bitmap);
  g_free(self);
}

/*****************************************************************************/
/* returns cache id */
int xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap)
{
  int i;
  int j;
  int min_use;
  int min_use_index1;
  int min_use_index2;
  struct xrdp_bitmap* b;

  /* look for match */
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 600; j++)
    {
      if (xrdp_bitmap_compare(self->bitmap_items[i][j].bitmap, bitmap))
      {
        self->bitmap_items[i][j].use_count++;
        DEBUG(("found bitmap at %d %d\n", i, j));
        return (i << 16) | j;
      }
    }
  }
  /* look for least used */
  min_use_index1 = 0;
  min_use_index2 = 0;
  min_use = 999999;
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 600; j++)
    {
      if (self->bitmap_items[i][j].use_count < min_use)
      {
        min_use = self->bitmap_items[i][j].use_count;
        min_use_index1 = i;
        min_use_index2 = j;
      }
    }
  }
  DEBUG(("adding bitmap at %d %d\n", min_use_index1, min_use_index2));
  /* set, send bitmap and return */
  xrdp_bitmap_delete(self->bitmap_items[min_use_index1]
                                       [min_use_index2].bitmap);
  b = xrdp_bitmap_create(bitmap->width, bitmap->height, bitmap->bpp, 0);
  xrdp_bitmap_copy_box(bitmap, b, 0, 0, bitmap->width, bitmap->height);
  self->bitmap_items[min_use_index1][min_use_index2].bitmap = b;
  self->bitmap_items[min_use_index1][min_use_index2].use_count++;
  xrdp_orders_send_raw_bitmap(self->orders, b, min_use_index1,
                              min_use_index2);
  return (min_use_index1 << 16) | min_use_index2;
}

/*****************************************************************************/
int xrdp_cache_add_palette(struct xrdp_cache* self, int* palette)
{
  int i;
  int min_use;
  int min_use_index;

  if (self == 0)
    return 0;
  if (palette == 0)
    return 0;
  if (self->wm->screen->bpp > 8)
    return 0;
  /* look for match */
  for (i = 0; i < 6; i++)
  {
    if (g_memcmp(palette, self->palette_items[i].palette,
                 256 * sizeof(int)) == 0)
    {
      self->palette_items[i].use_count++;
      return i;
    }
  }
  /* look for least used */
  min_use_index = 0;
  min_use = 999999;
  for (i = 0; i < 6; i++)
  {
    if (self->palette_items[i].use_count < min_use)
    {
      min_use = self->palette_items[i].use_count;
      min_use_index = i;
    }
  }
  /* set, send palette and return */
  g_memcpy(self->palette_items[min_use_index].palette, palette,
           256 * sizeof(int));
  self->palette_items[min_use_index].use_count++;
  xrdp_orders_send_palette(self->orders, palette, min_use_index);
  return min_use_index;
}
