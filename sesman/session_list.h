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
 * @file session_list.h
 * @brief Session list management definitions
 * @author Jay Sorg, Simone Fedele
 *
 */


#ifndef SESSION_LIST_H
#define SESSION_LIST_H

#include <time.h>

#include "guid.h"
#include "scp_application_types.h"
#include "xrdp_constants.h"

struct session_parameters;

#define SESMAN_SESSION_STATUS_ACTIVE        0x01
#define SESMAN_SESSION_STATUS_IDLE          0x02
#define SESMAN_SESSION_STATUS_DISCONNECTED  0x04
/* future expansion
#define SESMAN_SESSION_STATUS_REMCONTROL    0x08
*/
#define SESMAN_SESSION_STATUS_ALL           0xFF

enum session_kill_status
{
    SESMAN_SESSION_KILL_OK = 0,
    SESMAN_SESSION_KILL_NULLITEM,
    SESMAN_SESSION_KILL_NOTFOUND
};

struct scp_session_info;

/**
 * Object describing a session
 */
struct session_item
{
    int uid; /* UID of session */
    int pid; /* pid of sesman waiting for wm to end */
    int display;
    int width;
    int height;
    int bpp;
    struct auth_info *auth_info;

    /* status info */
    unsigned char status;
    enum scp_session_type type;

    /* time data  */
    time_t start_time;
    // struct session_date disconnect_time; // Currently unused
    // struct session_date idle_time; // Currently unused
    char start_ip_addr[MAX_PEER_ADDRSTRLEN];
    struct guid guid;
};

struct session_chain
{
    struct session_chain *next;
    struct session_item *item;
};

/**
 * Returns the number of sessions currently active
 * @return Session count
 */
unsigned int
session_list_get_count(void);

/**
 * Adds a new session item to the chain
 */
void
session_list_add(struct session_chain *element);

/**
 * Get the next available display
 */
int
session_list_get_available_display(void);


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
 *
 * @brief kills a session
 * @param pid the pid of the session to be killed
 * @return
 *
 */
enum session_kill_status
session_list_kill(int pid);

/**
 *
 * @brief sends sigkill to all sessions
 * @return
 *
 */
void
session_list_sigkill_all(void);

/**
 *
 * @brief retrieves a session's descriptor
 * @param pid the session pid
 * @return a pointer to the session descriptor on success, NULL otherwise
 *
 */
struct session_item *
session_list_get_bypid(int pid);

/**
 *
 * @brief retrieves session descriptions
 * @param UID the UID for the descriptions
 * @return A block of session descriptions
 *
 * Pass the return result to free_session_info_list() after use
 *
 */
struct scp_session_info *
session_list_get_byuid(int uid, unsigned int *cnt, unsigned char flags);

/**
 *
 * @brief Frees the result of session_get_byuser()
 * @param sesslist Resuit of session_get_byuser()
 * @param cnt  Number of entries in sess
 */
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt);

#endif // SESSION_LIST_H
