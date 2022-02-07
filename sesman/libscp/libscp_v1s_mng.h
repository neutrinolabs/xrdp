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
 * @param atrans connection descriptor
 * @param s session descriptor pointer
 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_accept(struct trans *atrans, struct SCP_SESSION *s);

/**
 *
 * @brief allows connection to sesman
 * @param c connection descriptor
 *
 */
/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_allow_connection(struct trans *atrans, struct SCP_SESSION *s);

/**
 *
 * @brief denies connection to sesman
 * @param c connection descriptor
 * @param reason pointer to a string containing the reason for denying connection
 *
 */
/* 003 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_deny_connection(struct trans *atrans, const char *reason);

/**
 *
 * @brief sends session list
 * @param c connection descriptor
 *
 */
/* 006 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_list_sessions(struct trans *atrans, struct SCP_SESSION *s,
                          int sescnt, struct SCP_DISCONNECTED_SESSION *ds);
//                           SCP_SID* sid);

#endif
