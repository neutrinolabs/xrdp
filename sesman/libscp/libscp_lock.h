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

#ifndef LIBSCP_LOCK_H
#define LIBSCP_LOCK_H

#include "libscp_types.h"

#define LIBSCP_LOCK_FORK_BLOCKER   1
#define LIBSCP_LOCK_FORK_WAITING   0

/**
 *
 * @brief initializes all the locks
 *
 */
void
scp_lock_init(void);

/**
 *
 * @brief requires to fork a new child process
 *
 */
void
scp_lock_fork_request(void);

/**
 *
 * @brief releases a fork() request
 *
 */
void
scp_lock_fork_release(void);

/**
 *
 * @brief starts a section that is critical for forking
 *
 * starts a section that is critical for forking, that is no one can fork()
 * while I'm in a critical section. But if someone wanted to fork we have
 * to wait until he finishes with lock_fork_release()
 *
 * @return
 *
 */
int
scp_lock_fork_critical_section_start(void);

/**
 *
 * @brief closes the critical section
 *
 */
void
scp_lock_fork_critical_section_end(int blocking);

#endif
