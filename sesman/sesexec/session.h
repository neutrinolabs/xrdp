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

struct login_info;
struct proc_exit_status;

/**
 * Information used to start a session
 */
struct session_parameters
{
    int x11_display;   // >= 0 for X11 only
    enum scp_session_type type;
    unsigned short width;
    unsigned short height;
    unsigned char  bpp;
    unsigned short dpi;  // Client monitor DPI, or 0 for none (Xorg only)
    struct guid guid;
    const char *shell;  // Must not be NULL
    const char *directory;  // Must not be NULL
    const char *instance_name;  //Must not be NULL
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
 * Processes a SIGCHLD event
 *
 * Any pending SIGCHLD events are processed.
 *
 * The PID of a failed child process is removed from the session_data.
 *
 * @param sd session_data for this session
 */
void
session_process_sigchld_event(struct session_data *sd);

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
 * @return session start time
 */
time_t
session_get_start_time(const struct session_data *sd);

/**
 * Returns the connect count for an active session
 * @param sd session_data for this session
 * @return connect count
 */
unsigned int
session_get_connect_count(const struct session_data *sd);

/**
 * Get a pointer to the session display name
 *
 * @param self session_data object
 * @return session name ("X11-n" or "wayland-n")
 *
 * A session name is set by a successful call to session_start()
 */
const char *
session_get_display(const struct session_data *sd);

/**
 * Increment the connect count for an active session
 * @param sd session_data for this session
 * @return Pre-increment value of the connect count
 */
unsigned int
session_increment_connect_count(struct session_data *sd);

/**
 * Returns the parameters used to start the session
 *
 * @param sd session_data for this session
 * @return Pointer to parameters
 *
 * The pointed-to data returned must not be modified in
 * any way.
 */
const struct session_parameters *
session_get_parameters(const struct session_data *sd);

/***
 * Ask a session to terminate by signalling the window manager
 *
 * @param sd session_data for this session
 * @param wait_for_all != 0 to wait for all processes in the session
 *                     to terminate
 */
void
session_send_term(struct session_data *sd, int wait_for_all);

/**
 * Frees a session_data object
 *
 * @param session_data session_data for this session
 *
 * Do not call this until session_active() returns zero, or you
 * lose the ability to track the session PIDs
 */
void
session_data_free(struct session_data *session_data);

/**
 * Runs the reconnect script for the session
 * @param login_info Login info for the session
 * @param sd Session data for the session
 * @param vars environment variables for the reconnect script
 *
 * The vars parameter points to an array of strings in pairs. The
 * first string in the pair is the name of an environment variable to
 * set, and the second string is the value
 */
void
session_run_reconnect_script(const struct login_info *login_info,
                             const struct session_data *sd,
                             const char *vars[]);

/**
 * Connects a file descriptor to the display server
 */
int
session_get_display_server_fd(const struct login_info *login_info,
                              const struct session_data *sd);

/**
 * Connects a file descriptor to chansrv
 */
int
session_get_chansrv_fd(const struct login_info *login_info,
                       const struct session_data *sd);

#endif // SESSION_H
