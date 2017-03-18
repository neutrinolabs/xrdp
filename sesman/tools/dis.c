/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xrdp_sockets.h"

int main(int argc, char **argv)
{
    int sck;
    int dis;
    struct sockaddr_un sa;
    size_t len;
    char *p;
    char *display;

    if (argc != 1)
    {
        printf("xrdp disconnect utility\n");
        printf("run with no parameters to disconnect you xrdp session\n");
        return 0;
    }

    display = getenv("DISPLAY");

    if (display == 0)
    {
        printf("display not set\n");
        return 1;
    }

    dis = strtol(display + 1, &p, 10);
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    sprintf(sa.sun_path, XRDP_DISCONNECT_STR, dis);

    if (access(sa.sun_path, F_OK) != 0)
    {
        printf("not in an xrdp session\n");
        return 1;
    }

    if ((sck = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket open error\n");
        return 1;
    }

    len = sizeof(sa);

    if (sendto(sck, "sig", 4, 0, (struct sockaddr *)&sa, len) > 0)
    {
        printf("message sent ok\n");
    }

    return 0;
}
