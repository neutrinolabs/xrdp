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
   Copyright (C) Jay Sorg 2009-2010
*/

#include "arch.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "trans.h"
#include "chansrv.h"
#include "defines.h"
#include "sound.h"
#include "clipboard.h"
#include "devredir.h"
#include "list.h"
#include "file.h"
#include "file_loc.h"
#include "log.h"

static struct trans* g_lis_trans = 0;
static struct trans* g_con_trans = 0;
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_cliprdr_index = -1;
static int g_rdpsnd_index = -1;
static int g_rdpdr_index = -1;

static tbus g_term_event = 0;
static tbus g_thread_done_event = 0;

static int g_use_unix_socket = 0;

int g_display_num = 0;
int g_cliprdr_chan_id = -1; /* cliprdr */
int g_rdpsnd_chan_id = -1; /* rdpsnd */
int g_rdpdr_chan_id = -1; /* rdpdr */

/*****************************************************************************/
/* returns error */
int APP_CC
send_channel_data(int chan_id, char* data, int size)
{
  struct stream * s = (struct stream *)NULL;
  int chan_flags = 0;
  int total_size = 0;
  int sent = 0;
  int rv = 0;

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
    s = trans_get_out_s(g_con_trans, 8192);
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_init_response_message(void)
{
  struct stream * s = (struct stream *)NULL;

  log_message(LOG_LEVEL_INFO,"send_init_response_message:");
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
  struct stream * s = (struct stream *)NULL;

  log_message(LOG_LEVEL_DEBUG,  "send_channel_setup_response_message:");
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
  struct stream * s = (struct stream *)NULL;

  log_message(LOG_LEVEL_DEBUG, "send_channel_data_response_message:");
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
  log_message(LOG_LEVEL_DEBUG,"process_message_init:");
  return send_init_response_message();
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_setup(struct stream* s)
{
  int num_chans = 0;
  int index = 0;
  int rv = 0;
  struct chan_item* ci = (struct chan_item *)NULL;

  g_num_chan_items = 0;
  g_cliprdr_index = -1;
  g_rdpsnd_index = -1;
  g_rdpdr_index = -1;
  g_cliprdr_chan_id = -1;
  g_rdpsnd_chan_id = -1;
  g_rdpdr_chan_id = -1;
  log_message(LOG_LEVEL_DEBUG, "process_message_channel_setup:");
  in_uint16_le(s, num_chans);
  log_message(LOG_LEVEL_DEBUG,"process_message_channel_setup: num_chans %d", num_chans);
  for (index = 0; index < num_chans; index++)
  {
    ci = &(g_chan_items[g_num_chan_items]);
    g_memset(ci->name, 0, sizeof(ci->name));
    in_uint8a(s, ci->name, 8);
    in_uint16_le(s, ci->id);
    in_uint16_le(s, ci->flags);
    log_message(LOG_LEVEL_DEBUG, "process_message_channel_setup: chan name '%s' "
             "id %d flags %8.8x", ci->name, ci->id, ci->flags);
    if (g_strcasecmp(ci->name, "cliprdr") == 0)
    {
      g_cliprdr_index = g_num_chan_items;
      g_cliprdr_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpsnd") == 0)
    {
      g_rdpsnd_index = g_num_chan_items;
      g_rdpsnd_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpdr") == 0)
    {
      g_rdpdr_index = g_num_chan_items;
      g_rdpdr_chan_id = ci->id;
    }
    g_num_chan_items++;
  }
  rv = send_channel_setup_response_message();
  if (g_cliprdr_index >= 0)
  {
    clipboard_init();
  }
  if (g_rdpsnd_index >= 0)
  {
    sound_init();
  }
  if (g_rdpdr_index >= 0)
  {
    dev_redir_init();
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data(struct stream* s)
{
  int chan_id = 0;
  int chan_flags = 0;
  int rv = 0;
  int length = 0;
  int total_length = 0;

  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, length);
  in_uint32_le(s, total_length);
  log_message(LOG_LEVEL_DEBUG,"process_message_channel_data: chan_id %d "
           "chan_flags %d", chan_id, chan_flags);
  rv = send_channel_data_response_message();
  if (rv == 0)
  {
    if (chan_id == g_cliprdr_chan_id)
    {
      rv = clipboard_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpsnd_chan_id)
    {
      rv = sound_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpdr_chan_id)
    {
      rv = dev_redir_data_in(s, chan_id, chan_flags, length, total_length);
    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data_response(struct stream* s)
{
  LOG(10, ("process_message_channel_data_response:"));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message(void)
{
  struct stream * s = (struct stream *)NULL;
  int size = 0;
  int id = 0;
  int rv = 0;
  char* next_msg = (char *)NULL;

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
        log_message(LOG_LEVEL_ERROR, "process_message: error in process_message ",
                "unknown msg %d", id);
        break;
    }
    if (rv != 0)
    {
      break;
    }
    else {
      s->p = next_msg;
	}
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
my_trans_data_in(struct trans* trans)
{
  struct stream * s = (struct stream *)NULL;
  int id = 0;
  int size = 0;
  int error = 0;

  if (trans == 0)
  {
    return 0;
  }
  if (trans != g_con_trans)
  {
    return 1;
  }
  log_message(LOG_LEVEL_DEBUG,"my_trans_data_in:");
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
  log_message(LOG_LEVEL_DEBUG, "my_trans_conn_in:");
  g_con_trans = new_trans;
  g_con_trans->trans_data_in = my_trans_data_in;
  g_con_trans->header_size = 8;
  /* stop listening */
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  return 0;
}

/*****************************************************************************/
static int APP_CC
setup_listen(void)
{
  char port[256];
  int error = 0;

  if (g_lis_trans != 0)
  {
    trans_delete(g_lis_trans);
  }
  if (g_use_unix_socket)
  {
    g_lis_trans = trans_create(2, 8192, 8192);
    g_snprintf(port, 255, "/tmp/.xrdp/xrdp_chansrv_socket_%d", 7200 + g_display_num);
  }
  else
  {
    g_lis_trans = trans_create(1, 8192, 8192);
    g_snprintf(port, 255, "%d", 7200 + g_display_num);
  }
  g_lis_trans->trans_conn_in = my_trans_conn_in;
  error = trans_listen(g_lis_trans, port);
  if (error != 0)
  {
    log_message(LOG_LEVEL_ERROR, "setup_listen: trans_listen failed for port %s", port);
    return 1;
  }
  return 0;
}

/*****************************************************************************/
THREAD_RV THREAD_CC
channel_thread_loop(void* in_val)
{
  tbus objs[32];
  int num_objs = 0;
  int timeout = 0;
  int error = 0;
  THREAD_RV rv = 0;

  log_message(LOG_LEVEL_INFO, "channel_thread_loop: thread start");
  rv = 0;
  error = setup_listen();
  if (error == 0)
  {
    timeout = -1;
    num_objs = 0;
    objs[num_objs] = g_term_event;
    num_objs++;
    trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
    while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0)
    {
      if (g_is_wait_obj_set(g_term_event))
      {
        log_message(LOG_LEVEL_INFO, "channel_thread_loop: g_term_event set");
        clipboard_deinit();
        sound_deinit();
        dev_redir_deinit();
        break;
      }
      if (g_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_lis_trans) != 0)
        {
          log_message(LOG_LEVEL_INFO, "channel_thread_loop: trans_check_wait_objs error");
        }
      }
      if (g_con_trans != 0)
      {
        if (trans_check_wait_objs(g_con_trans) != 0)
        {
          log_message(LOG_LEVEL_INFO, "channel_thread_loop: "
                  "trans_check_wait_objs error resetting");
          clipboard_deinit();
          sound_deinit();
          dev_redir_deinit();
          /* delete g_con_trans */
          trans_delete(g_con_trans);
          g_con_trans = 0;
          /* create new listener */
          error = setup_listen();
          if (error != 0)
          {
            break;
          }
        }
      }
      clipboard_check_wait_objs();
      sound_check_wait_objs();
      dev_redir_check_wait_objs();
      timeout = -1;
      num_objs = 0;
      objs[num_objs] = g_term_event;
      num_objs++;
      trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
      trans_get_wait_objs(g_con_trans, objs, &num_objs, &timeout);
      clipboard_get_wait_objs(objs, &num_objs, &timeout);
      sound_get_wait_objs(objs, &num_objs, &timeout);
      dev_redir_get_wait_objs(objs, &num_objs, &timeout);
    }
  }
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  trans_delete(g_con_trans);
  g_con_trans = 0;
  log_message(LOG_LEVEL_INFO, "channel_thread_loop: thread stop");
  g_set_wait_obj(g_thread_done_event);
  return rv;
}

/*****************************************************************************/
void DEFAULT_CC
term_signal_handler(int sig)
{
  log_message(LOG_LEVEL_INFO,"term_signal_handler: got signal %d", sig);
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void DEFAULT_CC
nil_signal_handler(int sig)
{
  log_message(LOG_LEVEL_INFO, "nil_signal_handler: got signal %d", sig);
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
static int APP_CC
get_display_num_from_display(char * display_text)
{
  int index = 0;
  int mode = 0;
  int host_index = 0;
  int disp_index = 0;
  int scre_index = 0;
  char host[256] = "";
  char disp[256] = "";
  char scre[256] = "";

  g_memset(host,0,256);
  g_memset(disp,0,256);
  g_memset(scre,0,256);

  index = 0;
  host_index = 0;
  disp_index = 0;
  scre_index = 0;
  mode = 0;
  while (display_text[index] != 0)
  {
    if (display_text[index] == ':')
    {
      mode = 1;
    }
    else if (display_text[index] == '.')
    {
      mode = 2;
    }
    else if (mode == 0)
    {
      host[host_index] = display_text[index];
      host_index++;
    }
    else if (mode == 1)
    {
      disp[disp_index] = display_text[index];
      disp_index++;
    }
    else if (mode == 2)
    {
      scre[scre_index] = display_text[index];
      scre_index++;
    }
    index++;
  }
  host[host_index] = 0;
  disp[disp_index] = 0;
  scre[scre_index] = 0;
  g_display_num = g_atoi(disp);
  return 0;
}

/*****************************************************************************/
int APP_CC
main_cleanup(void)
{
  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_thread_done_event);
  g_deinit(); /* os_calls */
  return 0;
}

/*****************************************************************************/
static int APP_CC
read_ini(void)
{
  char filename[256] = "";
  struct list* names = (struct list *)NULL;
  struct list* values = (struct list *)NULL;
  char* name  = (char *)NULL;
  char* value = (char *)NULL;
  int index = 0;

  g_memset(filename,0,(sizeof(char)*256));

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_use_unix_socket = 0;
  g_snprintf(filename, 255, "%s/sesman.ini", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, "Globals", names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (g_strcasecmp(name, "ListenAddress") == 0)
      {
        if (g_strcasecmp(value, "127.0.0.1") == 0)
        {
          g_use_unix_socket = 1;
        }
      }
    }
  }
  list_delete(names);
  list_delete(values);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int pid = 0;
  char text[256] = "";
  char* display_text = (char *)NULL;
  enum logReturns error ;
  char cfg_file[256];

  g_init("xrdp-chansrv"); /* os_calls */
  read_ini();
  pid = g_getpid();
  
  /* starting logging subsystem */
  g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);
  error = log_start(cfg_file,"XRDP-Chansrv");
  if (error != LOG_STARTUP_OK)
  {
    char buf[256] ;  
    switch (error)
    {
     case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot start logging. quitting.\n");
        break;
     case LOG_ERROR_FILE_OPEN:
        g_printf("error opening log file [%s]. quitting.\n", getLogFile(buf,255));
        break;
    }
    g_exit(1);
  }
  log_message(LOG_LEVEL_ALWAYS,"main: app started pid %d(0x%8.8x)", pid, pid);

  /*  set up signal handler  */
  g_signal_kill(term_signal_handler); /* SIGKILL */
  g_signal_terminate(term_signal_handler); /* SIGTERM */
  g_signal_user_interrupt(term_signal_handler); /* SIGINT */
  g_signal_pipe(nil_signal_handler); /* SIGPIPE */
  display_text = g_getenv("DISPLAY");
  log_message(LOG_LEVEL_INFO, "main: DISPLAY env var set to %s", display_text);
  get_display_num_from_display(display_text);
  if (g_display_num == 0)
  {
    log_message(LOG_LEVEL_ERROR, "main: error, display is zero");
    return 1;
  }
  log_message(LOG_LEVEL_INFO,"main: using DISPLAY %d", g_display_num);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_main_term", pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_thread_done", pid);
  g_thread_done_event = g_create_wait_obj(text);
  tc_thread_create(channel_thread_loop, 0);
  while (g_term_event > 0 && !g_is_wait_obj_set(g_term_event))
  {
    if (g_obj_wait(&g_term_event, 1, 0, 0, 0) != 0)
    {
      log_message(LOG_LEVEL_ERROR, "main: error, g_obj_wait failed");
      break;
    }
  }
  while (g_thread_done_event > 0 && !g_is_wait_obj_set(g_thread_done_event))
  {
    /* wait for thread to exit */
    if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
    {
      log_message(LOG_LEVEL_ERROR, "main: error, g_obj_wait failed");
      break;
    }
  }
  /* cleanup */
  main_cleanup();
  log_message(LOG_LEVEL_INFO, "main: app exiting pid %d(0x%8.8x)", pid, pid);
  g_deinit();
  return 0;
}
