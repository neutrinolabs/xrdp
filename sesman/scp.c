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
   Copyright (C) Jay Sorg 2005-2006
*/

/**
 *
 * @file scp.c
 * @brief scp (sesman control protocol) common code
 *        scp (sesman control protocol) common code
 *        This code controls which version is being used and starts the appropriate process
 * @author Jay Sorg, Simone Fedele
 * 
 */

#include "sesman.h"

extern int thread_sck;

/******************************************************************************/
void* DEFAULT_CC
scp_process_start(void* sck)
{
  int socket;
  int version;
  int size;
  struct stream* in_s;
  struct stream* out_s;

  /* making a local copy of the socket (it's on the stack) */
  /* probably this is just paranoia                        */
  //socket = *((int*) sck);
  socket = thread_sck;
  LOG_DBG("started scp thread on socket %d", socket);
  
  /* unlocking thread_sck */
  lock_socket_release();
  
  make_stream(in_s);
  make_stream(out_s);
  
  init_stream(in_s, 8192);
  if (tcp_force_recv(socket, in_s->data, 8) == 0)
  {
    in_uint32_be(in_s, version);
    in_uint32_be(in_s, size);
    init_stream(in_s, 8192);
    if (tcp_force_recv(socket, in_s->data, size - 8) == 0)
    {
      if (version == 0)
      {
        /* starts processing an scp v0 connection */
        scp_v0_process(socket, in_s, out_s);
      }
#warning scp v1 is disabled
      /* this is temporarily disabled...
      else if (version == 1)
      {
        / * starts processing an scp v0 connection * /
        //scp_v1_process();
      }*/
      else
      {
        /* an unknown scp version was requested, so we shut down the */
	/* connection (and log the fact)                             */
	log_message(LOG_LEVEL_WARNING,"unknown protocol version specified. connection refused.");
      }
    }
  }
  g_tcp_close(socket);
  free_stream(in_s);
  free_stream(out_s);
  return 0;
}

