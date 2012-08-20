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

/*
   window manager info
   http://www.freedesktop.org/wiki/Specifications/wm-spec
*/

#include <X11/Xlib.h>
#include "chansrv.h"
#include "rail.h"
#include "xcommon.h"
#include "log.h"
#include "os_calls.h"
#include "thread_calls.h"

extern int g_rail_chan_id;      /* in chansrv.c */
extern int g_display_num;       /* in chansrv.c */
extern char* g_exec_name;       /* in chansrv.c */
extern tbus g_exec_event;       /* in chansrv.c */
extern tbus g_exec_mutex;       /* in chansrv.c */
extern tbus g_exec_sem;         /* in chansrv.c */

extern Display* g_display;           /* in xcommon.c */
extern Screen* g_screen;             /* in xcommon.c */
extern Window g_root_window;         /* in xcommon.c */
extern Atom g_wm_delete_window_atom; /* in xcommon.c */
extern Atom g_wm_protocols_atom;     /* in xcommon.c */

int g_rail_up = 0;

/* for rail_is_another_wm_running */
static int g_rail_running = 1;

/* Indicates a Client Execute PDU from client to server. */
#define TS_RAIL_ORDER_EXEC 0x0001
/* Indicates a Client Activate PDU from client to server. */
#define TS_RAIL_ORDER_ACTIVATE 0x0002
/* Indicates a Client System Parameters Update PDU from client
   to server or a Server System Parameters Update PDU
   from server to client. */
#define TS_RAIL_ORDER_SYSPARAM 0x0003
/* Indicates a Client System Command PDU from client to server. */
#define TS_RAIL_ORDER_SYSCOMMAND 0x0004
/* Indicates a bi-directional Handshake PDU. */
#define TS_RAIL_ORDER_HANDSHAKE 0x0005
/* Indicates a Client Notify Event PDU from client to server. */
#define TS_RAIL_ORDER_NOTIFY_EVENT 0x0006
/* Indicates a Client Window Move PDU from client to server. */
#define TS_RAIL_ORDER_WINDOWMOVE 0x0008
/* Indicates a Server Move/Size Start PDU and a Server Move/Size
   End PDU from server to client. */
#define TS_RAIL_ORDER_LOCALMOVESIZE 0x0009
/* Indicates a Server Min Max Info PDU from server to client. */
#define TS_RAIL_ORDER_MINMAXINFO 0x000a
/* Indicates a Client Information PDU from client to server. */
#define TS_RAIL_ORDER_CLIENTSTATUS 0x000b
/* Indicates a Client System Menu PDU from client to server. */
#define TS_RAIL_ORDER_SYSMENU 0x000c
/* Indicates a Server Language Bar Information PDU from server to
   client, or a Client Language Bar Information PDU from client to server. */
#define TS_RAIL_ORDER_LANGBARINFO 0x000d
/* Indicates a Server Execute Result PDU from server to client. */
#define TS_RAIL_ORDER_EXEC_RESULT 0x0080
/* Indicates a Client Get Application ID PDU from client to server. */
#define TS_RAIL_ORDER_GET_APPID_REQ 0x000E
/* Indicates a Server Get Application ID Response PDU from
   server to client. */
#define TS_RAIL_ORDER_GET_APPID_RESP 0x000F

/* Resize the window. */
#define SC_SIZE 0xF000
/* Move the window. */
#define SC_MOVE 0xF010
/* Minimize the window. */
#define SC_MINIMIZE 0xF020
/* Maximize the window. */
#define SC_MAXIMIZE 0xF030
/* Close the window. */
#define SC_CLOSE 0xF060
/* The ALT + SPACE key combination was pressed; display
   the window's system menu. */
#define SC_KEYMENU 0xF100
/* Restore the window to its original shape and size. */
#define SC_RESTORE 0xF120
/* Perform the default action of the window's system menu. */
#define SC_DEFAULT 0xF160

