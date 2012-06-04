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
 * @file tcp.c
 * @brief Tcp stream funcions
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "libscp_tcp.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

extern struct log_config* s_log;

/*****************************************************************************/
int DEFAULT_CC
scp_tcp_force_recv(int sck, char* data, int len)
{
  int rcvd;
  int block;

  LOG_DBG("scp_tcp_force_recv()");
  block = scp_lock_fork_critical_section_start();

  while (len > 0)
  {
    rcvd = g_tcp_recv(sck, data, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(sck))
      {
        g_sleep(1);
      }
      else
      {
        scp_lock_fork_critical_section_end(block);
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      scp_lock_fork_critical_section_end(block);
      return 1;
    }
    else
    {
      data += rcvd;
      len -= rcvd;
    }
  }

  scp_lock_fork_critical_section_end(block);

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
scp_tcp_force_send(int sck, char* data, int len)
{
  int sent;
  int block;

  LOG_DBG("scp_tcp_force_send()");
  block = scp_lock_fork_critical_section_start();

  while (len > 0)
  {
    sent = g_tcp_send(sck, data, len, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(sck))
      {
        g_sleep(1);
      }
      else
      {
        scp_lock_fork_critical_section_end(block);
        return 1;
      }
    }
    else if (sent == 0)
    {
      scp_lock_fork_critical_section_end(block);
      return 1;
    }
    else
    {
      data += sent;
      len -= sent;
    }
  }

  scp_lock_fork_critical_section_end(block);

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
scp_tcp_bind(int sck, char* addr, char* port)
{
  struct sockaddr_in s;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_port = htons(atoi(port));
  s.sin_addr.s_addr = inet_addr(addr);
  return bind(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

