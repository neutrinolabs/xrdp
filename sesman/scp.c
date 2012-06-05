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
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file scp.c
 * @brief scp (sesman control protocol) common code
 *        scp (sesman control protocol) common code
 *        This code controls which version is being used and starts the
 *        appropriate process
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

extern int g_thread_sck; /* in thread.c */
extern struct config_sesman* g_cfg; /* in sesman.c */

/******************************************************************************/
void* DEFAULT_CC
scp_process_start(void* sck)
{
  struct SCP_CONNECTION scon;
  struct SCP_SESSION* sdata;

  /* making a local copy of the socket (it's on the stack) */
  /* probably this is just paranoia                        */
  scon.in_sck = g_thread_sck;
  LOG_DBG("started scp thread on socket %d", scon.in_sck);

  /* unlocking g_thread_sck */
  lock_socket_release();

  make_stream(scon.in_s);
  make_stream(scon.out_s);

  init_stream(scon.in_s, 8192);
  init_stream(scon.out_s, 8192);

  switch (scp_vXs_accept(&scon, &(sdata)))
  {
    case SCP_SERVER_STATE_OK:
      if (sdata->version == 0)
      {
        /* starts processing an scp v0 connection */
        LOG_DBG("accept ok, go on with scp v0\n",0);
        scp_v0_process(&scon, sdata);
      }
      else
      {
        LOG_DBG("accept ok, go on with scp v1\n",0);
        /*LOG_DBG("user: %s\npass: %s",sdata->username, sdata->password);*/
        scp_v1_process(&scon, sdata);
      }
      break;
    case SCP_SERVER_STATE_START_MANAGE:
      /* starting a management session */
      log_message(LOG_LEVEL_WARNING,
        "starting a sesman management session...");
      scp_v1_mng_process(&scon, sdata);
      break;
    case SCP_SERVER_STATE_VERSION_ERR:
      /* an unknown scp version was requested, so we shut down the */
      /* connection (and log the fact)                             */
      log_message(LOG_LEVEL_WARNING,
        "unknown protocol version specified. connection refused.");
      break;
    case SCP_SERVER_STATE_NETWORK_ERR:
      log_message(LOG_LEVEL_WARNING, "libscp network error.");
      break;
    case SCP_SERVER_STATE_SEQUENCE_ERR:
      log_message(LOG_LEVEL_WARNING, "libscp sequence error.");
      break;
    case SCP_SERVER_STATE_INTERNAL_ERR:
      /* internal error occurred (eg. malloc() error, ecc.) */
      log_message(LOG_LEVEL_ERROR, "libscp internal error occurred.");
      break;
    default:
      log_message(LOG_LEVEL_ALWAYS, "unknown return from scp_vXs_accept()");
  }
  g_tcp_close(scon.in_sck);
  free_stream(scon.in_s);
  free_stream(scon.out_s);
  return 0;
}
