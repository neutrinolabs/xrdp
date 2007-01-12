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
 * @file sig.h
 * @brief Signal handling function declarations
 * @author Jay Sorg, Simone Fedele
 *
 */

#ifndef SIG_H
#define SIG_H

/**
 *
 * @brief Shutdown signal code
 * @param sig The received signal
 *
 */
void DEFAULT_CC
sig_sesman_shutdown(int sig);

/**
 *
 * @brief SIGHUP handling code
 * @param sig The received signal
 *
 */
void DEFAULT_CC
sig_sesman_reload_cfg(int sig);

/**
 *
 * @brief SIGCHLD handling code
 * @param sig The received signal
 *
 */
void DEFAULT_CC
sig_sesman_session_end(int sig);

/**
 *
 * @brief signal handling thread
 *
 */
void* DEFAULT_CC
sig_handler_thread(void* arg);

#endif

