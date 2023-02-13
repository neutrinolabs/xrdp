/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * simple list
 */

#if !defined(LIST_H)
#define LIST_H

#include "arch.h"

/* list */
struct list
{
    tintptr *items;
    int count;
    int alloc_size;
    int grow_by;
    int auto_free;
};

struct list *
list_create(void);
/**
 * Creates a list with at least the specified number of items
 * reserved
 * @param size Number of items to reserve
 * @return list, or NULL if no memory
 */
struct list *
list_create_sized(unsigned int size);
void
list_delete(struct list *self);
/**
 * Adds an item to a list
 * @param self The list
 * @param item The item to add
 * @result 0 if a memory allocation failure occurred. In this
 *         case the item is not added
 *
 * Memory allocation failures will not occur if the list is
 * sized appropriately when created.
 */
int
list_add_item(struct list *self, tintptr item);
tintptr
list_get_item(const struct list *self, int index);
void
list_clear(struct list *self);
int
list_index_of(struct list *self, tintptr item);
void
list_remove_item(struct list *self, int index);
/**
 * Inserts an item into a list
 * @param self The list
 * @param index The location to insert the item before
 * @param item The item to add
 * @result 0 if a memory allocation failure occurred. In this
 *         case the item is not added
 *
 * Memory allocation failures will not occur if the list is
 * sized appropriately when created.
 */
int
list_insert_item(struct list *self, int index, tintptr item);
/**
 * Adds strings to a list from another list
 * @param self The source list
 * @param dest Destination list
 * @param start_index Index to start on the source list (zero based)
 *
 * @result 0 if a memory allocation failure occurred. In this
 *         case the destination list is unaltered.
 *
 * Strings from the source list are copied with strdup()
 * The dest list should have auto_free set, or memory leaks will occur
 */
int
list_append_list_strdup(struct list *self, struct list *dest, int start_index);
void
list_dump_items(struct list *self);

#endif
