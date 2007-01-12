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

#ifndef LOCK_H
#define LOCK_H

#include "sesman.h"

#define SESMAN_LOCK_FORK_BLOCKER   1
#define SESMAN_LOCK_FORK_WAITING   0

/**
 *
 * @brief initializes all the locks
 *
 */
void DEFAULT_CC
lock_init();

/**
 *
 * @brief acquires the lock for the session chain
 *
 */
void DEFAULT_CC
lock_chain_acquire();

/**
 *
 * @brief releases the sessiona chain lock
 *
 */
void DEFAULT_CC
lock_chain_release();

/**
 *
 * @brief acquires config lock
 *
 */
void DEFAULT_CC
lock_cfg_acquire();

/**
 *
 * @brief releases config lock
 *
 */
void DEFAULT_CC
lock_cfg_release();

/**
 *
 * @brief request the socket lock
 *
 */
void DEFAULT_CC
lock_socket_acquire();

/**
 *
 * @brief releases the socket lock
 *
 */
void DEFAULT_CC
lock_socket_release();

/**
 *
 * @brief requires to fork a new child process
 *
 */
void DEFAULT_CC
lock_fork_request();

/**
 *
 * @brief releases a fork() request
 *
 */
void DEFAULT_CC
lock_fork_release();

/**
 *
 * @brief starts a section that is critical for forking
 *
 * starts a section that is critical for forking, that is noone can fork() 
 * while i'm in a critical section. But if someone wanted to fork we have 
 * to wait until he finishes with lock_fork_release()
 *
 * @return
 *
 */
int DEFAULT_CC
lock_fork_critical_section_start();

/**
 *
 * @brief closes the critical section
 *
 */
void DEFAULT_CC
lock_fork_critical_section_end(int blocking);

#endif
