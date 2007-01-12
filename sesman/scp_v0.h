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
 * @file scp_v0.h
 * @brief scp version 0 declarations
 * @author Simone Fedele
 * 
 */

#ifndef SCP_V0_H
#define SCP_V0_H

#include "libscp.h"

/**
 *
 * @brief processes the stream using scp version 0
 * @param in_sck connection socket
 * @param in_s input stream
 * @param out_s output stream
 *
 */
void DEFAULT_CC 
scp_v0_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

#endif
