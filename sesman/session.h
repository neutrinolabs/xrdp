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
 * @file session.h
 * @brief Session management definitions
 * @author Jay Sorg, Simone Fedele
 *
 */


#ifndef SESSION_H
#define SESSION_H

#include <time.h>

#include "guid.h"
#include "scp_application_types.h"

struct auth_info;

/**
 * Information used to start a session
 */
struct session_parameters
{
    unsigned int display;
    int uid;
    struct guid guid;
    enum scp_session_type type;
    unsigned short height;
    unsigned short width;
    unsigned char  bpp;
    const char *shell;
    const char *directory;
};

/**
 *
 * @brief starts a session
 *
 * This call does not return.
 *
 * @return Connection status.
 */
void
session_start(struct auth_info *auth_info,
              const struct session_parameters *s);

int
session_reconnect(int display, int uid,
                  struct auth_info *auth_info);

#endif // SESSION_H
