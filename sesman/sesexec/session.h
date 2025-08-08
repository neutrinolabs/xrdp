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
 * @brief Session management function declarations
 * @author Jay Sorg, Simone Fedele
 *
 */


#ifndef SESSION_H
#define SESSION_H

#include <time.h>

#include "scp_application_types.h"

struct login_info;
struct proc_exit_status;
struct session_parameters;

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
 * Frees a session_data object
 *
 * @param self session_data object
 *
 * Do not call this until session_active() returns zero, or you
 * lose the ability to track the session PIDs
 */
void
session_data_free(struct session_data *self);

/**
 * Process a SIGCHLD event for a session
 *
 * Any pending SIGCHLD events are processed.
 *
 * @param self session_data object
 */
void
session_process_sigchld_event(struct session_data *self);

/**
 * Returns a count of active processes in the session
 *
 * @param self session_data object
 */
unsigned int
session_active(const struct session_data *self);

/**
 * Get the session start time
 *
 * @param self session_data object
 * @return session start time
 */
time_t
session_get_start_time(const struct session_data *self);

/**
 * Increment the connect count for an active session
 * @param self session_data object
 * @return Pre-increment value of the connect count
 */
unsigned int
session_increment_connect_count(struct session_data *self);

/**
 * Runs the reconnect script for the session
 * @param self session_data object
 * @param login_info Login info for the session
 * @param vars environment variables for the reconnect script
 *
 * The vars parameter points to an array of strings in pairs. The
 * first string in the pair is the name of an environment variable to
 * set, and the second string is the value
 */
void
session_run_reconnect_script(struct session_data *self,
                             const struct login_info *login_info,
                             const char *vars[]);

/**
 * Returns the parameters used to start the session
 *
 * @param self session_data object
 * @return Pointer to parameters
 *
 * The pointed-to data returned must not be modified in
 * any way.
 */
const struct session_parameters *
session_get_parameters(const struct session_data *self);

/***
 * Ask a session to terminate by signalling the window manager
 *
 * @param self session_data object
 * @param wait_for_all != 0 to wait for all processes in the session
 *                     to terminate
 */
void
session_send_term(struct session_data *self, int wait_for_all);

/**
 * Get the session display server file descriptor
 *
 * @param self session_data_object
 * @param login_info Login info for the session
 * @return display server fd, or -1
 */
int
session_get_display_server_fd(const struct session_data *self,
                              const struct login_info *login_info);

/**
 * Connects a file descriptor to chansrv
 * @param self session_data object
 * @param login_info Login info for the session
 * @return file descriptor connected to chansrv, or -1
 */
int
session_get_chansrv_fd(const struct session_data *self,
                       const struct login_info *login_info);

#endif // SESSION_H
