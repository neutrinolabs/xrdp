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

   tcp layer

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_tcp* xrdp_tcp_create(struct xrdp_iso* owner)
{
  struct xrdp_tcp* self;

  self = (struct xrdp_tcp*)g_malloc(sizeof(struct xrdp_tcp), 1);
  self->iso_layer = owner;
  self->in_s = owner->in_s;
  self->out_s = owner->out_s;
  /* get sck from xrdp_process */
  self->sck = owner->mcs_layer->sec_layer->rdp_layer->pro_layer->sck;
  return self;
}

/*****************************************************************************/
void xrdp_tcp_delete(struct xrdp_tcp* self)
{
  g_free(self);
}

/*****************************************************************************/
/* get out stream ready for data */
/* returns error */
int xrdp_tcp_init(struct xrdp_tcp* self, int len)
{
  init_stream(self->out_s, len);
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_tcp_recv(struct xrdp_tcp* self, int len)
{
  int rcvd;

  DEBUG(("    in xrdp_tcp_recv, gota get %d bytes\n", len))
  init_stream(self->in_s, len);
  while (len > 0)
  {
    if (g_is_term())
      return 1;
    rcvd = g_tcp_recv(self->sck, self->in_s->end, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
        g_sleep(1);
      else
      {
        DEBUG(("    error = -1 in xrdp_tcp_recv socket %d\n", self->sck))
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      DEBUG(("    error = 0 in xrdp_tcp_recv socket %d\n", self->sck))
      return 1;
    }
    else
    {
      self->in_s->end += rcvd;
      len -= rcvd;
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_tcp_send(struct xrdp_tcp* self)
{
  int len;
  int total;
  int sent;

  len = self->out_s->end - self->out_s->data;
  DEBUG(("    in xrdp_tcp_send, gota send %d bytes\n", len))
  total = 0;
  while (total < len)
  {
    if (g_is_term())
      return 1;
    sent = g_tcp_send(self->sck, self->out_s->data + total, len - total, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
        g_sleep(1);
      else
      {
        DEBUG(("    error = -1 in xrdp_tcp_send socket %d\n", self->sck))
        return 1;
      }
    }
    else if (sent == 0)
    {
      DEBUG(("    error = 0 in xrdp_tcp_send socket %d\n", self->sck))
      return 1;
    }
    else
      total = total + sent;
  }
  return 0;
}
