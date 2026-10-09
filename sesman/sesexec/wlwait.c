/**
 * xrdp: A Remote Desktop Protocol server.
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
 * @file wlwait.c
 * @brief Wait for a Wayland compositor to start listening
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "wlwait.h"

/* Most file locks a compositor is expected to hold */
#define MAX_LOCKS 64

#if defined(__linux__)
struct file_lock
{
    unsigned int major;
    unsigned int minor;
    unsigned long inode;
};

/******************************************************************************/
/**
 * Gets the files a process holds a flock() on, from /proc/locks
 *
 * /proc/locks can be read by everyone, unlike /proc/<pid>/fd which needs
 * ptrace rights over the process.
 *
 * @return Number of locks found
 */
static int
get_flocks(pid_t pid, struct file_lock locks[], int max_locks)
{
    char line[256];
    FILE *fp;
    int count = 0;

    if ((fp = fopen("/proc/locks", "r")) == NULL)
    {
        return 0;
    }
    while (count < max_locks && fgets(line, sizeof(line), fp) != NULL)
    {
        /* e.g. "3: FLOCK  ADVISORY  WRITE 1234 00:1c:11658970 0 EOF"
         * Lines with "->" are processes waiting for a lock */
        char type[16];
        int lock_pid;
        struct file_lock *l = &locks[count];

        if (strstr(line, "->") == NULL &&
                sscanf(line, "%*s %15s %*s %*s %d %x:%x:%lu",
                       type, &lock_pid, &l->major, &l->minor,
                       &l->inode) == 5 &&
                strcmp(type, "FLOCK") == 0 && lock_pid == (int)pid)
        {
            ++count;
        }
    }
    fclose(fp);
    return count;
}

/******************************************************************************/
/**
 * Looks in a directory for the "wayland-*.lock" file a process holds a
 * lock on. libwayland locks this file for as long as the socket of the
 * same name (without ".lock") is in use.
 *
 * @return 0 if found
 */
static int
find_locked_socket(const char *dirname,
                   const struct file_lock locks[], int count,
                   char *path, unsigned int path_size)
{
    struct dirent *entry;
    DIR *dir;
    int rv = 1;

    if ((dir = opendir(dirname)) == NULL)
    {
        return 1;
    }
    while (rv != 0 && (entry = readdir(dir)) != NULL)
    {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        char lock_path[512];
        struct stat st;
        int i;

        if (strncmp(name, "wayland-", 8) != 0 || len < 5 ||
                strcmp(name + len - 5, ".lock") != 0)
        {
            continue;
        }
        if (snprintf(lock_path, sizeof(lock_path), "%s/%s",
                     dirname, name) >= (int)sizeof(lock_path) ||
                stat(lock_path, &st) != 0)
        {
            continue;
        }
        for (i = 0; i < count; ++i)
        {
            if (locks[i].inode == (unsigned long)st.st_ino &&
                    locks[i].major == major(st.st_dev) &&
                    locks[i].minor == minor(st.st_dev))
            {
                /* The socket is the lock file name without ".lock" */
                lock_path[strlen(lock_path) - 5] = '\0';
                if (stat(lock_path, &st) == 0 && S_ISSOCK(st.st_mode))
                {
                    g_strncpy(path, lock_path, path_size - 1);
                    rv = 0;
                }
                break;
            }
        }
    }
    closedir(dir);
    return rv;
}
#endif

/******************************************************************************/
int
wait_for_wayland_socket(pid_t pid, const char *runtime_dir,
                        unsigned int timeout_ms,
                        char *path, unsigned int path_size)
{
#if defined(__linux__)
    struct file_lock locks[MAX_LOCKS];
    unsigned int start = g_get_elapsed_ms();

    while (g_get_elapsed_ms() - start < timeout_ms)
    {
        int count = get_flocks(pid, locks, MAX_LOCKS);
        if (count > 0 &&
                find_locked_socket(runtime_dir, locks, count,
                                   path, path_size) == 0)
        {
            return 0;
        }
        g_sleep(100);
    }
    LOG(LOG_LEVEL_ERROR, "Timed out waiting for the Wayland compositor "
        "(pid %d) to listen on a socket in %s", (int)pid, runtime_dir);
#else
    LOG(LOG_LEVEL_ERROR, "Finding the socket of a Wayland compositor is "
        "only implemented for Linux");
#endif
    return 1;
}
