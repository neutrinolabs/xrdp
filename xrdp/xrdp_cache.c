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

   cache

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_cache* APP_CC
xrdp_cache_create(struct xrdp_wm* owner,
                  struct xrdp_session* session,
                  struct xrdp_client_info* client_info)
{
  struct xrdp_cache* self;

  self = (struct xrdp_cache*)g_malloc(sizeof(struct xrdp_cache), 1);
  self->wm = owner;
  self->session = session;
  self->use_bitmap_comp = client_info->use_bitmap_comp;
  self->cache1_entries = client_info->cache1_entries;
  self->cache1_size = client_info->cache1_size;
  self->cache2_entries = client_info->cache2_entries;
  self->cache2_size = client_info->cache2_size;
  self->cache3_entries = client_info->cache3_entries;
  self->cache3_size = client_info->cache3_size;
  self->bitmap_cache_persist_enable = client_info->bitmap_cache_persist_enable;
  self->bitmap_cache_version = client_info->bitmap_cache_version;
  self->pointer_cache_entries = client_info->pointer_cache_entries;
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_cache_delete(struct xrdp_cache* self)
{
  int i;
  int j;

  if (self == 0)
  {
    return;
  }
  /* free all the cached bitmaps */
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 2000; j++)
    {
      xrdp_bitmap_delete(self->bitmap_items[i][j].bitmap);
    }
  }
  /* free all the cached font items */
  for (i = 0; i < 12; i++)
  {
    for (j = 0; j < 256; j++)
    {
      g_free(self->char_items[i][j].font_item.data);
    }
  }
  g_free(self);
}

/*****************************************************************************/
int APP_CC
xrdp_cache_reset(struct xrdp_cache* self,
                 struct xrdp_client_info* client_info)
{
  struct xrdp_wm* wm;
  struct xrdp_session* session;
  int i;
  int j;

  /* free all the cached bitmaps */
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 2000; j++)
    {
      xrdp_bitmap_delete(self->bitmap_items[i][j].bitmap);
    }
  }
  /* free all the cached font items */
  for (i = 0; i < 12; i++)
  {
    for (j = 0; j < 256; j++)
    {
      g_free(self->char_items[i][j].font_item.data);
    }
  }
  /* save these */
  wm = self->wm;
  session = self->session;
  /* set whole struct to zero */
  g_memset(self, 0, sizeof(struct xrdp_cache));
  /* set some stuff back */
  self->wm = wm;
  self->session = session;
  self->use_bitmap_comp = client_info->use_bitmap_comp;
  self->cache1_entries = client_info->cache1_entries;
  self->cache1_size = client_info->cache1_size;
  self->cache2_entries = client_info->cache2_entries;
  self->cache2_size = client_info->cache2_size;
  self->cache3_entries = client_info->cache3_entries;
  self->cache3_size = client_info->cache3_size;
  self->bitmap_cache_persist_enable = client_info->bitmap_cache_persist_enable;
  self->bitmap_cache_version = client_info->bitmap_cache_version;
  self->pointer_cache_entries = client_info->pointer_cache_entries;
  return 0;
}

/*****************************************************************************/
/* returns cache id */
int APP_CC
xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap)
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
  if (bmp_size <= self->cache1_size)
  {
    i = 0;
    for (j = 0; j < self->cache1_entries; j++)
    {
#ifdef USE_CRC
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
#else
      if (xrdp_bitmap_compare(self->bitmap_items[i][j].bitmap, bitmap))
#endif
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d", i, j));
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(j, i);
      }
    }
  }
  else if (bmp_size <= self->cache2_size)
  {
    i = 1;
    for (j = 0; j < self->cache2_entries; j++)
    {
#ifdef USE_CRC
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
#else
      if (xrdp_bitmap_compare(self->bitmap_items[i][j].bitmap, bitmap))
#endif
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d", i, j));
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(j, i);
      }
    }
  }
  else if (bmp_size <= self->cache3_size)
  {
    i = 2;
    for (j = 0; j < self->cache3_entries; j++)
    {
#ifdef USE_CRC
      if (xrdp_bitmap_compare_with_crc(self->bitmap_items[i][j].bitmap, bitmap))
#else
      if (xrdp_bitmap_compare(self->bitmap_items[i][j].bitmap, bitmap))
#endif
      {
        self->bitmap_items[i][j].stamp = self->bitmap_stamp;
        DEBUG(("found bitmap at %d %d", i, j));
        xrdp_bitmap_delete(bitmap);
        return MAKELONG(j, i);
      }
    }
  }
  else
  {
    g_writeln("error in xrdp_cache_add_bitmap, too big(%d)", bmp_size);
  }
  /* look for oldest */
  cache_id = 0;
  cache_idx = 0;
  oldest = 0x7fffffff;
  if (bmp_size <= self->cache1_size)
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
  else if (bmp_size <= self->cache2_size)
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
  else if (bmp_size <= self->cache3_size)
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
  DEBUG(("adding bitmap at %d %d", cache_id, cache_idx));
  /* set, send bitmap and return */
  xrdp_bitmap_delete(self->bitmap_items[cache_id][cache_idx].bitmap);
  self->bitmap_items[cache_id][cache_idx].bitmap = bitmap;
  self->bitmap_items[cache_id][cache_idx].stamp = self->bitmap_stamp;
  if (self->bitmap_cache_version == 0) /* orginal version */
  {
    if (self->use_bitmap_comp)
    {
      libxrdp_orders_send_bitmap(self->session, bitmap->width,
                                 bitmap->height, bitmap->bpp,
                                 bitmap->data, cache_id, cache_idx);
    }
    else
    {
      libxrdp_orders_send_raw_bitmap(self->session, bitmap->width,
                                     bitmap->height, bitmap->bpp,
                                     bitmap->data, cache_id, cache_idx);
    }
  }
  else
  {
    if (self->use_bitmap_comp)
    {
      libxrdp_orders_send_bitmap2(self->session, bitmap->width,
                                  bitmap->height, bitmap->bpp,
                                  bitmap->data, cache_id, cache_idx);
    }
    else
    {
      libxrdp_orders_send_raw_bitmap2(self->session, bitmap->width,
                                      bitmap->height, bitmap->bpp,
                                      bitmap->data, cache_id, cache_idx);
    }
  }
  return MAKELONG(cache_idx, cache_id);
}

