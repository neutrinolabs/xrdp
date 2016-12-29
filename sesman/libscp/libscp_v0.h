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
scp_v0s_allow_connection(struct SCP_CONNECTION* c, SCP_DISPLAY d, const tui8 *guid);

/**
 *
 * @brief denies the connection to TS
 * @param c connection descriptor
 *
 */
enum SCP_SERVER_STATES_E
scp_v0s_deny_connection(struct SCP_CONNECTION* c);

/**
 * @brief send reply to an authentication request
 * @param c connection descriptor
 * @param value the reply code 0 means ok
 * @return
 */
enum SCP_SERVER_STATES_E
scp_v0s_replyauthentication(struct SCP_CONNECTION* c, unsigned short int value, tui8 type);

#endif
