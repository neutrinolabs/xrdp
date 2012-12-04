/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
