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
   Copyright (C) Jay Sorg 2005-2009
*/

/**
 *
 * @file libscp_session.h
 * @brief SCP_SESSION handling code
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_SESSION_H
#define LIBSCP_SESSION_H

#include "libscp.h"

/**
 *
 * @brief creates a new connection
 * @param sck the connection socket
 *
 * @return a struct SCP_SESSION* object on success, NULL otherwise
 *
 */
struct SCP_SESSION*
scp_session_create();

int
scp_session_set_type(struct SCP_SESSION* s, tui8 type);

int
scp_session_set_version(struct SCP_SESSION* s, tui32 version);

int
scp_session_set_height(struct SCP_SESSION* s, tui16 h);

int
scp_session_set_width(struct SCP_SESSION* s, tui16 w);

int
scp_session_set_bpp(struct SCP_SESSION* s, tui8 bpp);

int
scp_session_set_rsr(struct SCP_SESSION* s, tui8 rsr);

int
scp_session_set_locale(struct SCP_SESSION* s, char* str);

int
scp_session_set_username(struct SCP_SESSION* s, char* str);

int
scp_session_set_password(struct SCP_SESSION* s, char* str);

int
scp_session_set_domain(struct SCP_SESSION* s, char* str);

int
scp_session_set_program(struct SCP_SESSION* s, char* str);

int
scp_session_set_directory(struct SCP_SESSION* s, char* str);

int
scp_session_set_hostname(struct SCP_SESSION* s, char* str);

int
scp_session_set_addr(struct SCP_SESSION* s, int type, void* addr);

int
scp_session_set_display(struct SCP_SESSION* s, SCP_DISPLAY display);

int
scp_session_set_errstr(struct SCP_SESSION* s, char* str);

/**
 *
 * @brief destroys a session object
 * @param s the object to be destroyed
 *
 */
void
scp_session_destroy(struct SCP_SESSION* s);

#endif
