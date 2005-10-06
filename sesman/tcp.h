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
   Copyright (C) Jay Sorg 2005

   session manager
   linux only

   tcp.h: tcp stream functions declarations
   
*/

/**
 * 
 * @file tcp stream functions
 *
 */

#ifndef TCP_H
#define TCP_H

/**
 *
 * force receiving data from tcp stream
 *
 * @param sck the socket to read from
 * @param data buffer
 * @param len buffer size
 *
 * @return 0: ok, 1: error
 * 
 */

int DEFAULT_CC
tcp_force_recv(int sck, char* data, int len);

/**
 *
 * force sending data to tcp stream
 *
 * @param sck the socket to write to
 * @param data buffer
 * @param len buffer size
 *
 * @return 0: ok, 1: error
 * 
 */

int DEFAULT_CC
tcp_force_send(int sck, char* data, int len);

#endif

