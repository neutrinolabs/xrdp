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
 * @file thread.h
 * @brief thread stuff...
 * @author Simone Fedele
 *
 */

#ifndef THREAD_H
#define THREAD_H

/**
 *
 * @brief Starts the signal handling thread
 * @retval 0 on success
 * @retval 1 on error
 *
 */
int DEFAULT_CC
thread_sighandler_start(void);

/**
 *
 * @brief Starts the session update thread
 *
 */
int DEFAULT_CC
thread_session_update_start(void);

/**
 *
 * @brief Starts a thread to handle an incoming connection
 *
 */
int DEFAULT_CC
thread_scp_start(int skt);

#endif
