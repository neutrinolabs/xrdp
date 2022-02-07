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
 * @file libscp_commands.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_COMMANDS_H
#define LIBSCP_COMMANDS_H

#include "libscp_commands_mng.h"

/* Message numbers
 * SCP_CMD_* are client to server, SCP_REPLY_* are server to client */

/* Login sequence */
#define SCP_CMD_LOGIN                   1
#define SCP_REPLY_LOGIN_DENIED          2
#define SCP_REPLY_REREQUEST_CREDS       3
#define SCP_CMD_RESEND_CREDS            4
#define SCP_REPLY_CHANGE_PASSWD        20
#define SCP_REPLY_NEW_SESSION          30
#define SCP_REPLY_USER_SESSIONS_EXIST  40

/* List sessions */
#define SCP_CMD_GET_SESSION_LIST       41
#define SCP_REPLY_SESSIONS_INFO        42
#define SCP_CMD_SELECT_SESSION         43
#define SCP_CMD_SELECT_SESSION_CANCEL  44

/* Other */
#define SCP_CMD_FORCE_NEW_CONN         45
#define SCP_REPLY_SESSION_RECONNECTED  46
#define SCP_REPLY_CMD_CONN_ERROR       0xFFFF

#endif
