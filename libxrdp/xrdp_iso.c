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

   iso layer

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_iso* APP_CC
xrdp_iso_create(struct xrdp_mcs* owner, int sck)
{
  struct xrdp_iso* self;

  DEBUG(("   in xrdp_iso_create"));
  self = (struct xrdp_iso*)g_malloc(sizeof(struct xrdp_iso), 1);
  self->mcs_layer = owner;
  self->tcp_layer = xrdp_tcp_create(self, sck);
  DEBUG(("   out xrdp_iso_create"));
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_iso_delete(struct xrdp_iso* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_tcp_delete(self->tcp_layer);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_iso_recv_msg(struct xrdp_iso* self, struct stream* s, int* code)
{
  int ver;
  int len;

  *code = 0;
  if (xrdp_tcp_recv(self->tcp_layer, s, 4) != 0)
  {
    return 1;
  }
  in_uint8(s, ver);
  if (ver != 3)
  {
    return 1;
  }
  in_uint8s(s, 1);
  in_uint16_be(s, len);
  if (xrdp_tcp_recv(self->tcp_layer, s, len - 4) != 0)
  {
    return 1;
  }
  in_uint8s(s, 1);
  in_uint8(s, *code);
  if (*code == ISO_PDU_DT)
  {
    in_uint8s(s, 1);
  }
  else
  {
    in_uint8s(s, 5);
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_recv(struct xrdp_iso* self, struct stream* s)
{
  int code;

  DEBUG(("   in xrdp_iso_recv"));
  if (xrdp_iso_recv_msg(self, s, &code) != 0)
  {
    DEBUG(("   out xrdp_iso_recv xrdp_iso_recv_msg return non zero"));
    return 1;
  }
  if (code != ISO_PDU_DT)
  {
    DEBUG(("   out xrdp_iso_recv code != ISO_PDU_DT"));
    return 1;
  }
  DEBUG(("   out xrdp_iso_recv"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_iso_send_msg(struct xrdp_iso* self, struct stream* s, int code)
{
  if (xrdp_tcp_init(self->tcp_layer, s) != 0)
  {
    return 1;
  }
  out_uint8(s, 3);
  out_uint8(s, 0);
  out_uint16_be(s, 11); /* length */
  out_uint8(s, 6);
  out_uint8(s, code);
  out_uint16_le(s, 0);
  out_uint16_le(s, 0);
  out_uint8(s, 0);
  s_mark_end(s);
  if (xrdp_tcp_send(self->tcp_layer, s) != 0)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_incoming(struct xrdp_iso* self)
{
  int code;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  DEBUG(("   in xrdp_iso_incoming"));
  if (xrdp_iso_recv_msg(self, s, &code) != 0)
  {
    free_stream(s);
    return 1;
  }
  if (code != ISO_PDU_CR)
  {
    free_stream(s);
    return 1;
  }
  if (xrdp_iso_send_msg(self, s, ISO_PDU_CC) != 0)
  {
    free_stream(s);
    return 1;
  }
  DEBUG(("   out xrdp_iso_incoming"));
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_init(struct xrdp_iso* self, struct stream* s)
{
  xrdp_tcp_init(self->tcp_layer, s);
  s_push_layer(s, iso_hdr, 7);
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_send(struct xrdp_iso* self, struct stream* s)
{
  int len;

  DEBUG(("   in xrdp_iso_send"));
  s_pop_layer(s, iso_hdr);
  len = s->end - s->p;
  out_uint8(s, 3);
  out_uint8(s, 0);
  out_uint16_be(s, len);
  out_uint8(s, 2);
  out_uint8(s, ISO_PDU_DT);
  out_uint8(s, 0x80);
  if (xrdp_tcp_send(self->tcp_layer, s) != 0)
  {
    return 1;
  }
  DEBUG(("   out xrdp_iso_send"));
  return 0;
}
