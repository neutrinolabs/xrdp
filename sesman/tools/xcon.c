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
#include <sys/types.h>
#include <X11/Xlib.h>

Display *g_display = 0;
int g_x_socket = 0;

int main(int argc, char **argv)
{
    fd_set rfds;
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
        FD_ZERO(&rfds);
        FD_SET(g_x_socket, &rfds);
        i1 = select(g_x_socket + 1, &rfds, 0, 0, 0);

        if (i1 < 0)
        {
            break;
        }

        XNextEvent(g_display, &xevent);
    }

    return 0;
}
