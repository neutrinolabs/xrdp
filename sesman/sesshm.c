/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
 * Copyright (C) Ben Cohen 2017
 *
 * BSD process grouping by:
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland.
 * Copyright (c) 2000-2001 Markus Friedl.
 * Copyright (c) 2011-2015 Koichiro Iwao, Kyushu Institute of Technology.
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
 * @file session.c
 * @brief Session discovery and shared memory code
 * @author Jay Sorg, Simone Fedele, Ben Cohen
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "sesshm.h"
#include "sesman.h"
#include "libscp_types.h"
#include "xauth.h"
#include "xrdp_sockets.h"
#include "thread_calls.h"

#ifdef DEBUG_SESSION_LOCK
#define sesshm_try_lock()    sesshm_try_lock_dbg(__func__, __LINE__)
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef DONT_USE_SHM
static int g_sesshm_thread_going = 1;
static struct session_shared_data *g_shm_mapping;
#endif

extern struct session_chain *g_sessions;
extern int g_session_count;
extern int g_pid;         /* in sesman.c */


/******************************************************************************/
/**
 *
 * @brief obtain lock on the mutex in the shared mapping
 * @return 0 on success; non-zero otherwise
 *
 */
#ifdef DEBUG_SESSION_LOCK
int
sesshm_lock_dbg(const char *caller_func, int caller_line)
#else
int
sesshm_lock()
#endif
{
#ifdef DONT_USE_SHM
    return 0;
#else
    int rc;

#ifdef DEBUG_SESSION_LOCK
    log_message(LOG_LEVEL_DEBUG,
                "pid %d tid %d: %s() called from %s at line %d",
                g_getpid(), (int)tc_get_threadid(), __func__,
                caller_func, caller_line);
#endif

    rc = tc_mutex_lock((tbus)&g_shm_mapping->mutex);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR,
                    "tc_mutex_lock() failed (%d)", g_get_errno());
    }
    return rc;
#endif
}


/******************************************************************************/
/**
 *
 * @brief try for up to one second to obtain lock on the mutex in the shared mapping
 * @return 0 on success; non-zero otherwise
 *
 */
#ifndef DONT_USE_SHM
#ifdef DEBUG_SESSION_LOCK
static int
sesshm_try_lock_dbg(const char *caller_func, int caller_line)
#else
static int
sesshm_try_lock()
#endif
{
    int rc;

#ifdef DEBUG_SESSION_LOCK
    log_message(LOG_LEVEL_DEBUG,
                "pid %d tid %d: %s() called from %s at line %d",
                g_getpid(), (int)tc_get_threadid(), __func__,
                caller_func, caller_line);
#endif

    rc = tc_mutex_timed_lock((tbus)&g_shm_mapping->mutex, 1 * 1000);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR,
                    "tc_mutex_timed_lock() failed (%d)", g_get_errno());
    }
    return rc;
}
#endif


/******************************************************************************/
/**
 *
 * @brief release lock on the mutex in the shared mapping
 * @return 0 on success; non-zero otherwise
 *
 */
#ifdef DEBUG_SESSION_LOCK
int
sesshm_unlock_dbg(const char *caller_func, int caller_line)
#else
int
sesshm_unlock()
#endif
{
#ifdef DONT_USE_SHM
    return 0;
#else
    int rc;

#ifdef DEBUG_SESSION_LOCK
    log_message(LOG_LEVEL_DEBUG,
                "pid %d tid %d: %s() called from %s at line %d",
                g_getpid(), (int)tc_get_threadid(), __func__,
                caller_func, caller_line);
#endif

    rc = tc_mutex_unlock((tbus) &g_shm_mapping->mutex);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR,
                    "tc_mutex_unlock() failed (%d)", g_get_errno());
    }
    return rc;
#endif
}


/******************************************************************************/
/**
 *
 * @brief thread to poll the shared memory for various changes
 * @return NULL
 *
 */
