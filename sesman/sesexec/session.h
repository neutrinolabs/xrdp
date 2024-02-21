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
#include "xrdp_constants.h"

struct login_info;
struct proc_exit_status;

/**
 * Information used to start a session
 */
struct session_parameters
{
    unsigned int display;
    enum scp_session_type type;
    unsigned short width;
    unsigned short height;
    unsigned char  bpp;
    struct guid guid;
    const char *shell;  // Must not be NULL
    const char *directory;  // Must not be NULL
};


/**
 * Data involved in running a session (opaque type)
 *
 * Allocate with session_start() and free with
 * session_data_free() once session_active() returns zero.
 */
struct session_data;

/**
 *
 * @brief starts a session
 *
 * @param login_info info for logged in user
 * @param s Session parameters
 * @param[out] session_data Pointer to session data for the session
 *
 * session_data is only set if E_SCP_CREATE_OK is returned
 * @return status
 */
enum scp_screate_status
session_start(struct login_info *login_info,
              const struct session_parameters *s,
              struct session_data **session_data);

/**
 * Processes an exited child process
 *
 * The PID of the child process is removed from the session_data.
 *
 * @param sd session_data for this session
 * @param pid PID of exited process
 * @param e Exit status of the exited process
 */
void
session_process_child_exit(struct session_data *sd,
                           int pid,
                           const struct proc_exit_status *e);

/**
 * Returns a count of active processes in the session
 *
 * @param sd session_data for this session
 */
unsigned int
session_active(const struct session_data *sd);

/**
 * Returns the start time for an active session
 *
 * @param sd session_data for this session
 */
time_t
session_get_start_time(const struct session_data *sd);

/***
 * Ask a session to terminate by signalling the window manager
 *
 * @param sd session_data for this session
 */
void
session_send_term(struct session_data *sd);

/**
 * Frees a session_data object
 *
 * @param sd session_data for this session
 *
 * Do not call this until session_active() returns zero, or you
 * lose the ability to track the session PIDs
 */
void
session_data_free(struct session_data *session_data);

/**
 * Runs the reconnect script for the session
 */
void
session_reconnect(struct login_info *login_info,
                  struct session_data *sd);

#endif // SESSION_H
