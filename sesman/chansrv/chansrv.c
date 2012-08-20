/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2012
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
#include "rail.h"
#include "xcommon.h"

static struct trans* g_lis_trans = 0;
static struct trans* g_con_trans = 0;
static struct trans* g_api_lis_trans = 0;
static struct trans* g_api_con_trans = 0;
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_cliprdr_index = -1;
static int g_rdpsnd_index = -1;
static int g_rdpdr_index = -1;
static int g_rail_index = -1;

static tbus g_term_event = 0;
static tbus g_thread_done_event = 0;

static int g_use_unix_socket = 0;

int g_display_num = 0;
int g_cliprdr_chan_id = -1; /* cliprdr */
int g_rdpsnd_chan_id = -1; /* rdpsnd */
int g_rdpdr_chan_id = -1; /* rdpdr */
int g_rail_chan_id = -1; /* rail */

char* g_exec_name;
tbus g_exec_event;
tbus g_exec_mutex;
tbus g_exec_sem;
int g_exec_pid = 0;

/* data in struct trans::callback_data */
struct xrdp_api_data
{
  int chan_id;
  char header[64];
  int flags;
};

/*****************************************************************************/
/* add data to chan_item, on its way to the client */
/* returns error */
static int APP_CC
add_data_to_chan_item(struct chan_item* chan_item, char* data, int size)
{
  struct stream* s;
  struct chan_out_data* cod;

  make_stream(s);
  init_stream(s, size);
  g_memcpy(s->data, data, size);
  s->end = s->data + size;
  cod = (struct chan_out_data*)g_malloc(sizeof(struct chan_out_data), 1);
  cod->s = s;
  if (chan_item->tail == 0)
  {
    chan_item->tail = cod;
    chan_item->head = cod;
  }
  else
  {
    chan_item->tail->next = cod;
    chan_item->tail = cod;
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_data_from_chan_item(struct chan_item* chan_item)
{
  struct stream* s;
  struct chan_out_data* cod;
  int bytes_left;
  int size;
  int chan_flags;
  int error;

  if (chan_item->head == 0)
  {
    return 0;
  }
  cod = chan_item->head;
  bytes_left = (int)(cod->s->end - cod->s->p);
  size = MIN(1600, bytes_left);
  chan_flags = 0;
  if (cod->s->p == cod->s->data)
  {
    chan_flags |= 1; /* first */
  }
  if (cod->s->p + size >= cod->s->end)
  {
    chan_flags |= 2; /* last */
  }
  s = trans_get_out_s(g_con_trans, 8192);
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + size); /* size */
  out_uint32_le(s, 8); /* msg id */
  out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + size); /* size */
  out_uint16_le(s, chan_item->id);
  out_uint16_le(s, chan_flags);
  out_uint16_le(s, size);
  out_uint32_le(s, cod->s->size);
  out_uint8a(s, cod->s->p, size);
  s_mark_end(s);
  LOGM((LOG_LEVEL_DEBUG, "chansrv::send_channel_data: -- "
        "size %d chan_flags 0x%8.8x", size, chan_flags));
  error = trans_force_write(g_con_trans);
  if (error != 0)
  {
    return 1;
  }
  cod->s->p += size;
  if (cod->s->p >= cod->s->end)
  {
    free_stream(cod->s);
    chan_item->head = chan_item->head->next;
    if (chan_item->head == 0)
    {
      chan_item->tail = 0;
    }
    g_free(cod);
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
check_chan_items(void)
{
  int index;

  for (index = 0; index < g_num_chan_items; index++)
  {
    if (g_chan_items[index].head != 0)
    {
      send_data_from_chan_item(g_chan_items + index);
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
send_channel_data(int chan_id, char* data, int size)
{
  int index;

  LOGM((LOG_LEVEL_DEBUG, "chansrv::send_channel_data: size %d", size));
  if (chan_id == -1)
  {
    return 1;
  }
  for (index = 0; index < g_num_chan_items; index++)
  {
    if (g_chan_items[index].id == chan_id)
    {
      add_data_to_chan_item(g_chan_items + index, data, size);
      check_chan_items();
      return 0;
    }
  }
  return 1;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_init_response_message(void)
{
  struct stream * s = (struct stream *)NULL;

  LOGM((LOG_LEVEL_INFO, "send_init_response_message:"));
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

  LOGM((LOG_LEVEL_DEBUG, "send_channel_setup_response_message:"));
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

  LOGM((LOG_LEVEL_DEBUG, "send_channel_data_response_message:"));
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
  LOGM((LOG_LEVEL_DEBUG, "process_message_init:"));
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

  g_num_chan_items = 0;
  g_cliprdr_index = -1;
  g_rdpsnd_index = -1;
  g_rdpdr_index = -1;
  g_rail_index = -1;
  g_cliprdr_chan_id = -1;
  g_rdpsnd_chan_id = -1;
  g_rdpdr_chan_id = -1;
  g_rail_chan_id = -1;
  LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup:"));
  in_uint16_le(s, num_chans);
  LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup: num_chans %d",
        num_chans));
  for (index = 0; index < num_chans; index++)
  {
    ci = &(g_chan_items[g_num_chan_items]);
    g_memset(ci->name, 0, sizeof(ci->name));
    in_uint8a(s, ci->name, 8);
    in_uint16_le(s, ci->id);
    in_uint16_le(s, ci->flags);
    LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup: chan name '%s' "
          "id %d flags %8.8x", ci->name, ci->id, ci->flags));
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
    else if (g_strcasecmp(ci->name, "rail") == 0)
    {
      g_rail_index = g_num_chan_items;
      g_rail_chan_id = ci->id;
    }
    else
    {
      LOG(10, ("other %s", ci->name));
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
  if (g_rail_index >= 0)
  {
    rail_init();
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
  struct stream* ls;

  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, length);
  in_uint32_le(s, total_length);
  LOGM((LOG_LEVEL_DEBUG,"process_message_channel_data: chan_id %d "
        "chan_flags %d", chan_id, chan_flags));
  LOG(10, ("process_message_channel_data"));
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
    else if (chan_id == g_rail_chan_id)
    {
      rv = rail_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == ((struct xrdp_api_data*)
                         (g_api_con_trans->callback_data))->chan_id)
    {
      LOG(10, ("process_message_channel_data length %d total_length %d "
          "chan_flags 0x%8.8x", length, total_length, chan_flags));
      ls = g_api_con_trans->out_s;
      if (chan_flags & 1) /* first */
      {
        init_stream(ls, total_length);
      }
      out_uint8a(ls, s->p, length);
      if (chan_flags & 2) /* last */
      {
        s_mark_end(ls);
        trans_force_write(g_api_con_trans);
      }
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
  check_chan_items();
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
        LOGM((LOG_LEVEL_ERROR, "process_message: error in process_message ",
              "unknown msg %d", id));
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
  LOGM((LOG_LEVEL_DEBUG, "my_trans_data_in:"));
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
/* returns error */
int DEFAULT_CC
my_api_trans_data_in(struct trans* trans)
{
  struct stream* s;
  int error;
  struct xrdp_api_data* ad;

  LOG(10, ("my_api_trans_data_in:"));
  if (trans == 0)
  {
    return 0;
  }
  if (trans != g_api_con_trans)
  {
    return 1;
  }
  LOGM((LOG_LEVEL_DEBUG, "my_api_trans_data_in:"));
  s = trans_get_in_s(trans);
  error = g_tcp_recv(trans->sck, s->data, 8192, 0);
  if (error > 0)
  {
    LOG(10, ("my_api_trans_data_in: got data %d", error));
    ad = (struct xrdp_api_data*)(trans->callback_data);
    if (send_channel_data(ad->chan_id, s->data, error) != 0)
    {
      LOG(0, ("my_api_trans_data_in: send_channel_data failed"));
    }
  }
  else
  {
    LOG(10, ("my_api_trans_data_in: g_tcp_recv failed, or disconnected"));
    return 1;
  }
  return 0;
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
  LOGM((LOG_LEVEL_DEBUG, "my_trans_conn_in:"));
  g_con_trans = new_trans;
  g_con_trans->trans_data_in = my_trans_data_in;
  g_con_trans->header_size = 8;
  /* stop listening */
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
my_api_trans_conn_in(struct trans* trans, struct trans* new_trans)
{
  struct xrdp_api_data* ad;
  int error;
  int index;
  int found;
  struct stream* s;

  if (trans == 0)
  {
    return 1;
  }
  if (trans != g_api_lis_trans)
  {
    return 1;
  }
  if (new_trans == 0)
  {
    return 1;
  }
  LOGM((LOG_LEVEL_DEBUG, "my_api_trans_conn_in:"));

  LOG(10, ("my_api_trans_conn_in: got incoming"));

  s = trans_get_in_s(new_trans);
  s->end = s->data;
  error = trans_force_read(new_trans, 64);
  if (error != 0)
  {
    LOG(0, ("my_api_trans_conn_in: trans_force_read failed"));
    trans_delete(new_trans);
  }
  s->end = s->data;

  ad = (struct xrdp_api_data*)g_malloc(sizeof(struct xrdp_api_data), 1);

  g_memcpy(ad->header, s->data, 64);

  ad->flags = GGET_UINT32(ad->header, 16);

  found = 0;
  if (ad->flags | 1) /* WTS_CHANNEL_OPTION_DYNAMIC */
  {
    /* TODO */
    found = 0;
  }
  else
  {
    for (index = 0; index < g_num_chan_items; index++)
    {
      LOG(10, ("  %s %s", ad->header, g_chan_items[index].name));
      if (g_strcasecmp(ad->header, g_chan_items[index].name) == 0)
      {
        LOG(10, ("my_api_trans_conn_in: found it at %d", index));
        ad->chan_id = g_chan_items[index].id;
        found = 1;
        break;
      }
    }
  }
  LOG(10, ("my_api_trans_conn_in: found %d", found));
  if (!found)
  {
    ad->chan_id = -1;
  }

  new_trans->callback_data = ad;

  trans_delete(g_api_con_trans);
  g_api_con_trans = new_trans;
  g_api_con_trans->trans_data_in = my_api_trans_data_in;
  g_api_con_trans->header_size = 0;

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
    g_snprintf(port, 255, "/tmp/.xrdp/xrdp_chansrv_socket_%d",
               7200 + g_display_num);
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
    LOGM((LOG_LEVEL_ERROR, "setup_listen: trans_listen failed for port %s",
          port));
    return 1;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
setup_api_listen(void)
{
  char port[256];
  int error = 0;

  g_api_lis_trans = trans_create(2, 8192, 8192);
  g_snprintf(port, 255, "/tmp/.xrdp/xrdpapi_%d", g_display_num);
  g_api_lis_trans->trans_conn_in = my_api_trans_conn_in;
  error = trans_listen(g_api_lis_trans, port);
  if (error != 0)
  {
    LOGM((LOG_LEVEL_ERROR, "setup_api_listen: trans_listen failed for port %s",
          port));
    return 1;
  }
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
  THREAD_RV rv;

  LOGM((LOG_LEVEL_INFO, "channel_thread_loop: thread start"));
  rv = 0;
  setup_api_listen();
  error = setup_listen();
  if (error == 0)
  {
    timeout = -1;
    num_objs = 0;
    objs[num_objs] = g_term_event;
    num_objs++;
    trans_get_wait_objs(g_lis_trans, objs, &num_objs);
    trans_get_wait_objs(g_api_lis_trans, objs, &num_objs);
    while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0)
    {
      if (g_is_wait_obj_set(g_term_event))
      {
        LOGM((LOG_LEVEL_INFO, "channel_thread_loop: g_term_event set"));
        clipboard_deinit();
        sound_deinit();
        dev_redir_deinit();
        rail_deinit();
        break;
      }
      if (g_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_lis_trans) != 0)
        {
          LOGM((LOG_LEVEL_INFO, "channel_thread_loop: "
                "trans_check_wait_objs error"));
        }
      }
      if (g_con_trans != 0)
      {
        if (trans_check_wait_objs(g_con_trans) != 0)
        {
          LOGM((LOG_LEVEL_INFO, "channel_thread_loop: "
                "trans_check_wait_objs error resetting"));
          clipboard_deinit();
          sound_deinit();
          dev_redir_deinit();
          rail_deinit();
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
      if (g_api_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_api_lis_trans) != 0)
        {
          LOG(0, ("channel_thread_loop: trans_check_wait_objs failed"));
        }
      }

      LOG(10, ("0 %p", g_api_con_trans));
      if (g_api_con_trans != 0)
      {
        LOG(10, ("1 %p %d", g_api_con_trans, g_tcp_can_recv(g_api_con_trans->sck, 0)));
        if (trans_check_wait_objs(g_api_con_trans) != 0)
        {
          LOG(10, ("channel_thread_loop: trans_check_wait_objs failed, "
              "or disconnected"));
          g_free(g_api_con_trans->callback_data);
          trans_delete(g_api_con_trans);
          g_api_con_trans = 0;
        }
      }

      xcommon_check_wait_objs();
      sound_check_wait_objs();
      dev_redir_check_wait_objs();
      timeout = -1;
      num_objs = 0;
      objs[num_objs] = g_term_event;
      num_objs++;
      trans_get_wait_objs(g_lis_trans, objs, &num_objs);
      trans_get_wait_objs(g_con_trans, objs, &num_objs);
      trans_get_wait_objs(g_api_lis_trans, objs, &num_objs);
      trans_get_wait_objs(g_api_con_trans, objs, &num_objs);
      xcommon_get_wait_objs(objs, &num_objs, &timeout);
      sound_get_wait_objs(objs, &num_objs, &timeout);
      dev_redir_get_wait_objs(objs, &num_objs, &timeout);
    }
  }
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  trans_delete(g_con_trans);
  g_con_trans = 0;
  trans_delete(g_api_lis_trans);
  g_api_lis_trans = 0;
  trans_delete(g_api_con_trans);
  g_api_con_trans = 0;
  LOGM((LOG_LEVEL_INFO, "channel_thread_loop: thread stop"));
  g_set_wait_obj(g_thread_done_event);
  return rv;
}

/*****************************************************************************/
void DEFAULT_CC
term_signal_handler(int sig)
{
  LOGM((LOG_LEVEL_INFO, "term_signal_handler: got signal %d", sig));
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void DEFAULT_CC
nil_signal_handler(int sig)
{
  LOGM((LOG_LEVEL_INFO, "nil_signal_handler: got signal %d", sig));
}

/*****************************************************************************/
void DEFAULT_CC
child_signal_handler(int sig)
{
  int i1;

  LOG(10, ("child_signal_handler:"));
  do
  {
    i1 = g_waitchild();
    if (i1 == g_exec_pid)
    {
      LOG(0, ("child_signal_handler: found pid %d", i1));
      //shutdownx();
    }
    LOG(10, ("  %d", i1));
  } while (i1 >= 0);
}

/*****************************************************************************/
static int APP_CC
get_display_num_from_display(char* display_text)
{
  int index;
  int mode;
  int host_index;
  int disp_index;
  int scre_index;
  char host[256];
  char disp[256];
  char scre[256];

  g_memset(host, 0, 256);
  g_memset(disp, 0, 256);
  g_memset(scre, 0, 256);

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
  g_delete_wait_obj(g_exec_event);
  tc_mutex_delete(g_exec_mutex);
  g_deinit(); /* os_calls */
  return 0;
}

/*****************************************************************************/
static int APP_CC
read_ini(void)
{
  char filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;

  g_memset(filename,0,(sizeof(char) * 256));
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
static int APP_CC
run_exec(void)
{
  int pid;

  LOG(10, ("run_exec:"));
  pid = g_fork();
  if (pid == 0)
  {
    trans_delete(g_con_trans);
    g_close_wait_obj(g_term_event);
    g_close_wait_obj(g_thread_done_event);
    g_close_wait_obj(g_exec_event);
    tc_mutex_delete(g_exec_mutex);
    tc_sem_delete(g_exec_sem);
    g_execlp3(g_exec_name, g_exec_name, 0);
    g_exit(0);
  }
  g_exec_pid = pid;
  tc_sem_inc(g_exec_sem);

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  tbus waiters[4];
  int pid = 0;
  char text[256];
  char* home_text;
  char* display_text;
  char log_file[256];
  enum logReturns error;
  struct log_config logconfig;

  g_init("xrdp-chansrv"); /* os_calls */

  home_text = g_getenv("HOME");
  if (home_text == 0)
  {
    g_writeln("error reading HOME environment variable");
    g_deinit();
    return 1;
  }

  read_ini();
  pid = g_getpid();

  /* starting logging subsystem */
  g_memset(&logconfig, 0, sizeof(struct log_config));
  logconfig.program_name = "XRDP-Chansrv";
  g_snprintf(log_file, 255, "%s/xrdp-chansrv.log", home_text);
  g_writeln("chansrv::main: using log file [%s]", log_file);
  if (g_file_exist(log_file))
  {
    g_file_delete(log_file);
  }
  logconfig.log_file = log_file;
  logconfig.fd = -1;
  logconfig.log_level = LOG_LEVEL_ERROR;
  logconfig.enable_syslog = 0;
  logconfig.syslog_level = 0;
  error = log_start_from_param(&logconfig);
  if (error != LOG_STARTUP_OK)
  {
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_writeln("error on malloc. cannot start logging. quitting.");
        break;
      case LOG_ERROR_FILE_OPEN:
        g_writeln("error opening log file [%s]. quitting.",
                 getLogFile(text, 255));
        break;
      default:
        g_writeln("log_start error");
        break;
    }
    g_deinit();
    return 1;
  }
  LOGM((LOG_LEVEL_ALWAYS, "main: app started pid %d(0x%8.8x)", pid, pid));
  /*  set up signal handler  */
  g_signal_kill(term_signal_handler); /* SIGKILL */
  g_signal_terminate(term_signal_handler); /* SIGTERM */
  g_signal_user_interrupt(term_signal_handler); /* SIGINT */
  g_signal_pipe(nil_signal_handler); /* SIGPIPE */
  g_signal_child_stop(child_signal_handler); /* SIGCHLD */
  display_text = g_getenv("DISPLAY");
  LOGM((LOG_LEVEL_INFO, "main: DISPLAY env var set to %s", display_text));
  get_display_num_from_display(display_text);
  if (g_display_num == 0)
  {
    LOGM((LOG_LEVEL_ERROR, "main: error, display is zero"));
    g_deinit();
    return 1;
  }
  LOGM((LOG_LEVEL_INFO, "main: using DISPLAY %d", g_display_num));
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_main_term", pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_thread_done", pid);
  g_thread_done_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_exec", pid);
  g_exec_event = g_create_wait_obj(text);
  g_exec_mutex = tc_mutex_create();
  g_exec_sem = tc_sem_create(0);
  tc_thread_create(channel_thread_loop, 0);
  while (g_term_event > 0 && !g_is_wait_obj_set(g_term_event))
  {
    waiters[0] = g_term_event;
    waiters[1] = g_exec_event;
    if (g_obj_wait(waiters, 2, 0, 0, 0) != 0)
    {
      LOGM((LOG_LEVEL_ERROR, "main: error, g_obj_wait failed"));
      break;
    }
    if (g_is_wait_obj_set(g_term_event))
    {
      break;
    }
    if (g_is_wait_obj_set(g_exec_event))
    {
      g_reset_wait_obj(g_exec_event);
      run_exec();
    }
  }
  while (g_thread_done_event > 0 && !g_is_wait_obj_set(g_thread_done_event))
  {
    /* wait for thread to exit */
    if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
    {
      LOGM((LOG_LEVEL_ERROR, "main: error, g_obj_wait failed"));
      break;
    }
  }
  /* cleanup */
  main_cleanup();
  LOGM((LOG_LEVEL_INFO, "main: app exiting pid %d(0x%8.8x)", pid, pid));
  g_deinit();
  return 0;
}
