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
 * Information used to start or find a session
 */
struct session_parameters
{
    int uid;
    enum scp_session_type type;
    unsigned short height;
    unsigned short width;
    unsigned char  bpp;
    const char *shell;
    const char *directory;
    const char *ip_addr;
};

/**
 *
 * @brief finds a session matching the supplied parameters
 * @return session data or 0
 *
 */
struct session_item *
session_get_bydata(const struct session_parameters *params);
#ifndef session_find_item
#define session_find_item(a) session_get_bydata(a)
#endif

/**
 *
 * @brief starts a session
 *
 * @return Connection status.
 */
enum scp_screate_status
session_start(struct auth_info *auth_info,
              const struct session_parameters *params,
              int *display,
              struct guid *guid);

int
session_reconnect(int display, int uid,
                  struct auth_info *auth_info);

/**
 *
 * @brief kills a session
 * @param pid the pid of the session to be killed
 * @return
 *
 */
enum session_kill_status
session_kill(int pid);

/**
 *
 * @brief sends sigkill to all sessions
 * @return
 *
 */
void
session_sigkill_all(void);

/**
 *
 * @brief retrieves a session's descriptor
 * @param pid the session pid
 * @return a pointer to the session descriptor on success, NULL otherwise
 *
 */
struct session_item *
session_get_bypid(int pid);

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
session_get_byuid(int uid, unsigned int *cnt, unsigned char flags);

/**
 *
 * @brief Frees the result of session_get_byuser()
 * @param sesslist Resuit of session_get_byuser()
 * @param cnt  Number of entries in sess
 */
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt);

/**
 *
 * @brief delete socket files
 * @param display number
 * @return non-zero value (number of errors) if failed
 */
int cleanup_sockets(int display);

/**
 * Clone a session_parameters structure
 *
 * @param sp Parameters to clone
 * @return Cloned parameters, or NULL if no memory
 *
 * The cloned structure can be free'd with a single call to g_free()
 */
struct session_parameters *
clone_session_params(const struct session_parameters *sp);
#endif
