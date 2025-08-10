/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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
 * @file session_x11.h
 * @brief Derived class for X11 session objects
 * @author Matt Burt
 *
 */


#ifndef SESSION_X11_H
#define SESSION_X11_H


#include "arch.h"
#include "session_base.h"

/**
 * Data involved in running an X11 session (derived class)
 */
struct session_data_x11
{
    struct session_data base;
    pid_t x_server_pid; ///< PID of X server
    pid_t win_mgr_pid; ///< PID of window manager
};

/**
 * Allocates memory for a new X11 session object
 * @return New object, or NULL for no memory.
 *
 * The vtable of the base, and all the x11-specific member variables
 * will be initialised. The caller must initialise the rest of the
 * base object.
 */
struct session_data_x11 *
session_x11_new(void);

#endif // SESSION_X11_H