#ifndef DONT_USE_SHM
static void *
sesshm_thread(void *arg)
{
    int pid;

    pid = g_getpid();
    while (g_sesshm_thread_going)
    {
        g_sleep_secs(SESMAN_SHAREDMEM_HEARTBEAT_INTERVAL);

        if (g_pid == pid)
        {
            int current_time;
            struct session_chain *tmp;
            struct session_chain *prev;

            /* Daemon process
             * Check that the file hasn't been hijacked by a new daemon
             * process and that the heartbeat hasn't timed out for the
             * sessions. */
            sesshm_lock();
            if (g_pid != g_shm_mapping->daemon_pid)
            {
                log_message(LOG_LEVEL_ERROR,
                            "new daemon pid %d entered in shm!  quitting (%d)",
                            g_shm_mapping->daemon_pid, g_pid);
                exit(1);
            }

            current_time = g_time1();
            tmp = g_sessions;
            prev = 0;
            while (tmp != 0)
            {
                if (tmp->item == 0)
                {
                    log_message(LOG_LEVEL_ERROR, "found null session "
                                "descriptor!");
                }
                else if (current_time - tmp->item->shm_heartbeat_time
                         > SESMAN_SHAREDMEM_HEARTBEAT_TIMEOUT)
                {
                    /* deleting the session */
                    log_message(LOG_LEVEL_INFO, "++ terminated session (heartbeat timed out):  username %s, display :%d.0, session_pid %d, ip %s", tmp->item->name, tmp->item->display, tmp->item->pid, tmp->item->client_ip);

                    tmp->item->shm_is_allocated = 0;

                    if (prev == 0)
                    {
                        /* prev does no exist, so it's the first element - so we set
                           g_sessions */
                        g_sessions = tmp->next;
                    }
                    else
                    {
                        prev->next = tmp->next;
                    }

                    g_free(tmp);
                    g_session_count--;
                }

                /* go on */
                prev = tmp;
                tmp = tmp->next;
            }
            sesshm_unlock();
        }
        else
        {
            /* Session process */
            sesshm_lock();
            /* Check that this process hadn't been timed out or otherwise
             * told to exit */
            int okay = 0;
            int i;
            for (i = 0; i < g_shm_mapping->max_sessions; i ++)
            {
                if (g_shm_mapping->sessions[i].shm_is_allocated
                    && pid == g_shm_mapping->sessions[i].pid)
                {
                    if (g_shm_mapping->sessions[i].shm_is_allocated)
                    {
                        okay = 1;

                        /* Update the heartbeat time */
                        g_shm_mapping->sessions[i].shm_heartbeat_time = g_time1();
                    }
                }
            }
            if (!okay)
            {
                log_message(LOG_LEVEL_INFO, "++ killed session (deallocated in shm):  session_pid %d", pid);
                sesshm_unlock();

                // TODO XXX Kill the child X server and/or window manager

                exit(1);
            }
            sesshm_unlock();
        }
    }

    return NULL;
}
#endif


/******************************************************************************/
/**
 *
 * @brief create a new session_shared_data shared memory file
 * @return 0 on success, nonzero otherwise
 *
 */
#ifndef DONT_USE_SHM
static int
sesshm_create_new_shm()
{
    int rc;
    int i;
    int fd;

    log_message(LOG_LEVEL_INFO, "Creating shm file %s",
                SESMAN_SHAREDMEM_FILENAME);

    g_file_delete(SESMAN_SHAREDMEM_FILENAME);
    fd = g_file_open_ex(SESMAN_SHAREDMEM_FILENAME, 1, 1, 1, 0);
    if (fd == -1)
    {
        log_message(LOG_LEVEL_ERROR, "open() failed (%d)", g_get_errno());
        return 1;
    }
    rc = g_ftruncate(fd, SESMAN_SHAREDMEM_LENGTH);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR, "truncate() failed (%d)", g_get_errno());
        return 2;
    }

    /* Map it into memory */
    g_shm_mapping = ((struct session_shared_data *)
                     g_map_file_shared(fd, SESMAN_SHAREDMEM_LENGTH));
    g_file_close(fd);
    if (g_shm_mapping == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "mmap() failed (%d)", g_get_errno());
        return 3;
    }

    /* Initalise mutex */
    rc = tc_shared_mutex_create((tbus) &g_shm_mapping->mutex);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR, "tc_shared_mutex_create() failed (%d)",
                    g_get_errno());
        return 6;
    }

    /* Initialise data */
    sesshm_lock();
    g_strncpy(g_shm_mapping->tag, SESMAN_SHAREDMEM_TAG,
              sizeof(g_shm_mapping->tag));
    g_shm_mapping->data_format_version = SESMAN_SHAREDMEM_FORMAT_VERSION;
    g_shm_mapping->max_sessions = SESMAN_SHAREDMEM_MAX_SESSIONS;
    g_shm_mapping->daemon_pid = g_pid;
    for (i = 0; i < g_shm_mapping->max_sessions; i ++)
    {
        g_shm_mapping->sessions[i].shm_is_allocated = 0;
    }
    sesshm_unlock();

    return 0;
}
#endif


