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
 * @file libscp_types_mng.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_TYPES_MNG_H
#define LIBSCP_TYPES_MNG_H

#include "os_calls.h"
#include "parse.h"
#include "arch.h"
#include "log.h"

enum SCP_MNG_COMMAND
{
    SCP_MNG_CMD_KILL,
    SCP_MNG_CMD_DISCONNECT
};

struct SCP_MNG_DATA
{
    enum SCP_MNG_COMMAND cmd;
    SCP_SID sid;
};

#endif
