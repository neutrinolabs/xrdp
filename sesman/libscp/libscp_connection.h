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
 * @brief creates a new SCP connection
 * @param host Hostname to connect to (NULL for default)
 * @param port Port to connect to (NULL for default)
 * @param term_func Transport termination function (or NULL)
 * @param data_in_func Transport 'data in' function
 * @param callback_data Closure data for data in function
 *
 * Returned object can be freed with trans_delete()
 *
 * @return a struct trans* object on success, NULL otherwise
 *
 */
struct trans *
scp_connect(const char *host, const  char *port,
            tis_term term_func,
            ttrans_data_in data_in_func,
            void *callback_data);

/**
 * @brief Maps SCP_CLIENT_TYPES_E to a string
 * @param e
 *
 * @return Pointer to a string
 *
 */
const char *scp_client_state_to_str(enum SCP_CLIENT_STATES_E e);

#endif
