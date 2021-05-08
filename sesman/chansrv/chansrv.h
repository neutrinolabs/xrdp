/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2013
 * Copyright (C) Laxmikant Rashinkar 2009-2013
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

#if !defined(CHANSRV_H)
#define CHANSRV_H

#include "arch.h"
#include "parse.h"
#include "log.h"

struct chan_item
{
    int id;
    int flags;
    char name[16];
};

int
g_is_term(void);

int send_channel_data(int chan_id, const char *data, int size);
int send_rail_drawing_orders(char *data, int size);
int main_cleanup(void);
int add_timeout(int msoffset, void (*callback)(void *data), void *data);

#ifndef GSET_UINT8
#define GSET_UINT8(_ptr, _offset, _data) \
    *((unsigned char*) (((unsigned char*)(_ptr)) + (_offset))) = (unsigned char)(_data)
#define GGET_UINT8(_ptr, _offset) \
    (*((unsigned char*) (((unsigned char*)(_ptr)) + (_offset))))
#define GSET_UINT16(_ptr, _offset, _data) \
    GSET_UINT8(_ptr, _offset, _data); \
    GSET_UINT8(_ptr, (_offset) + 1, (_data) >> 8)
#define GGET_UINT16(_ptr, _offset) \
    (GGET_UINT8(_ptr, _offset)) | \
    ((GGET_UINT8(_ptr, (_offset) + 1)) << 8)
#define GSET_UINT32(_ptr, _offset, _data) \
    GSET_UINT16(_ptr, _offset, _data); \
    GSET_UINT16(_ptr, (_offset) + 2, (_data) >> 16)
#define GGET_UINT32(_ptr, _offset) \
    (GGET_UINT16(_ptr, _offset)) | \
    ((GGET_UINT16(_ptr, (_offset) + 2)) << 16)
#endif

struct chansrv_drdynvc_procs
{
    int (*open_response)(int chan_id, int creation_status);
    int (*close_response)(int chan_id);
    int (*data_first)(int chan_id, char *data, int bytes, int total_bytes);
    int (*data)(int chan_id, char *data, int bytes);
};

int
chansrv_drdynvc_open(const char *name, int flags,
                     struct chansrv_drdynvc_procs *procs, int *chan_id);
int
chansrv_drdynvc_close(int chan_id);
int
chansrv_drdynvc_data_first(int chan_id, const char *data, int data_bytes,
                           int total_data_bytes);
int
chansrv_drdynvc_data(int chan_id, const char *data, int data_bytes);
int
chansrv_drdynvc_send_data(int chan_id, const char *data, int data_bytes);

#endif
