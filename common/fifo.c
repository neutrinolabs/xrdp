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
 *
 * FIFO implementation to store pointer to data struct
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "fifo.h"
#include "os_calls.h"

/**
 * Create new fifo data struct
 *
 * @return pointer to new FIFO or NULL if system out of memory
 *****************************************************************************/

FIFO *
fifo_create(void)
{
    return (FIFO *) g_malloc(sizeof(FIFO), 1);
}

/**
 * Delete specified FIFO
 *****************************************************************************/

void
fifo_delete(FIFO *self)
{
    USER_DATA *udp;

    if (!self)
        return;

     if (!self->head)
     {
         /* FIFO is empty */
         g_free(self);
         return;
     }

    if (self->head == self->tail)
    {
        /* only one item in FIFO */
        if (self->auto_free)
            g_free(self->head->item);

        g_free(self->head);
        g_free(self);
        return;
    }

    /* more then one item in FIFO */
    while (self->head)
    {
        udp = self->head;

        if (self->auto_free)
            g_free(udp->item);

        self->head = udp->next;
        g_free(udp);
    }

    g_free(self);
}

/**
 * Add an item to the specified FIFO
 *
 * @param self FIFO to operate on
 * @param item item to add to specified FIFO
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

int
fifo_add_item(FIFO *self, void *item)
{
    USER_DATA *udp;

    if (!self || !item)
        return -1;

    if ((udp = (USER_DATA *) g_malloc(sizeof(USER_DATA), 0)) == 0)
        return -1;

    udp->item = item;
    udp->next = 0;

    /* if fifo is empty, add to head */
    if (!self->head)
    {
        self->head = udp;
        self->tail = udp;
        return 0;
    }

    /* add to tail */
    self->tail->next = udp;
    self->tail = udp;

    return 0;
}

/**
 * Return an item from top of FIFO
 *
 * @param self FIFO to operate on
 *
 * @return top item from FIFO or NULL if FIFO is empty
 *****************************************************************************/

void *
fifo_remove_item(FIFO *self)
{
    void      *item;
    USER_DATA *udp;

    if (!self || !self->head)
        return 0;

    if (self->head == self->tail)
    {
        /* only one item in FIFO */
        item = self->head->item;
        g_free(self->head);
        self->head = 0;
        self->tail = 0;
        return item;
    }

    /* more then one item in FIFO */
    udp = self->head;
    item = self->head->item;
    self->head = self->head->next;
    g_free(udp);
    return item;
}

/**
 * Return FIFO status
 *
 * @param self FIFO to operate on
 *
 * @return true if FIFO is empty, false otherwise
 *****************************************************************************/

int
fifo_is_empty(FIFO *self)
{
    if (!self)
        return 1;

    return (self->head == 0);
}
