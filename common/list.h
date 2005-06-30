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

*/

#if !defined(LIST_H)
#define LIST_H

/* list */
struct list
{
  long* items;
  int count;
  int alloc_size;
  int grow_by;
  int auto_free;
};

struct list*
list_create(void);
void
list_delete(struct list* self);
void
list_add_item(struct list* self, long item);
long
list_get_item(struct list* self, int index);
void
list_clear(struct list* self);
int
list_index_of(struct list* self, long item);
void
list_remove_item(struct list* self, int index);
void
list_insert_item(struct list* self, int index, long item);

#endif
