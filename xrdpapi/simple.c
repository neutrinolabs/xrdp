/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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

/*
 * Basic test for virtual channel use
 */

// These headers are required for the windows terminal services calls.
#include "xrdpapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DSIZE (1024 * 4)

int main()
{

    // Initialize the data for send/receive
    void *hFile;
    char *data;
    char *data1;
    data = (char *)malloc(DSIZE);
    data1 = (char *)malloc(DSIZE);
    int ret;
    void *vcFileHandlePtr = NULL;
    memset(data, 0xca, DSIZE);
    memset(data1, 0, DSIZE);
    unsigned int written = 0;

    // Open the skel channel in current session

    //void* channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "skel", 0);
    void *channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "TSMF", WTS_CHANNEL_OPTION_DYNAMIC);
    ret = WTSVirtualChannelQuery(channel, WTSVirtualFileHandle, vcFileHandlePtr, &written);

    // Write the data to the channel
    ret = WTSVirtualChannelWrite(channel, data, DSIZE, &written);

    if (!ret)
    {

        long err = errno;
        printf("error 1 0x%8.8x\n", err);
        usleep(100000);
        return 1;
    }
    else
    {
        printf("Sent bytes!\n");
    }

    if (written != DSIZE)
    {
        long err = errno;
        printf("error 2 0x%8.8x\n", err);
        usleep(100000);
        return 1;
    }
    else
    {
        printf("Read bytes!\n");
    }

    ret = WTSVirtualChannelRead(channel, 100, data1, DSIZE, &written);

    if (!ret)
    {
        long err = errno;
        printf("error 3 0x%8.8x\n", err);
        usleep(100000);
        return 1;
    }

    if (written != DSIZE)
    {
        long err = errno;
        printf("error 4 0x%8.8x\n", err);
        usleep(100000);
        return 1;
    }
    else
    {
        printf("Read bytes!\n");
    }

    ret = WTSVirtualChannelClose(channel);

    if (memcmp(data, data1, DSIZE) == 0)
    {
    }
    else
    {
        printf("error data no match\n");
        return 1;
    }

    printf("Done!\n");

    usleep(100000);
    return 0;
}