/******************************************************************************/
static int APP_CC
is_window_valid_child_of_root(unsigned int window_id)
{
  int found;
  unsigned int i;
  unsigned int nchild;
  Window r;
  Window p;
  Window* children;

  found = 0;
  XQueryTree(g_display, g_root_window, &r, &p, &children, &nchild);
  for (i = 0; i < nchild; i++)
  {
    if (window_id == children[i])
    {
      found = 1;
      break;
    }
  }
  XFree(children);
  return found;
}

/*****************************************************************************/
static int APP_CC
rail_send_init(void)
{
  struct stream* s;
  int bytes;
  char* size_ptr;

  LOG(10, ("chansrv::rail_send_init:"));
  make_stream(s);
  init_stream(s, 8182);
  out_uint16_le(s, TS_RAIL_ORDER_HANDSHAKE);
  size_ptr = s->p;
  out_uint16_le(s, 0);        /* size, set later */
  out_uint32_le(s, 1);        /* build number */
  s_mark_end(s);
  bytes = (int)((s->end - s->data) - 4);
  size_ptr[0] = bytes;
  size_ptr[1] = bytes >> 8;
  bytes = (int)(s->end - s->data);
  send_channel_data(g_rail_chan_id, s->data, bytes);
  free_stream(s);
  return 0;
}

/******************************************************************************/
static int DEFAULT_CC
anotherWMRunning(Display* display, XErrorEvent* xe)
{
  g_rail_running = 0;
  return -1;
}

