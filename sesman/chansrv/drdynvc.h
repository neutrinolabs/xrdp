/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012 LK.Rashinkar@gmail.com
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

#ifndef _DRDYNVC_H_
#define _DRDYNVC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "arch.h"
#include "chansrv.h"
#include "xcommon.h"
#include "log.h"
#include "os_calls.h"
#include "trans.h"

/* move this to tsmf.c */
#define TSMF_CHAN_ID 0x1000

/* get number of bytes in stream before s->p */
#define stream_length_before_p(s) (int) ((s)->p - (s)->data)

/* get number of bytes in stream after s->p */
#define stream_length_after_p(s) (int) ((s)->end - (s)->p)

#define rewind_stream(s) do      \
{                                \
    (s)->p = (s)->data;          \
    (s)->end = (s)->data;        \
} while (0)

/* max number of bytes we can send in one pkt */
#define MAX_PDU_SIZE            1600

/* commands used to manage dynamic virtual channels */
#define CMD_DVC_OPEN_CHANNEL    0x10
#define CMD_DVC_DATA_FIRST      0x20
#define CMD_DVC_DATA            0x30
#define CMD_DVC_CLOSE_CHANNEL   0x40
#define CMD_DVC_CAPABILITY      0x50

int drdynvc_init(void);
int drdynvc_send_open_channel_request(int chan_pri, unsigned int chan_id,
                                             char *chan_name);
int drdynvc_send_close_channel_request(unsigned int chan_id);
int drdynvc_write_data(uint32_t chan_id, char *data, int data_size);
int drdynvc_data_in(struct stream* s, int chan_id, int chan_flags,
                           int length, int total_length);

#endif
