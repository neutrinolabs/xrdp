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
 * @file lock_uds.c
 * @brief Providing a locking facility for Unix Domain Sockets
 * @author Matt Burt
 *
 * It is difficult for a server to determine whether a socket it wishes
 * to listen on is already active or not. The purpose of this module is to
 * provide a locking facility which can be used as a proxy for this.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"
#include "lock_uds.h"

struct lock_uds
{
    char *filename; ///<< Name of the lock file
    int fd; ///<< File descriptor for open file
    int pid; ///<< PID of process originally taking out lock
};

/******************************************************************************/
struct lock_uds *
lock_uds(const char *sockname)
{
    struct lock_uds *lock = NULL;
    char *filename = NULL;
    int fd = -1;

    if (sockname == NULL || sockname[0] != '/')
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Invalid sockname '%s'",
                  (sockname == NULL) ? "<null>" : sockname);
    }
    else
    {
        /* Allocate space for lock filename and result */
        filename = (char *)g_malloc(g_strlen(sockname) + 1 + 5 + 1, 0);
        lock = g_new0(struct lock_uds, 1);
        if (lock == NULL || filename == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "%s : Out of memory", __func__);
        }
        else
        {
            int saved_umask;
            /* Construct the filename */
            /* This call is guaranteed to succeed as we know that sockname
             * contains at least one '/' */
            char *p = filename;
            const char *basename = g_strrchr(sockname, '/') + 1;
            g_memcpy(p, sockname, basename - sockname);
            p += basename - sockname;
            *p++ = '.';
            g_strcpy(p, basename);
            p += g_strlen(p);
            *p++ = '.';
            *p++ = 'l';
            *p++ = 'o';
            *p++ = 'c';
            *p++ = 'k';
            *p++ = '\0';

            saved_umask = g_umask_hex(0x77);
            fd = g_file_open_rw(filename);
            g_umask_hex(saved_umask);
            if (fd < 0)
            {
                LOG(LOG_LEVEL_ERROR, "Unable to create '%s' [%s]",
                    filename, g_get_strerror());
            }
            else if (g_file_lock(fd, 0, 0) == 0)
            {
                LOG(LOG_LEVEL_ERROR, "Unable to get lock for '%s' - "
                    "program already running?", sockname);
                g_file_close(fd);
                fd = -1;
            }
            else
            {
                (void)g_file_set_cloexec(fd, 1);
            }
        }
    }

    if (fd >= 0)
    {
        /* Success - finish off */
        lock->filename = filename;
        lock->fd = fd;
        lock->pid = g_getpid();
    }
    else
    {
        g_free(filename);
        g_free(lock);
        lock = NULL;
    }

    return lock;
}

/******************************************************************************/
void
unlock_uds(struct lock_uds *lock)
{
    if (lock != NULL)
    {
        if (lock->fd >= 0)
        {
            g_file_close(lock->fd);
            lock->fd = -1; // In case of use-after-free
        }

        /* Only delete the lock file if we are the process which
         * created it */
        if (g_getpid() == lock->pid)
        {
            g_file_delete(lock->filename);
        }
        g_free(lock->filename);
        lock->filename = NULL;
        g_free(lock);
    }
}
