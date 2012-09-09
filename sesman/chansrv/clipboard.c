/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2012
 * Copyright (C) Laxmikant Rashinkar 2012
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
   for help see
   http://tronche.com/gui/x/icccm/sec-2.html#s-2
   .../kde/kdebase/workspace/klipper/clipboardpoll.cpp

  Revision:
  Aug 05, 2012:
    Laxmikant Rashinkar (LK dot Rashinkar at gmail.com)
    added clipboard support for BMP images
*/

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "clipboard.h"
#include "xcommon.h"

static char g_bmp_image_header[] =
{
    /* this is known to work */
    //0x42, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

    /* THIS IS BEING SENT BY WIN2008 */
    0x42, 0x4d, 0x16, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00
};

extern int g_cliprdr_chan_id;   /* in chansrv.c */

extern Display* g_display;      /* in xcommon.c */
extern int g_x_socket;          /* in xcommon.c */
extern tbus g_x_wait_obj;       /* in xcommon.c */
extern Screen* g_screen;        /* in xcommon.c */
extern int g_screen_num;        /* in xcommon.c */

int g_clip_up = 0;
int g_waiting_for_data_response = 0;
int g_waiting_for_data_response_time = 0;

static Atom g_clipboard_atom = 0;
static Atom g_clip_property_atom = 0;
static Atom g_timestamp_atom = 0;
static Atom g_multiple_atom = 0;
static Atom g_targets_atom = 0;
static Atom g_primary_atom = 0;
static Atom g_secondary_atom = 0;
static Atom g_get_time_atom = 0;
static Atom g_utf8_atom = 0;
static Atom g_image_bmp_atom = 0;

static Window g_wnd = 0;
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
static int g_data_in_up_to_date = 0;
static int g_got_format_announce = 0;

/* for image data */
static int g_want_image_data = 0;
static XSelectionRequestEvent g_saved_selection_req_event;

/* for clipboard INCR transfers */
static Atom g_incr_atom;
static Atom g_incr_atom_type;
static Atom g_incr_atom_target;
static char* g_incr_data = 0;
static int g_incr_data_size = 0;
static int g_incr_in_progress = 0;

static int clipboard_format_id = CB_FORMAT_UNICODETEXT;

/*****************************************************************************/
/* this is one way to get the current time from the x server */
static Time APP_CC
clipboard_get_server_time(void)
{
  XEvent xevent;
  unsigned char no_text[4];

  /* append nothing */
  no_text[0] = 0;
  XChangeProperty(g_display, g_wnd, g_get_time_atom, XA_STRING, 8,
                  PropModeAppend, no_text, 0);
  /* wait for PropertyNotify */
  do
  {
    XMaskEvent(g_display, PropertyChangeMask, &xevent);
  } while (xevent.type != PropertyNotify);
  return xevent.xproperty.time;
}

/*****************************************************************************/
/* returns time in miliseconds
   this is like g_time2 in os_calls, but not miliseconds since machine was
   up, something else
   this is a time value similar to what the xserver uses */
