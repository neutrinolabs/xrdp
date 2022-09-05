/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022
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
 * @file tools_common.c
 * @brief Common definitions for the tools utilities
 * @author Matt Burt
 *
 */

#if !defined(TOOLS_COMMON_H)
#define TOOLS_COMMON_H

#include "scp.h"

struct trans;

/**************************************************************************//**
 * Waits for an expected reply from sesman
 *
 * Any other incoming messages are ignored.
 *
 * Following this call, the message can be parsed in the usual way.
 *
 * @param t SCP transport
 * @param wait_msgno Code of the message we're waiting for.
 * @result 0 for success
 */
int
wait_for_sesman_reply(struct trans *t, enum scp_msg_code wait_msgno);

#endif /* TOOLS_COMMON_H */
