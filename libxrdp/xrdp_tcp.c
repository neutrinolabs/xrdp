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

   tcp layer

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_tcp* APP_CC
xrdp_tcp_create(struct xrdp_iso* owner, int sck)
{
  struct xrdp_tcp* self;

  DEBUG(("    in xrdp_tcp_create"));
  self = (struct xrdp_tcp*)g_malloc(sizeof(struct xrdp_tcp), 1);
  self->iso_layer = owner;
  self->sck = sck;
  DEBUG(("    out xrdp_tcp_create"));
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_tcp_delete(struct xrdp_tcp* self)
{
  g_free(self);
}

/*****************************************************************************/
/* get out stream ready for data */
/* returns error */
int APP_CC
xrdp_tcp_init(struct xrdp_tcp* self, struct stream* s)
{
  init_stream(s, 8192);
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_tcp_recv(struct xrdp_tcp* self, struct stream* s, int len)
{
  int rcvd;
  struct xrdp_session* session;

  if (self->sck_closed)
  {
    DEBUG(("    in xrdp_tcp_recv, sck closed"));
    return 1;
  }
  DEBUG(("    in xrdp_tcp_recv, gota get %d bytes", len));
  session = self->iso_layer->mcs_layer->sec_layer->rdp_layer->session;
  init_stream(s, len);
  while (len > 0)
  {
    rcvd = g_tcp_recv(self->sck, s->end, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (!g_tcp_can_recv(self->sck, 10))
        {
          if (session->is_term != 0)
          {
            if (session->is_term())
            {
              DEBUG(("    out xrdp_tcp_recv, terminated"));
              return 1;
            }
          }
        }
      }
      else
      {
        self->sck_closed = 1;
        DEBUG(("    error = -1 in xrdp_tcp_recv socket %d", self->sck));
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      self->sck_closed = 1;
      DEBUG(("    error = 0 in xrdp_tcp_recv socket %d", self->sck));
      return 1;
    }
    else
    {
      s->end += rcvd;
      len -= rcvd;
    }
  }
  DEBUG(("    out xrdp_tcp_recv"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_tcp_send(struct xrdp_tcp* self, struct stream* s)
{
  int len;
  int total;
  int sent;
  struct xrdp_session* session;

  if (self->sck_closed)
  {
    DEBUG(("    in xrdp_tcp_send, sck closed"));
    return 1;
  }
  len = s->end - s->data;
  DEBUG(("    in xrdp_tcp_send, gota send %d bytes", len));
  session = self->iso_layer->mcs_layer->sec_layer->rdp_layer->session;
  total = 0;
  while (total < len)
  {
    sent = g_tcp_send(self->sck, s->data + total, len - total, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (!g_tcp_can_send(self->sck, 10))
        {
          if (session->is_term != 0)
          {
            if (session->is_term())
            {
              DEBUG(("    out xrdp_tcp_send, terminated"));
              return 1;
            }
          }
        }
      }
      else
      {
        self->sck_closed = 1;
        DEBUG(("    error = -1 in xrdp_tcp_send socket %d", self->sck));
        return 1;
      }
    }
    else if (sent == 0)
    {
      self->sck_closed = 1;
      DEBUG(("    error = 0 in xrdp_tcp_send socket %d", self->sck));
      return 1;
    }
    else
    {
#if defined(XRDP_DEBUG)
      g_hexdump(s->data + total, sent);
#endif
      total = total + sent;
    }
  }
  DEBUG(("    out xrdp_tcp_send, sent %d bytes ok", len));
  return 0;
}
