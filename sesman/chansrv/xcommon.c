/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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

#include <X11/Xlib.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "log.h"
#include "clipboard.h"
#include "rail.h"
#include "xcommon.h"

extern int g_clip_up;                         /* in clipboard.c */

extern int g_rail_up;                         /* in rail.c */

Display *g_display = 0;
int g_x_socket = 0;
tbus g_x_wait_obj = 0;
Screen *g_screen = 0;
int g_screen_num = 0;
Window g_root_window = 0;
Atom g_wm_delete_window_atom = 0;
Atom g_wm_protocols_atom = 0;
Atom g_utf8_string = 0;
Atom g_net_wm_name = 0;
Atom g_wm_state = 0;

static x_server_fatal_cb_type x_server_fatal_handler = 0;


/*****************************************************************************/
static int
xcommon_error_handler(Display *dis, XErrorEvent *xer)
{
    char text[256];

    XGetErrorText(dis, xer->error_code, text, 255);
    LOG_DEVEL(LOG_LEVEL_ERROR, "X error [%s](%d) opcodes %d/%d "
              "resource 0x%lx", text, xer->error_code,
              xer->request_code, xer->minor_code, xer->resourceid);
    return 0;
}

/*****************************************************************************/
/* Allow the caller to be notified on X server failure
   Specified callback can do any cleanup that needs to be done on exit,
   like removing temporary files. This is the last function called.
   Don't worry about memory leaks */
void
xcommon_set_x_server_fatal_handler(x_server_fatal_cb_type handler)
{
    x_server_fatal_handler = handler;
}

/*****************************************************************************/
/* The X server had an internal error */
static int
xcommon_fatal_handler(Display *dis)
{
    if (x_server_fatal_handler)
    {
        x_server_fatal_handler();
    }
    return 0;
}

/*****************************************************************************/
/* returns time in milliseconds
   this is like g_time2 in os_calls, but not milliseconds since machine was
   up, something else
   this is a time value similar to what the xserver uses */
int
xcommon_get_local_time(void)
{
    return g_time3();
}

/******************************************************************************/
/* this should be called first */
int
xcommon_init(void)
{
    if (g_display != 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xcommon_init: xcommon_init already called");
        return 0;
    }

    g_display = XOpenDisplay(0);

    if (g_display == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "xcommon_init: error, XOpenDisplay failed");
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_INFO, "xcommon_init: connected to display ok");

    /* setting the error handlers can cause problem when shutting down
       chansrv on some xlibs */
    XSetErrorHandler(xcommon_error_handler);
    XSetIOErrorHandler(xcommon_fatal_handler);

    g_x_socket = XConnectionNumber(g_display);

    if (g_x_socket == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "xcommon_init: XConnectionNumber failed");
        return 1;
    }

    g_x_wait_obj = g_create_wait_obj_from_socket(g_x_socket, 0);
    g_screen_num = DefaultScreen(g_display);
    g_screen = ScreenOfDisplay(g_display, g_screen_num);

    g_root_window = RootWindowOfScreen(g_screen);

    g_wm_delete_window_atom = XInternAtom(g_display, "WM_DELETE_WINDOW", 0);
    g_wm_protocols_atom = XInternAtom(g_display, "WM_PROTOCOLS", 0);
    g_utf8_string = XInternAtom(g_display, "UTF8_STRING", 0);
    g_net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", 0);
    g_wm_state = XInternAtom(g_display, "WM_STATE", 0);

    return 0;
}

/*****************************************************************************/
/* returns error
   this is called to get any wait objects for the main loop
   timeout can be nil */
int
xcommon_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    int lcount;

    if (((!g_clip_up) && (!g_rail_up)) || (objs == 0) || (count == 0))
    {
        return 0;
    }
    lcount = *count;
    objs[lcount] = g_x_wait_obj;
    lcount++;
    *count = lcount;
    return 0;
}

/*****************************************************************************/
int
xcommon_check_wait_objs(void)
{
    XEvent xevent;
    int clip_rv;
    int rail_rv;

    if ((!g_clip_up) && (!g_rail_up))
    {
        return 0;
    }
    while (XPending(g_display) > 0)
    {
        g_memset(&xevent, 0, sizeof(xevent));
        XNextEvent(g_display, &xevent);
        clip_rv = clipboard_xevent(&xevent);
        rail_rv = rail_xevent(&xevent);
        if ((clip_rv == 1) && (rail_rv == 1))
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xcommon_check_wait_objs unknown xevent type %d",
                      xevent.type);
        }
    }
    return 0;
}
