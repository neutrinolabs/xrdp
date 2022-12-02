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

#ifndef _FIFO_H
#define _FIFO_H

#include "arch.h"

typedef struct user_data USER_DATA;

struct user_data
{
    USER_DATA *next;
    void      *item;
};

typedef struct fifo
{
    USER_DATA *head;
    USER_DATA *tail;
    int        auto_free;
} FIFO;

FIFO *fifo_create(void);
void   fifo_delete(FIFO *self);
int    fifo_add_item(FIFO *self, void *item);
void *fifo_remove_item(FIFO *self);
int    fifo_is_empty(FIFO *self);

#endif
