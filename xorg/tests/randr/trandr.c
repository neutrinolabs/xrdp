/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2013
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

Display *g_disp = 0;
Window g_win = 0;
int g_winWidth = 50;
int g_winHeight = 50;

int
main(int argc, char **argv)
{
    XEvent ev;
    int screenNumber;
    int white;
    int black;

    g_disp = XOpenDisplay(0);

    screenNumber = DefaultScreen(g_disp);
    white = WhitePixel(g_disp, screenNumber);
    black = BlackPixel(g_disp, screenNumber);

    g_win = XCreateSimpleWindow(g_disp, DefaultRootWindow(g_disp),
                                50, 50, g_winWidth, g_winHeight,
                                0, black, white);
    while (1)
    {
        XNextEvent(g_disp, &ev)
    }

    return 0;
}