/******************************************************************************/
/**
 *
 * @brief try to open an existing session_shared_data shared memory file
 * @return 0 on success, nonzero otherwise
 *
 */
#ifndef DONT_USE_SHM
static int
sesshm_try_open_existing_shm()
{
    int rc;
    int fd;
    int i;
    off_t end;
    char tag[SESMAN_SHAREDMEM_TAG_LENGTH];

    log_message(LOG_LEVEL_INFO, "Looking for existing shm file %s",
                SESMAN_SHAREDMEM_FILENAME);

    /* Does the shared file already exist? */
    fd = g_file_open_ex(SESMAN_SHAREDMEM_FILENAME, 1, 1, 0, 0);
    if (fd == -1)
    {
        log_message(LOG_LEVEL_ERROR, "open() failed (%d)", g_get_errno());
        return 1;
    }

    rc = g_file_read(fd, tag, SESMAN_SHAREDMEM_TAG_LENGTH);
    if (rc != SESMAN_SHAREDMEM_TAG_LENGTH)
    {
        log_message(LOG_LEVEL_ERROR, "read() failed (%d)", g_get_errno());
        return 2;
    }
    if (g_strncmp(tag, SESMAN_SHAREDMEM_TAG, SESMAN_SHAREDMEM_TAG_LENGTH))
    {
        log_message(LOG_LEVEL_ERROR, "tag is wrong file shm file %s",
                   SESMAN_SHAREDMEM_FILENAME);
        // XXX Should we exit(1) here instead?
        return 3;
    }

    /* Is it the right size? */
    end = g_file_seek_rel_end(fd, 0);
    if (end != SESMAN_SHAREDMEM_LENGTH)
    {
        log_message(LOG_LEVEL_ERROR, "shm file %s has wrong length",
                   SESMAN_SHAREDMEM_FILENAME);
        return 4;
    }
    rc = g_file_seek(fd, 0);
    if (rc != 0)
    {
        log_message(LOG_LEVEL_ERROR, "seek() failed (%d)", g_get_errno());
        return 5;
    }

    /* Map it into memory */
    g_shm_mapping = ((struct session_shared_data *)
                     g_map_file_shared(fd, SESMAN_SHAREDMEM_LENGTH));
    g_file_close(fd);
    if (g_shm_mapping == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "mmap() failed (%d)", g_get_errno());
        return 6;
    }

    /* Check that it isn't already locked.  Otherwise if it was locked by a
     * process that crashed then we will wait forever. */
    rc = sesshm_try_lock();
    if (rc)
    {
        log_message(LOG_LEVEL_ERROR, "sesshm_try_lock() failed (%d)",
                    g_get_errno());
        // XXX Should we exit(1) here instead?
        return 7;
    }

    if (g_shm_mapping->data_format_version != SESMAN_SHAREDMEM_FORMAT_VERSION)
    {
        sesshm_unlock();
        log_message(LOG_LEVEL_ERROR, "wrong data version (%d)", g_get_errno());
        // XXX Should we exit(1) here instead?
        return 8;
    }

    g_shm_mapping->daemon_pid = g_pid;
    for (i = 0; i < g_shm_mapping->max_sessions; i ++)
    {
        if (g_shm_mapping->sessions[i].shm_is_allocated)
        {
            struct session_chain *temp;

            temp =
                (struct session_chain *)g_malloc(sizeof(struct session_chain),
                                                 0);
            temp->item = &g_shm_mapping->sessions[i];
            temp->next = g_sessions;
            g_sessions = temp;
            g_session_count ++;
        }
    }
    sesshm_unlock();
    log_message(LOG_LEVEL_INFO, "Existing shm file ok: found %d sessions",
                g_session_count);

    return 0;
}
#endif


