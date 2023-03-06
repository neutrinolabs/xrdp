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

/**
 * Splits a string on a separation character and then returns a list of
 * the string split by the character, without the character contained within
 * the pieces.
 *
 * The list must be disposed of by the caller.
 *
 * @param str String to split.
 * @param character Character used as the delimiter between strings.
 * @param start_index Index to start on the source list (zero based)
 *
 * @result 0 if a memory allocation failure occurred.
 *
 * String fragments in the list are created with strdup()
 */
struct list *
split_string_into_list(const char *str, char character);

/**
 * As list_add_item() but for a C string
 *
 * This is a convenience function for a common operation
 * @param self List to append to
 * @param str String to append
 *
 * The passed-in string is strdup'd onto the list, so if auto_free
 * isn't set, memory leaks will occur.
 *
 * A NULL pointer will be added as a NULL entry.
 *
 * @result 0 if any memory allocation failure occurred. In this case
 * the list is unchanged.
 */

int
list_add_strdup(struct list *self, const char *str);

/**
 * Add multiple strings to a list
 *
 * This is a convenience function for a common operation
 * @param self List to append to
 * @param ... Strings to append. Terminate the list with a NULL.
 *
 * @result 0 if any memory allocation failure occurred. In this case
 * the list is unchanged.
 */

int
list_add_strdup_multi(struct list *self, ...);

#endif
