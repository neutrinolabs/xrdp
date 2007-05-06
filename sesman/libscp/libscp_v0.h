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
 * @file libscp_v0.h
 * @brief libscp version 0 declarations
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_V0_H
#define LIBSCP_V0_H

#include "libscp.h"

/* client API */
/**
 *
 * @brief connects to sesman using scp v0
 * @param c connection descriptor
 * @param s session descriptor
 * @param d display
 *
 */
enum SCP_CLIENT_STATES_E 
scp_v0c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

/* server API */
/**
 *
 * @brief processes the stream using scp version 0
 * @param c connection descriptor
 * @param s session descriptor
 * @param skipVchk if set to !0 skips the version control (to be used after
 *                 scp_vXs_accept() )
 *
 */
enum SCP_SERVER_STATES_E 
scp_v0s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk);

/**
 *
 * @brief allows the connection to TS, returning the display port
 * @param c connection descriptor
 *
 */
enum SCP_SERVER_STATES_E 
scp_v0s_allow_connection(struct SCP_CONNECTION* c, SCP_DISPLAY d);

/**
 *
 * @brief denies the connection to TS
 * @param c connection descriptor
 *
 */
enum SCP_SERVER_STATES_E 
scp_v0s_deny_connection(struct SCP_CONNECTION* c);

#endif

