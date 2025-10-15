/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2023
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
 * @file session_list.h
 * @brief Session list management definitions
 * @author Jay Sorg, Simone Fedele
 *
 */


#ifndef SESSION_LIST_H
#define SESSION_LIST_H

#include <sys/types.h>

#include "guid.h"
#include "scp_application_types.h"
#include "xrdp_constants.h"

struct set_int;

enum session_state
{
    /**
     * Session definition is little more than a sesexec process. We're
     * waiting for more details of the session from sesexec */
    E_SESSION_STARTING,
    /** Session is fully active */
    E_SESSION_RUNNING
};

/**
 * Object describing a session
 *
 * Unless otherwide noted, fields are only valid if
 * the status is E_SESSION_RUNNING
 */
struct session_item
{
    enum session_state state;
    struct trans *sesexec_trans; // trans for sesexec process. Always valid.
    pid_t sesexec_pid; // pid for sesexec process. Always valid
    /**
     * May be valid if known when the session is starting, otherwise "" */
    char display[SCP_DISPLAY_NAME_SIZE];
    uid_t uid;
    enum scp_session_type type;
    unsigned short start_width;
    unsigned short start_height;
    unsigned char bpp;
    struct guid guid;
    char start_ip_addr[MAX_PEER_ADDRSTRLEN];
    time_t start_time;
    char client_ip[MAX_PEER_ADDRSTRLEN];
    char client_name[INFO_CLIENT_NAME_BYTES_UTF8];
    time_t last_connect_disconnect;
};

/**
 * Initialise the module
 * @return 0 for success
 *
 * Errors are logged
 */
int
session_list_init(void);

/**
 * Clean up the module on program exit
 */
void
session_list_cleanup(void);

/**
 * Returns the number of sessions currently active
 * @return Session count
 */
unsigned int
session_list_get_count(void);

/**
 * @brief Get the number of sessions in a particular state
 * @param state to count
 * @return session count
 */
unsigned int
session_list_get_count_by_state(enum session_state state);

/**
 * Allocates a new session on the list
 *
 * state will be E_SESSION_STARTING. Other data must be filled in by
 * the caller as appropriate.
 *
 * @return pointer to new session object or NULL for no memory
 *
 * After allocating the session, you must initialise the sesexec_trans field
 * with a valid transport.
 *
 * The session is removed by session_check_wait_objs() when the transport
 * goes down (or wasn't allocated in the first place).
 */
struct session_item *
session_list_new(void);

/**
 * @brief Get all session displays
 *
 * Adds X11 displays allocated to sessions to a set
 */
void
session_list_get_session_x11_displays(struct set_int *alloc_displays);

/**
 *
 * @brief finds a session matching the supplied parameters
 * @return session data or 0
 *
 */
struct session_item *
session_list_get_bydata(uid_t uid,
                        enum scp_session_type type,
                        unsigned short width,
                        unsigned short height,
                        unsigned char  bpp,
                        const char *ip_addr);

/**
 * @brief retrieves session descriptions
 * @param uid the UID for the descriptions by reference, or NULL for
 *            all UIDs
 * @param[out] cnt The number of sessions returned
 * @param flags Future expansion
 * @return A block of session descriptions
 *
 * Pass the return result to free_session_info_list() after use
 *
 */
struct scp_session_info *
session_list_get_byuid(const uid_t *uid, unsigned int *cnt, unsigned int flags);

/**
 * @brief retrieves a session by GUID
 * @param guid GUID of session
 * @return Pointer to session, or NULL.
 *
 * The caller is responsible for checking the client has
 * permissions to access the session.
 */
struct session_item *
session_list_get_byguid(const struct guid *guid);

/**
 *
 * @brief Frees the result of session_get_byuser()
 * @param sesslist Resuit of session_get_byuser()
 * @param cnt  Number of entries in sess
 */
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt);

/**
 * @brief Get the wait objs for the session list module
 * @param @robjs Objects array to update
 * @param robjs_count Elements in robjs (by reference)
 * @return 0 for success
 */
int
session_list_get_wait_objs(tbus robjs[], int *robjs_count);


/**
 * @brief Check the wait objs for the session list module
 * @return 0 for success
 */
int
session_list_check_wait_objs(void);

#endif // SESSION_LIST_H
