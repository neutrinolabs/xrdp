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
   Copyright (C) Jay Sorg 2005-2009
*/

/**
 *
 * @file libscp_connection.h
 * @brief SCP_CONNECTION handling code
 * @author Simone Fedele
 * 
 */

#ifndef LIBSCP_CONNECTION_H
#define LIBSCP_CONNECTION_H

#include "libscp.h"

/**
 *
 * @brief creates a new connection
 * @param sck the connection socket
 *
 * @return a struct SCP_CONNECTION* object on success, NULL otherwise
 *
 */	
struct SCP_CONNECTION*
scp_connection_create(int sck);

/**
 *
 * @brief destroys a struct SCP_CONNECTION* object
 * @param c the object to be destroyed
 * 
 */
void
scp_connection_destroy(struct SCP_CONNECTION* c);

#endif

