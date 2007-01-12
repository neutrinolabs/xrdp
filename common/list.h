/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   simple list
*/

#if !defined(LIST_H)
#define LIST_H

#include "arch.h"

/* list */
struct list
{
  long* items;
  int count;
  int alloc_size;
  int grow_by;
  int auto_free;
};

struct list* APP_CC
list_create(void);
void APP_CC
list_delete(struct list* self);
void APP_CC
list_add_item(struct list* self, long item);
long APP_CC
list_get_item(struct list* self, int index);
void APP_CC
list_clear(struct list* self);
int APP_CC
list_index_of(struct list* self, long item);
void APP_CC
list_remove_item(struct list* self, int index);
void APP_CC
list_insert_item(struct list* self, int index, long item);
void APP_CC
list_append_list_strdup(struct list* self, struct list* dest, int start_index);

#endif
