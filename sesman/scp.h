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
 * @file scp.h
 * @brief scp (sesman control protocol) common definitions
 * @author Simone Fedele
 * 
 */

#ifndef SCP_H
#define SCP_H

//#include "libscp.h"
#include "scp_v0.h"
#include "scp_v1.h"

/**
 *
 * @brief Starts a an scp protocol thread.
 *        Starts a an scp protocol thread.
 *        But does only version control....
 * @param socket the connection socket
 *
 */
void* DEFAULT_CC
scp_process_start(void* sck);

#endif
