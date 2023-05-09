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
 * @file pre_session_list.h
 * @brief List of pre-session connections to sesman (declarations)
 *
 * Items on this list are moved to the session list once they have
 * authenticated and a session is started.
 *
 * @author Matt Burt
 *
 */

#ifndef PRE_SESSION_LIST_H
#define PRE_SESSION_LIST_H

#include <sys/types.h>

#include "xrdp_constants.h"

/**
 * Type describing the login state of a pre-session item
 */
enum ps_login_state
{
    E_PS_LOGIN_NOT_LOGGED_IN = 0,
    E_PS_LOGIN_SYS,
    E_PS_LOGIN_UDS
};

/**
 * Action we require the dispatcher to do for us
 *
 * We can't do some things in an SCP or EICP callback, so we have to
 * ask the dispatcher to do them. For example, we can't delete the
 * client_trans as the callback stack won't be expecting this.
 */
enum pre_session_dispatcher_action
{
    E_PSD_NONE = 0,
    E_PSD_REMOVE_CLIENT_TRANS,
    E_PSD_TERMINATE_PRE_SESSION
};

/**
 * Type for managing sesman connections from SCP clients (xrdp, etc)
 * and any sesexec processes we've created for them.
 */
struct pre_session_item
{
    struct trans *client_trans; ///< SCP link to sesman client
    struct trans *sesexec_trans; ///< ECP link to sesexec
    pid_t sesexec_pid; ///< PID of sesexec (if sesexec is active)
    char peername[15 + 1]; ///< Name of peer, if known, for logging
    enum ps_login_state login_state; ///< Login state
    /**
     * Any action which a callback requires the dispatcher to
     * do out of scope of the callback */
    enum pre_session_dispatcher_action dispatcher_action;
    uid_t uid; ///< User
    char *username; ///< Username from UID (at time of logon)
    char start_ip_addr[MAX_PEER_ADDRSTRLEN];
};


/**
 * Initialise the module
 * @param list_size Number of pre-session items allowed
 * @return 0 for success
 *
 * Errors are logged
 */
int
pre_session_list_init(unsigned int list_size);

/**
 * Clean up the module on program exit
 */
void
pre_session_list_cleanup(void);

/**
 * Returns the number of items on the pre-session list
 * @return Item count
 */
unsigned int
pre_session_list_get_count(void);

/**
 * Allocates a new pre-session item on the list
 *
 * @return pointer to new pre-session object or NULL for no memory
 *
 * After allocating the session, you must initialise the sesexec_trans field
 * with a valid transport.
 *
 * The session is removed by pre_session_list_get_wait_objs() or
 * pre_session_check_wait_objs() when the client
 * transport goes down (or wasn't allocated in the first place).
 */
struct pre_session_item *
pre_session_list_new(void);

/**
 * Set the peername of a pre-session
 *
 * @param psi pre-session-item
 * @param name Name to set
 * @result 0 for success
 */
int
pre_session_list_set_peername(struct pre_session_item *psi, const char *name);

/**
 * @brief Get the wait objs for the pre-session list module
 * @param @robjs Objects array to update
 * @param robjs_count Elements in robjs (by reference)
 * @return 0 for success
 */
int
pre_session_list_get_wait_objs(tbus robjs[], int *robjs_count);


/**
 * @brief Check the wait objs for the pre-session list module
 * @return 0 for success
 */
int
pre_session_list_check_wait_objs(void);

#endif // PRE_SESSION_LIST_H
