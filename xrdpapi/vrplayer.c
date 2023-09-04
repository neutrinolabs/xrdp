/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012-2013 LK.Rashinkar@gmail.com
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
 * a program that uses xrdpapi and ffmpeg to redirect media streams
 * to a FreeRDP client where it is decompressed and played locally
 *
 */

/******************************************************************************
 * build instructions:  make -f vrplayer.mk
 * setup environment:   export LD_LIBRARY_PATH=".libs:../xrdpvr/.libs"
 * run vrplayer:        vrplayer <media file>
 *****************************************************************************/

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifdef __WIN32__
#include <mstsapi.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "xrdpapi.h"
#include "xrdpvr.h"

void
extract_32(uint32_t *value, char *buf)
{
    *value =  ((buf[3] << 24) & 0xff000000)
              | ((buf[2] << 16) & 0x00ff0000)
              | ((buf[1] <<  8) & 0x0000ff00)
              |  (buf[0]        & 0x000000ff);
}

int
main(int argc, char **argv)
{
    void    *channel;
    int     written = 0;
    int     first_time = 1;

    if (argc < 2)
    {
        printf("usage: vrplayer <media filename> >\n");
        return 1;
    }

    /* open a virtual channel and connect to remote client */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "xrdpvr", 0);

    if (channel == NULL)
    {
        fprintf(stderr, "WTSVirtualChannelOpenEx() failed\n");
        return 1;
    }

    /* initialize the player */
    if (xrdpvr_init_player(channel, 101, argv[1]))
    {
        fprintf(stderr, "failed to initialize the player\n");
        return 1;
    }

    /* send compressed media data to client; client will decompress */
    /* the media and play it locally                                */
    xrdpvr_play_media(channel, 101, argv[1]);

    /* perform clean up */
    xrdpvr_deinit_player(channel, 101);

    WTSVirtualChannelClose(channel);
    return 0;
}
