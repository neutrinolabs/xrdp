/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file scp_v0.h
 * @brief scp version 0 declarations
 * @author Simone Fedele
 *
 */

#ifndef SCP_V0_H
#define SCP_V0_H

#include "libscp.h"

enum SCP_SERVER_STATES_E
scp_v0_process(struct trans *t, struct SCP_SESSION *s);

#endif
