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
#include <X11/extensions/Xrandr.h>

static int
process_randr(Display *disp, Screen *screen, int screenNumber, Window win,
              int event_base, XEvent *ev)
{
    XRRScreenChangeNotifyEvent *rr_screen_change_notify;

    switch (ev->type - event_base)
    {
        case RRScreenChangeNotify:
            XRRUpdateConfiguration(ev);
            rr_screen_change_notify = (XRRScreenChangeNotifyEvent *) ev;
            printf("RRScreenChangeNotify: width %d height %d\n",
                   rr_screen_change_notify->width,
                   rr_screen_change_notify->height);
            printf("DisplayWidth %d DisplayHeight %d\n",
                   DisplayWidth(disp, screenNumber),
                   DisplayHeight(disp, screenNumber));
            break;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    XEvent ev;
    Display *disp;
    Window win;
    Window root_window;
    Screen *screen;
    int screenNumber;
    int eventMask;
    int white;
    int black;
    int rr_event_base;
    int rr_error_base;
    int ver_maj;
    int ver_min;
    int cont;

    disp = XOpenDisplay(0);
    if (disp == 0)
    {
        printf("error opening display\n");
        return 1;
    }
    screenNumber = DefaultScreen(disp);
    white = WhitePixel(disp, screenNumber);
    black = BlackPixel(disp, screenNumber);

    screen = ScreenOfDisplay(disp, screenNumber);
    root_window = RootWindowOfScreen(screen);

    eventMask = StructureNotifyMask;
    XSelectInput(disp, root_window, eventMask);

    win = XCreateSimpleWindow(disp, root_window, 50, 50, 250, 250,
                              0, black, white);

    XMapWindow(disp, win);
    eventMask = StructureNotifyMask | VisibilityChangeMask;
    XSelectInput(disp, win, eventMask);

    eventMask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                ButtonReleaseMask | VisibilityChangeMask |
                FocusChangeMask | StructureNotifyMask |
                PointerMotionMask | ExposureMask | PropertyChangeMask;
    XSelectInput(disp, win, eventMask);

    if (!XRRQueryExtension(disp, &rr_event_base, &rr_error_base))
    {
        printf("error randr\n");
        return 1;
    }
    XRRQueryVersion(disp, &ver_maj, &ver_min);
    printf("randr version %d %d\n", ver_maj, ver_min);

    XRRSelectInput(disp, win, RRScreenChangeNotifyMask);

    cont = 1;
    while (cont)
    {
        XNextEvent(disp, &ev);
        switch (ev.type)
        {
            case ButtonPress:
                cont = 0;
                break;
            case ClientMessage:
                printf("ClientMessage\n");
                break;
            case ConfigureNotify:
                if (ev.xconfigure.window == root_window)
                {
                    printf("ConfigureNotify for root window width %d height %d\n",
                           ev.xconfigure.width, ev.xconfigure.height);
                }
                break;
            default:
                if ((ev.type >= rr_event_base) &&
                    (ev.type < rr_event_base + RRNumberEvents))
                {
                    printf("randr\n");
                    process_randr(disp, screen, screenNumber, win,
                                  rr_event_base, &ev);
                }
                break;
        }
    }

    return 0;
}