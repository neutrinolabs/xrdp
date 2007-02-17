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
 * @file tcp.c
 * @brief Tcp stream funcions
 * @author Jay Sorg
 * 
 */

#include "sesman.h"

/*****************************************************************************/
int DEFAULT_CC
tcp_force_recv(int sck, char* data, int len)
{
  int rcvd;

#ifndef LIBSCP_CLIENT
  int block;
  block = lock_fork_critical_section_start();
#endif

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
#ifndef LIBSCP_CLIENT
        lock_fork_critical_section_end(block);
#endif
        return 1;
      }
    }
    else if (rcvd == 0)
    {
#ifndef LIBSCP_CLIENT
      lock_fork_critical_section_end(block);
#endif
      return 1;
    }
    else
    {
      data += rcvd;
      len -= rcvd;
    }
  }

#ifndef LIBSCP_CLIENT
  lock_fork_critical_section_end(block);
#endif

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
tcp_force_send(int sck, char* data, int len)
{
  int sent;

#ifndef LIBSCP_CLIENT
  int block;
  block = lock_fork_critical_section_start();
#endif

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
#ifndef LIBSCP_CLIENT
        lock_fork_critical_section_end(block);
#endif
        return 1;
      }
    }
    else if (sent == 0)
    {
#ifndef LIBSCP_CLIENT
      lock_fork_critical_section_end(block);
#endif
      return 1;
    }
    else
    {
      data += sent;
      len -= sent;
    }
  }

#ifndef LIBSCP_CLIENT
  lock_fork_critical_section_end(block);
#endif

  return 0;
}
