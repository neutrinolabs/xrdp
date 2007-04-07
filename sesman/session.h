/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2007
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

#define SESMAN_SESSION_TYPE_XRDP  1
#define SESMAN_SESSION_TYPE_XVNC  2

#define SESMAN_SESSION_STATUS_ACTIVE        1
#define SESMAN_SESSION_STATUS_IDLE          2
#define SESMAN_SESSION_STATUS_DISCONNECTED  3
/* future expansion
#define SESMAN_SESSION_STATUS_REMCONTROL    4
*/

#define SESMAN_SESSION_KILL_OK        0
#define SESMAN_SESSION_KILL_NULLITEM  1
#define SESMAN_SESSION_KILL_NOTFOUND  2

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
  int connect_time;
  int disconnect_time;
  int idle_time;
};

struct session_chain
{
  struct session_chain* next;
  struct session_item* item;
};

/**
 *
 * @brief finds a session matching the supplied parameters
 * @return session data or 0
 *
 */
struct session_item* DEFAULT_CC
session_get_bydata(char* name, int width, int height, int bpp);
#ifndef session_find_item
  #define session_find_item(a, b, c, d) session_get_bydata(a, b, c, d);
#endif

/**
 *
 * @brief starts a session
 * @return 0 on error, display number if success
 *
 */
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data, unsigned char type);

/**
 *
 * @brief kills a session
 * @param pid the pid of the session to be killed
 * @return
 *
 */
int DEFAULT_CC
session_kill(int pid);

/**
 *
 * @brief sends sigkill to all sessions
 * @return
 *
 */
void DEFAULT_CC
session_sigkill_all();

/**
 *
 * @brief retrieves a session's descriptor
 * @param pid the session pid
 * @return a pointer to the session descriptor on success, NULL otherwise
 *
 */
struct session_item* DEFAULT_CC
session_get_bypid(int pid);

/**
 *
 * @brief retrieves a session's descriptor
 * @param pid the session pid
 * @return a pointer to the session descriptor on success, NULL otherwise
 *
 */
struct SCP_DISCONNECTED_SESSION*
session_get_byuser(char* user, int* cnt);

#endif

