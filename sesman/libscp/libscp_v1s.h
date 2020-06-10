/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
 * this function places in *s the address of a newly allocated SCP_SESSION structure
 * that should be free()d
 */
enum SCP_SERVER_STATES_E
scp_v1s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk);

/**
 *
 * @brief processes the stream using scp version 1
 * @param trans connection trans
 * @param s pointer to session descriptor pointer
 *
 * this function places in *s the address of a newly allocated SCP_SESSION structure
 * that should be free()d
 */
enum SCP_SERVER_STATES_E
scp_v1s_accept_msg(struct trans *atrans, struct SCP_SESSION** s);

/**
 *
 * @brief denies connection to sesman
 * @param c connection descriptor
 * @param reason pointer to a string containing the reason for denying connection
 *
 */
/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct SCP_CONNECTION* c, const char *reason);

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct SCP_CONNECTION* c, struct SCP_SESSION* s,
                         const char *reason);

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
scp_v1s_connection_error(struct SCP_CONNECTION* c, const char *error);

/* 040 */
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION* c, int sescnt,
                      struct SCP_DISCONNECTED_SESSION* ds, SCP_SID* sid);

/* 046 was: 031 struct SCP_DISCONNECTED_SESSION* ds, */
enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct SCP_CONNECTION* c, SCP_DISPLAY d);

#endif
