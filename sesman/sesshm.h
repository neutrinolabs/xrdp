/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
 * Copyright (C) Ben Cohen 2017
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
 * @brief Session discovery and shared memory definitions
 * @author Jay Sorg, Simone Fedele, Ben Cohen
 *
 */


#ifndef SESSHM_H
#define SESSHM_H

#include "session.h"

//#define DONT_USE_SHM
//#define DEBUG_SESSION_LOCK

// XXX No reason why this needs to be a constant
#define SESMAN_SHAREDMEM_MAX_SESSIONS       64
#define SESMAN_SHAREDMEM_FORMAT_VERSION     1
#define SESMAN_SHAREDMEM_HEARTBEAT_INTERVAL 15 /* seconds */
#define SESMAN_SHAREDMEM_HEARTBEAT_TIMEOUT  (60 * 3) /* seconds */
#define SESMAN_SHAREDMEM_FILENAME           "/var/run/xrdp-sesman.shm"
#define SESMAN_SHAREDMEM_TAG                "XRDP-SESMAN-SHM"
#define SESMAN_SHAREDMEM_TAG_LENGTH         16
#define SESMAN_SHAREDMEM_LENGTH             sizeof(struct session_shared_data)

struct session_shared_data
{
  char                         tag[SESMAN_SHAREDMEM_TAG_LENGTH];
  int                          data_format_version;
  pthread_mutex_t              mutex;
  int                          daemon_pid;
  int                          max_sessions;
  struct session_item          sessions[SESMAN_SHAREDMEM_MAX_SESSIONS];
};

/**
 *
 * @brief initialises the session shared data
 * @return 0 on success, nonzero otherwise
 *
 */
int
session_init_shared(void);

/**
 *
 * @brief releases the session shared data
 * @return 0 on success, nonzero otherwise
 *
 */
int
session_close_shared();

/**
 *
 * @brief allocate a session and mark it as used
 * @return the session_item allocated
 *
 */
struct session_item *
alloc_session_item();

/**
 *
 * @brief deallocate a session and mark it as unused
 * @param item the session_item to deallocate
 *
 */
void
free_session_item(struct session_item *item);

/**
 *
 * @brief obtain lock on the mutex in the shared mapping
 * @return 0 on success; non-zero otherwise
 *
 */
#ifdef DEBUG_SESSION_LOCK
int
sesshm_lock_dbg(const char *caller_func, int caller_line);
#else
int
sesshm_lock();
#endif

/**
 *
 * @brief release lock on the mutex in the shared mapping
 * @return 0 on success; non-zero otherwise
 *
 */
#ifdef DEBUG_SESSION_LOCK
int
sesshm_unlock_dbg(const char *caller_func, int caller_line);
#else
int
sesshm_unlock();
#endif

#ifdef DEBUG_SESSION_LOCK
#define sesshm_lock()        sesshm_lock_dbg(__func__, __LINE__)
#define sesshm_unlock()      sesshm_unlock_dbg(__func__, __LINE__)
#endif

#endif
