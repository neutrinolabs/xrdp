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

#if !defined(LIST16_H)
#define LIST16_H

#include "arch.h"

/* list */
struct list16
{
    tui16 *items;
    int count;
    int max_count;
    tui16 mitems[4];
};

struct list16 *
list16_create(void);
void
list16_delete(struct list16 *self);
void
list16_init(struct list16 *self);
void
list16_deinit(struct list16 *self);
void
list16_add_item(struct list16 *self, tui16 item);
tui16
list16_get_item(struct list16 *self, int index);
void
list16_clear(struct list16 *self);
int
list16_index_of(struct list16 *self, tui16 item);
void
list16_remove_item(struct list16 *self, int index);
void
list16_insert_item(struct list16 *self, int index, tui16 item);

#endif
