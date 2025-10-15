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
 * @file session_base.h
 * @brief Base class for session objects
 * @author Jay Sorg, Simone Fedele
 *
 */


#ifndef SESSION_BASE_H
#define SESSION_BASE_H

#include <time.h>
#include <sys/types.h>

#include "arch.h"
#include "scp_application_types.h"

struct session_data;
struct session_parameters;
struct login_info;
struct proc_exit_status;

/**
 * Methods which can be used with a session_data item.
 */
struct session_data_vtable
{
    /**
     * Start a session
     * @param baseobj session_data object
     * @param login_info info for logged in user
     * @param s Session parameters
     * @return creation status
     */
    enum scp_screate_status
    (*start)(struct session_data *baseobj,
             struct login_info *login_info,
             const struct session_parameters *s);

    /**
     * Frees session object derived class members
     *
     * @param baseobj session_data object
     */
    void
    (*free)(struct session_data *baseobj);

    /**
     * Processes a child exit event reaped from a SIGCHLD
     *
     * @param baseobj session_data object
     * @param pid PID of exited process
     * @param e Exit status of the exited process
     */
    void
    (*process_child_exit)(struct session_data *baseobj,
                          int pid,
                          const struct proc_exit_status *e);

    /**
     * Returns a count of active processes in the session
     *
     * @param baseobj session_data object
     */
    unsigned int
    (*active_processes)(const struct session_data *baseobj);

    /**
     * Is the main session process (Window manager, whatever) still active?
     *
     * This can be true while the active_processes is > 0, if the session
     * has started to fail.
     * @param baseobj session_data object
     */
    int
    (*main_sess_proc_active)(const struct session_data *baseobj);

    /**
     * Gets the process group ID for the session, or -1
     *
     * This is the process group PID (if any) the session is started under
     * @param baseobj session_data object
     * @return process group PID, or -1
     */
    pid_t
    (*getpgid)(const struct session_data *baseobj);

    /***
     * Ask a session to terminate by signalling the window manager
     *
     * @param baseobj session_data object
     */
    void
    (*send_term)(struct session_data *baseobj);

    /**
     * Connects a file descriptor to the display server
     * @param baseobj session_data object
     * @param login_info Login info for the session
     * @return file descriptor connected to chansrv, or -1
     */
    int
    (*get_display_server_fd)(const struct session_data *baseobj,
                             const struct login_info *login_info);

};

/**
 * Data involved in running a session (base class)
 */
struct session_data
{
    const struct session_data_vtable *vtable;
    struct session_parameters *params;
    char display[SCP_DISPLAY_NAME_SIZE]; // Set by derived class start()
    pid_t chansrv_pid; // Set by derived class start()
    time_t start_time; // Set by derived class start()
    unsigned int connect_count;
};

/**
 * Creates a new session_data object (factory method)
 * @param sp Session parameters
 * @return session_data object, or NULL
 */
struct session_data *
session_base_new(const struct session_parameters *sp);

/**
 * Destroys a session_data object
 * @param self Object to destroy
 */
void
session_base_destroy(struct session_data *self);

/**
 * Starts chansrv (base class method)
 *
 * @param self session data object
 * @param login_info Login info for user
 * @param closure (unused) Value passed to session_base_fork_child.
 *
 * Use this function with session_base_fork_child to start chansrv
 *
 * This function must be called (if appropriate) from the start
 * method of the derived class
 */
void
session_base_start_chansrv(struct session_data *self,
                           const struct login_info *login_info,
                           void *closure);

/**
 * Starts a child process (base class method)
 *
 * @param self Session object
 * @param login_info Login info for user
 * @param group_pid If >= 0, group pid to run proc under
 * @param runproc Function to run in child (see e.g.
 *                session_base_start_chansrv)
 * @param closure Value passed to runproc
 *
 * @return PID of child.
 */
int
session_base_fork_child(
    struct session_data *self,
    const struct login_info *login_info,
    pid_t group_pid,
    void (*runproc)(struct session_data *self,
                    const struct login_info *,
                    void *closure),
    void *closure);

/**
 * Processes the startup wait time (base class method)
 *
 * @param self Session object
 *
 * @return != 0 if the session has failed in the startup wait time
 *
 * The startup wait time is obtained from the global configuration.
 */
int
session_base_process_startup_wait_time(struct session_data *self);

/**
 * Cleanup sockets allocated to the base class (base class method)
 *
 * Delete sockets, possibly left from a previous run
 * @param self Base object of session object
 * @return Error count
 */
int
session_base_cleanup_sockets(struct session_data *self);

/**
 * Process a SIGCHLD event for a session (base class method)
 *
 * Any pending SIGCHLD events are processed.
 *
 * @param self session_data object
 */
void
session_base_process_sigchld_event(struct session_data *self);


/***
 * Ask a session to terminate by signalling the window manager
 *
 * This is a base class method
 *
 * @param self session_data object
 * @param wait_for_all != 0 to wait for all processes in the session
 *                     to terminate
 */
void
session_base_send_term(struct session_data *self, int wait_for_all);

#endif // SESSION_BASE_H
