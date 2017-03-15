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
#include "list16.h"

/*****************************************************************************/
struct list16 *
list16_create(void)
{
    struct list16 *self;

    self = (struct list16 *)g_malloc(sizeof(struct list16), 0);
    list16_init(self);
    return self;
}

/*****************************************************************************/
void
list16_delete(struct list16 *self)
{
    if (self == 0)
    {
        return;
    }

    list16_deinit(self);
    g_free(self);
}

/*****************************************************************************/
void
list16_init(struct list16* self)
{
    g_memset(self, 0, sizeof(struct list16));
    self->max_count = 4;
    self->items = self->mitems;
}

/*****************************************************************************/
void
list16_deinit(struct list16* self)
{
    if (self->items != self->mitems)
    {
        g_free(self->items);
    }
}

/*****************************************************************************/
void
list16_add_item(struct list16 *self, tui16 item)
{
    tui16 *p;
    int i;

    if (self->count >= self->max_count)
    {
        i = self->max_count;
        self->max_count += 4;
        p = (tui16 *)g_malloc(sizeof(tui16) * self->max_count, 1);
        g_memcpy(p, self->items, sizeof(tui16) * i);
        if (self->items != self->mitems)
        {
            g_free(self->items);
        }
        self->items = p;
    }

    self->items[self->count] = item;
    self->count++;
}

/*****************************************************************************/
tui16
list16_get_item(struct list16 *self, int index)
{
    if (index < 0 || index >= self->count)
    {
        return 0;
    }

    return self->items[index];
}

/*****************************************************************************/
void
list16_clear(struct list16 *self)
{
    if (self->items != self->mitems)
    {
        g_free(self->items);
    }
    self->count = 0;
    self->max_count = 4;
    self->items = self->mitems;
}

/*****************************************************************************/
int
list16_index_of(struct list16 *self, tui16 item)
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
list16_remove_item(struct list16 *self, int index)
{
    int i;

    if (index >= 0 && index < self->count)
    {
        for (i = index; i < (self->count - 1); i++)
        {
            self->items[i] = self->items[i + 1];
        }

        self->count--;
    }
}

/*****************************************************************************/
void
list16_insert_item(struct list16 *self, int index, tui16 item)
{
    tui16 *p;
    int i;

    if (index == self->count)
    {
        list16_add_item(self, item);
        return;
    }

    if (index >= 0 && index < self->count)
    {
        self->count++;

        if (self->count > self->max_count)
        {
            i = self->max_count;
            self->max_count += 4;
            p = (tui16 *)g_malloc(sizeof(tui16) * self->max_count, 1);
            g_memcpy(p, self->items, sizeof(tui16) * i);
            if (self->items != self->mitems)
            {
                g_free(self->items);
            }
            self->items = p;
        }

        for (i = (self->count - 2); i >= index; i--)
        {
            self->items[i + 1] = self->items[i];
        }

        self->items[index] = item;
    }
}
