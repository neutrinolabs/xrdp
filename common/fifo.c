/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Matt Burt 2023
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
 * @file    common/fifo.c
 * @brief   Fifo for storing generic pointers
 *
 * Defines an unbounded FIFO-queue for void * pointers
 *
 * The stored pointers are called 'items' below.
 *
 * Items are stored in groups called 'chunks'. Chunks are linked together
 * in a chain:-
 *
 * +-------------+    +--------+    +--------+    +--------+
 * | first_chunk |--->|  next  |--->|  next  |--->|  NULL  |<-+
 * | last_chunk  |-+  +--------+    +--------+    +--------+  |
 * | . . .       | |  | item.0 |    | item.0 |    | item.0 |  |
 * +-------------+ |  |   ...  |    |   ...  |    |   ...  |  |
 *                 |  | item.n |    | item.n |    | item.n |  |
 *                 |  +--------+    +--------+    +--------+  |
 *                 |                                          |
 *                 +------------------------------------------+
 *
 * This allows items to be added to the FIFO by allocating blocks
 * as each one fills up.
 *
 * The code to read from the FIFO de-allocates blocks as each one is
 * consumed.
 *
 * There is always at least one chunk in the FIFO.
 */


#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>

#include "fifo.h"

#define ITEMS_PER_CHUNK 31

struct chunk
{
    struct chunk *next;
    void *items[ITEMS_PER_CHUNK];
};

struct fifo
{
    struct chunk *first_chunk;
    struct chunk *last_chunk;
    /** Next address to write in 'last_chunk' */
    unsigned short writer;
    /** Next address to read in 'first_chunk' */
    unsigned short reader;
    /** Item destructor function, or NULL */
    fifo_item_destructor item_destructor;
};

/*****************************************************************************/

struct fifo *
fifo_create(fifo_item_destructor item_destructor)
{
    struct fifo *result = NULL;
    struct chunk *cptr = (struct chunk *)malloc(sizeof(struct chunk));
    if (cptr != NULL)
    {
        /* 'next' pointer in last block is always NULL */
        cptr->next = NULL;
        result = (struct fifo *)malloc(sizeof(struct fifo));
        if (result == NULL)
        {
            free(cptr);
        }
        else
        {
            result->first_chunk = cptr;
            result->last_chunk = cptr;
            result->writer = 0;
            result->reader = 0;
            result->item_destructor = item_destructor;
        }
    }
    return result;
}

/*****************************************************************************/
/**
 * Internal function to call the destructor function on all items in the fifo
 *
 * @param self fifo. Can't be NULL
 * @param closure Additional argument to destructor function
 */
static void
call_item_destructor(struct fifo *self, void *closure)
{
    if (self->item_destructor != NULL)
    {
        struct chunk *cptr = self->first_chunk;
        unsigned int i = self->reader;

        // Process all the chunks up to the last one
        while (cptr != self->last_chunk)
        {
            (*self->item_destructor)(cptr->items[i++], closure);
            if (i == ITEMS_PER_CHUNK)
            {
                cptr = cptr->next;
                i = 0;
            }
        }

        // Process all the items in the last chunk
        while (i < self->writer)
        {
            (*self->item_destructor)(cptr->items[i++], closure);
        }
    }
}


/*****************************************************************************/
void
fifo_delete(struct fifo *self, void *closure)
{
    if (self != NULL)
    {
        call_item_destructor(self, closure);

        // Now free all the chunks
        struct chunk *cptr = self->first_chunk;
        while (cptr != NULL)
        {
            struct chunk *next = cptr->next;
            free(cptr);
            cptr = next;
        }

        free(self);
    }
}


/*****************************************************************************/
void
fifo_clear(struct fifo *self, void *closure)
{
    if (self != NULL)
    {
        call_item_destructor(self, closure);

        // Now free all the chunks except the last one
        struct chunk *cptr = self->first_chunk;
        while (cptr->next != NULL)
        {
            struct chunk *next = cptr->next;
            free(cptr);
            cptr = next;
        }

        // Re-initialise fifo fields
        self->first_chunk = cptr;
        self->last_chunk = cptr;
        self->reader = 0;
        self->writer = 0;
    }
}

/*****************************************************************************/
int
fifo_add_item(struct fifo *self, void *item)
{
    int rv = 0;
    if (self != NULL && item != NULL)
    {
        if (self->writer == ITEMS_PER_CHUNK)
        {
            // Add another chunk to the chain
            struct chunk *cptr;
            cptr = (struct chunk *)malloc(sizeof(struct chunk));
            if (cptr == NULL)
            {
                return 0;
            }
            cptr->next = NULL;
            self->last_chunk->next = cptr;
            self->last_chunk = cptr;
            self->writer = 0;
        }

        self->last_chunk->items[self->writer++] = item;
        rv = 1;
    }
    return rv;
}

/*****************************************************************************/
void *
fifo_remove_item(struct fifo *self)
{
    void *item = NULL;
    if (self != NULL)
    {
        // More than one chunk in the fifo?
        if (self->first_chunk != self->last_chunk)
        {
            /* We're not reading the last chunk. There
             * must be something in the fifo */
            item = self->first_chunk->items[self->reader++];

            /* At the end of this chunk? */
            if (self->reader == ITEMS_PER_CHUNK)
            {
                struct chunk *old_chunk = self->first_chunk;
                self->first_chunk = old_chunk->next;
                free(old_chunk);
                self->reader = 0;
            }
        }
        else if (self->reader < self->writer)
        {
            /* We're reading the last chunk */
            item = self->first_chunk->items[self->reader++];
            if (self->reader == self->writer)
            {
                // fifo is now empty. We can reset the pointers
                // to prevent unnecessary allocations in the future.
                self->reader = 0;
                self->writer = 0;
            }
        }
    }
    return item;
}

/*****************************************************************************/

int
fifo_is_empty(struct fifo *self)
{
    return (self == NULL ||
            (self->first_chunk == self->last_chunk &&
             self->reader == self->writer));
}
