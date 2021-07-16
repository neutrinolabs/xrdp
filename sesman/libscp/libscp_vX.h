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
 * @file libscp_vX.h
 * @brief libscp version neutral code header
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_VX_H
#define LIBSCP_VX_H

#include "libscp_types.h"

#include "libscp_v0.h"
#include "libscp_v1s.h"

/* server API */
/**
 *
 * @brief version neutral server accept function
 * @param atrans connection trans
 * @param s session descriptor
 *
 */
enum SCP_SERVER_STATES_E
scp_vXs_accept(struct trans *atrans, struct SCP_SESSION *s);

#endif
