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
 * @file libscp_tcp.h
 * @brief Tcp stream functions declarations
 * @author Jay Sorg, Simone Fedele
 *
 */

#ifndef LIBSCP_TCP_H
#define LIBSCP_TCP_H

#include "libscp.h"

/**
 *
 * @brief Force receiving data from tcp stream
 * @param sck The socket to read from
 * @param data Data buffer
 * @param len Data buffer size
 * @return 0 on success, 1 on error
 *
 */
int DEFAULT_CC
scp_tcp_force_recv(int sck, char* data, int len);

/**
 *
 * @brief Force sending data to tcp stream
 * @param sck the socket to write to
 * @param data Data buffer
 * @param len Data buffer size
 * @return 0 on success, 1 on error
 *
 */
int DEFAULT_CC
scp_tcp_force_send(int sck, char* data, int len);

/**
 *
 * @brief Binds the listening socket
 * @param sck Listening socket
 * @param addr Listening address
 * @param port Listening port
 * @return 0 on success, -1 on error
 *
 */
int DEFAULT_CC
scp_tcp_bind(int sck, char* addr, char* port);

#endif
