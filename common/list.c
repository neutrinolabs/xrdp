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

#include <stdlib.h>
#include <stdarg.h>

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "list.h"
#include "log.h"

enum
{
    DEFAULT_LIST_SIZE = 10,
    DEFAULT_GROW_BY_SIZE = 10
};

/******************************************************************************/
struct list *
list_create_sized(unsigned int size)
{
    struct list *self;

    if (size < DEFAULT_LIST_SIZE)
    {
        size = DEFAULT_LIST_SIZE;
    }
    self = (struct list *)calloc(sizeof(struct list), 1);
    if (self != NULL)
    {
        self->items = (tbus *)malloc(sizeof(tbus) * size);
        if (self->items == NULL)
        {
            free(self);
            self = NULL;
        }
        else
        {
            self->grow_by = DEFAULT_GROW_BY_SIZE;
            self->alloc_size = size;
        }
    }
    return self;
}

/******************************************************************************/
struct list *
list_create(void)
{
    return list_create_sized(DEFAULT_LIST_SIZE);
}

/******************************************************************************/
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
            free((void *)self->items[i]);
            self->items[i] = 0;
        }
    }

    free(self->items);
    free(self);
}

/******************************************************************************/
static int
grow_list(struct list *self)
{
    int rv = 1;
    unsigned int new_alloc_size = self->alloc_size + self->grow_by;
    tbus *p = (tbus *)realloc(self->items, sizeof(tbus) * new_alloc_size);
    if (p == NULL)
    {
        rv = 0;
    }
    else
    {
        self->alloc_size = new_alloc_size;
        self->items = p;
    }
    return rv;
}

/*****************************************************************************/
int
list_add_item(struct list *self, tbus item)
{
    if (self->count == self->alloc_size && !grow_list(self))
    {
        return 0;
    }

    self->items[self->count] = item;
    self->count++;
    return 1;
}

/******************************************************************************/
tbus
list_get_item(const struct list *self, int index)
{
    if (index < 0 || index >= self->count)
    {
        return 0;
    }

    return self->items[index];
}

/******************************************************************************/
void
list_clear(struct list *self)
{
    int i;

    if (self->auto_free)
    {
        for (i = 0; i < self->count; i++)
        {
            free((void *)self->items[i]);
            self->items[i] = 0;
        }
    }

    self->count = 0;
    self->grow_by = DEFAULT_GROW_BY_SIZE;
    self->alloc_size = DEFAULT_LIST_SIZE;
    self->items = (tbus *)realloc(self->items, sizeof(tbus) * self->alloc_size);
}

/******************************************************************************/
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

/******************************************************************************/
void
list_remove_item(struct list *self, int index)
{
    int i;

    if (index >= 0 && index < self->count)
    {
        if (self->auto_free)
        {
            free((void *)self->items[index]);
            self->items[index] = 0;
        }

        for (i = index; i < (self->count - 1); i++)
        {
            self->items[i] = self->items[i + 1];
        }

        self->count--;
    }
}

int
list_insert_item(struct list *self, int index, tbus item)
{
    int i;

    if (index > self->count)
    {
        index = self->count;
    }
    else if (index < 0)
    {
        index = 0;
    }

    if (self->count == self->alloc_size && !grow_list(self))
    {
        return 0;
    }

    // Move all the items above this location up one
    for (i = self->count ; i > index ; --i)
    {
        self->items[i] = self->items[i - 1];
    }

    self->count++;

    self->items[index] = item;
    return 1;
}


/******************************************************************************/
int
list_add_strdup(struct list *self, const char *str)
{
    int rv;
    char *dup;

    if (str == NULL)
    {
        rv = list_add_item(self, (tintptr)str);
    }
    else if ((dup = g_strdup(str)) == NULL)
    {
        rv = 0;
    }
    else
    {
        rv = list_add_item(self, (tintptr)dup);
        if (!rv)
        {
            g_free(dup);
        }
    }

    return rv;
}

/******************************************************************************/
int
list_add_strdup_multi(struct list *self, ...)
{
    va_list ap;
    int entry_count = self->count;
    const char *s;
    int rv = 1;

    va_start(ap, self);
    while ((s = va_arg(ap, const char *)) != NULL)
    {
        if (!list_add_strdup(self, s))
        {
            rv = 0;
            break;
        }
    }
    va_end(ap);

    if (rv == 0)
    {
        // Remove the additional items we added
        while (self->count > entry_count)
        {
            list_remove_item(self, self->count - 1);
        }
    }

    return rv;
}
/******************************************************************************/
/* append one list to another using strdup for each item in the list */
/* begins copy at start_index, a zero based index on the source list */
int
list_append_list_strdup(struct list *self, struct list *dest, int start_index)
{
    int index;
    int rv = 1;
    int entry_dest_count = dest->count;

    for (index = start_index; index < self->count; index++)
    {
        const char *item = (const char *)list_get_item(self, index);
        if (!list_add_strdup(dest, item))
        {
            rv = 0;
            break;
        }
    }

    if (rv == 0)
    {
        // Remove the additional items we added
        while (dest->count > entry_dest_count)
        {
            list_remove_item(dest, dest->count - 1);
        }
    }

    return rv;
}

/******************************************************************************/
void
list_dump_items(struct list *self)
{
    int index;

    if (self->count == 0)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "List is empty");
    }

    for (index = 0; index < self->count; index++)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "%d: %p", index, (void *) list_get_item(self, index));
    }
}

/******************************************************************************/
/**
 * Appends a string fragment to a list
 * @param[in,out] start Pointer to start of fragment (by reference)
 * @param end Pointer to one past end of fragment
 * @param list List to append to
 * @result 1 for success
 *
 * In the event of a memory failure, 0 is returned and the list is deleted.
 */
int
split_string_append_fragment(const char **start, const char *end,
                             struct list *list)
{
    const unsigned int len = end - *start;
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL)
    {
        list_delete(list);
        return 0;
    }
    g_memcpy(copy, *start, len);
    copy[len] = '\0';
    if (!list_add_item(list, (tintptr)copy))
    {
        g_free(copy);
        list_delete(list);
        return 0;
    }
    *start = end + 1;
    return 1;
}

/******************************************************************************/
struct list *
split_string_into_list(const char *str, char character)
{
    struct list *result = list_create();
    if (result == NULL)
    {
        return result;
    }
    result->auto_free = 1;

    if (str == NULL)
    {
        return result;
    }

    const char *p;
    while ((p = g_strchr(str, character)) != NULL)
    {
        if (!split_string_append_fragment(&str, p, result))
        {
            return NULL;
        }
    }

    if (*str != '\0')
    {
        if (!split_string_append_fragment(&str, str + g_strlen(str), result))
        {
            return NULL;
        }
    }
    return result;
}
