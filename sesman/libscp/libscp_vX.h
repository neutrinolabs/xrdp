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
 * @file libscp_vX.h
 * @brief libscp version neutral code header
 * @author Simone Fedele
 * 
 */

#ifndef LIBSCP_VX_H
#define LIBSCP_VX_H

#include "libscp_types.h"

#include "libscp_v0.h"
#include "libscp_v1s.h"

/* server API */
/**
 *
 * @brief version neutral server accept function
 * @param c connection descriptor
 * @param s session descriptor pointer address.
 *          it will return a newely allocated descriptor.
 *          It this memory needs to be g_free()d
 *
 */
enum SCP_SERVER_STATES_E scp_vXs_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s);

#endif
