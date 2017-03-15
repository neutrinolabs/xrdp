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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "list.h"

/*****************************************************************************/
struct list *
list_create(void)
{
    struct list *self;

    self = (struct list *)g_malloc(sizeof(struct list), 1);
    self->grow_by = 10;
    self->alloc_size = 10;
    self->items = (tbus *)g_malloc(sizeof(tbus) * 10, 1);
    return self;
}

/*****************************************************************************/
void
list_delete(struct list *self)
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
            g_free((void *)self->items[i]);
            self->items[i] = 0;
        }
    }

    g_free(self->items);
    g_free(self);
}

/*****************************************************************************/
void
list_add_item(struct list *self, tbus item)
{
    tbus *p;
    int i;

    if (self->count >= self->alloc_size)
    {
        i = self->alloc_size;
        self->alloc_size += self->grow_by;
        p = (tbus *)g_malloc(sizeof(tbus) * self->alloc_size, 1);
        g_memcpy(p, self->items, sizeof(tbus) * i);
        g_free(self->items);
        self->items = p;
    }

    self->items[self->count] = item;
    self->count++;
}

/*****************************************************************************/
tbus
list_get_item(const struct list *self, int index)
{
    if (index < 0 || index >= self->count)
    {
        return 0;
    }

    return self->items[index];
}

/*****************************************************************************/
void
list_clear(struct list *self)
{
    int i;

    if (self->auto_free)
    {
        for (i = 0; i < self->count; i++)
        {
            g_free((void *)self->items[i]);
            self->items[i] = 0;
        }
    }

    g_free(self->items);
    self->count = 0;
    self->grow_by = 10;
    self->alloc_size = 10;
    self->items = (tbus *)g_malloc(sizeof(tbus) * 10, 1);
}

/*****************************************************************************/
int
list_index_of(struct list *self, tbus item)
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
void
list_remove_item(struct list *self, int index)
{
    int i;

    if (index >= 0 && index < self->count)
    {
        if (self->auto_free)
        {
            g_free((void *)self->items[index]);
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
void
list_insert_item(struct list *self, int index, tbus item)
{
    tbus *p;
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
            p = (tbus *)g_malloc(sizeof(tbus) * self->alloc_size, 1);
            g_memcpy(p, self->items, sizeof(tbus) * i);
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
/* begins copy at start_index, a zero based index on the source list */
void
list_append_list_strdup(struct list *self, struct list *dest, int start_index)
{
    int index;
    tbus item;
    char *dup;

    for (index = start_index; index < self->count; index++)
    {
        item = list_get_item(self, index);
        dup = g_strdup((char *)item);
        list_add_item(dest, (tbus)dup);
    }
}

/*****************************************************************************/
void
list_dump_items(struct list *self)
{
    int index;

    if (self->count == 0)
    {
        g_writeln("List is empty");
    }

    for (index = 0; index < self->count; index++)
    {
        g_writeln("%d: %p", index, (void *) list_get_item(self, index));
    }
}
