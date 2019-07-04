/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2019
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os_calls.h"
#include "chansrv.h"
#include "log.h"

static struct chansrv_drdynvc_procs g_audin_info;
static int g_audio_chanid = 0;

/*****************************************************************************/
static int
audio_send_version(int chan_id)
{
    int error;
    struct stream *s;

    LOG(0, ("audio_send_version:"));
    make_stream(s);
    init_stream(s, 32);
    out_uint8(s, 1); /* MSG_SNDIN_VERSION */
    out_uint32_le(s, 1); /* version */
    error = chansrv_drdynvc_data(chan_id, s->data, 5);
    free_stream(s);
    return error;
}

/*****************************************************************************/
static int
audin_open_response(int chan_id, int creation_status)
{
    LOG(0, ("audin_open_response: creation_status %d", creation_status));
    if (creation_status == 0)
    {
        return audio_send_version(chan_id);
    }
    return 0;
}

/*****************************************************************************/
static int
audin_close_response(int chan_id)
{
    LOG(0, ("audin_close_response:"));
    return 0;
}

/*****************************************************************************/
static int
audin_data_first(int chan_id, char *data, int bytes, int total_bytes)
{
    LOG(0, ("audin_data_first:"));
    return 0;
}

/*****************************************************************************/
static int
audin_data(int chan_id, char *data, int bytes)
{
    LOG(0, ("audin_data:"));
    g_hexdump(data, bytes);
    return 0;
}

/*****************************************************************************/
int
audin_init(void)
{
    int error;

    LOG(0, ("audin_init:"));
    g_memset(&g_audin_info, 0, sizeof(g_audin_info));
    g_audin_info.open_response = audin_open_response;
    g_audin_info.close_response = audin_close_response;
    g_audin_info.data_first = audin_data_first;
    g_audin_info.data = audin_data;
    error = chansrv_drdynvc_open("AUDIO_INPUT", 1, &g_audin_info,
                                 &g_audio_chanid);
    LOG(0, ("audin_init: error %d", error));
    return error;
}
