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
scp_v1c_connect(struct trans *t, struct SCP_SESSION *s);

/* 004 */
enum SCP_CLIENT_STATES_E
scp_v1c_resend_credentials(struct trans *t, struct SCP_SESSION *s);

/* 021 */
enum SCP_CLIENT_STATES_E
scp_v1c_pwd_change(struct trans *t, char *newpass);

/* 022 */
enum SCP_CLIENT_STATES_E
scp_v1c_pwd_change_cancel(struct trans *t);

/* 041 */
enum SCP_CLIENT_STATES_E
scp_v1c_get_session_list(struct trans *t, int *scount,
                         struct SCP_DISCONNECTED_SESSION **s);

/* 043 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session(struct trans *t, struct SCP_SESSION *s,
                       SCP_SID sid);

/* 044 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session_cancel(struct trans *t);

#endif
