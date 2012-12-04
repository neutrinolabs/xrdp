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
 * @file libscp_connection.h
 * @brief SCP_CONNECTION handling code
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_CONNECTION_H
#define LIBSCP_CONNECTION_H

#include "libscp.h"

/**
 *
 * @brief creates a new connection
 * @param sck the connection socket
 *
 * @return a struct SCP_CONNECTION* object on success, NULL otherwise
 *
 */
struct SCP_CONNECTION*
scp_connection_create(int sck);

/**
 *
 * @brief destroys a struct SCP_CONNECTION* object
 * @param c the object to be destroyed
 *
 */
void
scp_connection_destroy(struct SCP_CONNECTION* c);

#endif
