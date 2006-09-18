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
   Copyright (C) Jay Sorg 2004-2006

   simple list

*/

#include "arch.h"
#include "os_calls.h"
#include "list.h"

/*****************************************************************************/
struct list* APP_CC
list_create(void)
{
  struct list* self;

  self = (struct list*)g_malloc(sizeof(struct list), 1);
  self->grow_by = 10;
  self->alloc_size = 10;
  self->items = (long*)g_malloc(sizeof(long) * 10, 1);
  return self;
}

/*****************************************************************************/
void APP_CC
list_delete(struct list* self)
{
  int i;

  if (self == 0)
  {
    return;
  }
  if (self->auto_free)
  {
    for (i = 0; i < self->count; i++)
    {
      g_free((void*)self->items[i]);
      self->items[i] = 0;
    }
  }
  g_free(self->items);
  g_free(self);
}

/*****************************************************************************/
void APP_CC
list_add_item(struct list* self, long item)
{
  long* p;
  int i;

  if (self->count >= self->alloc_size)
  {
    i = self->alloc_size;
    self->alloc_size += self->grow_by;
    p = (long*)g_malloc(sizeof(long) * self->alloc_size, 1);
    g_memcpy(p, self->items, sizeof(long) * i);
    g_free(self->items);
    self->items = p;
  }
  self->items[self->count] = item;
  self->count++;
}

/*****************************************************************************/
long APP_CC
list_get_item(struct list* self, int index)
{
  if (index < 0 || index >= self->count)
  {
    return 0;
  }
  return self->items[index];
}

/*****************************************************************************/
void APP_CC
list_clear(struct list* self)
{
  int i;

  if (self->auto_free)
  {
    for (i = 0; i < self->count; i++)
    {
      g_free((void*)self->items[i]);
      self->items[i] = 0;
    }
  }
  g_free(self->items);
  self->count = 0;
  self->grow_by = 10;
  self->alloc_size = 10;
  self->items = (long*)g_malloc(sizeof(long) * 10, 1);
}

/*****************************************************************************/
int APP_CC
list_index_of(struct list* self, long item)
{
  int i;

  for (i = 0; i < self->count; i++)
  {
    if (self->items[i] == item)
    {
      return i;
    }
  }
  return -1;
}

/*****************************************************************************/
void APP_CC
list_remove_item(struct list* self, int index)
{
  int i;

  if (index >= 0 && index < self->count)
  {
    if (self->auto_free)
    {
      g_free((void*)self->items[index]);
      self->items[index] = 0;
    }
    for (i = index; i < (self->count - 1); i++)
    {
      self->items[i] = self->items[i + 1];
    }
    self->count--;
  }
}

/*****************************************************************************/
void APP_CC
list_insert_item(struct list* self, int index, long item)
{
  long* p;
  int i;

  if (index == self->count)
  {
    list_add_item(self, item);
    return;
  }
  if (index >= 0 && index < self->count)
  {
    self->count++;
    if (self->count > self->alloc_size)
    {
      i = self->alloc_size;
      self->alloc_size += self->grow_by;
      p = (long*)g_malloc(sizeof(long) * self->alloc_size, 1);
      g_memcpy(p, self->items, sizeof(long) * i);
      g_free(self->items);
      self->items = p;
    }
    for (i = (self->count - 2); i >= index; i--)
    {
      self->items[i + 1] = self->items[i];
    }
    self->items[index] = item;
  }
}

/*****************************************************************************/
/* append one list to another using strdup for each item in the list */
/* begins copy at start_index, a zero based index on the soure list */
void APP_CC
list_append_list_strdup(struct list* self, struct list* dest, int start_index)
{
  int index;
  long item;
  char* dup;

  for (index = start_index; index < self->count; index++)
  {
    item = list_get_item(self, index);
    dup = g_strdup((char*)item);
    list_add_item(dest, (long)dup);
  }
}
