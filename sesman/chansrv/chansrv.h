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

#define MAX_DVC_CHANNELS 32

struct chan_out_data
{
    struct stream *s;
    struct chan_out_data *next;
};

struct chan_item
{
    int id;
    int flags;
    char name[16];
    struct chan_out_data *head;
    struct chan_out_data *tail;
};

/* data in struct trans::callback_data */
struct xrdp_api_data
{
    int  chan_id;
    char header[64];
    int  flags;

    /* for dynamic virtual channels */
    struct trans *transp;
    int           dvc_chan_id;
    int           is_connected;
};

int
g_is_term(void);

int send_channel_data(int chan_id, char *data, int size);
int send_rail_drawing_orders(char* data, int size);
int main_cleanup(void);
int add_timeout(int msoffset, void (*callback)(void* data), void* data);
int find_empty_slot_in_dvc_channels(void);
struct xrdp_api_data * struct_from_dvc_chan_id(tui32 dvc_chan_id);
int remove_struct_with_chan_id(tui32 dvc_chan_id);

#define LOG_LEVEL 5

#define LOG(_a, _params) \
    { \
        if (_a < LOG_LEVEL) \
        { \
            g_write("xrdp-chansrv [%10.10u]: ", g_time3()); \
            g_writeln _params ; \
        } \
    }

#define LOGM(_args) do { log_message _args ; } while (0)

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

#endif
