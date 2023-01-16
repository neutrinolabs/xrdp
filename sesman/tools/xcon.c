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
#include <errno.h>
#include <poll.h>
#include <X11/Xlib.h>
#include <sys/select.h>

Display *g_display = 0;
int g_x_socket = 0;

int main(int argc, char **argv)
{
    int i1;
    XEvent xevent;

    g_display = XOpenDisplay(0);

    if (g_display == 0)
    {
        printf("XOpenDisplay failed\n");
        return 0;
    }

    g_x_socket = XConnectionNumber(g_display);

    while (1)
    {
        struct pollfd pollfd;
        pollfd.fd = g_x_socket;
        pollfd.events = POLLIN;
        pollfd.revents = 0;
        do
        {
            i1 = poll(&pollfd, 1, -1);
        }
        while (i1 < 0 && errno == EINTR);

        if (i1 < 0)
        {
            break;
        }

        XNextEvent(g_display, &xevent);
    }

    return 0;
}