/******************************************************************************/
static int APP_CC
rail_is_another_wm_running(void)
{
  XErrorHandler old;

  g_rail_running = 1;
  old = XSetErrorHandler((XErrorHandler)anotherWMRunning);
  XSelectInput(g_display, g_root_window,
               PropertyChangeMask | StructureNotifyMask |
               SubstructureRedirectMask | ButtonPressMask |
               SubstructureNotifyMask | FocusChangeMask |
               EnterWindowMask | LeaveWindowMask);
  XSync(g_display, 0);
  XSetErrorHandler((XErrorHandler)old);
  g_rail_up = g_rail_running;
  if (!g_rail_up)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
rail_init(void)
{
  LOG(10, ("chansrv::rail_init:"));
  xcommon_init();
  if (rail_is_another_wm_running())
  {
    log_message(LOG_LEVEL_ERROR, "rail_init: another window manager "
                "is running");
  }
  rail_send_init();
  g_rail_up = 1;
  return 0;
}

/*****************************************************************************/
int APP_CC
rail_deinit(void)
{
  if (g_rail_up)
  {
    /* no longer window manager */
    XSelectInput(g_display, g_root_window, 0);
    g_rail_up = 0;
  }
  return 0;
}

/*****************************************************************************/
static char* APP_CC
read_uni(struct stream* s, int num_chars)
{
  twchar* rchrs;
  char* rv;
  int index;
  int lchars;

  rchrs = 0;
  rv = 0;
  if (num_chars > 0)
  {
    rchrs = (twchar*)g_malloc((num_chars + 1) * sizeof(twchar), 0);
    for (index = 0; index < num_chars; index++)
    {
      in_uint16_le(s, rchrs[index]);
    }
    rchrs[num_chars] = 0;
    lchars = g_wcstombs(0, rchrs, 0);
    if (lchars > 0)
    {
      rv = (char*)g_malloc((lchars + 1) * 4, 0);
      g_wcstombs(rv, rchrs, lchars);
      rv[lchars] = 0;
    }
  }
  g_free(rchrs);
  return rv;
}

/*****************************************************************************/
static int APP_CC
rail_process_exec(struct stream* s, int size)
{
  int pid;
  int flags;
  int ExeOrFileLength;
  int WorkingDirLength;
  int ArgumentsLen;
  char* ExeOrFile;
  char* WorkingDir;
  char* Arguments;

  LOG(0, ("chansrv::rail_process_exec:"));
  in_uint16_le(s, flags);
  in_uint16_le(s, ExeOrFileLength);
  in_uint16_le(s, WorkingDirLength);
  in_uint16_le(s, ArgumentsLen);
  ExeOrFile = read_uni(s, ExeOrFileLength);
  WorkingDir = read_uni(s, WorkingDirLength);
  Arguments = read_uni(s, ArgumentsLen);
  LOG(10, ("  flags 0x%8.8x ExeOrFileLength %d WorkingDirLength %d "
      "ArgumentsLen %d ExeOrFile [%s] WorkingDir [%s] "
      "Arguments [%s]", flags, ExeOrFileLength, WorkingDirLength,
      ArgumentsLen, ExeOrFile, WorkingDir, Arguments));
  if (g_strlen(ExeOrFile) > 0)
  {
    LOG(10, ("rail_process_exec: pre"));
    /* ask main thread to fork */
    tc_mutex_lock(g_exec_mutex);
    g_exec_name = ExeOrFile;
    g_set_wait_obj(g_exec_event);
    tc_sem_dec(g_exec_sem);
    tc_mutex_unlock(g_exec_mutex);
    LOG(10, ("rail_process_exec: post"));
  }
  g_free(ExeOrFile);
  g_free(WorkingDir);
  g_free(Arguments);
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_activate(struct stream* s, int size)
{
  int window_id;
  int enabled;

  LOG(10, ("chansrv::rail_process_activate:"));
  in_uint32_le(s, window_id);
  in_uint8(s, enabled);
  LOG(10, ("  window_id 0x%8.8x enabled %d", window_id, enabled));
  if (enabled)
  {
    LOG(10, ("chansrv::rail_process_activate: calling XRaiseWindow 0x%8.8x", window_id));
    XRaiseWindow(g_display, window_id);
    LOG(10, ("chansrv::rail_process_activate: calling XSetInputFocus 0x%8.8x", window_id));
    XSetInputFocus(g_display, window_id, RevertToParent, CurrentTime);
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_system_param(struct stream* s, int size)
{
  int system_param;

  LOG(10, ("chansrv::rail_process_system_param:"));
  in_uint32_le(s, system_param);
  LOG(10, ("  system_param 0x%8.8x", system_param));
  return 0;
}

/******************************************************************************/
static int APP_CC
rail_close_window(int window_id)
{
  XEvent ce;

  LOG(0, ("chansrv::rail_close_window:"));
  g_memset(&ce, 0, sizeof(ce));
  ce.xclient.type = ClientMessage;
  ce.xclient.message_type = g_wm_protocols_atom;
  ce.xclient.display = g_display;
  ce.xclient.window = window_id;
  ce.xclient.format = 32;
  ce.xclient.data.l[0] = g_wm_delete_window_atom;
  ce.xclient.data.l[1] = CurrentTime;
  XSendEvent(g_display, window_id, False, NoEventMask, &ce);
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_system_command(struct stream* s, int size)
{
  int window_id;
  int command;

  LOG(10, ("chansrv::rail_process_system_command:"));
  in_uint32_le(s, window_id);
  in_uint16_le(s, command);
  switch (command)
  {
    case SC_SIZE:
      LOG(10, ("  window_id 0x%8.8x SC_SIZE", window_id));
      break;
    case SC_MOVE:
      LOG(10, ("  window_id 0x%8.8x SC_MOVE", window_id));
      break;
    case SC_MINIMIZE:
      LOG(10, ("  window_id 0x%8.8x SC_MINIMIZE", window_id));
      break;
    case SC_MAXIMIZE:
      LOG(10, ("  window_id 0x%8.8x SC_MAXIMIZE", window_id));
      break;
    case SC_CLOSE:
      LOG(10, ("  window_id 0x%8.8x SC_CLOSE", window_id));
      rail_close_window(window_id);
      break;
    case SC_KEYMENU:
      LOG(10, ("  window_id 0x%8.8x SC_KEYMENU", window_id));
      break;
    case SC_RESTORE:
      LOG(10, ("  window_id 0x%8.8x SC_RESTORE", window_id));
      break;
    case SC_DEFAULT:
      LOG(10, ("  window_id 0x%8.8x SC_DEFAULT", window_id));
      break;
    default:
      LOG(10, ("  window_id 0x%8.8x unknown command command %d",
          window_id, command));
      break;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_handshake(struct stream* s, int size)
{
  int build_number;

  LOG(10, ("chansrv::rail_process_handshake:"));
  in_uint32_le(s, build_number);
  LOG(10, ("  build_number 0x%8.8x", build_number));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_notify_event(struct stream* s, int size)
{
  int window_id;
  int notify_id;
  int message;

  LOG(10, ("chansrv::rail_process_notify_event:"));
  in_uint32_le(s, window_id);
  in_uint32_le(s, notify_id);
  in_uint32_le(s, message);
  LOG(10, ("  window_id 0x%8.8x notify_id 0x%8.8x message 0x%8.8x",
      window_id, notify_id, message));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_window_move(struct stream* s, int size)
{
  int window_id;
  int left;
  int top;
  int right;
  int bottom;

  LOG(10, ("chansrv::rail_process_window_move:"));
  in_uint32_le(s, window_id);
  in_uint16_le(s, left);
  in_uint16_le(s, top);
  in_uint16_le(s, right);
  in_uint16_le(s, bottom);
  LOG(10, ("  window_id 0x%8.8x left %d top %d right %d bottom %d width %d height %d",
      window_id, left, top, right, bottom, right - left, bottom - top));
  XMoveResizeWindow(g_display, window_id, left, top, right - left, bottom - top);
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_local_move_size(struct stream* s, int size)
{
  int window_id;
  int is_move_size_start;
  int move_size_type;
  int pos_x;
  int pos_y;

  LOG(10, ("chansrv::rail_process_local_move_size:"));
  in_uint32_le(s, window_id);
  in_uint16_le(s, is_move_size_start);
  in_uint16_le(s, move_size_type);
  in_uint16_le(s, pos_x);
  in_uint16_le(s, pos_y);
  LOG(10, ("  window_id 0x%8.8x is_move_size_start %d move_size_type %d "
      "pos_x %d pos_y %d", window_id, is_move_size_start, move_size_type,
      pos_x, pos_y));
  return 0;
}

/*****************************************************************************/
/* server to client only */
static int APP_CC
rail_process_min_max_info(struct stream* s, int size)
{
  LOG(10, ("chansrv::rail_process_min_max_info:"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_client_status(struct stream* s, int size)
{
  int flags;

  LOG(10, ("chansrv::rail_process_client_status:"));
  in_uint32_le(s, flags);
  LOG(10, ("  flags 0x%8.8x", flags));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_sys_menu(struct stream* s, int size)
{
  int window_id;
  int left;
  int top;

  LOG(10, ("chansrv::rail_process_sys_menu:"));
  in_uint32_le(s, window_id);
  in_uint16_le(s, left);
  in_uint16_le(s, top);
  LOG(10, ("  window_id 0x%8.8x left %d top %d", window_id, left, top));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_lang_bar_info(struct stream* s, int size)
{
  int language_bar_status;

  LOG(10, ("chansrv::rail_process_lang_bar_info:"));
  in_uint32_le(s, language_bar_status);
  LOG(10, ("  language_bar_status 0x%8.8x", language_bar_status));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_appid_req(struct stream* s, int size)
{
  LOG(10, ("chansrv::rail_process_appid_req:"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
rail_process_appid_resp(struct stream* s, int size)
{
  LOG(10, ("chansrv::rail_process_appid_resp:"));
  return 0;
}

/*****************************************************************************/
/* server to client only */
static int APP_CC
rail_process_exec_result(struct stream* s, int size)
{
  LOG(10, ("chansrv::rail_process_exec_result:"));
  return 0;
}

/*****************************************************************************/
/* data in from client ( client -> xrdp -> chansrv ) */
int APP_CC
rail_data_in(struct stream* s, int chan_id, int chan_flags, int length,
             int total_length)
{
  int code;
  int size;

  LOG(10, ("chansrv::rail_data_in:"));
  in_uint8(s, code);
  in_uint8s(s, 1);
  in_uint16_le(s, size);
  switch (code)
  {
    case TS_RAIL_ORDER_EXEC: /* 1 */
      rail_process_exec(s, size);
      break;
    case TS_RAIL_ORDER_ACTIVATE: /* 2 */
      rail_process_activate(s, size);
      break;
    case TS_RAIL_ORDER_SYSPARAM: /* 3 */
      rail_process_system_param(s, size);
      break;
    case TS_RAIL_ORDER_SYSCOMMAND: /* 4 */
      rail_process_system_command(s, size);
      break;
    case TS_RAIL_ORDER_HANDSHAKE: /* 5 */
      rail_process_handshake(s, size);
      break;
    case TS_RAIL_ORDER_NOTIFY_EVENT: /* 6 */
      rail_process_notify_event(s, size);
      break;
    case TS_RAIL_ORDER_WINDOWMOVE: /* 8 */
      rail_process_window_move(s, size);
      break;
    case TS_RAIL_ORDER_LOCALMOVESIZE: /* 9 */
      rail_process_local_move_size(s, size);
      break;
    case TS_RAIL_ORDER_MINMAXINFO: /* 10 */
      rail_process_min_max_info(s, size);
      break;
    case TS_RAIL_ORDER_CLIENTSTATUS: /* 11 */
      rail_process_client_status(s, size);
      break;
    case TS_RAIL_ORDER_SYSMENU: /* 12 */
      rail_process_sys_menu(s, size);
      break;
    case TS_RAIL_ORDER_LANGBARINFO: /* 13 */
      rail_process_lang_bar_info(s, size);
      break;
    case TS_RAIL_ORDER_GET_APPID_REQ: /* 14 */
      rail_process_appid_req(s, size);
      break;
    case TS_RAIL_ORDER_GET_APPID_RESP: /* 15 */
      rail_process_appid_resp(s, size);
      break;
    case TS_RAIL_ORDER_EXEC_RESULT: /* 128 */
      rail_process_exec_result(s, size);
      break;
    default:
      LOG(10, ("rail_data_in: unknown code %d size %d", code, size));
      break;
  }
  XFlush(g_display);
  return  0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int APP_CC
rail_xevent(void* xevent)
{
  XEvent* lxevent;
  XWindowChanges xwc;
  int rv;
  int nchildren_return = 0;
  Window root_return;
  Window parent_return;
  Window *children_return;
  Window wreturn;
  int revert_to;
  XWindowAttributes wnd_attributes;

  LOG(10, ("chansrv::rail_xevent:"));
  if (!g_rail_up)
  {
    return 1;
  }
  rv = 1;
  lxevent = (XEvent*)xevent;
  switch (lxevent->type)
  {
    case ConfigureRequest:
      LOG(10, ("  got ConfigureRequest window_id 0x%8.8x", lxevent->xconfigurerequest.window));
      g_memset(&xwc, 0, sizeof(xwc));
      xwc.x = lxevent->xconfigurerequest.x;
      xwc.y = lxevent->xconfigurerequest.y;
      xwc.width = lxevent->xconfigurerequest.width;
      xwc.height = lxevent->xconfigurerequest.height;
      xwc.border_width = lxevent->xconfigurerequest.border_width;
      xwc.sibling = lxevent->xconfigurerequest.above;
      xwc.stack_mode = lxevent->xconfigurerequest.detail;
      XConfigureWindow(g_display,
                       lxevent->xconfigurerequest.window,
                       lxevent->xconfigurerequest.value_mask,
                       &xwc);
      rv = 0;
      break;

    case MapRequest:
      LOG(10, ("  got MapRequest"));
      XMapWindow(g_display, lxevent->xmaprequest.window);
      rv = 0;
      break;

    case MapNotify:
      LOG(10, ("  got MapNotify"));
      break;

    case UnmapNotify:
      LOG(10, ("  got UnmapNotify"));
      break;

    case ConfigureNotify:
      LOG(10, ("  got ConfigureNotify"));
      break;

    case FocusIn:
      LOG(10, ("  got FocusIn"));
      break;

    case ButtonPress:
      LOG(10, ("  got ButtonPress"));
      break;

    case EnterNotify:
      LOG(10, ("  got EnterNotify"));
      break;

    case LeaveNotify:
      LOG(10, ("  got LeaveNotify"));
      break;

  }
  return rv;
}
