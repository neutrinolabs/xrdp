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

   simple list

   this list is used to track mem leaks so g_malloc1 and
   g_free1 should be used for internal allocs but not
   for auto_free items

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_list* xrdp_list_create(void)
{
  struct xrdp_list* self;

  self = (struct xrdp_list*)g_malloc1(sizeof(struct xrdp_list), 1);
  self->grow_by = 10;
  self->alloc_size = 10;
  self->items = (int*)g_malloc1(sizeof(int) * 10, 1);
  return self;
}

/*****************************************************************************/
void xrdp_list_delete(struct xrdp_list* self)
{
  int i;

  if (self == 0)
    return;
  if (self->auto_free)
    for (i = 0; i < self->count; i++)
      g_free((void*)self->items[i]);
  g_free1(self->items);
  g_free1(self);
}

/*****************************************************************************/
void xrdp_list_add_item(struct xrdp_list* self, int item)
{
  int* p;
  int i;

  if (self->count >= self->alloc_size)
  {
    i = self->alloc_size;
    self->alloc_size += self->grow_by;
    p = (int*)g_malloc1(sizeof(int) * self->alloc_size, 1);
    g_memcpy(p, self->items, sizeof(int) * i);
    g_free1(self->items);
    self->items = p;
  }
  self->items[self->count] = item;
  self->count++;
}

/*****************************************************************************/
int xrdp_list_get_item(struct xrdp_list* self, int index)
{
  if (index < 0 || index >= self->count)
    return 0;
  return self->items[index];
}

/*****************************************************************************/
void xrdp_list_clear(struct xrdp_list* self)
{
  int i;

  if (self->auto_free)
    for (i = 0; i < self->count; i++)
      g_free((void*)self->items[i]);
  g_free1(self->items);
  self->count = 0;
  self->grow_by = 10;
  self->alloc_size = 10;
  self->items = (int*)g_malloc1(sizeof(int) * 10, 1);
}

/*****************************************************************************/
int xrdp_list_index_of(struct xrdp_list* self, int item)
{
  int i;

  for (i = 0; i < self->count; i++)
    if (self->items[i] == item)
      return i;
  return -1;
}

/*****************************************************************************/
void xrdp_list_remove_item(struct xrdp_list* self, int index)
{
  int i;

  if (index >= 0 && index < self->count)
  {
    if (self->auto_free)
      g_free((void*)self->items[index]);
    for (i = index; i < (self->count - 1); i++)
      self->items[i] = self->items[i + 1];
    self->count--;
  }
}

/*****************************************************************************/
void xrdp_list_insert_item(struct xrdp_list* self, int index, int item)
{
  int* p;
  int i;

  if (index == self->count)
  {
    xrdp_list_add_item(self, item);
    return;
  }
  if (index >= 0 && index < self->count)
  {
    self->count++;
    if (self->count > self->alloc_size)
    {
      i = self->alloc_size;
      self->alloc_size += self->grow_by;
      p = (int*)g_malloc1(sizeof(int) * self->alloc_size, 1);
      g_memcpy(p, self->items, sizeof(int) * i);
      g_free1(self->items);
      self->items = p;
    }
    for (i = (self->count - 2); i >= index; i--)
      self->items[i + 1] = self->items[i];
    self->items[index] = item;
  }
}
