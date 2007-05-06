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
 * @file libscp_v1s.h
 * @brief libscp version 1 server api declarations
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_V1S_H
#define LIBSCP_V1S_H

#include "libscp.h"

/* server API */
/**
 *
 * @brief processes the stream using scp version 1
 * @param c connection descriptor
 * @param s pointer to session descriptor pointer
 * @param skipVchk if set to !0 skips the version control (to be used after
 *                 scp_vXs_accept() )
 *
 * this function places in *s the address of a newely allocated SCP_SESSION structure
 * that should be free()d
 */
enum SCP_SERVER_STATES_E
scp_v1s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk);

/**
 *
 * @brief denies connection to sesman
 * @param c connection descriptor
 * @param reason pointer to a string containinge the reason for denying connection
 *
 */
/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct SCP_CONNECTION* c, char* reason);

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct SCP_CONNECTION* c, struct SCP_SESSION* s, char* reason);

/* 020 */
enum SCP_SERVER_STATES_E
scp_v1s_request_pwd_change(struct SCP_CONNECTION* c, char* reason, char* npw);

/* 023 */
enum SCP_SERVER_STATES_E
scp_v1s_pwd_change_error(struct SCP_CONNECTION* c, char* error, int retry, char* npw);

/* 030 */
enum SCP_SERVER_STATES_E
scp_v1s_connect_new_session(struct SCP_CONNECTION* c, SCP_DISPLAY d);

/* 032 */
enum SCP_SERVER_STATES_E
scp_v1s_connection_error(struct SCP_CONNECTION* c, char* error);

/* 040 */
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION* c, int sescnt,
                      struct SCP_DISCONNECTED_SESSION* ds, SCP_SID* sid);

/* 046 was: 031 struct SCP_DISCONNECTED_SESSION* ds, */
enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct SCP_CONNECTION* c, SCP_DISPLAY d);

#endif
