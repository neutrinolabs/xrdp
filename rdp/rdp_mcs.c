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
   Copyright (C) Jay Sorg 2005-2007

   librdp mcs layer

*/

#include "rdp.h"

/*****************************************************************************/
struct rdp_mcs* APP_CC
rdp_mcs_create(struct rdp_sec* owner,
               struct stream* client_mcs_data,
               struct stream* server_mcs_data)
{
  struct rdp_mcs* self;

  self = (struct rdp_mcs*)g_malloc(sizeof(struct rdp_mcs), 1);
  self->sec_layer = owner;
  self->userid = 1;
  self->client_mcs_data = client_mcs_data;
  self->server_mcs_data = server_mcs_data;
  self->iso_layer = rdp_iso_create(self);
  return self;
}

/*****************************************************************************/
void APP_CC
rdp_mcs_delete(struct rdp_mcs* self)
{
  if (self == 0)
  {
    return;
  }
  rdp_iso_delete(self->iso_layer);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_mcs_recv(struct rdp_mcs* self, struct stream* s, int* chan)
{
  int appid;
  int opcode;
  int len;

  DEBUG(("  in rdp_mcs_recv"));
  if (rdp_iso_recv(self->iso_layer, s) != 0)
  {
    return 1;
  }
  in_uint8(s, opcode);
  appid = opcode >> 2;
  if (appid != MCS_SDIN)
  {
    DEBUG(("  out rdp_mcs_recv error"));
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
  DEBUG(("  out rdp_mcs_recv"));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_ber_out_header(struct rdp_mcs* self, struct stream* s,
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

#if 0
/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_ber_out_int8(struct rdp_mcs* self, struct stream* s, int value)
{
  rdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
  out_uint8(s, value);
  return 0;
}
#endif

/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_ber_out_int16(struct rdp_mcs* self, struct stream* s, int value)
{
  rdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 2);
  out_uint8(s, (value >> 8));
  out_uint8(s, value);
  return 0;
}

#if 0
/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_ber_out_int24(struct rdp_mcs* self, struct stream* s, int value)
{
  rdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 3);
  out_uint8(s, (value >> 16));
  out_uint8(s, (value >> 8));
  out_uint8(s, value);
  return 0;
}
#endif

/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_out_domain_params(struct rdp_mcs* self, struct stream* s,
                           int max_channels,
                           int max_users, int max_tokens,
                           int max_pdu_size)
{
  rdp_mcs_ber_out_header(self, s, MCS_TAG_DOMAIN_PARAMS, 32);
  rdp_mcs_ber_out_int16(self, s, max_channels);
  rdp_mcs_ber_out_int16(self, s, max_users);
  rdp_mcs_ber_out_int16(self, s, max_tokens);
  rdp_mcs_ber_out_int16(self, s, 1);
  rdp_mcs_ber_out_int16(self, s, 0);
  rdp_mcs_ber_out_int16(self, s, 1);
  rdp_mcs_ber_out_int16(self, s, max_pdu_size);
  rdp_mcs_ber_out_int16(self, s, 2);
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_mcs_send_connection_initial(struct rdp_mcs* self)
{
  int data_len;
  int len;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  data_len = self->client_mcs_data->end - self->client_mcs_data->data;
  len = 7 + 3 * 34 + 4 + data_len;
  if (rdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  rdp_mcs_ber_out_header(self, s, MCS_CONNECT_INITIAL, len);
  rdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, 0); /* calling domain */
  rdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, 0); /* called domain */
  rdp_mcs_ber_out_header(self, s, BER_TAG_BOOLEAN, 1);
  out_uint8(s, 0xff); /* upward flag */
  rdp_mcs_out_domain_params(self, s, 2, 2, 0, 0xffff); /* target params */
  rdp_mcs_out_domain_params(self, s, 1, 1, 1, 0x420); /* min params */
  rdp_mcs_out_domain_params(self, s, 0xffff, 0xfc17, 0xffff, 0xffff); /* max params */
  rdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, data_len);
  out_uint8p(s, self->client_mcs_data->data, data_len);
  s_mark_end(s);
  if (rdp_iso_send(self->iso_layer, s) != 0)
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
rdp_mcs_ber_parse_header(struct rdp_mcs* self, struct stream* s,
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
rdp_mcs_parse_domain_params(struct rdp_mcs* self, struct stream* s)
{
  int len;

  if (rdp_mcs_ber_parse_header(self, s, MCS_TAG_DOMAIN_PARAMS, &len) != 0)
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
rdp_mcs_recv_connection_response(struct rdp_mcs* self)
{
  int len;
  int res;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  rdp_mcs_ber_parse_header(self, s, MCS_CONNECT_RESPONSE, &len);
  rdp_mcs_ber_parse_header(self, s, BER_TAG_RESULT, &len);
  in_uint8(s, res);
  if (res != 0)
  {
    free_stream(s);
    return 1;
  }
  rdp_mcs_ber_parse_header(self, s, BER_TAG_INTEGER, &len);
  in_uint8s(s, len); /* connect id */
  rdp_mcs_parse_domain_params(self, s);
  rdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len);
  if (len > self->server_mcs_data->size)
  {
    len = self->server_mcs_data->size;
  }
  in_uint8a(s, self->server_mcs_data->data, len);
  self->server_mcs_data->p = self->server_mcs_data->data;
  self->server_mcs_data->end = self->server_mcs_data->data + len;
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
rdp_mcs_send_edrq(struct rdp_mcs* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint8(s, (MCS_EDRQ << 2));
  out_uint16_be(s, 0x100); /* height */
  out_uint16_be(s, 0x100); /* interval */
  s_mark_end(s);
  if (rdp_iso_send(self->iso_layer, s) != 0)
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
rdp_mcs_send_aurq(struct rdp_mcs* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint8(s, (MCS_AURQ << 2));
  s_mark_end(s);
  if (rdp_iso_send(self->iso_layer, s) != 0)
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
rdp_mcs_recv_aucf(struct rdp_mcs* self)
{
  int opcode;
  int res;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, opcode);
  if ((opcode >> 2) != MCS_AUCF)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, res);
  if (res != 0)
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
rdp_mcs_send_cjrq(struct rdp_mcs* self, int chanid)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_init(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint8(s, (MCS_CJRQ << 2));
  out_uint16_be(s, self->userid);
  out_uint16_be(s, chanid);
  s_mark_end(s);
  if (rdp_iso_send(self->iso_layer, s) != 0)
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
rdp_mcs_recv_cjcf(struct rdp_mcs* self)
{
  int opcode;
  int res;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (rdp_iso_recv(self->iso_layer, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, opcode);
  if ((opcode >> 2) != MCS_CJCF)
  {
    free_stream(s);
    return 1;
  }
  in_uint8(s, res);
  if (res != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 4); /* mcs_userid, req_chanid */
  if (opcode & 2)
  {
    in_uint8s(s, 2); /* join_chanid */
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
int APP_CC
rdp_mcs_connect(struct rdp_mcs* self, char* ip, char* port)
{
  DEBUG(("  in rdp_mcs_connect"));
  if (rdp_iso_connect(self->iso_layer, ip, port) != 0)
  {
    DEBUG(("  out rdp_mcs_connect error rdp_iso_connect failed"));
    return 1;
  }
  rdp_mcs_send_connection_initial(self);
  if (rdp_mcs_recv_connection_response(self) != 0)
  {
    rdp_iso_disconnect(self->iso_layer);
    DEBUG(("  out rdp_mcs_connect error rdp_mcs_recv_connection_response \
failed"));
    return 1;
  }
  rdp_mcs_send_edrq(self);
  rdp_mcs_send_aurq(self);
  if (rdp_mcs_recv_aucf(self) != 0)
  {
    rdp_iso_disconnect(self->iso_layer);
    DEBUG(("  out rdp_mcs_connect error rdp_mcs_recv_aucf failed"));
    return 1;
  }
  rdp_mcs_send_cjrq(self, self->userid + 1001);
  if (rdp_mcs_recv_cjcf(self) != 0)
  {
    rdp_iso_disconnect(self->iso_layer);
    DEBUG(("  out rdp_mcs_connect error rdp_mcs_recv_cjcf 1 failed"));
    return 1;
  }
  rdp_mcs_send_cjrq(self, MCS_GLOBAL_CHANNEL);
  if (rdp_mcs_recv_cjcf(self) != 0)
  {
    rdp_iso_disconnect(self->iso_layer);
    DEBUG(("  out rdp_mcs_connect error rdp_mcs_recv_cjcf 2 failed"));
    return 1;
  }
  DEBUG(("  out rdp_mcs_connect"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_mcs_init(struct rdp_mcs* self, struct stream* s)
{
  rdp_iso_init(self->iso_layer, s);
  s_push_layer(s, mcs_hdr, 8);
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_mcs_send(struct rdp_mcs* self, struct stream* s)
{
  int len;

  s_pop_layer(s, mcs_hdr);
  len = (s->end - s->p) - 8;
  len = len | 0x8000;
  out_uint8(s, MCS_SDRQ << 2);
  out_uint16_be(s, self->userid);
  out_uint16_be(s, MCS_GLOBAL_CHANNEL);
  out_uint8(s, 0x70);
  out_uint16_be(s, len);
  if (rdp_iso_send(self->iso_layer, s) != 0)
  {
    return 1;
  }
  return 0;
}
