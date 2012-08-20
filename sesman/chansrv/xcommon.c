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

#include <X11/Xlib.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "log.h"
#include "clipboard.h"
#include "rail.h"

extern int g_clip_up;                         /* in clipboard.c */
extern int g_waiting_for_data_response;       /* in clipboard.c */
extern int g_waiting_for_data_response_time;  /* in clipboard.c */

extern int g_rail_up;                         /* in rail.c */

Display* g_display = 0;
int g_x_socket = 0;
tbus g_x_wait_obj = 0;
Screen* g_screen = 0;
int g_screen_num = 0;
Window g_root_window = 0;
Atom g_wm_delete_window_atom = 0;
Atom g_wm_protocols_atom = 0;

/*****************************************************************************/
static int DEFAULT_CC
xcommon_error_handler(Display* dis, XErrorEvent* xer)
{
  char text[256];

  XGetErrorText(dis, xer->error_code, text, 255);
  LOGM((LOG_LEVEL_ERROR, "X error [%s](%d) opcodes %d/%d "
       "resource 0x%lx", text, xer->error_code,
       xer->request_code, xer->minor_code, xer->resourceid));
  return 0;
}

/*****************************************************************************/
/* The X server had an internal error.  This is the last function called.
   Do any cleanup that needs to be done on exit, like removing temporary files.
   Don't worry about memory leaks */
static int DEFAULT_CC
xcommon_fatal_handler(Display* dis)
{
  return 0;
}

/*****************************************************************************/
/* returns time in miliseconds
   this is like g_time2 in os_calls, but not miliseconds since machine was
   up, something else
   this is a time value similar to what the xserver uses */
int APP_CC
xcommon_get_local_time(void)
{
  return g_time3();
}

/******************************************************************************/
/* this should be called first */
int APP_CC
xcommon_init(void)
{
  if (g_display != 0)
  {
    LOG(10, ("xcommon_init: xcommon_init already called"));
    return 0;
  }
  g_display = XOpenDisplay(0);
  if (g_display == 0)
  {
    LOGM((LOG_LEVEL_ERROR, "xcommon_init: error, XOpenDisplay failed"));
    return 1;
  }

  LOG(0, ("xcommon_init: connected to display ok"));

  /* setting the error handlers can cause problem when shutting down
     chansrv on some xlibs */
  XSetErrorHandler(xcommon_error_handler);
  //XSetIOErrorHandler(xcommon_fatal_handler);

  g_x_socket = XConnectionNumber(g_display);
  if (g_x_socket == 0)
  {
    LOGM((LOG_LEVEL_ERROR, "xcommon_init: XConnectionNumber failed"));
    return 1;
  }

  g_x_wait_obj = g_create_wait_obj_from_socket(g_x_socket, 0);
  g_screen_num = DefaultScreen(g_display);
  g_screen = ScreenOfDisplay(g_display, g_screen_num);

  g_root_window = RootWindowOfScreen(g_screen);

  g_wm_delete_window_atom = XInternAtom(g_display, "WM_DELETE_WINDOW", 0);
  g_wm_protocols_atom = XInternAtom(g_display, "WM_PROTOCOLS", 0);

  return 0;
}

/*****************************************************************************/
/* returns error
   this is called to get any wait objects for the main loop
   timeout can be nil */
int APP_CC
xcommon_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;

  if (((!g_clip_up) && (!g_rail_up)) || (objs == 0) || (count == 0))
  {
    LOG(10, ("xcommon_get_wait_objs: nothing to do"));
    return 0;
  }
  lcount = *count;
  objs[lcount] = g_x_wait_obj;
  lcount++;
  *count = lcount;
  return 0;
}

/*****************************************************************************/
int APP_CC
xcommon_check_wait_objs(void)
{
  XEvent xevent;
  int time_diff;
  int clip_rv;
  int rail_rv;

  if ((!g_clip_up) && (!g_rail_up))
  {
    LOG(10, ("xcommon_check_wait_objs: nothing to do"));
    return 0;
  }
  if (g_is_wait_obj_set(g_x_wait_obj))
  {
    if (XPending(g_display) < 1)
    {
      /* something is wrong, should not get here */
      LOGM((LOG_LEVEL_ERROR, "xcommon_check_wait_objs: sck closed"));
      return 0;
    }
    if (g_waiting_for_data_response)
    {
      time_diff = xcommon_get_local_time() -
                  g_waiting_for_data_response_time;
      if (time_diff > 10000)
      {
        LOGM((LOG_LEVEL_ERROR, "xcommon_check_wait_objs: warning, "
              "waiting for data response too long"));
      }
    }
    while (XPending(g_display) > 0)
    {
      g_memset(&xevent, 0, sizeof(xevent));
      XNextEvent(g_display, &xevent);

      clip_rv = clipboard_xevent(&xevent);
      rail_rv = rail_xevent(&xevent);
      if ((clip_rv == 1) && (rail_rv == 1))
      {
        LOG(10, ("xcommon_check_wait_objs unknown xevent type %d", xevent.type));
      }
    }
  }
  return 0;
}
