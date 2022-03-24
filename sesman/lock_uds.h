/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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
 * @file lock_uds.h
 * @brief Providing a locking facility for Unix Domain Sockets
 * @author Matt Burt
 *
 * It is difficult for a server to determine whether a socket it wishes
 * to listen on is already active or not. The purpose of this module is to
 * provide a locking facility which can be used as a proxy for this.
 */

#ifndef LOCK_UDS_H
#define LOCK_UDS_H

struct lock_uds;

/**
 * Take out a lock for the specified Unix Domain socket
 * @param sockname Name of socket. Must start with a '/'
 * @return A struct lock_uds instance if the lock was successfully acquired.
 *
 * A NULL return may indicate a lack of memory, or that another
 * process has the lock.
 *
 * A file is created in the same directory as the socket. The name
 * of the file is ".${basename}.lock", where basename is the base name
 * of the socket. THE file is removed when the lock is relinquished. */
struct lock_uds *
lock_uds(const char *sockname);

/**
 * Relinquish a lock on the specified Unix Domain Socket.
 * @param lock to relinquish
 *
 * If the process which has originally taken out the lock forks, this
 * routine should be called from the child process to prevent the lock
 * inadvertently being passed to the child. */
void
unlock_uds(struct lock_uds *lock);

#endif // LOCK_UDS_H
