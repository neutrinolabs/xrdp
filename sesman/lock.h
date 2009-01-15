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
   Copyright (C) Jay Sorg 2005-2008
*/

#ifndef LOCK_H
#define LOCK_H

#include "sesman.h"

/**
 *
 * @brief initializes all the locks
 *
 */
void APP_CC
lock_init(void);

/**
 *
 * @brief cleanup all the locks
 *
 */
void APP_CC
lock_deinit(void);

/**
 *
 * @brief acquires the lock for the session chain
 *
 */
void APP_CC
lock_chain_acquire(void);

/**
 *
 * @brief releases the session chain lock
 *
 */
void APP_CC
lock_chain_release(void);

/**
 *
 * @brief request the socket lock
 *
 */
void APP_CC
lock_socket_acquire(void);

/**
 *
 * @brief releases the socket lock
 *
 */
void APP_CC
lock_socket_release(void);

/**
 *
 * @brief request the main sync lock
 *
 */
void APP_CC
lock_sync_acquire(void);

/**
 *
 * @brief releases the main sync lock
 *
 */
void APP_CC
lock_sync_release(void);

/**
 *
 * @brief request the sync sem lock
 *
 */
void APP_CC
lock_sync_sem_acquire(void);

/**
 *
 * @brief releases the sync sem lock
 *
 */
void APP_CC
lock_sync_sem_release(void);

#endif