static int APP_CC
clipboard_get_local_time(void)
{
  return g_time3();
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

  LOGM((LOG_LEVEL_DEBUG, "xrdp-chansrv: in clipboard_init"));
  if (g_clip_up)
  {
    return 0;
  }
  xcommon_init();
  clipboard_deinit();
  rv = 0;
  if (rv == 0)
  {
    g_clipboard_atom = XInternAtom(g_display, "CLIPBOARD", False);
    if (g_clipboard_atom == None)
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_init: XInternAtom failed"));
      rv = 3;
    }
  }
  if (rv == 0)
  {
    if (!XFixesQueryExtension(g_display, &g_xfixes_event_base, &dummy))
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_init: no xfixes"));
      rv = 5;
    }
  }
  if (rv == 0)
  {
    LOGM((LOG_LEVEL_DEBUG, "clipboard_init: g_xfixes_event_base %d",
          g_xfixes_event_base));
    st = XFixesQueryVersion(g_display, &ver_maj, &ver_min);
    LOGM((LOG_LEVEL_DEBUG, "clipboard_init st %d, maj %d min %d", st,
          ver_maj, ver_min));
    g_clip_property_atom = XInternAtom(g_display, "XRDP_CLIP_PROPERTY_ATOM",
                                       False);
    g_get_time_atom = XInternAtom(g_display, "XRDP_GET_TIME_ATOM",
                                  False);
    g_timestamp_atom = XInternAtom(g_display, "TIMESTAMP", False);
    g_targets_atom = XInternAtom(g_display, "TARGETS", False);
    g_multiple_atom = XInternAtom(g_display, "MULTIPLE", False);
    g_primary_atom = XInternAtom(g_display, "PRIMARY", False);
    g_secondary_atom = XInternAtom(g_display, "SECONDARY", False);
    g_utf8_atom = XInternAtom(g_display, "UTF8_STRING", False);

    g_image_bmp_atom = XInternAtom(g_display, "image/bmp", False);
    g_incr_atom = XInternAtom(g_display, "INCR", False);
    if (g_image_bmp_atom == None)
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_init: g_image_bmp_atom was "
            "not allocated"));
    }

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
    LOGM((LOG_LEVEL_DEBUG, "clipboard_init: data out, sending "
          "CLIPRDR_CONNECT (clip_msg_id = 1)"));
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    if (rv != 0)
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_init: send_channel_data failed "
            "rv = %d", rv));
      rv = 4;
    }
    free_stream(s);
  }
  if (rv == 0)
  {
    g_clip_up = 1;
    make_stream(g_ins);
    init_stream(g_ins, 8192);
  }
  else
  {
    LOGM((LOG_LEVEL_ERROR, "xrdp-chansrv: clipboard_init: error on exit"));
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
clipboard_deinit(void)
{
  if (g_wnd != 0)
  {
    XDestroyWindow(g_display, g_wnd);
    g_wnd = 0;
  }
  g_free(g_last_clip_data);
  g_last_clip_data = 0;
  g_last_clip_size = 0;
  free_stream(g_ins);
  g_ins = 0;
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

  LOGM((LOG_LEVEL_DEBUG, "clipboard_send_data_request:"));
  if (!g_got_format_announce)
  {
    LOGM((LOG_LEVEL_ERROR, "clipboard_send_data_request: error, "
          "no format announce"));
    return 0;
  }
  g_got_format_announce = 0;
  make_stream(s);
  init_stream(s, 8192);
  out_uint16_le(s, 4); /* CLIPRDR_DATA_REQUEST */
  out_uint16_le(s, 0); /* status */
  out_uint32_le(s, 4); /* length */
  out_uint32_le(s, clipboard_format_id);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOGM((LOG_LEVEL_DEBUG,"clipboard_send_data_request: data out, sending "
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
  LOGM((LOG_LEVEL_DEBUG,"clipboard_send_format_ack: data out, sending "
        "CLIPRDR_FORMAT_ACK (clip_msg_id = 3)"));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

static char windows_native_format[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*****************************************************************************/
static int APP_CC
clipboard_send_format_announce(tui32 format_id, char* format_name)
{
  struct stream* s;
  int size;
  int rv;

  make_stream(s);
  init_stream(s, 8192);
  out_uint16_le(s, 2); /* CLIPRDR_FORMAT_ANNOUNCE */
  out_uint16_le(s, 0); /* status */
  out_uint32_le(s, 4 + sizeof(windows_native_format)); /* length */
  out_uint32_le(s, format_id);
  out_uint8p(s, windows_native_format, sizeof(windows_native_format));
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOGM((LOG_LEVEL_DEBUG,"clipboard_send_format_announce: data out, sending "
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
clipboard_send_data_response_for_image(void)
{
  struct stream* s;
  int size;
  int rv;

  LOG(10, ("clipboard_send_data_response_for_image: g_last_clip_size %d\n",
      g_last_clip_size));
  make_stream(s);
  init_stream(s, 64 + g_last_clip_size);
  out_uint16_le(s, 5); /* CLIPRDR_DATA_RESPONSE */
  out_uint16_le(s, 1); /* status */
  out_uint32_le(s, g_last_clip_size); /* length */
  /* insert image data */
  if (g_last_clip_type == g_image_bmp_atom)
  {
    /* do not insert first header */
    out_uint8p(s, g_last_clip_data + 14, g_last_clip_size - 14);
  }
  out_uint16_le(s, 0); /* nil for string */
  out_uint32_le(s, 0);
  out_uint32_le(s, 0);
  out_uint32_le(s, 0);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  /* HANGING HERE WHEN IMAGE DATA IS TOO BIG!!!! */
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
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
    if (g_last_clip_type == g_image_bmp_atom)
    {
      return clipboard_send_data_response_for_image();
    }
    if ((g_last_clip_type == XA_STRING) || (g_last_clip_type == g_utf8_atom))
    {
      num_chars = g_mbstowcs(0, g_last_clip_data, 0);
      if (num_chars < 0)
      {
        LOGM((LOG_LEVEL_ERROR, "clipboard_send_data_response: bad string"));
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
    LOGM((LOG_LEVEL_ERROR,"clipboard_send_data_response: error "
          "clipboard_out_unicode didn't write right number of bytes"));
  }
  out_uint16_le(s, 0); /* nil for string */
  out_uint32_le(s, 0);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  LOGM((LOG_LEVEL_DEBUG,"clipboard_send_data_response: data out, sending "
        "CLIPRDR_DATA_RESPONSE (clip_msg_id = 5) size %d num_chars %d",
        size, num_chars));
  rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_set_selection_owner(void)
{
  Window owner;

  g_selection_time = clipboard_get_server_time();
  XSetSelectionOwner(g_display, g_clipboard_atom, g_wnd, g_selection_time);
  owner = XGetSelectionOwner(g_display, g_clipboard_atom);
  if (owner != g_wnd)
  {
    g_got_selection = 0;
    return 1;
  }
  g_got_selection = 1;
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_provide_selection(XSelectionRequestEvent* req, Atom type, int format,
                            char* data, int length)
{
  XEvent xev;

  XChangeProperty(g_display, req->requestor, req->property,
                  type, format, PropModeReplace, (tui8*)data, length);
  g_memset(&xev, 0, sizeof(xev));
  xev.xselection.type = SelectionNotify;
  xev.xselection.send_event = True;
  xev.xselection.display = req->display;
  xev.xselection.requestor = req->requestor;
  xev.xselection.selection = req->selection;
  xev.xselection.target = req->target;
  xev.xselection.property = req->property;
  xev.xselection.time = req->time;
  XSendEvent(g_display, req->requestor, False, NoEventMask, &xev);
  return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_refuse_selection(XSelectionRequestEvent* req)
{
  XEvent xev;

  g_memset(&xev, 0, sizeof(xev));
  xev.xselection.type = SelectionNotify;
  xev.xselection.send_event = True;
  xev.xselection.display = req->display;
  xev.xselection.requestor = req->requestor;
  xev.xselection.selection = req->selection;
  xev.xselection.target = req->target;
  xev.xselection.property = None;
  xev.xselection.time = req->time;
  XSendEvent(g_display, req->requestor, False, NoEventMask, &xev);
  return 0;
}

/*****************************************************************************/
/* sent by client or server when its local system clipboard is
   updated with new clipboard data; contains Clipboard Format ID
   and name pairs of new Clipboard Formats on the clipboard. */
static int APP_CC
clipboard_process_format_announce(struct stream* s, int clip_msg_status,
                                  int clip_msg_len)
{
  LOGM((LOG_LEVEL_DEBUG, "clipboard_process_format_announce: "
        "CLIPRDR_FORMAT_ANNOUNCE"));
  //g_hexdump(s->p, s->end - s->p);
  clipboard_send_format_ack();
  g_got_format_announce = 1;
  g_data_in_up_to_date = 0;
  if (clipboard_set_selection_owner() != 0)
  {
    LOGM((LOG_LEVEL_ERROR, "clipboard_process_format_announce: "
          "XSetSelectionOwner failed"));
  }
  return 0;
}

/*****************************************************************************/
/* response to CB_FORMAT_LIST; used to indicate whether
   processing of the Format List PDU was successful */
static int APP_CC
clipboard_prcoess_format_ack(struct stream* s, int clip_msg_status,
                             int clip_msg_len)
{
  LOGM((LOG_LEVEL_DEBUG,"clipboard_prcoess_format_ack: CLIPRDR_FORMAT_ACK"));
  //g_hexdump(s->p, s->end - s->p);
  return 0;
}

/*****************************************************************************/
/* sent by recipient of CB_FORMAT_LIST; used to request data for one
   of the formats that was listed in CB_FORMAT_LIST */
static int APP_CC
clipboard_process_data_request(struct stream* s, int clip_msg_status,
                               int clip_msg_len)
{
  LOGM((LOG_LEVEL_DEBUG,"clipboard_process_data_request: "
        "CLIPRDR_DATA_REQUEST"));
  //g_hexdump(s->p, s->end - s->p);
  clipboard_send_data_response();
  return 0;
}

/*****************************************************************************/
/* sent as a reply to CB_FORMAT_DATA_REQUEST; used to indicate whether
   processing of the CB_FORMAT_DATA_REQUEST was successful; if processing
   was successful, CB_FORMAT_DATA_RESPONSE includes contents of requested
   clipboard data. */
static int APP_CC
clipboard_process_data_response_for_image(struct stream * s,
                                          int clip_msg_status,
                                          int clip_msg_len)
{
  XSelectionRequestEvent* lxev = &g_saved_selection_req_event;
  char* cptr;
  char cdata;
  int len;
  int index;

  LOGM((LOG_LEVEL_DEBUG, "clipboard_process_data_response_for_image: "
        "CLIPRDR_DATA_RESPONSE_FOR_IMAGE"));
  g_waiting_for_data_response = 0;
  len = (int)(s->end - s->p);
  if (len < 1)
  {
    return 0;
  }
  if (g_last_clip_type == g_image_bmp_atom)
  {
    /* space for inserting bmp image header */
    len += 14;
    cptr = (char*)g_malloc(len, 0);
    if (cptr == 0)
    {
      return 0;
    }
    g_memcpy(cptr, g_bmp_image_header, 14);
    index = 14;
  }
  else
  {
    return 0;
  }
  while (s_check(s))
  {
    in_uint8(s, cdata);
    cptr[index++] = cdata;
  }
  if (len >= 0)
  {
    g_data_in = cptr;
    g_data_in_size = len;
    g_data_in_time = clipboard_get_local_time();
    g_data_in_up_to_date = 1;
  }
  clipboard_provide_selection(lxev, lxev->target, 8, cptr, len);
  return 0;
}

/*****************************************************************************/
/* sent as a reply to CB_FORMAT_DATA_REQUEST; used to indicate whether
   processing of the CB_FORMAT_DATA_REQUEST was successful; if processing was
   successful, CB_FORMAT_DATA_RESPONSE includes contents of requested
   clipboard data. */
/*****************************************************************************/
static int APP_CC
clipboard_process_data_response(struct stream* s, int clip_msg_status,
                                int clip_msg_len)
{
  XSelectionRequestEvent* lxev;
  twchar* wtext;
  twchar wchr;
  int len;
  int index;
  int data_in_len;

  if (g_want_image_data)
  {
    g_want_image_data = 0;
    clipboard_process_data_response_for_image(s, clip_msg_status, clip_msg_len);
    return 0;
  }
  LOGM((LOG_LEVEL_DEBUG,"clipboard_process_data_response: "
        "CLIPRDR_DATA_RESPONSE"));
  g_waiting_for_data_response = 0;
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
    g_data_in_time = xcommon_get_local_time();
    g_data_in_up_to_date = 1;
  }
  if (g_data_in != 0)
  {
    data_in_len = g_strlen(g_data_in);
    for (index = 0; index < g_selection_request_event_count; index++)
    {
      lxev = &(g_selection_request_event[index]);
      clipboard_provide_selection(lxev, lxev->target, 8, g_data_in,
                                  data_in_len);
      LOGM((LOG_LEVEL_DEBUG,"clipboard_process_data_response: requestor %d "
            "data_in_len %d", lxev->requestor, data_in_len));
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
      init_stream(g_ins, total_length);
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
    /* sent by client or server when its local system clipboard is   */
    /* updated with new clipboard data; contains Clipboard Format ID */
    /* and name pairs of new Clipboard Formats on the clipboard.     */
    case CB_FORMAT_LIST: /* CLIPRDR_FORMAT_ANNOUNCE */
      rv = clipboard_process_format_announce(ls, clip_msg_status,
                                             clip_msg_len);
      break;
    /* response to CB_FORMAT_LIST; used to indicate whether */
    /* processing of the Format List PDU was successful     */
    case CB_FORMAT_LIST_RESPONSE: /* CLIPRDR_FORMAT_ACK */
      rv = clipboard_prcoess_format_ack(ls, clip_msg_status,
                                        clip_msg_len);
      break;
    /* sent by recipient of CB_FORMAT_LIST; used to request data for one */
    /* of the formats that was listed in CB_FORMAT_LIST                  */
    case CB_FORMAT_DATA_REQUEST: /* CLIPRDR_DATA_REQUEST */
      rv = clipboard_process_data_request(ls, clip_msg_status,
                                          clip_msg_len);
      break;
    /* sent as a reply to CB_FORMAT_DATA_REQUEST; used to indicate */
    /* whether processing of the CB_FORMAT_DATA_REQUEST was        */
    /* successful; if processing was successful,                   */
    /* CB_FORMAT_DATA_RESPONSE includes contents of requested      */
    /* clipboard data.                                             */
    case CB_FORMAT_DATA_RESPONSE: /* CLIPRDR_DATA_RESPONSE */
      rv = clipboard_process_data_response(ls, clip_msg_status,
                                           clip_msg_len);
      break;
    default:
      LOGM((LOG_LEVEL_ERROR, "clipboard_data_in: unknown clip_msg_id %d",
            clip_msg_id));
      break;
  }
  XFlush(g_display);
  return rv;
}

/*****************************************************************************/
/* this happens when a new app copies something to the clipboard
   'CLIPBOARD' Atom
   typedef struct
   {
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
  LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_owner_notify: "
        "window %d subtype %d owner %d g_wnd %d",
        lxevent->window, lxevent->subtype, lxevent->owner, g_wnd));
  if (lxevent->owner == g_wnd)
  {
    LOGM((LOG_LEVEL_DEBUG,"clipboard_event_selection_owner_notify: skipping, "
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
  if (lxdata != 0)
  {
    XFree(lxdata);
  }
  if (ltype == 0)
  {
    /* XGetWindowProperty failed */
    return 1;
  }

  if (ltype == g_incr_atom)
  {
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_property_notify: INCR start"));
    g_incr_in_progress = 1;
    g_incr_atom_type = prop;
    g_incr_data_size = 0;
    g_free(g_incr_data);
    g_incr_data = 0;
    XDeleteProperty(g_display, g_wnd, prop);
    return 0;
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
    if (lxdata != 0)
    {
      XFree(lxdata);
    }
    return 1;
  }
  lxdata_size = (lfmt / 8) * ln_items;
  if (lxdata_size < 1)
  {
    /* should not happen */
    if (lxdata != 0)
    {
      XFree(lxdata);
    }
    return 2;
  }
  if (llen_after > 0)
  {
    /* should not happen */
    if (lxdata != 0)
    {
      XFree(lxdata);
    }
    return 3;
  }
  if (xdata != 0)
  {
    *xdata = (char*)g_malloc(lxdata_size, 0);
    g_memcpy(*xdata, lxdata, lxdata_size);
  }
  if (lxdata != 0)
  {
    XFree(lxdata);
  }
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
  int convert_to_utf8;
  int convert_to_bmp_image;
  int send_format_announce;
  int atom;
  Atom* atoms;
  Atom type;
  tui32 format_id;
  char format_name[32];

  LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_notify:"));
  data_size = 0;
  n_items = 0;
  fmt = 0;
  convert_to_string = 0;
  convert_to_utf8 = 0;
  convert_to_bmp_image = 0;
  send_format_announce = 0;
  format_id = 0;
  rv = 0;
  data = 0;
  type = 0;
  lxevent = (XSelectionEvent*)xevent;
  g_memset(format_name, 0, 32);
  if (lxevent->property == None)
  {
    LOGM((LOG_LEVEL_ERROR, "clipboard_event_selection_notify: clip could "
         "not be converted"));
    rv = 1;
  }
  if (rv == 0)
  {
    /* we need this if the call below turns out to be a
       clipboard INCR operation */
    if (!g_incr_in_progress)
    {
      g_incr_atom_target = lxevent->target;
    }

    rv = clipboard_get_window_property(lxevent->requestor, lxevent->property,
                                       &type, &fmt,
                                       &n_items, &data, &data_size);
    if (rv != 0)
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_event_selection_notify: "
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
        /* on a 64 bit machine, actual_format_return of 32 implies long */
        if ((type == XA_ATOM) && (fmt == 32))
        {
          atoms = (Atom*)data;
          for (index = 0; index < n_items; index++)
          {
            atom = atoms[index];
            LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_notify: %d %s %d",
                  atom, XGetAtomName(g_display, atom), XA_STRING));
            if (atom == g_utf8_atom)
            {
              convert_to_utf8 = 1;
            }
            else if (atom == XA_STRING)
            {
              convert_to_string = 1;
            }
            else if (atom == g_image_bmp_atom)
            {
              convert_to_bmp_image = 1;
            }
          }
        }
        else
        {
          LOGM((LOG_LEVEL_ERROR, "clipboard_event_selection_notify: error, "
                "target is 'TARGETS' and type[%d] or fmt[%d] not right, "
                "should be type[%d], fmt[%d]", type, fmt, XA_ATOM, 32));
        }
      }
      else if (lxevent->target == g_utf8_atom)
      {
        LOGM((LOG_LEVEL_DEBUG,"clipboard_event_selection_notify: UTF8_STRING "
              "data_size %d", data_size));
        g_free(g_last_clip_data);
        g_last_clip_data = 0;
        g_last_clip_size = data_size;
        g_last_clip_data = g_malloc(g_last_clip_size + 1, 0);
        g_last_clip_type = g_utf8_atom;
        g_memcpy(g_last_clip_data, data, g_last_clip_size);
        g_last_clip_data[g_last_clip_size] = 0;
        send_format_announce = 1;
        format_id = CB_FORMAT_UNICODETEXT;
      }
      else if (lxevent->target == XA_STRING)
      {
        LOGM((LOG_LEVEL_DEBUG,"clipboard_event_selection_notify: XA_STRING "
              "data_size %d", data_size));
        g_free(g_last_clip_data);
        g_last_clip_data = 0;
        g_last_clip_size = data_size;
        g_last_clip_data = g_malloc(g_last_clip_size + 1, 0);
        g_last_clip_type = XA_STRING;
        g_memcpy(g_last_clip_data, data, g_last_clip_size);
        g_last_clip_data[g_last_clip_size] = 0;
        send_format_announce = 1;
        format_id = CB_FORMAT_UNICODETEXT;
      }
      else if (lxevent->target == g_image_bmp_atom)
      {
        LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_notify: image/bmp "
              "data_size %d", data_size));
        g_free(g_last_clip_data);
        g_last_clip_data = 0;
        g_last_clip_size = data_size;
        g_last_clip_data = g_malloc(data_size, 0);
        g_last_clip_type = g_image_bmp_atom;
        g_memcpy(g_last_clip_data, data, data_size);
        send_format_announce = 1;
        format_id = CB_FORMAT_DIB;
        g_strcpy(format_name, "image/bmp");
      }
      else
      {
        LOGM((LOG_LEVEL_ERROR, "clipboard_event_selection_notify: "
              "unknown target"));
      }
    }
    else
    {
      LOGM((LOG_LEVEL_ERROR,"clipboard_event_selection_notify: "
            "unknown selection"));
    }
  }
  if (convert_to_utf8)
  {
    XConvertSelection(g_display, g_clipboard_atom, g_utf8_atom,
                      g_clip_property_atom, g_wnd, lxevent->time);
  }
  else if (convert_to_string)
  {
    XConvertSelection(g_display, g_clipboard_atom, XA_STRING,
                      g_clip_property_atom, g_wnd, lxevent->time);
  }
  else if (convert_to_bmp_image)
  {
    XConvertSelection(g_display, g_clipboard_atom, g_image_bmp_atom,
                      g_clip_property_atom, g_wnd, lxevent->time);
  }
  if (send_format_announce)
  {
    if (clipboard_send_format_announce(format_id, format_name) != 0)
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
/*
 * When XGetWindowProperty and XChangeProperty talk about "format 32" it
 * doesn't mean a 32bit value, but actually a long. So 32 means 4 bytes on
 * a 32bit machine and 8 bytes on a 64 machine
 */
static int APP_CC
clipboard_event_selection_request(XEvent* xevent)
{
  XSelectionRequestEvent* lxev;
  XEvent xev;
  Atom atom_buf[10];
  Atom type;
  int fmt;
  int n_items;
  int xdata_size;
  char* xdata;

  lxev = (XSelectionRequestEvent*)xevent;
  LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: g_wnd %d, "
        ".requestor %d .owner %d .selection %d '%s' .target %d .property %d",
        g_wnd, lxev->requestor, lxev->owner, lxev->selection,
        XGetAtomName(g_display, lxev->selection),
        lxev->target, lxev->property));
  if (lxev->property == None)
  {
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: "
          "lxev->property is None"));
  }
  else if (lxev->target == g_targets_atom)
  {
    /* requestor is asking what the selection can be converted to */
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: "
          "g_targets_atom"));
    atom_buf[0] = g_targets_atom;
    atom_buf[1] = g_timestamp_atom;
    atom_buf[2] = g_multiple_atom;
    atom_buf[3] = XA_STRING;
    atom_buf[4] = g_utf8_atom;
    atom_buf[5] = g_image_bmp_atom;
    return clipboard_provide_selection(lxev, XA_ATOM, 32, (char*)atom_buf, 6);
  }
  else if (lxev->target == g_timestamp_atom)
  {
    /* requestor is asking the time I got the selection */
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: "
          "g_timestamp_atom"));
    atom_buf[0] = g_selection_time;
    return clipboard_provide_selection(lxev, XA_INTEGER, 32, (char*)atom_buf, 1);
  }
  else if (lxev->target == g_multiple_atom)
  {
    /* target, property pairs */
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: "
          "g_multiple_atom"));
    if (clipboard_get_window_property(xev.xselection.requestor,
                                      xev.xselection.property,
                                      &type, &fmt, &n_items, &xdata,
                                      &xdata_size) == 0)
    {
      LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: g_multiple_atom "
            "n_items %d", n_items));
      /* todo */
      g_free(xdata);
    }
  }
  else if ((lxev->target == XA_STRING) || (lxev->target == g_utf8_atom))
  {
    LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_request: %s",
          XGetAtomName(g_display, lxev->target)));
    clipboard_format_id = CB_FORMAT_UNICODETEXT;
    if (g_data_in_up_to_date)
    {
      return clipboard_provide_selection(lxev, lxev->target, 8,
                                         g_data_in, g_strlen(g_data_in));
    }
    if (g_selection_request_event_count > 10)
    {
      LOGM((LOG_LEVEL_ERROR, "clipboard_event_selection_request: error, "
            "too many requests"));
    }
    else
    {
      g_memcpy(&(g_selection_request_event[g_selection_request_event_count]),
               lxev, sizeof(g_selection_request_event[0]));
      if (g_selection_request_event_count == 0)
      {
        clipboard_send_data_request();
        g_waiting_for_data_response = 1;
        g_waiting_for_data_response_time = xcommon_get_local_time();
      }
      g_selection_request_event_count++;
      return 0;
    }
  }
  else if (lxev->target == g_image_bmp_atom)
  {
    g_memcpy(&g_saved_selection_req_event, lxev,
             sizeof(g_saved_selection_req_event));
    g_last_clip_type = g_image_bmp_atom;
    g_want_image_data = 1;
    clipboard_format_id = CB_FORMAT_DIB;
    clipboard_send_data_request();
    g_waiting_for_data_response = 1;
    g_waiting_for_data_response_time = clipboard_get_local_time();
    return 0;
  }
  else
  {
    LOGM((LOG_LEVEL_ERROR,"clipboard_event_selection_request: unknown "
          "target %s", XGetAtomName(g_display, lxev->target)));
  }
  clipboard_refuse_selection(lxev);
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
  LOGM((LOG_LEVEL_DEBUG, "clipboard_event_selection_clear:"));
  return 0;
}

