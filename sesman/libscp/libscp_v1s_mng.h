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
 * @file libscp_v1s_mng.h
 * @brief libscp version 1 server api declarations - session management
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_V1S_MNG_H
#define LIBSCP_V1S_MNG_H

#include "libscp.h"

/* server API */
/**
 *
 * @brief processes the stream using scp version 1
 * @param c connection descriptor
 * @param s pointer to session descriptor pointer
 *
 * this function places in *s the address of a newely allocated SCP_SESSION structure
 * that should be free()d
 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s);

/**
 *
 * @brief allows connection to sesman
 * @param c connection descriptor
 *
 */
/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_allow_connection(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

/**
 *
 * @brief denies connection to sesman
 * @param c connection descriptor
 * @param reason pointer to a string containinge the reason for denying connection
 *
 */
/* 003 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_deny_connection(struct SCP_CONNECTION* c, char* reason);

/**
 *
 * @brief sends session list
 * @param c connection descriptor
 *
 */
/* 006 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_list_sessions(struct SCP_CONNECTION* c, struct SCP_SESSION* s,
                          int sescnt, struct SCP_DISCONNECTED_SESSION* ds);
//                           SCP_SID* sid);

#endif
