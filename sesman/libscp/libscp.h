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
 * @file libscp.h
 * @brief libscp main header
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_H
#define LIBSCP_H

#include "libscp_types.h"
#include "libscp_commands.h"

#include "libscp_connection.h"
#include "libscp_session.h"
#include "libscp_init.h"
#include "libscp_tcp.h"
#include "libscp_lock.h"

#include "libscp_vX.h"
#include "libscp_v0.h"
#include "libscp_v1s.h"
#include "libscp_v1c.h"
#include "libscp_v1s_mng.h"
#include "libscp_v1c_mng.h"

#endif
