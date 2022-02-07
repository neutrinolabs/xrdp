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

#include "libscp_types.h"

#define SESMAN_SESSION_TYPE_XRDP      1
#define SESMAN_SESSION_TYPE_XVNC      2
#define SESMAN_SESSION_TYPE_XORG      3

#define SESMAN_SESSION_STATUS_ACTIVE        0x01
#define SESMAN_SESSION_STATUS_IDLE          0x02
#define SESMAN_SESSION_STATUS_DISCONNECTED  0x04
/* future expansion
#define SESMAN_SESSION_STATUS_REMCONTROL    0x08
*/
#define SESMAN_SESSION_STATUS_ALL           0xFF

#define SESMAN_SESSION_KILL_OK        0
#define SESMAN_SESSION_KILL_NULLITEM  1
#define SESMAN_SESSION_KILL_NOTFOUND  2

struct session_date
{
    tui16 year;
    tui8  month;
    tui8  day;
    tui8  hour;
    tui8  minute;
};

#define zero_time(s) { (s)->year=0; (s)->month=0; (s)->day=0; (s)->hour=0; (s)->minute=0; }

struct session_item
{
    char name[256];
    int pid; /* pid of sesman waiting for wm to end */
    int display;
    int width;
    int height;
    int bpp;
    long data;

    /* status info */
    unsigned char status;
    unsigned char type;

    /* time data  */
    struct session_date connect_time;
    struct session_date disconnect_time;
    struct session_date idle_time;
    char client_ip[256];
    tui8 guid[16];
};

struct session_chain
{
    struct session_chain *next;
    struct session_item *item;
};

/**
 *
 * @brief finds a session matching the supplied parameters
 * @return session data or 0
 *
 */
struct session_item *
session_get_bydata(const char *name, int width, int height, int bpp, int type,
                   const char *client_ip);
#ifndef session_find_item
#define session_find_item(a, b, c, d, e, f) session_get_bydata(a, b, c, d, e, f);
#endif

/**
 *
 * @brief starts a session
 * @return 0 on error, display number if success
 *
 */
int
session_start(long data, tui8 type, struct SCP_SESSION *s);

int
session_reconnect(int display, char *username, long data);

/**
 *
 * @brief kills a session
 * @param pid the pid of the session to be killed
 * @return
 *
 */
int
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
 * @brief retrieves a session's descriptor
 * @param pid the session pid
 * @return a pointer to the session descriptor on success, NULL otherwise
 *
 */
struct SCP_DISCONNECTED_SESSION *
session_get_byuser(const char *user, int *cnt, unsigned char flags);

/**
 *
 * @brief delete socket files
 * @param display number
 * @return non-zero value (number of errors) if failed
 */
int cleanup_sockets(int display);
#endif
