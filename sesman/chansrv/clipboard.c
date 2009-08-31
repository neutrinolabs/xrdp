/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2009

   for help see
   http://tronche.com/gui/x/icccm/sec-2.html#s-2
   .../kde/kdebase/workspace/klipper/clipboardpoll.cpp

*/

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"

static Atom g_clipboard_atom = 0;
static Atom g_clip_property_atom = 0;
static Atom g_timestamp_atom = 0;
static Atom g_multiple_atom = 0;
static Atom g_targets_atom = 0;
static Atom g_primary_atom = 0;
static Atom g_secondary_atom = 0;
static Atom g_get_time_atom = 0;
static int g_x_socket = 0;
static tbus g_x_wait_obj = 0;
static int g_clip_up = 0;
static Window g_wnd = 0;
static Screen* g_screen = 0;
static int g_screen_num = 0;
static int g_xfixes_event_base = 0;

static int g_last_clip_size = 0;
static char* g_last_clip_data = 0;
static Atom g_last_clip_type = 0;

static int g_got_selection = 0; /* boolean */
static Time g_selection_time = 0;

static struct stream* g_ins = 0;

static XSelectionRequestEvent g_selection_request_event[16];
static int g_selection_request_event_count = 0;

static char* g_data_in = 0;
static int g_data_in_size = 0;
static int g_data_in_time = 0;

extern int g_cliprdr_chan_id; /* in chansrv.c */
extern Display* g_display; /* in chansrv.c */

/* returns time in miliseconds
   this is like g_time2 in os_calls, but not miliseconds since machine was
   up, something else
   this is a time value similar to what the xserver uses */

/*****************************************************************************/
/* this is one way to get the current time from the x server */
static Time APP_CC
clipboard_get_time(void)
{
  XEvent xevent;

  /* append nothing */
  XChangeProperty(g_display, g_wnd, g_get_time_atom, XA_STRING, 8,
                  PropModeAppend, "", 0);
  /* wait for PropertyNotify */
  do
  {
    XMaskEvent(g_display, PropertyChangeMask, &xevent);
  } while (xevent.type != PropertyNotify);
  return xevent.xproperty.time;
}

