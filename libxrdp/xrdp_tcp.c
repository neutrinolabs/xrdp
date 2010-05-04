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
   Copyright (C) Jay Sorg 2004-2010

   tcp layer

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_tcp* APP_CC
xrdp_tcp_create(struct xrdp_iso* owner, struct trans* trans)
{
  struct xrdp_tcp* self;

  DEBUG(("    in xrdp_tcp_create"));
  self = (struct xrdp_tcp*)g_malloc(sizeof(struct xrdp_tcp), 1);
  self->iso_layer = owner;
  self->trans = trans;
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
  DEBUG(("    in xrdp_tcp_recv, gota get %d bytes", len));
  init_stream(s, len);
  if (trans_force_read_s(self->trans, s, len) != 0)
  {
    DEBUG(("    error in trans_force_read_s"));
    return 1;
  }
  DEBUG(("    out xrdp_tcp_recv"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_tcp_send(struct xrdp_tcp* self, struct stream* s)
{
  DEBUG(("    in xrdp_tcp_send, gota send %d bytes", len));
  if (trans_force_write_s(self->trans, s) != 0)
  {
    DEBUG(("    error in trans_force_write_s"));
    return 1;
  }
  DEBUG(("    out xrdp_tcp_send, sent %d bytes ok", len));
  return 0;
}
