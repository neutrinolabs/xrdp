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
*/

#include "arch.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "trans.h"
#include "chansrv.h"
#include "defines.h"
#include "sound.h"

static tbus g_thread_done_event = 0;
static struct trans* g_lis_trans = 0;
static struct trans* g_con_trans = 0;
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_clip_index = -1;
static int g_sound_index = -1;

tbus g_term_event = 0;
int g_display = 0;
int g_clip_chan_id = -1;
int g_sound_chan_id = -1;

/*****************************************************************************/
/* returns error */
int APP_CC
send_channel_data(int chan_id, char* data, int size)
{
  struct stream* s;
  int chan_flags;
  int total_size;
  int sent;
  int rv;

  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  rv = 0;
  sent = 0;
  total_size = size;
  while (sent < total_size)
  {
    size = MIN(1600, total_size - sent);
    chan_flags = 0;
    if (sent == 0)
    {
      chan_flags |= 1; /* first */
    }
    if (size + sent == total_size)
    {
      chan_flags |= 2; /* last */
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint32_le(s, 8); /* msg id */
    out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint16_le(s, chan_id);
    out_uint16_le(s, chan_flags);
    out_uint16_le(s, size);
    out_uint32_le(s, total_size);
    out_uint8a(s, data + sent, size);
    s_mark_end(s);
    rv = trans_force_write(g_con_trans);
    if (rv != 0)
    {
      break;
    }
    sent += size;
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_init_response_message(void)
{
  struct stream* s;

  g_writeln("xrdp-chansrv: in send_init_response_message");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 2); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_setup_response_message(void)
{
  struct stream* s;

  g_writeln("xrdp-chansrv: in send_channel_setup_response_message");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 4); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_data_response_message(void)
{
  struct stream* s;

  g_writeln("xrdp-chansrv: in send_channel_data_response_message");
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 6); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_init(struct stream* s)
{
  g_writeln("xrdp-chansrv: in process_message_init");
  return send_init_response_message();
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_setup(struct stream* s)
{
  int num_chans;
  int index;
  int rv;
  struct chan_item* ci;

  g_writeln("xrdp-chansrv: in process_message_channel_setup");
  in_uint16_le(s, num_chans);
  for (index = 0; index < num_chans; index++)
  {
    ci = &(g_chan_items[g_num_chan_items]);
    g_memset(ci->name, 0, sizeof(ci->name));
    in_uint8a(s, ci->name, 8);
    in_uint16_le(s, ci->id);
    in_uint16_le(s, ci->flags);
    g_writeln("xrdp-chansrv: chan name %s id %d flags %d", ci->name, ci->id,
              ci->flags);
    if (g_strcasecmp(ci->name, "cliprdr") == 0)
    {
      g_clip_index = g_num_chan_items;
      g_clip_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpsnd") == 0)
    {
      g_sound_index = g_num_chan_items;
      g_sound_chan_id = ci->id;
    }
    g_num_chan_items++;
  }
  rv = send_channel_setup_response_message();
  if (g_clip_index >= 0)
  {
    clipboard_init();
  }
  if (g_sound_index >= 0)
  {
    sound_init();
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data(struct stream* s)
{
  int chan_id;
  int chan_flags;
  int rv;
  int length;
  int total_length;

  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, length);
  in_uint32_le(s, total_length);
  g_writeln("xrdp-chansrv: log channel data chan_id %d chan_flags %d",
            chan_id, chan_flags);
  rv = send_channel_data_response_message();
  if (rv == 0)
  {
    if (chan_id == g_clip_chan_id)
    {
      rv = clipboard_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_sound_chan_id)
    {
      rv = sound_data_in(s, chan_id, chan_flags, length, total_length);
    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data_response(struct stream* s)
{
  g_writeln("xrdp-chansrv: in process_message_channel_data_response");
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message(void)
{
  struct stream* s;
  int size;
  int id;
  int rv;
  char* next_msg;

  if (g_con_trans == 0)
  {
    return 1;
  }
  s = trans_get_in_s(g_con_trans);
  if (s == 0)
  {
    return 1;
  }
  rv = 0;
  while (s_check_rem(s, 8))
  {
    next_msg = s->p;
    in_uint32_le(s, id);
    in_uint32_le(s, size);
    next_msg += size;
    switch (id)
    {
      case 1: /* init */
        rv = process_message_init(s);
        break;
      case 3: /* channel setup */
        rv = process_message_channel_setup(s);
        break;
      case 5: /* channel data */
        rv = process_message_channel_data(s);
        break;
      case 7: /* channel data response */
        rv = process_message_channel_data_response(s);
        break;
      default:
        g_writeln("xrdp-chansrv: error in process_message unknown msg %d",
                  id);
        break;
    }
    if (rv != 0)
    {
      break;
    }
    s->p = next_msg;
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
my_trans_data_in(struct trans* trans)
{
  struct stream* s;
  int id;
  int size;
  int error;

  if (trans == 0)
  {
    return 0;
  }
  if (trans != g_con_trans)
  {
    return 1;
  }
  g_writeln("xrdp-chansrv: my_trans_data_in");
  s = trans_get_in_s(trans);
  in_uint32_le(s, id);
  in_uint32_le(s, size);
  error = trans_force_read(trans, size - 8);
  if (error == 0)
  {
    /* here, the entire message block is read in, process it */
    error = process_message();
  }
  return error;
}

/*****************************************************************************/
int DEFAULT_CC
my_trans_conn_in(struct trans* trans, struct trans* new_trans)
{
  if (trans == 0)
  {
    return 1;
  }
  if (trans != g_lis_trans)
  {
    return 1;
  }
  if (g_con_trans != 0) /* if already set, error */
  {
    return 1;
  }
  if (new_trans == 0)
  {
    return 1;
  }
  g_writeln("xrdp-chansrv: my_trans_conn_in");
  g_con_trans = new_trans;
  g_con_trans->trans_data_in = my_trans_data_in;
  g_con_trans->header_size = 8;
  /* stop listening */
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  return 0;
}

/*****************************************************************************/
THREAD_RV THREAD_CC
channel_thread_loop(void* in_val)
{
  tbus objs[32];
  int num_objs;
  int timeout;
  int error;
  char text[256];
  THREAD_RV rv;

  g_writeln("xrdp-chansrv: thread start");
  rv = 0;
  g_lis_trans = trans_create(1, 8192, 8192);
  g_lis_trans->trans_conn_in = my_trans_conn_in;
  g_snprintf(text, 255, "%d", 7200 + g_display);
  error = trans_listen(g_lis_trans, text);
  if (error != 0)
  {
    g_writeln("xrdp-chansrv: trans_listen failed");
  }
  if (error == 0)
  {
    timeout = 0;
    num_objs = 0;
    objs[num_objs] = g_term_event;
    num_objs++;
    trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
    while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0)
    {
      if (g_is_wait_obj_set(g_term_event))
      {
        g_writeln("xrdp-chansrv: g_term_event set");
        break;
      }
      if (g_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_lis_trans) != 0)
        {
          g_writeln("xrdp-chansrv: trans_check_wait_objs error");
        }
      }
      if (g_con_trans != 0)
      {
        if (trans_check_wait_objs(g_con_trans) != 0)
        {
          g_writeln("xrdp-chansrv: trans_check_wait_objs error resetting");
          /* delete g_con_trans */
          trans_delete(g_con_trans);
          g_con_trans = 0;
          /* create new listener */
          g_lis_trans = trans_create(1, 8192, 8192);
          g_lis_trans->trans_conn_in = my_trans_conn_in;
          g_snprintf(text, 255, "%d", 7200 + g_display);
          error = trans_listen(g_lis_trans, text);
          if (error != 0)
          {
            g_writeln("xrdp-chansrv: trans_listen failed");
            break;
          }
        }
      }
      clipboard_check_wait_objs();
      sound_check_wait_objs();
      timeout = 0;
      num_objs = 0;
      objs[num_objs] = g_term_event;
      num_objs++;
      trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
      trans_get_wait_objs(g_con_trans, objs, &num_objs, &timeout);
      clipboard_get_wait_objs(objs, &num_objs, &timeout);
      sound_get_wait_objs(objs, &num_objs, &timeout);
    }
  }
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  trans_delete(g_con_trans);
  g_con_trans = 0;
  g_writeln("xrdp-chansrv: thread stop");
  g_set_wait_obj(g_thread_done_event);
  return rv;
}

/*****************************************************************************/
void DEFAULT_CC
term_signal_handler(int sig)
{
  g_writeln("xrdp-chansrv: term_signal_handler: got signal %d", sig);
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void DEFAULT_CC
nil_signal_handler(int sig)
{
  g_writeln("xrdp-chansrv: nil_signal_handler: got signal %d", sig);
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int pid;
  char text[256];
  char* display_text;

  pid = g_getpid();
  g_writeln("xrdp-chansrv: started pid %d(0x%8.8x)", pid, pid);
  g_signal_kill(term_signal_handler); /* SIGKILL */
  g_signal_terminate(term_signal_handler); /* SIGTERM */
  g_signal_user_interrupt(term_signal_handler); /* SIGINT */
  g_signal_pipe(nil_signal_handler); /* SIGPIPE */
  display_text = g_getenv("XRDP_SESSVC_DISPLAY");
  if (display_text == 0)
  {
    g_writeln("xrdp-chansrv: error, XRDP_SESSVC_DISPLAY not set");
    return 1;
  }
  else
  {
    g_display = g_atoi(display_text);
  }
  if (g_display == 0)
  {
    g_writeln("xrdp-chansrv: error, display is zero");
    return 1;
  }
  g_snprintf(text, 255, "xrdp_chansrv_term_%8.8x", pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_thread_done_%8.8x", pid);
  g_thread_done_event = g_create_wait_obj(text);
  tc_thread_create(channel_thread_loop, 0);
  if (g_obj_wait(&g_term_event, 1, 0, 0, 0) != 0)
  {
    g_writeln("xrdp-chansrv: error, g_obj_wait failed");
  }
  /* wait for thread to exit */
  if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
  {
    g_writeln("xrdp-chansrv: error, g_obj_wait failed");
  }
  /* cleanup */
  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_thread_done_event);
  return 0;
}
