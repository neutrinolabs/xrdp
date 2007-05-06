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
 * @file libscp_v1c.h
 * @brief libscp version 1 client api declarations
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_V1C_H
#define LIBSCP_V1C_H

#include "libscp.h"

/* client API */
/* 001 */
enum SCP_CLIENT_STATES_E
scp_v1c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

/* 004 */
enum SCP_CLIENT_STATES_E
scp_v1c_resend_credentials(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

/* 021 */
enum SCP_CLIENT_STATES_E
scp_v1c_pwd_change(struct SCP_CONNECTION* c, char* newpass);

/* 022 */
enum SCP_CLIENT_STATES_E
scp_v1c_pwd_change_cancel(struct SCP_CONNECTION* c);

/* 041 */
enum SCP_CLIENT_STATES_E
scp_v1c_get_session_list(struct SCP_CONNECTION* c, int* scount,
                         struct SCP_DISCONNECTED_SESSION** s);

/* 043 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session(struct SCP_CONNECTION* c, struct SCP_SESSION* s,
                       SCP_SID sid);

/* 044 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session_cancel(struct SCP_CONNECTION* c);

#endif
