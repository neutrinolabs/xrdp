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
 * @file scp_list.h
 * @brief List of SCP connections to sesman (declarations)
 *
 * @author Matt Burt
 *
 */

#ifndef SCP_LIST_H
#define SCP_LIST_H

#include <sys/types.h>

#include "xrdp_constants.h"

struct set_int;

/**
 * Type describing the login state of an SCP list item
 */
enum sli_login_state
{
    E_SLI_LOGIN_NOT_LOGGED_IN = 0,
    E_SLI_LOGIN_SYS,
    E_SLI_LOGIN_UDS
};

/**
 * Action we require the dispatcher to do for us
 *
 * We can't do some things in an SCP or EICP callback, so we have to
 * ask the dispatcher to do them. For example, we can't delete the
 * client_trans as the callback stack won't be expecting this.
 */
enum scp_list_dispatcher_action
{
    E_SLD_NONE = 0,
    /**
     * Remove the client transport as sesexec is
     * temporarily handling the call.
     */
    E_SLD_REMOVE_CLIENT_TRANS,
    /**
     * Completely remove the client transport as we won't
     * be using it again
     */
    E_SLD_TERMINATE_SCP_CONN
};

/**
 * Type for managing sesman connections from SCP clients (xrdp, etc)
 * and any sesexec processes we've created for them.
 */
struct scp_list_item
{
    struct trans *client_trans; ///< SCP link to sesman client
    struct trans *sesexec_trans; ///< ECP link to sesexec
    pid_t sesexec_pid; ///< PID of sesexec (if sesexec is active)
    char peername[15 + 1]; ///< Name of peer, if known, for logging
    enum sli_login_state login_state; ///< Login state
    /**
     * Any action which a callback requires the dispatcher to
     * do out of scope of the callback */
    enum scp_list_dispatcher_action dispatcher_action;
    uid_t uid; ///< User
    char *username; ///< Username from UID (at time of logon)
    char start_ip_addr[MAX_PEER_ADDRSTRLEN];
    int is_admin;
    int create_session_in_progress; ///< Already handling a create_session
    /// Display allocated for session (-1 if N/A)
    int session_x11_display;
};


/**
 * Initialise the module
 * @param list_size Number of SCP list items allowed
 * @return 0 for success
 *
 * Errors are logged
 */
int
scp_list_init(unsigned int list_size);

/**
 * Clean up the module on program exit
 */
void
scp_list_cleanup(void);

/**
 * Returns the number of items on the SCP list
 * @return Item count
 */
unsigned int
scp_list_get_count(void);

/**
 * Allocates a new item on the SCP list
 *
 * @return pointer to new SCP list item or NULL for no memory
 *
 * After allocating the item, you must initialise the sesexec_trans field
 * with a valid transport.
 *
 * The session is removed by scp_list_get_wait_objs() or
 * scp_list_check_wait_objs() when the client
 * transport goes down (or wasn't allocated in the first place).
 */
struct scp_list_item *
scp_list_item_new(void);

/**
 * Set the peername of an SCP list item
 *
 * @param sli SCP list item
 * @param name Name to set
 * @result 0 for success
 */
int
scp_list_set_peername(struct scp_list_item *sli, const char *name);

/**
 * @brief Get the wait objs for the SCP list module
 * @param @robjs Objects array to update
 * @param robjs_count Elements in robjs (by reference)
 * @return 0 for success
 */
int
scp_list_get_wait_objs(tbus robjs[], int *robjs_count);


/**
 * @brief Check the wait objs for the SCP list module
 * @return 0 for success
 */
int
scp_list_check_wait_objs(void);

/**
 * @brief Get all create-session X11 displays
 *
 * Adds X11 displays allocated to create-session operations to a set
 */
void
scp_list_get_create_session_x11_displays(struct set_int *alloc_displays);

#endif // SCP_LIST_H
