/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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

/* FIFO implementation to store a pointer to a user struct */

typedef struct fifo
{
    long *user_data;
    int   rd_ptr;
    int   wr_ptr;
    int   entries;
} FIFO;

int   fifo_init(FIFO *fp, int num_entries);
int   fifo_deinit(FIFO *fp);
int   fifo_is_empty(FIFO *fp);
int   fifo_insert(FIFO *fp, void *data);
void *fifo_remove(FIFO *fp);
void *fifo_peek(FIFO *fp);