/******************************************************************************/
/**
 *
 * @brief initialises the session shared data
 * @return 0 on success, nonzero otherwise
 *
 */
int
session_init_shared()
{
#ifdef DONT_USE_SHM
    return 0;
#else
    int rc;

    /* Look for an existing shared file */
    rc = sesshm_try_open_existing_shm();

    /* If it's not okay then create it */
    if (rc != 0)
    {
        rc = sesshm_create_new_shm();
    }

    /* Start polling thread */
    if (rc == 0)
    {
        tc_thread_create(sesshm_thread, 0);
    }

    return rc;
#endif
}


/******************************************************************************/
/**
 *
 * @brief releases the session shared data
 * @return 0 on success, nonzero otherwise
 *
 */
int
session_close_shared()
{
#ifdef DONT_USE_SHM
    return 0;
#else
    int rc;

    g_sesshm_thread_going = 0;

    sesshm_lock();
    if (g_shm_mapping->daemon_pid == g_getpid())
    {
        g_shm_mapping->daemon_pid = -1;
    }
    sesshm_unlock();

    rc = g_unmap_file_shared(g_shm_mapping, SESMAN_SHAREDMEM_LENGTH);
    if (rc != 0)
    {
        return 1;
    }

    return 0;
#endif
}


/******************************************************************************/
/**
 *
 * @brief allocate a session and mark it as used
 * @return the session_item allocated
 *
 */
struct session_item *
alloc_session_item()
{
#ifdef DONT_USE_SHM
    return (struct session_item *)g_malloc(sizeof(struct session_item), 0);
#else
    int i;
    sesshm_lock();
    for (i = 0; i < g_shm_mapping->max_sessions; i ++)
    {
        if (!g_shm_mapping->sessions[i].shm_is_allocated)
        {
            g_memset(&g_shm_mapping->sessions[i],
                     0,
                     sizeof(g_shm_mapping->sessions[i]));
            g_shm_mapping->sessions[i].shm_is_allocated = 1;
            sesshm_unlock();
            return &g_shm_mapping->sessions[i];
        }
    }
    sesshm_unlock();
    return 0;
#endif
}


/******************************************************************************/
/**
 *
 * @brief validate whether a session_item pointer is valid
 * @param item a pointer to a session_item
 * @return 1 if valid or 0 if not
 *
 */
#ifndef DONT_USE_SHM
static int
validate_session_item_ptr(struct session_item *item)
{
    char *start;
    int diff;
    int size;
    int valid;

    start = (char *) &g_shm_mapping->sessions[0];
    diff = (char *) item - start;
    size = sizeof(g_shm_mapping->sessions[0]);
    valid = (diff % size == 0
             && diff >= 0
             && (diff / size < g_shm_mapping->max_sessions));

    if (!valid)
    {
        log_message(LOG_LEVEL_ALWAYS,
                    "validate_session_item_ptr: bad pointer %p",
                    item);
    }

    return valid;
}
#endif


/******************************************************************************/
/**
 *
 * @brief deallocate a session and mark it as unused
 * @param item the session_item to deallocate
 *
 */
void
free_session_item(struct session_item *item)
{
#ifdef DONT_USE_SHM
    g_free(item);
#else
    /* This assumes that the caller has called sesshm_lock() */
    if (!validate_session_item_ptr(item))
    {
        log_message(LOG_LEVEL_ALWAYS, "free_session_item: bad pointer");
    }
    else if (!item->shm_is_allocated)
    {
        log_message(LOG_LEVEL_ALWAYS, "free_session_item: session not active %p", item);
    }
    else
    {
        item->shm_is_allocated = 0;
    }
#endif
}
