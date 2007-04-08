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
   Copyright (C) Jay Sorg 2004-2007

   mcs layer

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_mcs* APP_CC
xrdp_mcs_create(struct xrdp_sec* owner, int sck,
                struct stream* client_mcs_data,
                struct stream* server_mcs_data)
{
  struct xrdp_mcs* self;

  DEBUG(("  in xrdp_mcs_create"));
  self = (struct xrdp_mcs*)g_malloc(sizeof(struct xrdp_mcs), 1);
  self->sec_layer = owner;
  self->userid = 1;
  self->chanid = 1001;
  self->client_mcs_data = client_mcs_data;
  self->server_mcs_data = server_mcs_data;
  self->iso_layer = xrdp_iso_create(self, sck);
  self->channel_list = list_create();
  DEBUG(("  out xrdp_mcs_create"));
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_mcs_delete(struct xrdp_mcs* self)
{
  struct mcs_channel_item* channel_item;
  int index;
  int count;

  if (self == 0)
  {
    return;
  }
  /* here we have to free the channel items and anything in them */
  count = self->channel_list->count;
  for (index = count - 1; index >= 0; index--)
  {
    channel_item = (struct mcs_channel_item*)
                      list_get_item(self->channel_list, index);
    if (channel_item != 0)
    {
      free_stream(channel_item->in_s);
      g_free(channel_item);
    }
  }
  list_delete(self->channel_list);
  xrdp_iso_delete(self->iso_layer);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_send_cjcf(struct xrdp_mcs* self, int userid, int chanid)
{
  struct stream* s;

  DEBUG(("  in xrdp_mcs_send_cjcf"));
  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_cjcf error"));
    return 1;
  }
  out_uint8(s, (MCS_CJCF << 2) | 2);
  out_uint8(s, 0);
  out_uint16_be(s, userid);
  out_uint16_be(s, chanid);
  out_uint16_be(s, chanid);
  s_mark_end(s);
  if (xrdp_iso_send(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_cjcf error"));
    return 1;
  }
  free_stream(s);
  DEBUG(("  out xrdp_mcs_send_cjcf"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_mcs_recv(struct xrdp_mcs* self, struct stream* s, int* chan)
{
  int appid;
  int opcode;
  int len;
  int userid;
  int chanid;

  DEBUG(("  in xrdp_mcs_recv"));
  while (1)
  {
    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
      DEBUG(("  out xrdp_mcs_recv xrdp_iso_recv returned non zero"));
      return 1;
    }
    in_uint8(s, opcode);
    appid = opcode >> 2;
    if (appid == MCS_DPUM)
    {
      DEBUG(("  out xrdp_mcs_recv appid != MCS_DPUM"));
      return 1;
    }
    /* this is channels getting added from the client */
    if (appid == MCS_CJRQ)
    {
      in_uint16_be(s, userid);
      in_uint16_be(s, chanid);
      DEBUG(("  adding channel %4.4x", chanid));
      xrdp_mcs_send_cjcf(self, userid, chanid);
      continue;
    }
    break;
  }
  if (appid != MCS_SDRQ)
  {
    DEBUG(("  out xrdp_mcs_recv err got 0x%x need MCS_SDRQ", appid));
    return 1;
  }
  in_uint8s(s, 2);
  in_uint16_be(s, *chan);
  in_uint8s(s, 1);
  in_uint8(s, len);
  if (len & 0x80)
  {
    in_uint8s(s, 1);
  }
  DEBUG(("  out xrdp_mcs_recv"));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_ber_parse_header(struct xrdp_mcs* self, struct stream* s,
                          int tag_val, int* len)
{
  int tag;
  int l;
  int i;

  if (tag_val > 0xff)
  {
    in_uint16_be(s, tag);
  }
  else
  {
    in_uint8(s, tag);
  }
  if (tag != tag_val)
  {
    return 1;
  }
  in_uint8(s, l);
  if (l & 0x80)
  {
    l = l & ~0x80;
    *len = 0;
    while (l > 0)
    {
      in_uint8(s, i);
      *len = (*len << 8) | i;
      l--;
    }
  }
  else
  {
    *len = l;
  }
  if (s_check(s))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_parse_domain_params(struct xrdp_mcs* self, struct stream* s)
{
  int len;

  if (xrdp_mcs_ber_parse_header(self, s, MCS_TAG_DOMAIN_PARAMS, &len) != 0)
  {
    return 1;
  }
  in_uint8s(s, len);
  if (s_check(s))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_recv_connect_initial(struct xrdp_mcs* self)
{
  int len;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_mcs_ber_parse_header(self, s, MCS_CONNECT_INITIAL, &len) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, len);
  if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, len);
  if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_BOOLEAN, &len) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, len);
  if (xrdp_mcs_parse_domain_params(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_mcs_parse_domain_params(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_mcs_parse_domain_params(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
  {
    free_stream(s);
    return 1;
  }
  /* make a copy of client mcs data */
  init_stream(self->client_mcs_data, len);
  out_uint8a(self->client_mcs_data, s->p, len);
  in_uint8s(s, len);
  s_mark_end(self->client_mcs_data);
  if (s_check_end(s))
  {
    free_stream(s);
    return 0;
  }
  else
  {
    free_stream(s);
    return 1;
  }
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_recv_edrq(struct xrdp_mcs* self)
{
  int opcode;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, opcode);
  if ((opcode >> 2) != MCS_EDRQ)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 2);
  in_uint8s(s, 2);
  if (opcode & 2)
  {
    in_uint16_be(s, self->userid);
  }
  if (!(s_check_end(s)))
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_recv_aurq(struct xrdp_mcs* self)
{
  int opcode;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, opcode);
  if ((opcode >> 2) != MCS_AURQ)
  {
    free_stream(s);
    return 1;
  }
  if (opcode & 2)
  {
    in_uint16_be(s, self->userid);
  }
  if (!(s_check_end(s)))
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_send_aucf(struct xrdp_mcs* self)
{
  struct stream* s;

  DEBUG(("  in xrdp_mcs_send_aucf"));
  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_aucf error"));
    return 1;
  }
  out_uint8(s, ((MCS_AUCF << 2) | 2));
  out_uint8s(s, 1);
  out_uint16_be(s, self->userid);
  s_mark_end(s);
  if (xrdp_iso_send(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_aucf error"));
    return 1;
  }
  free_stream(s);
  DEBUG(("  out xrdp_mcs_send_aucf"));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_recv_cjrq(struct xrdp_mcs* self)
{
  int opcode;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, opcode);
  if ((opcode >> 2) != MCS_CJRQ)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 4);
  if (opcode & 2)
  {
    in_uint8s(s, 2);
  }
  if (!(s_check_end(s)))
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_ber_out_header(struct xrdp_mcs* self, struct stream* s,
                        int tag_val, int len)
{
  if (tag_val > 0xff)
  {
    out_uint16_be(s, tag_val);
  }
  else
  {
    out_uint8(s, tag_val);
  }
  if (len >= 0x80)
  {
    out_uint8(s, 0x82);
    out_uint16_be(s, len);
  }
  else
  {
    out_uint8(s, len);
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_ber_out_int8(struct xrdp_mcs* self, struct stream* s, int value)
{
  xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
  out_uint8(s, value);
  return 0;
}

#if 0 /* not used */
/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_ber_out_int16(struct xrdp_mcs* self, struct stream* s, int value)
{
  xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 2);
  out_uint8(s, (value >> 8));
  out_uint8(s, value);
  return 0;
}
#endif

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_ber_out_int24(struct xrdp_mcs* self, struct stream* s, int value)
{
  xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 3);
  out_uint8(s, (value >> 16));
  out_uint8(s, (value >> 8));
  out_uint8(s, value);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_out_domain_params(struct xrdp_mcs* self, struct stream* s,
                           int max_channels,
                           int max_users, int max_tokens,
                           int max_pdu_size)
{
  xrdp_mcs_ber_out_header(self, s, MCS_TAG_DOMAIN_PARAMS, 26);
  xrdp_mcs_ber_out_int8(self, s, max_channels);
  xrdp_mcs_ber_out_int8(self, s, max_users);
  xrdp_mcs_ber_out_int8(self, s, max_tokens);
  xrdp_mcs_ber_out_int8(self, s, 1);
  xrdp_mcs_ber_out_int8(self, s, 0);
  xrdp_mcs_ber_out_int8(self, s, 1);
  xrdp_mcs_ber_out_int24(self, s, max_pdu_size);
  xrdp_mcs_ber_out_int8(self, s, 2);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mcs_send_connect_response(struct xrdp_mcs* self)
{
  int data_len;
  struct stream* s;

  DEBUG(("  in xrdp_mcs_send_connect_response"));
  make_stream(s);
  init_stream(s, 8192);
  data_len = self->server_mcs_data->end - self->server_mcs_data->data;
  xrdp_iso_init(self->iso_layer, s);
  xrdp_mcs_ber_out_header(self, s, MCS_CONNECT_RESPONSE, data_len + 38);
  xrdp_mcs_ber_out_header(self, s, BER_TAG_RESULT, 1);
  out_uint8(s, 0);
  xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
  out_uint8(s, 0);
  xrdp_mcs_out_domain_params(self, s, 22, 3, 0, 0xfff8);
  xrdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, data_len);
  /* mcs data */
  out_uint8a(s, self->server_mcs_data->data, data_len);
  s_mark_end(s);
  if (xrdp_iso_send(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_connect_response error"));
    return 1;
  }
  free_stream(s);
  DEBUG(("  out xrdp_mcs_send_connect_response"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_mcs_incoming(struct xrdp_mcs* self)
{
  DEBUG(("  in xrdp_mcs_incoming"));
  if (xrdp_iso_incoming(self->iso_layer) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_recv_connect_initial(self) != 0)
  {
    return 1;
  }
  /* in xrdp_sec.c */
  if (xrdp_sec_process_mcs_data(self->sec_layer) != 0)
  {
    return 1;
  }
  /* in xrdp_sec.c */
  if (xrdp_sec_out_mcs_data(self->sec_layer) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_send_connect_response(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_recv_edrq(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_recv_aurq(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_send_aucf(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_recv_cjrq(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_send_cjcf(self, self->userid,
                         self->userid + MCS_USERCHANNEL_BASE) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_recv_cjrq(self) != 0)
  {
    return 1;
  }
  if (xrdp_mcs_send_cjcf(self, self->userid, MCS_GLOBAL_CHANNEL) != 0)
  {
    return 1;
  }
  DEBUG(("  out xrdp_mcs_incoming"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_mcs_init(struct xrdp_mcs* self, struct stream* s)
{
  xrdp_iso_init(self->iso_layer, s);
  s_push_layer(s, mcs_hdr, 8);
  return 0;
}

/*****************************************************************************/
/* returns error */
/* Inform the callback that an mcs packet has been sent.  This is needed so
   the module can send any high priority mcs packets like audio. */
static int APP_CC
xrdp_mcs_call_callback(struct xrdp_mcs* self)
{
  int rv;
  struct xrdp_session* session;

  rv = 0;
  /* if there is a callback, call it here */
  session = self->sec_layer->rdp_layer->session;
  if (session != 0)
  {
    if (session->callback != 0)
    {
      /* in xrdp_wm.c */
      rv = session->callback(session->id, 0x5556, 0, 0, 0, 0);
    }
    else
    {
      g_writeln("in xrdp_mcs_send, session->callback is nil");
    }
  }
  else
  {
    g_writeln("in xrdp_mcs_send, session is nil");
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_mcs_send(struct xrdp_mcs* self, struct stream* s, int chan)
{
  int len;
  //static int max_len = 0;

  DEBUG(("  in xrdp_mcs_send"));
  s_pop_layer(s, mcs_hdr);
  len = (s->end - s->p) - 8;
  if (len > 8192 * 2)
  {
    g_writeln("error in xrdp_mcs_send, size too bog, its %d", len);
  }
  //if (len > max_len)
  //{
  //  max_len = len;
  //  g_printf("mcs max length is %d\r\n", max_len);
  //}
  //g_printf("mcs length %d max length is %d\r\n", len, max_len);
  //g_printf("mcs length %d\r\n", len);
  len = len | 0x8000;
  out_uint8(s, MCS_SDIN << 2);
  out_uint16_be(s, self->userid);
  out_uint16_be(s, chan);
  out_uint8(s, 0x70);
  out_uint16_be(s, len);
  if (xrdp_iso_send(self->iso_layer, s) != 0)
  {
    DEBUG(("  out xrdp_mcs_send error"));
    return 1;
  }
  /* todo, do we need to call this for every mcs packet,
     maybe every 5 or so */
  if (chan == MCS_GLOBAL_CHANNEL)
  {
    xrdp_mcs_call_callback(self);
  }
  DEBUG(("  out xrdp_mcs_send"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_mcs_disconnect(struct xrdp_mcs* self)
{
  struct stream* s;

  DEBUG(("  in xrdp_mcs_disconnect"));
  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_disconnect error"));
    return 1;
  }
  out_uint8(s, (MCS_DPUM << 2) | 1);
  out_uint8(s, 0x80);
  s_mark_end(s);
  if (xrdp_iso_send(self->iso_layer, s) != 0)
  {
    free_stream(s);
    DEBUG(("  out xrdp_mcs_disconnect error"));
    return 1;
  }
  free_stream(s);
  DEBUG(("  out xrdp_mcs_disconnect"));
  return 0;
}
