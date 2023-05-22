/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2004-2014
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
 */

/**
 * @file    common/fifo.h
 * @brief   Fifo for storing generic pointers
 *
 * Declares an unbounded FIFO-queue for void * pointers
 */

#ifndef _FIFO_H
#define _FIFO_H

struct fifo;

/**
 * Function used by fifo_clear()/fifo_delete() to destroy items
 *
 * @param item Item being deleted
 * @param closure Additional argument to function
 *
 * Use this function to free any allocated storage (e.g. if the items
 * are dynamically allocated)
 */
typedef void (*fifo_item_destructor)(void *item, void *closure);

/**
 * Create new fifo
 *
 * @param item_destructor Destructor for fifo items, or NULL for none
 * @return fifo, or NULL if no memory
 */
struct fifo *
fifo_create(fifo_item_destructor item_destructor);

/**
 * Delete an existing fifo
 *
 * Any existing entries on the fifo are passed in order to the
 * item destructor specified when the fifo was created.
 *
 * @param self fifo to delete (may be NULL)
 * @param closure Additional parameter for fifo item destructor
 */
void
fifo_delete(struct fifo *self, void *closure);

/**
 * Clear(empty) an existing fifo
 *
 * Any existing entries on the fifo are passed in order to the
 * item destructor specified when the fifo was created.
 *
 * @param self fifo to clear (may be NULL)
 * @param closure Additional parameter for fifo item destructor
 */
void
fifo_clear(struct fifo *self, void *closure);

/** Add an item to a fifo
 * @param self fifo
 * @param item Item to add
 * @return 1 if successful, 0 for no memory, or tried to add NULL
 */
int
fifo_add_item(struct fifo *self, void *item);

/** Remove an item from a fifo
 * @param self fifo
 * @return item if successful, NULL for no items in FIFO
 */
void *
fifo_remove_item(struct fifo *self);

/** Is fifo empty?
 *
 * @param self fifo
 * @return 1 if fifo is empty, 0 if not
 */
int
fifo_is_empty(struct fifo *self);

#endif
