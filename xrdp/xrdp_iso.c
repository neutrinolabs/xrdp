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

   Copyright (C) Jay Sorg 2004

   iso layer

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_iso* xrdp_iso_create(struct xrdp_mcs* owner)
{
  struct xrdp_iso* self;

  self = (struct xrdp_iso*)g_malloc(sizeof(struct xrdp_iso), 1);
  self->owner = owner;
  self->in_s = owner->in_s;
  self->out_s = owner->out_s;
  self->tcp_layer = xrdp_tcp_create(self);
  return self;
}

/*****************************************************************************/
void xrdp_iso_delete(struct xrdp_iso* self)
{
  xrdp_tcp_delete(self->tcp_layer);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
int xrdp_iso_recv_msg(struct xrdp_iso* self, int* code)
{
  int ver;
  int len;

  *code = 0;
  if (xrdp_tcp_recv(self->tcp_layer, 4) != 0)
    return 1;
  in_uint8(self->in_s, ver);
  if (ver != 3)
    return 1;
  in_uint8s(self->in_s, 1);
  in_uint16_be(self->in_s, len);
  if (xrdp_tcp_recv(self->tcp_layer, len - 4) != 0)
    return 1;
  in_uint8s(self->in_s, 1);
  in_uint8(self->in_s, *code);
  if (*code == ISO_PDU_DT)
  {
    in_uint8s(self->in_s, 1);
  }
  else
  {
    in_uint8s(self->in_s, 5);
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_iso_recv(struct xrdp_iso* self)
{
  int code;

  if (xrdp_iso_recv_msg(self, &code) != 0)
    return 1;
  if (code != ISO_PDU_DT)
    return 1;
  return 0;
}

/*****************************************************************************/
int xrdp_iso_send_msg(struct xrdp_iso* self, int code)
{
  if (xrdp_tcp_init(self->tcp_layer, 11) != 0)
    return 1;
  out_uint8(self->out_s, 3);
  out_uint8(self->out_s, 0);
  out_uint16_be(self->out_s, 11); /* length */
  out_uint8(self->out_s, 8);
  out_uint8(self->out_s, code);
  out_uint16(self->out_s, 0);
  out_uint16(self->out_s, 0);
  out_uint8(self->out_s, 0);
  s_mark_end(self->out_s);
  if (xrdp_tcp_send(self->tcp_layer) != 0)
    return 1;
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_iso_incoming(struct xrdp_iso* self)
{
  int code;

  if (xrdp_iso_recv_msg(self, &code) != 0)
    return 1;
  if (code != ISO_PDU_CR)
    return 1;
  if (xrdp_iso_send_msg(self, ISO_PDU_CC) != 0)
    return 1;
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_iso_init(struct xrdp_iso* self, int len)
{
  xrdp_tcp_init(self->tcp_layer, len + 7);
  s_push_layer(self->out_s, iso_hdr, 7);
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_iso_send(struct xrdp_iso* self)
{
  int len;

  s_pop_layer(self->out_s, iso_hdr);
  len = self->out_s->end - self->out_s->p;
  out_uint8(self->out_s, 3);
  out_uint8(self->out_s, 0);
  out_uint16_be(self->out_s, len);
  out_uint8(self->out_s, 2);
  out_uint8(self->out_s, ISO_PDU_DT);
  out_uint8(self->out_s, 0x80);
  if (xrdp_tcp_send(self->tcp_layer) != 0)
    return 1;
  return 0;
}