/*****************************************************************************/
/* returns error */
int APP_CC
clipboard_init(void)
{
  struct stream* s;
  int size;
  int rv;
  int input_mask;
  int dummy;
  int ver_maj;
  int ver_min;
  Status st;

  LOG(1, ("xrdp-chansrv: in clipboard_init"));
  if (g_clip_up)
  {
    return 0;
  }
  rv = 0;
  if (rv == 0)
  {
    g_x_socket = XConnectionNumber(g_display);
    if (g_x_socket == 0)
    {
      LOG(0, ("clipboard_init: XConnectionNumber failed"));
      rv = 2;
    }
    g_x_wait_obj = g_create_wait_obj_from_socket(g_x_socket, 0);
  }
  if (rv == 0)
  {
    g_clipboard_atom = XInternAtom(g_display, "CLIPBOARD", False);
    if (g_clipboard_atom == None)
    {
      LOG(0, ("clipboard_init: XInternAtom failed"));
      rv = 3;
    }
  }
  if (rv == 0)
  {
    if (!XFixesQueryExtension(g_display, &g_xfixes_event_base, &dummy))
    {
      LOG(0, ("clipboard_init: no xfixes"));
      rv = 5;
    }
  }
  if (rv == 0)
  {
    LOG(1, ("clipboard_init: g_xfixes_event_base %d", g_xfixes_event_base));
    st = XFixesQueryVersion(g_display, &ver_maj, &ver_min);
    LOG(1, ("clipboard_init st %d, maj %d min %d", st, ver_maj, ver_min));
    g_screen_num = DefaultScreen(g_display);
    g_screen = ScreenOfDisplay(g_display, g_screen_num);
    g_clip_property_atom = XInternAtom(g_display, "XRDP_CLIP_PROPERTY_ATOM",
                                       False);
    g_get_time_atom = XInternAtom(g_display, "XRDP_GET_TIME_ATOM",
                                  False);
    g_timestamp_atom = XInternAtom(g_display, "TIMESTAMP", False);
    g_targets_atom = XInternAtom(g_display, "TARGETS", False);
    g_multiple_atom = XInternAtom(g_display, "MULTIPLE", False);
    g_primary_atom = XInternAtom(g_display, "PRIMARY", False);
    g_secondary_atom = XInternAtom(g_display, "SECONDARY", False);
    g_wnd = XCreateSimpleWindow(g_display, RootWindowOfScreen(g_screen),
                                0, 0, 4, 4, 0, 0, 0);
    input_mask = StructureNotifyMask | PropertyChangeMask;
    XSelectInput(g_display, g_wnd, input_mask);
    //XMapWindow(g_display, g_wnd);
    XFixesSelectSelectionInput(g_display, g_wnd,
                               g_clipboard_atom,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);
  }
  if (rv == 0)
  {
    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, 1); /* CLIPRDR_CONNECT */
    out_uint16_le(s, 0); /* status */
    out_uint32_le(s, 0); /* length */
    out_uint32_le(s, 0); /* extra 4 bytes ? */
    s_mark_end(s);
    size = (int)(s->end - s->data);
    LOG(1, ("clipboard_init: data out, sending "
        "CLIPRDR_CONNECT (clip_msg_id = 1)"));
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    if (rv != 0)
    {
      LOG(0, ("clipboard_init: send_channel_data failed "
          "rv = %d", rv));
      rv = 4;
    }
    free_stream(s);
  }
  if (rv == 0)
  {
    g_clip_up = 1;
    make_stream(g_ins);
    init_stream(g_ins, 8192)
  }
  else
  {
    LOG(0, ("xrdp-chansrv: clipboard_init: error on exit"));
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
clipboard_deinit(void)
{
  if (!g_clip_up)
  {
    return 0;
  }
  g_delete_wait_obj_from_socket(g_x_wait_obj);
  g_x_wait_obj = 0;
  XDestroyWindow(g_display, g_wnd);
  g_wnd = 0;
  g_x_socket = 0;
  g_free(g_last_clip_data);
  g_last_clip_data = 0;
  g_last_clip_size = 0;
  free_stream(g_ins);
  g_clip_up = 0;
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_data_request(void)
{
  struct stream* s;
  int size;
  int rv;
  int num_chars;

  LOG(1, ("clipboard_send_data_request:"));
  make_stream(s);
  init_stream(s, 8192);
  out_uint16_le(s, 4); /* CLIPRDR_DATA_REQUEST */
  out_uint16_le(s, 0); /* status */
  out_uint32_le(s, 4); /* length */
  out_uint32_le(s, 0x0d);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOG(1, ("clipboard_send_data_request: data out, sending "
      "CLIPRDR_DATA_REQUEST (clip_msg_id = 4)"));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_format_ack(void)
{
  struct stream* s;
  int size;
  int rv;

  make_stream(s);
  init_stream(s, 8192);
  out_uint16_le(s, 3); /* CLIPRDR_FORMAT_ACK */
  out_uint16_le(s, 1); /* status */
  out_uint32_le(s, 0); /* length */
  out_uint32_le(s, 0); /* extra 4 bytes ? */
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOG(1, ("clipboard_send_format_ack: data out, sending "
      "CLIPRDR_FORMAT_ACK (clip_msg_id = 3)"));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_format_announce(void)
{
  struct stream* s;
  int size;
  int rv;

  make_stream(s);
  init_stream(s, 8192);
  out_uint16_le(s, 2); /* CLIPRDR_FORMAT_ANNOUNCE */
  out_uint16_le(s, 0); /* status */
  out_uint32_le(s, 0x90); /* length */
  out_uint32_le(s, 0x0d); /* extra 4 bytes ? */
  out_uint8s(s, 0x90);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOG(1, ("clipboard_send_format_announce: data out, sending "
      "CLIPRDR_FORMAT_ANNOUNCE (clip_msg_id = 2)"));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
/* returns number of bytes written */
static int APP_CC
clipboard_out_unicode(struct stream* s, char* text, int num_chars)
{
  int index;
  int lnum_chars;
  twchar* ltext;

  if ((num_chars < 1) || (text == 0))
  {
    return 0;
  }
  lnum_chars = g_mbstowcs(0, text, num_chars);
  if (lnum_chars < 0)
  {
    return 0;
  }
  ltext = g_malloc((num_chars + 1) * sizeof(twchar), 1);
  g_mbstowcs(ltext, text, num_chars);
  index = 0;
  while (index < num_chars)
  {
    out_uint16_le(s, ltext[index]);
    index++;
  }
  g_free(ltext);
  return index * 2;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_data_response(void)
{
  struct stream* s;
  int size;
  int rv;
  int num_chars;

  LOG(10, ("clipboard_send_data_response:"));
  num_chars = 0;
  if (g_last_clip_data != 0)
  {
    if (g_last_clip_type == XA_STRING)
    {
      num_chars = g_mbstowcs(0, g_last_clip_data, 0);
      if (num_chars < 0)
      {
        LOG(0, ("clipboard_send_data_response: bad string"));
        num_chars = 0;
      }
    }
  }
  LOG(10, ("clipboard_send_data_response: g_last_clip_size %d "
      "num_chars %d", g_last_clip_size, num_chars));
  make_stream(s);
  init_stream(s, 64 + num_chars * 2);
  out_uint16_le(s, 5); /* CLIPRDR_DATA_RESPONSE */
  out_uint16_le(s, 1); /* status */
  out_uint32_le(s, num_chars * 2 + 2); /* length */
  if (clipboard_out_unicode(s, g_last_clip_data, num_chars) != num_chars * 2)
  {
    LOG(0, ("clipboard_send_data_response: error "
        "clipboard_out_unicode didn't write right number of bytes"));
  }
  out_uint16_le(s, 0); /* nil for string */
  out_uint32_le(s, 0);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOG(1, ("clipboard_send_data_response: data out, sending "
      "CLIPRDR_DATA_RESPONSE (clip_msg_id = 5) size %d num_chars %d",
      size, num_chars));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_process_format_announce(struct stream* s, int clip_msg_status,
                                  int clip_msg_len)
{
  Window owner;

  LOG(1, ("clipboard_process_format_announce: CLIPRDR_FORMAT_ANNOUNCE"));
  g_hexdump(s->p, s->end - s->p);
  clipboard_send_format_ack();
  owner = XGetSelectionOwner(g_display, g_clipboard_atom);
  if (owner == g_wnd)
  {
    LOG(1, ("clipboard_process_format_announce: skipping, owner == g_wnd"));
    return 0;
  }
  g_selection_time = clipboard_get_time();
  XSetSelectionOwner(g_display, g_clipboard_atom, g_wnd, g_selection_time);
  //XSetSelectionOwner(g_display, XA_PRIMARY, g_wnd, g_selection_time);
  g_got_selection = 1;
  owner = XGetSelectionOwner(g_display, g_clipboard_atom);
  if (owner != g_wnd)
  {
    g_got_selection = 0;
    LOG(0, ("clipboard_process_format_announce: XSetSelectionOwner failed"));
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_prcoess_format_ack(struct stream* s, int clip_msg_status,
                             int clip_msg_len)
{
  LOG(1, ("clipboard_prcoess_format_ack: CLIPRDR_FORMAT_ACK"));
  //g_hexdump(s->p, s->end - s->p);
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_process_data_request(struct stream* s, int clip_msg_status,
                               int clip_msg_len)
{
  LOG(1, ("clipboard_process_data_request: CLIPRDR_DATA_REQUEST"));
  //g_hexdump(s->p, s->end - s->p);
  clipboard_send_data_response();
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_process_data_response(struct stream* s, int clip_msg_status,
                                int clip_msg_len)
{
  XEvent xev;
  XSelectionRequestEvent* lev;
  twchar* wtext;
  twchar wchr;
  int len;
  int index;
  int wtext_size;

  LOG(1, ("clipboard_process_data_response: CLIPRDR_DATA_RESPONSE"));
  len = (int)(s->end - s->p);
  if (len < 1)
  {
    return 0;
  }
  //g_hexdump(s->p, len);
  wtext = (twchar*)g_malloc(((len / 2) + 1) * sizeof(twchar), 0);
  if (wtext == 0)
  {
    return 0;
  }
  index = 0;
  while (s_check(s))
  {
    in_uint16_le(s, wchr);
    wtext[index] = wchr;
    if (wchr == 0)
    {
      break;
    }
    index++;
  }
  wtext[index] = 0;
  g_free(g_data_in);
  g_data_in = 0;
  g_data_in_size = 0;
  g_data_in_time = 0;
  len = g_wcstombs(0, wtext, 0);
  if (len >= 0)
  {
    g_data_in = (char*)g_malloc(len + 16, 0);
    if (g_data_in == 0)
    {
      g_free(wtext);
      return 0;
    }
    g_data_in_size = len;
    g_wcstombs(g_data_in, wtext, len + 1);
    len = g_strlen(g_data_in);
    g_data_in_time = g_time3();
  }
  if (g_data_in != 0)
  {
    for (index = 0; index < g_selection_request_event_count; index++)
    {
      lev = &(g_selection_request_event[index]);
      XChangeProperty(g_display, lev->requestor, lev->property,
                      XA_STRING, 8, PropModeReplace, "hi",
                      3);
//                      XA_STRING, 8, PropModeReplace, g_data_in,
//                      g_strlen(g_data_in));
      g_memset(&xev, 0, sizeof(xev));
      xev.xselection.type = SelectionNotify;
      xev.xselection.send_event = True;
      xev.xselection.display = lev->display;
      xev.xselection.requestor = lev->requestor;
      xev.xselection.selection = lev->selection;
      xev.xselection.target = lev->target;
      xev.xselection.property = lev->property;
      xev.xselection.time = CurrentTime;
      XSendEvent(g_display, lev->requestor, False, NoEventMask, &xev);
      LOG(1, ("clipboard_process_data_response: %d", lev->requestor));
    }
  }
  g_selection_request_event_count = 0;
  g_free(wtext);
  return 0;
}

/*****************************************************************************/
int APP_CC
clipboard_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
  int clip_msg_id;
  int clip_msg_len;
  int clip_msg_status;
  int rv;
  struct stream* ls;

  LOG(10, ("clipboard_data_in: chan_is %d "
      "chan_flags %d length %d total_length %d",
      chan_id, chan_flags, length, total_length));
  if ((chan_flags & 3) == 3)
  {
    ls = s;
  }
  else
  {
    if (chan_flags & 1)
    {
      init_stream(g_ins, total_length)
    }
    in_uint8a(s, g_ins->end, length);
    g_ins->end += length;
    if ((chan_flags & 2) == 0)
    {
      return 0;
    }
    ls = g_ins;
  }
  in_uint16_le(ls, clip_msg_id);
  in_uint16_le(ls, clip_msg_status);
  in_uint32_le(ls, clip_msg_len);
  LOG(10, ("clipboard_data_in: clip_msg_id %d "
      "clip_msg_status %d clip_msg_len %d",
      clip_msg_id, clip_msg_status, clip_msg_len));
  rv = 0;
  switch (clip_msg_id)
  {
    case 2: /* CLIPRDR_FORMAT_ANNOUNCE */
      rv = clipboard_process_format_announce(ls, clip_msg_status,
                                             clip_msg_len);
      break;
    case 3: /* CLIPRDR_FORMAT_ACK */
      rv = clipboard_prcoess_format_ack(ls, clip_msg_status,
                                        clip_msg_len);
      break;
    case 4: /* CLIPRDR_DATA_REQUEST */
      rv = clipboard_process_data_request(ls, clip_msg_status,
                                          clip_msg_len);
      break;
    case 5: /* CLIPRDR_DATA_RESPONSE */
      rv = clipboard_process_data_response(ls, clip_msg_status,
                                           clip_msg_len);
      break;
    default:
      LOG(0, ("clipboard_data_in: unknown clip_msg_id %d", clip_msg_id));
      break;
  }
  return rv;
}

/*****************************************************************************/
/* this happens when a new app copies something to the clipboard
   'CLIPBOARD' Atom
   typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    int subtype;
    Window owner;
    Atom selection;
    Time timestamp;
    Time selection_timestamp;
} XFixesSelectionNotifyEvent; */
static int APP_CC
clipboard_event_selection_owner_notify(XEvent* xevent)
{
  XFixesSelectionNotifyEvent* lxevent;

  lxevent = (XFixesSelectionNotifyEvent*)xevent;
  LOG(1, ("clipboard_event_selection_owner_notify: "
      "window %d subtype %d owner %d g_wnd %d",
      lxevent->window, lxevent->subtype, lxevent->owner, g_wnd));
  if (lxevent->owner == g_wnd)
  {
    LOG(1, ("clipboard_event_selection_owner_notify: skipping, "
        "onwer == g_wnd"));
    g_got_selection = 1;
    return 0;
  }
  g_got_selection = 0;
  XConvertSelection(g_display, g_clipboard_atom, g_targets_atom,
                    g_clip_property_atom, g_wnd, lxevent->timestamp);
  return 0;
}

/*****************************************************************************/
/* returns error
   get a window property from wnd */
static int APP_CC
clipboard_get_window_property(Window wnd, Atom prop, Atom* type, int* fmt,
                              int* n_items, char** xdata, int* xdata_size)
{
  int lfmt;
  int lxdata_size;
  unsigned long ln_items;
  unsigned long llen_after;
  tui8* lxdata;
  Atom ltype;

  lxdata = 0;
  ltype = 0;
  XGetWindowProperty(g_display, g_wnd, prop, 0, 0, 0,
                     AnyPropertyType, &ltype, &lfmt, &ln_items,
                     &llen_after, &lxdata);
  XFree(lxdata);
  if (ltype == 0)
  {
    /* XGetWindowProperty failed */
    return 1;
  }
  if (llen_after < 1)
  {
    /* no data, ok */
    return 0;
  }
  lxdata = 0;
  ltype = 0;
  XGetWindowProperty(g_display, g_wnd, prop, 0, (llen_after + 3) / 4, 0,
                     AnyPropertyType, &ltype, &lfmt, &ln_items,
                     &llen_after, &lxdata);
  if (ltype == 0)
  {
    /* XGetWindowProperty failed */
    XFree(lxdata);
    return 1;
  }
  lxdata_size = (lfmt / 8) * ln_items;
  if (lxdata_size < 1)
  {
    /* should not happen */
    XFree(lxdata);
    return 2;
  }
  if (llen_after > 0)
  {
    /* should not happen */
    XFree(lxdata);
    return 3;
  }
  if (xdata != 0)
  {
    *xdata = (char*)g_malloc(lxdata_size, 0);
    g_memcpy(*xdata, lxdata, lxdata_size);
  }
  XFree(lxdata);
  if (xdata_size != 0)
  {
    *xdata_size = lxdata_size;
  }
  if (fmt != 0)
  {
    *fmt = (int)lfmt;
  }
  if (n_items != 0)
  {
    *n_items = (int)ln_items;
  }
  if (type != 0)
  {
    *type = ltype;
  }
  return 0;
}

/*****************************************************************************/
/* returns error
   process the SelectionNotify X event, uses XSelectionEvent
   typedef struct {
     int type;             // SelectionNotify
     unsigned long serial; // # of last request processed by server
     Bool send_event;      // true if this came from a SendEvent request
     Display *display;     // Display the event was read from
     Window requestor;
     Atom selection;
     Atom target;
     Atom property;        // atom or None
     Time time;
   } XSelectionEvent; */
static int APP_CC
clipboard_event_selection_notify(XEvent* xevent)
{
  XSelectionEvent* lxevent;
  char* data;
  int data_size;
  int n_items;
  int fmt;
  int rv;
  int index;
  int convert_to_string;
  int send_format_announce;
  int atom;
  int* atoms;
  Atom type;

  LOG(1, ("clipboard_event_selection_notify:"));
  convert_to_string = 0;
  send_format_announce = 0;
  rv = 0;
  data = 0;
  type = 0;
  lxevent = (XSelectionEvent*)xevent;
  if (lxevent->property == None)
  {
    LOG(0, ("clipboard_event_selection_notify: clip could "
        "not be converted"));
    rv = 1;
  }
  if (rv == 0)
  {
    rv = clipboard_get_window_property(lxevent->requestor, lxevent->property,
                                       &type, &fmt,
                                       &n_items, &data, &data_size);
    if (rv != 0)
    {
      LOG(0, ("clipboard_event_selection_notify: "
          "clipboard_get_window_property failed error %d", rv));
    }
    XDeleteProperty(g_display, lxevent->requestor, lxevent->property);
  }
  if (rv == 0)
  {
    if (lxevent->selection == g_clipboard_atom)
    {
      if (lxevent->target == g_targets_atom)
      {
        if ((type == XA_ATOM) && (fmt == 32))
        {
          atoms = (int*)data;
          for (index = 0; index < n_items; index++)
          {
            atom = atoms[index];
            LOG(1, ("clipboard_event_selection_notify: %d %s %d", atom,
                XGetAtomName(g_display, atom), XA_STRING));
            if (atom == XA_STRING)
            {
              convert_to_string = 1;
            }
          }
        }
        else
        {
          LOG(0, ("clipboard_event_selection_notify: error, target is "
              "'TARGETS' and type[%d] or fmt[%d] not right, should be "
              "type[%d], fmt[%d]", type, fmt, XA_ATOM, 32));
        }
      }
      else if (lxevent->target == XA_STRING)
      {
        LOG(10, ("clipboard_event_selection_notify: XA_STRING data_size %d",
            data_size));
        g_free(g_last_clip_data);
        g_last_clip_size = data_size;
        g_last_clip_data = g_malloc(g_last_clip_size + 1, 0);
        g_last_clip_type = XA_STRING;
        g_memcpy(g_last_clip_data, data, g_last_clip_size);
        g_last_clip_data[g_last_clip_size] = 0;
        send_format_announce = 1;
      }
      else
      {
        LOG(0, ("clipboard_event_selection_notify: unknown target"));
      }
    }
    else
    {
      LOG(0, ("clipboard_event_selection_notify: unknown selection"));
    }
  }
  if (convert_to_string)
  {
    XConvertSelection(g_display, g_clipboard_atom, XA_STRING,
                      g_clip_property_atom, g_wnd, CurrentTime);
  }
  if (send_format_announce)
  {
    if (clipboard_send_format_announce() != 0)
    {
      rv = 4;
    }
  }
  g_free(data);
  return rv;
}

/*****************************************************************************/
/* returns error
   process the SelectionRequest X event, uses XSelectionRequestEvent
   typedef struct {
     int type;             // SelectionRequest
     unsigned long serial; // # of last request processed by server
     Bool send_event;      // true if this came from a SendEvent request
     Display *display;     // Display the event was read from
     Window owner;
     Window requestor;
     Atom selection;
     Atom target;
     Atom property;
     Time time;
   } XSelectionRequestEvent; */
static int APP_CC
clipboard_event_selection_request(XEvent* xevent)
{
  XEvent xev;
  XSelectionRequestEvent* lxevent;
  tui32 ui32[4];
  Atom type;
  int fmt;
  int n_items;
  int xdata_size;
  char* xdata;

  lxevent = (XSelectionRequestEvent*)xevent;
  LOG(1, ("clipboard_event_selection_request: g_wnd %d, "
      ".requestor %d .owner %d .select %d .target %d .property %d",
      g_wnd, lxevent->requestor, lxevent->owner, lxevent->selection,
      lxevent->target, lxevent->property));
  if (lxevent->property == None)
  {
    LOG(1, ("clipboard_event_selection_request: lxevent->property is None"));
  }
  else if (lxevent->target == g_targets_atom)
  {
    /* requestor is asking what the selection can be converted to */
    LOG(1, ("clipboard_event_selection_request: g_targets_atom"));
    ui32[0] = g_targets_atom;
    ui32[1] = g_timestamp_atom;
    ui32[2] = g_multiple_atom;
    ui32[3] = XA_STRING;
    XChangeProperty(g_display, lxevent->requestor, lxevent->property,
                    XA_ATOM, 32, PropModeReplace, (tui8*)ui32, 4);
    g_memset(&xev, 0, sizeof(xev));
    xev.xselection.type = SelectionNotify;
    xev.xselection.send_event = True;
    xev.xselection.display = lxevent->display;
    xev.xselection.requestor = lxevent->requestor;
    xev.xselection.selection = lxevent->selection;
    xev.xselection.target = lxevent->target;
    xev.xselection.property = lxevent->property;
    xev.xselection.time = lxevent->time;
    XSendEvent(g_display, lxevent->requestor, False, NoEventMask, &xev);
    return 0;
  }
  else if (lxevent->target == g_timestamp_atom)
  {
    /* requestor is asking the time I got the selection */
    LOG(1, ("clipboard_event_selection_request: g_timestamp_atom"));
    ui32[0] = g_selection_time;
    XChangeProperty(g_display, lxevent->requestor, lxevent->property,
                    XA_INTEGER, 32, PropModeReplace, (tui8*)ui32, 1);
    g_memset(&xev, 0, sizeof(xev));
    xev.xselection.type = SelectionNotify;
    xev.xselection.send_event = True;
    xev.xselection.display = lxevent->display;
    xev.xselection.requestor = lxevent->requestor;
    xev.xselection.selection = lxevent->selection;
    xev.xselection.target = lxevent->target;
    xev.xselection.property = lxevent->property;
    xev.xselection.time = lxevent->time;
    XSendEvent(g_display, lxevent->requestor, False, NoEventMask, &xev);
    return 0;
  }
  else if (lxevent->target == g_multiple_atom)
  {
    /* target, property pairs */
    LOG(1, ("clipboard_event_selection_request: g_multiple_atom"));
    if (clipboard_get_window_property(xev.xselection.requestor,
                                      xev.xselection.property,
                                      &type, &fmt, &n_items, &xdata,
                                      &xdata_size) == 0)
    {
      LOG(1, ("clipboard_event_selection_request: g_multiple_atom "
          "n_items %d", n_items));
      /* todo */
      g_free(xdata);
    }
  }
  else if (lxevent->target == XA_STRING)
  {
    LOG(1, ("clipboard_event_selection_request: XA_STRING"));
/*
    if (g_abs((g_time3() - g_data_in_time)) < 1000)
    {
      LOG(1, ("clipboard_event_selection_request: XA_STRING---------------------------"));
      XChangeProperty(g_display, lxevent->requestor, lxevent->property,
                      XA_STRING, 8, PropModeReplace, g_data_in,
                      g_strlen(g_data_in));
      g_memset(&xev, 0, sizeof(xev));
      xev.xselection.type = SelectionNotify;
      xev.xselection.send_event = True;
      xev.xselection.display = lxevent->display;
      xev.xselection.requestor = lxevent->requestor;
      xev.xselection.selection = lxevent->selection;
      xev.xselection.target = lxevent->target;
      xev.xselection.property = lxevent->property;
      xev.xselection.time = lxevent->time;
      XSendEvent(g_display, lxevent->requestor, False, NoEventMask, &xev);
      return 0;
    }
*/
/*
    XChangeProperty(g_display, lxevent->requestor, lxevent->property,
                    XA_STRING, 8, PropModeReplace, "Jay--", 5);
    g_memset(&xev, 0, sizeof(xev));
    xev.xselection.type = SelectionNotify;
    xev.xselection.send_event = True;
    xev.xselection.display = lxevent->display;
    xev.xselection.requestor = lxevent->requestor;
    xev.xselection.selection = lxevent->selection;
    xev.xselection.target = lxevent->target;
    xev.xselection.property = lxevent->property;
    xev.xselection.time = lxevent->time;
    XSendEvent(g_display, lxevent->requestor, False, NoEventMask, &xev);
    return 0;
*/
    if (g_selection_request_event_count > 10)
    {
      LOG(0, ("clipboard_event_selection_request: error, too many requests"));
    }
    else
    {
      g_memcpy(&(g_selection_request_event[g_selection_request_event_count]),
               lxevent, sizeof(g_selection_request_event[0]));
      if (g_selection_request_event_count == 0)
      {
        clipboard_send_data_request();
      }
      g_selection_request_event_count++;
      return 0;
    }
  }
  else
  {
    LOG(1, ("clipboard_event_selection_request: unknown "
        "target %s", XGetAtomName(g_display, lxevent->target)));
  }
  g_memset(&xev, 0, sizeof(xev));
  xev.xselection.type = SelectionNotify;
  xev.xselection.send_event = True;
  xev.xselection.display = lxevent->display;
  xev.xselection.requestor = lxevent->requestor;
  xev.xselection.selection = lxevent->selection;
  xev.xselection.target = lxevent->target;
  xev.xselection.property = None;
  xev.xselection.time = lxevent->time;
  XSendEvent(g_display, lxevent->requestor, False, NoEventMask, &xev);
  return 0;
}

/*****************************************************************************/
/* returns error
   process the SelectionClear X event, uses XSelectionClearEvent
   typedef struct {
     int type;                // SelectionClear
     unsigned long serial;    // # of last request processed by server
     Bool send_event;         // true if this came from a SendEvent request
     Display *display;        // Display the event was read from
     Window window;
     Atom selection;
     Time time;
} XSelectionClearEvent; */
static int APP_CC
clipboard_event_selection_clear(XEvent* xevent)
{
  LOG(1, ("clipboard_event_selection_clear:"));
  return 0;
}

/*****************************************************************************/
/* returns error
   this is called to get any wait objects for the main loop
   timeout can be nil */
int APP_CC
clipboard_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;

  if ((!g_clip_up) || (objs == 0) || (count == 0))
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
int APP_CC
clipboard_check_wait_objs(void)
{
  XEvent xevent;

  if (!g_clip_up)
  {
    return 0;
  }
  if (g_is_wait_obj_set(g_x_wait_obj))
  {
    if (XPending(g_display) < 1)
    {
      /* something is wrong, should not get here */
      LOG(0, ("clipboard_check_wait_objs: sck closed"));
      return 0;
    }
    while (XPending(g_display) > 0)
    {
      XNextEvent(g_display, &xevent);
      switch (xevent.type)
      {
        case SelectionNotify:
          clipboard_event_selection_notify(&xevent);
          break;
        case SelectionRequest:
          clipboard_event_selection_request(&xevent);
          break;
        case SelectionClear:
          clipboard_event_selection_clear(&xevent);
          break;
        case MappingNotify:
          break;
        case PropertyNotify:
          LOG(10, ("clipboard_check_wait_objs: PropertyNotify .window %d "
	      ".state %d", xevent.xproperty.window, xevent.xproperty.state));
          break;
        default:
          if (xevent.type == g_xfixes_event_base +
                             XFixesSetSelectionOwnerNotify)
          {
            clipboard_event_selection_owner_notify(&xevent);
            break;
          }
          LOG(0, ("clipboard_check_wait_objs unknown type %d",
              xevent.type));
          break;
      }
    }
  }
  return 0;
}
