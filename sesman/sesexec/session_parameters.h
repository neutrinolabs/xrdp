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
 * @file session_parameters.h
 * @brief Parameters used to start a session
 * @author Jay Sorg, Simone Fedele, Matt Burt
 *
 */


#ifndef SESSION_PARAMETERS_H
#define SESSION_PARAMETERS_H

#include "guid.h"
#include "scp_application_types.h"

/**
 * Information used to start a session
 */
struct session_parameters
{
    int x11_display;  // >= for X11 only
    enum scp_session_type type;
    unsigned short width;
    unsigned short height;
    unsigned char  bpp;
    struct guid guid;
    const char *shell;  // Must not be NULL
    const char *directory;  // Must not be NULL
};

/**
 * Make a copy of a session_parameters object
 *
 * The object is allocated so it can be freed with a single call to free()
 * @param sp Object to copy
 * @return deep copy of object
 */
struct session_parameters *
copy_session_parameters(const struct session_parameters *sp);

#endif // SESSION_PARAMETERS_H
