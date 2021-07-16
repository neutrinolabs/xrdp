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
 * @param trans connection trans
 * @param s pointer to session descriptor pointer
 *
 * this function places in *s the address of a newly allocated SCP_SESSION structure
 * that should be free()d
 */
enum SCP_SERVER_STATES_E
scp_v1s_accept(struct trans *t, struct SCP_SESSION *s);

/**
 *
 * @brief denies connection to sesman
 * @param c connection descriptor
 * @param reason pointer to a string containing the reason for denying connection
 *
 */
/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct trans *t, const char *reason);

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct trans *t, struct SCP_SESSION *s,
                         const char *reason);

enum SCP_SERVER_STATES_E
scp_v1s_accept_password_reply(struct trans *t, struct SCP_SESSION *s);

enum SCP_SERVER_STATES_E
scp_v1s_accept_list_sessions_reply(int cmd, struct trans *t);

/* 020 */
enum SCP_SERVER_STATES_E
scp_v1s_request_pwd_change(struct trans *t, char *reason, char *npw);

/* 023 */
enum SCP_SERVER_STATES_E
scp_v1s_pwd_change_error(struct trans *t, char *error, int retry, char *npw);

/* 030 */
enum SCP_SERVER_STATES_E
scp_v1s_connect_new_session(struct trans *t, SCP_DISPLAY d);

/* 032 */
enum SCP_SERVER_STATES_E
scp_v1s_connection_error(struct trans *t, const char *error);

/* 040 */
#if 0
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION *c, int sescnt,
                      struct SCP_DISCONNECTED_SESSION *ds, SCP_SID *sid);
#endif

enum SCP_SERVER_STATES_E
scp_v1s_list_sessions40(struct trans *t);

enum SCP_SERVER_STATES_E
scp_v1s_list_sessions42(struct trans *t, int sescnt, struct SCP_DISCONNECTED_SESSION *ds);

/* 046 was: 031 struct SCP_DISCONNECTED_SESSION* ds, */
enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct trans *t, SCP_DISPLAY d);

#endif
