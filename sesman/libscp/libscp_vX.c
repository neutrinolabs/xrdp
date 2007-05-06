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
*/

/**
 *
 * @file libscp_vX.c
 * @brief libscp version neutral code
 * @author Simone Fedele
 *
 */

#include "libscp_vX.h"

/* server API */
enum SCP_SERVER_STATES_E scp_vXs_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s)
{
  tui32 version;

  /* reading version and packet size */
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);

  if (version == 0)
  {
    return scp_v0s_accept(c, s, 1);
  }
  else if (version == 1)
  {
    return scp_v1s_accept(c, s, 1);
  }

  return SCP_SERVER_STATE_VERSION_ERR;
}