/*****************************************************************************/
/* returns error
   typedef struct {
     int type;                // PropertyNotify
     unsigned long serial;    // # of last request processed by server
     Bool send_event;         // true if this came from a SendEvent request
     Display *display;        // Display the event was read from
     Window window;
     Atom atom;
     Time time;
     int state;               // PropertyNewValue or PropertyDelete
} XPropertyEvent; */
static int APP_CC
clipboard_event_property_notify(XEvent* xevent)
{
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_returned;
  unsigned long bytes_left;
  unsigned char* data;
  int rv;
  int format_in_bytes;
  int new_data_len;
  char* cptr;
  char format_name[32];

  LOG(10, ("clipboard_check_wait_objs: PropertyNotify .window %d "
      ".state %d .atom %d", xevent->xproperty.window,
      xevent->xproperty.state, xevent->xproperty.atom));
  if (g_incr_in_progress &&
      (xevent->xproperty.atom == g_incr_atom_type) &&
      (xevent->xproperty.state == PropertyNewValue))
  {
    rv = XGetWindowProperty(g_display, g_wnd, g_incr_atom_type, 0, 0, 0,
                            AnyPropertyType, &actual_type_return, &actual_format_return,
                            &nitems_returned, &bytes_left, (unsigned char **) &data);
    if (data != 0)
    {
      XFree(data);
      data = 0;
    }
    if (bytes_left <= 0)
    {
      LOGM((LOG_LEVEL_DEBUG, "clipboard_event_property_notify: INCR done"));
      g_memset(format_name, 0, 32);
      /* clipboard INCR cycle has completed */
      g_incr_in_progress = 0;
      g_last_clip_size = g_incr_data_size;
      g_last_clip_data = g_incr_data;
      g_incr_data = 0;
      g_last_clip_type = g_incr_atom_target;
      if (g_incr_atom_target == g_image_bmp_atom)
      {
        g_snprintf(format_name, 31, "image/bmp");
        clipboard_send_format_announce(CB_FORMAT_DIB, format_name);
      }
      XDeleteProperty(g_display, g_wnd, g_incr_atom_type);
    }
    else
    {
      rv = XGetWindowProperty(g_display, g_wnd, g_incr_atom_type, 0, bytes_left, 0,
                              AnyPropertyType, &actual_type_return, &actual_format_return,
                              &nitems_returned, &bytes_left, (unsigned char **) &data);

      format_in_bytes = actual_format_return / 8;
      if ((actual_format_return == 32) && (sizeof(long) == 8))
      {
        /* on a 64 bit machine, actual_format_return of 32 implies long */
        format_in_bytes = 8;
      }
      new_data_len = nitems_returned * format_in_bytes;
      cptr = (char*)g_malloc(g_incr_data_size + new_data_len, 0);
      g_memcpy(cptr, g_incr_data, g_incr_data_size);
      g_free(g_incr_data);
      if (cptr == NULL)
      {
        g_incr_data = 0;
        /* cannot add any more data */
        if (data != 0)
        {
          XFree(data);
        }
        XDeleteProperty(g_display, g_wnd, g_incr_atom_type);
        return 0;
      }
      g_incr_data = cptr;
      g_memcpy(g_incr_data + g_incr_data_size, data, new_data_len);
      g_incr_data_size += new_data_len;
      if (data)
      {
        XFree(data);
      }
      XDeleteProperty(g_display, g_wnd, g_incr_atom_type);
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns 0, event handled, 1 unhandled */
int APP_CC
clipboard_xevent(void* xevent)
{
  XEvent* lxevent;

  if (!g_clip_up)
  {
    return 1;
  }
  lxevent = (XEvent*)xevent;
  switch (lxevent->type)
  {
    case SelectionNotify:
      clipboard_event_selection_notify(lxevent);
      break;
    case SelectionRequest:
      clipboard_event_selection_request(lxevent);
      break;
    case SelectionClear:
      clipboard_event_selection_clear(lxevent);
      break;
    case MappingNotify:
      break;
    case PropertyNotify:
      clipboard_event_property_notify(lxevent);
      break;
    case UnmapNotify:
      LOG(0, ("chansrv::clipboard_xevent: got UnmapNotify"));
      break;
    case ClientMessage:
      LOG(0, ("chansrv::clipboard_xevent: got ClientMessage"));
      break;
    default:
      if (lxevent->type == g_xfixes_event_base +
                           XFixesSetSelectionOwnerNotify)
      {
        clipboard_event_selection_owner_notify(lxevent);
        break;
      }
      /* we didn't handle this message */
      return 1;
  }
  return 0;
}
