/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
 * Copyright (C) Laxmikant Rashinkar 2012-2013
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
 * sample program to demonstrate use of xrdpapi
 *
 */

/*
 * build instructions:
 *     gcc simple.c -o simple -L./.libs -lxrdpapi
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifdef __WIN32__
#include <mstsapi.h>
#endif

#include "xrdpapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* forward declarations */
int run_echo_test(void);
int run_tsmf_test(void);

int
main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: simple <echo|tsmf>\n");
        return 1;
    }

    if (strcasecmp(argv[1], "echo") == 0)
    {
        return run_echo_test();
    }
    else if (strcasecmp(argv[1], "tsmf") == 0)
    {
        return run_tsmf_test();
    }
    else
    {
        printf("usage: simple <echo|tsmf>\n");
        return 1;
    }
}

/**
 * perform an ECHO test with a Microsoft Windows RDP client
 *
 * A Microsoft Windows RDP client echos data written
 * to a dynamic virtual channel named ECHO
 *
 * NOTE: THIS TEST WILL FAIL IF CONNECTED FROM A NON
 *       WINDOWS RDP CLIENT
 *
 * @return 0 on success, -1 on failure
 */
int
run_echo_test(void)
{
    char    out_buf[8192];
    char    in_buf[1700];
    void   *channel;
    int     bytes_left;
    int     rv;
    int     count;
    int     pkt_count;
    int     i;
    int     bytes_written;
    int     bytes_read;

    unsigned char c;
    unsigned char *rd_ptr;
    unsigned char *wr_ptr;

    /* fill out_buf[] with incremental values */
    for (i = 0, c = 0; i < 8192; i++, c++)
    {
        out_buf[i] = c;
    }

    /* open a virtual channel named ECHO */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "ECHO", WTS_CHANNEL_OPTION_DYNAMIC_PRI_LOW);

    if (channel == NULL)
    {
        printf("###  WTSVirtualChannelOpenEx() failed!\n");
        return -1;
    }

    bytes_left = 8192;
    wr_ptr = out_buf;
    rd_ptr = out_buf;
    pkt_count = 1;

    while (bytes_left > 0)
    {
        /* write data to virtual channel */
        count = (bytes_left > 1700) ? 1700 : bytes_left;
        rv = WTSVirtualChannelWrite(channel, wr_ptr, count, &bytes_written);

        if ((rv == 0) || (bytes_written == 0))
        {
            printf("### WTSVirtualChannelWrite() failed\n");
            return -1;
        }

        count = bytes_written;

        while (count)
        {
            /* read back the echo */
            rv = WTSVirtualChannelRead(channel, 5000, in_buf, count, &bytes_read);

            if ((rv == 0) || (bytes_read == 0))
            {
                printf("### WTSVirtualChannelRead() failed\n");
                return -1;
            }

            /* validate the echo */
            for (i = 0; i < bytes_read; i++, rd_ptr++)
            {
                if (*rd_ptr != (unsigned char) in_buf[i])
                {
                    printf("### data mismatch: expected 0x%x got 0x%x\n",
                           (unsigned char) *rd_ptr, (unsigned char) in_buf[i]);
                    return -1;
                }
            }

            count -= bytes_read;
        }

        bytes_left -= bytes_written;
        wr_ptr += bytes_written;
        printf("### pkt %d passed echo test\n", pkt_count++);
    }

    WTSVirtualChannelClose(channel);
    return 0;
}

int
run_tsmf_test(void)
{
    void *channel;

    printf("this test not yet implemented!\n");
    return 1;

    /* open virtual channel */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "TSMF", WTS_CHANNEL_OPTION_DYNAMIC_PRI_LOW);

    if (channel == NULL)
    {
        printf("###  WTSVirtualChannelOpenEx() failed!\n");
        return 1;
    }

    WTSVirtualChannelClose(channel);
    return 0;
}
