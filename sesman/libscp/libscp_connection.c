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
   Copyright (C) Jay Sorg 2005-2010
*/

/**
 *
 * @file libscp_connection.c
 * @brief SCP_CONNECTION handling code
 * @author Simone Fedele
 *
 */

#include "libscp_connection.h"

//extern struct log_config* s_log;

struct SCP_CONNECTION*
scp_connection_create(int sck)
{
  struct SCP_CONNECTION* conn;

  conn = g_malloc(sizeof(struct SCP_CONNECTION), 0);

  if (0 == conn)
  {
    log_message(LOG_LEVEL_WARNING, "[connection:%d] connection create: malloc error", __LINE__);
    return 0;
  }

  conn->in_sck=sck;
  make_stream(conn->in_s);
  init_stream(conn->in_s, 8196);
  make_stream(conn->out_s);
  init_stream(conn->out_s, 8196);

  return conn;
}

void
scp_connection_destroy(struct SCP_CONNECTION* c)
{
  free_stream(c->in_s);
  free_stream(c->out_s);
  g_free(c);
}