/*****************************************************************************/
/* not used */
/* not sure how to use a palette in rdp */
int APP_CC
xrdp_cache_add_palette(struct xrdp_cache* self, int* palette)
{
  int i;
  int oldest;
  int index;

  if (self == 0)
  {
    return 0;
  }
  if (palette == 0)
  {
    return 0;
  }
  if (self->wm->screen->bpp > 8)
  {
    return 0;
  }
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
  libxrdp_orders_send_palette(self->session, palette, index);
  return index;
}

/*****************************************************************************/
int APP_CC
xrdp_cache_add_char(struct xrdp_cache* self,
                    struct xrdp_font_char* font_item)
{
  int i;
  int j;
  int oldest;
  int f;
  int c;
  int datasize;
  struct xrdp_font_char* fi;

  self->char_stamp++;
  /* look for match */
  for (i = 7; i < 12; i++)
  {
    for (j = 0; j < 250; j++)
    {
      if (xrdp_font_item_compare(&self->char_items[i][j].font_item, font_item))
      {
        self->char_items[i][j].stamp = self->char_stamp;
        DEBUG(("found font at %d %d", i, j));
        return MAKELONG(j, i);
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
  DEBUG(("adding char at %d %d", f, c));
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
  libxrdp_orders_send_font(self->session, fi, f, c);
  return MAKELONG(c, f);
}

/*****************************************************************************/
/* added the pointer to the cache and send it to client, it also sets the
   client if it finds it
   returns the index in the cache
   does not take ownership of pointer_item */
int APP_CC
xrdp_cache_add_pointer(struct xrdp_cache* self,
                       struct xrdp_pointer_item* pointer_item)
{
  int i;
  int oldest;
  int index;

  if (self == 0)
  {
    return 0;
  }
  self->pointer_stamp++;
  /* look for match */
  for (i = 2; i < self->pointer_cache_entries; i++)
  {
    if (self->pointer_items[i].x == pointer_item->x &&
        self->pointer_items[i].y == pointer_item->y &&
        g_memcmp(self->pointer_items[i].data,
                 pointer_item->data, 32 * 32 * 3) == 0 &&
        g_memcmp(self->pointer_items[i].mask,
                 pointer_item->mask, 32 * 32 / 8) == 0)
    {
      self->pointer_items[i].stamp = self->pointer_stamp;
      xrdp_wm_set_pointer(self->wm, i);
      self->wm->current_pointer = i;
      DEBUG(("found pointer at %d", i));
      return i;
    }
  }
  /* look for oldest */
  index = 2;
  oldest = 0x7fffffff;
  for (i = 2; i < self->pointer_cache_entries; i++)
  {
    if (self->pointer_items[i].stamp < oldest)
    {
      oldest = self->pointer_items[i].stamp;
      index = i;
    }
  }
  self->pointer_items[index].x = pointer_item->x;
  self->pointer_items[index].y = pointer_item->y;
  g_memcpy(self->pointer_items[index].data,
           pointer_item->data, 32 * 32 * 3);
  g_memcpy(self->pointer_items[index].mask,
           pointer_item->mask, 32 * 32 / 8);
  self->pointer_items[index].stamp = self->pointer_stamp;
  xrdp_wm_send_pointer(self->wm, index,
                       self->pointer_items[index].data,
                       self->pointer_items[index].mask,
                       self->pointer_items[index].x,
                       self->pointer_items[index].y);
  self->wm->current_pointer = index;
  DEBUG(("adding pointer at %d", index));
  return index;
}

/*****************************************************************************/
/* this does not take owership of pointer_item, it makes a copy */
int APP_CC
xrdp_cache_add_pointer_static(struct xrdp_cache* self,
                              struct xrdp_pointer_item* pointer_item,
                              int index)
{

  if (self == 0)
  {
    return 0;
  }
  self->pointer_items[index].x = pointer_item->x;
  self->pointer_items[index].y = pointer_item->y;
  g_memcpy(self->pointer_items[index].data,
           pointer_item->data, 32 * 32 * 3);
  g_memcpy(self->pointer_items[index].mask,
           pointer_item->mask, 32 * 32 / 8);
  self->pointer_items[index].stamp = self->pointer_stamp;
  xrdp_wm_send_pointer(self->wm, index,
                       self->pointer_items[index].data,
                       self->pointer_items[index].mask,
                       self->pointer_items[index].x,
                       self->pointer_items[index].y);
  self->wm->current_pointer = index;
  DEBUG(("adding pointer at %d", index));
  return index;
}
