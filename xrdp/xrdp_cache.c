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
  /* free all the cached font items */
  for (i = 0; i < 12; i++)
    for (j = 0; j < 256; j++)
      g_free(self->char_items[i][j].font_item.data);
  g_free(self);
}

/*****************************************************************************/
/* returns cache id */
int xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap)
{
  int i;
  int j;
  int oldest;
  int cache_id;
  int cache_idx;
  int bmp_size;
  int e;
  int Bpp;

  e = bitmap->width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  Bpp = (bitmap->bpp + 7) / 8;
  bmp_size = (bitmap->width + e) * bitmap->height * Bpp;
  self->bitmap_stamp++;
  /* look for match */
  if (bmp_size < self->cache1_size)
  {
    i = 0;
    for (j = 0; j < self->cache1_entries; j++)
    {
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d\n\r", i, j));
        //g_printf("found bitmap at %d %d\n\r", i, j);
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(i, j);
      }
    }
  }
  else if (bmp_size < self->cache2_size)
  {
    i = 1;
    for (j = 0; j < self->cache2_entries; j++)
    {
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d\n\r", i, j));
        //g_printf("found bitmap at %d %d\n\r", i, j);
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(i, j);
      }
    }
  }
  else if (bmp_size < self->cache3_size)
  {
    i = 2;
    for (j = 0; j < self->cache3_entries; j++)
    {
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d\n\r", i, j));
        //g_printf("found bitmap at %d %d\n\r", i, j);
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(i, j);
      }
    }
  }
  /* look for oldest */
  cache_id = 0;
  cache_idx = 0;
  oldest = 0x7fffffff;
  if (bmp_size < self->cache1_size)
  {
    i = 0;
    for (j = 0; j < self->cache1_entries; j++)
    {
      if (self->bitmap_items[i][j].stamp < oldest)
      {
        oldest = self->bitmap_items[i][j].stamp;
        cache_id = i;
        cache_idx = j;
      }
    }
  }
  else if (bmp_size < self->cache2_size)
  {
    i = 1;
    for (j = 0; j < self->cache2_entries; j++)
    {
      if (self->bitmap_items[i][j].stamp < oldest)
      {
        oldest = self->bitmap_items[i][j].stamp;
        cache_id = i;
        cache_idx = j;
      }
    }
  }
  else if (bmp_size < self->cache3_size)
  {
    i = 2;
    for (j = 0; j < self->cache3_entries; j++)
    {
      if (self->bitmap_items[i][j].stamp < oldest)
      {
        oldest = self->bitmap_items[i][j].stamp;
        cache_id = i;
        cache_idx = j;
      }
    }
  }
  DEBUG(("adding bitmap at %d %d\n\r", cache_id, cache_idx));
  /* set, send bitmap and return */
  xrdp_bitmap_delete(self->bitmap_items[cache_id][cache_idx].bitmap);
  self->bitmap_items[cache_id][cache_idx].bitmap = bitmap;
  self->bitmap_items[cache_id][cache_idx].stamp = self->bitmap_stamp;
  if (self->use_bitmap_comp)
  {
    xrdp_orders_send_bitmap(self->orders, bitmap, cache_id, cache_idx);
  }
  else
  {
    xrdp_orders_send_raw_bitmap(self->orders, bitmap, cache_id, cache_idx);
  }
  return MAKELONG(cache_id, cache_idx);
}

/*****************************************************************************/
int xrdp_cache_add_palette(struct xrdp_cache* self, int* palette)
{
  int i;
  int oldest;
  int index;

  if (self == 0)
    return 0;
  if (palette == 0)
    return 0;
  if (self->wm->screen->bpp > 8)
    return 0;
  self->palette_stamp++;
  /* look for match */
  for (i = 0; i < 6; i++)
  {
    if (g_memcmp(palette, self->palette_items[i].palette,
                 256 * sizeof(int)) == 0)
    {
      self->palette_items[i].stamp = self->palette_stamp;
      return i;
    }
  }
  /* look for oldest */
  index = 0;
  oldest = 0x7fffffff;
  for (i = 0; i < 6; i++)
  {
    if (self->palette_items[i].stamp < oldest)
    {
      oldest = self->palette_items[i].stamp;
      index = i;
    }
  }
  /* set, send palette and return */
  g_memcpy(self->palette_items[index].palette, palette, 256 * sizeof(int));
  self->palette_items[index].stamp = self->palette_stamp;
  xrdp_orders_send_palette(self->orders, palette, index);
  return index;
}

/*****************************************************************************/
int xrdp_cache_add_char(struct xrdp_cache* self,
                        struct xrdp_font_item* font_item)
{
  int i;
  int j;
  int oldest;
  int f;
  int c;
  int datasize;
  struct xrdp_font_item* fi;

  self->char_stamp++;
  /* look for match */
  for (i = 7; i < 12; i++)
  {
    for (j = 0; j < 250; j++)
    {
      if (xrdp_font_item_compare(&self->char_items[i][j].font_item, font_item))
      {
        self->char_items[i][j].stamp = self->char_stamp;
        DEBUG(("found font at %d %d\n\r", i, j));
        return MAKELONG(i, j);
      }
    }
  }
  /* look for oldest */
  f = 0;
  c = 0;
  oldest = 0x7fffffff;
  for (i = 7; i < 12; i++)
  {
    for (j = 0; j < 250; j++)
    {
      if (self->char_items[i][j].stamp < oldest)
      {
        oldest = self->char_items[i][j].stamp;
        f = i;
        c = j;
      }
    }
  }
  DEBUG(("adding char at %d %d\n\r", f, c));
  /* set, send char and return */
  fi = &self->char_items[f][c].font_item;
  g_free(fi->data);
  datasize = FONT_DATASIZE(font_item);
  fi->data = (char*)g_malloc(datasize, 1);
  g_memcpy(fi->data, font_item->data, datasize);
  fi->offset = font_item->offset;
  fi->baseline = font_item->baseline;
  fi->width = font_item->width;
  fi->height = font_item->height;
  self->char_items[f][c].stamp = self->char_stamp;
  xrdp_orders_send_font(self->orders, fi, f, c);
  return MAKELONG(f, c);
}
