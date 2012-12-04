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
 * @file libscp_commands_mng.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_COMMANDS_MNG_H
#define LIBSCP_COMMANDS_MNG_H

#define SCP_CMD_MNG_LOGIN        0x0001
#define SCP_CMD_MNG_LOGIN_ALLOW  0x0002
#define SCP_CMD_MNG_LOGIN_DENY   0x0003
#define SCP_CMD_MNG_CMD_ERROR    0x0004
#define SCP_CMD_MNG_LIST_REQ     0x0005
#define SCP_CMD_MNG_LIST         0x0006
#define SCP_CMD_MNG_ACTION       0x0007

#endif
