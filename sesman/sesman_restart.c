/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Matt Burt 2024
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
 * @file sesman_restart.c
 * @brief Sesman restart definitions
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>

#include "os_calls.h"
#include "sesman.h"
#include "sesman_config.h"
#include "session_list.h"

#include "ercp.h"

#include "sesman_restart.h"
#include "xrdp_sockets.h"

// Result of calling init_restart_directory()
enum init_restart_dir_status
{
    E_RESTART_DIR_CREATED_OK,     ///< All good. Dir created
    E_RESTART_DIR_ALREADY_EXISTS, ///< All good. Dir already existed
    E_RESTART_DIR_ERROR           ///< Not good
};

enum
{
    MAX_DISCOVERY_WAIT_TIME = 5000  // Milli-seconds
};

/******************************************************************************/
static enum init_restart_dir_status
init_restart_directory(const char *restart_dir)
{
    enum init_restart_dir_status rv;

    if (g_directory_exist(restart_dir))
    {
        rv = E_RESTART_DIR_ALREADY_EXISTS;
    }
    else
    {
        // Create the restart directory for the next run
        if (g_mkdir(restart_dir) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't create restart directory %s [%s]",
                restart_dir, g_get_strerror());
            rv = E_RESTART_DIR_ERROR;
        }
        else
        {
            rv = E_RESTART_DIR_CREATED_OK;
        }
    }

    if (rv != E_RESTART_DIR_ERROR)
    {
        // Always set the permissions on the restart directory, whether
        // or not we created it
        if (g_chown(restart_dir, g_getuid(), g_getuid()) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't set ownership of '%s' [%s]",
                restart_dir, g_get_strerror());
            rv = E_RESTART_DIR_ERROR;
        }
        else if (g_chmod_hex(restart_dir, 0x700) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't set permissions on '%s' [%s]",
                restart_dir, g_get_strerror());
            rv = E_RESTART_DIR_ERROR;
        }
    }
    return rv;
}

/******************************************************************************/
/**
 * Attempts to add a sesexec Unix Domain Socket to the process_list
 * @param filename Name of UDS
 * @return Boolean for success
 *
 * The credentials of the process on the other end are checked.
 * We don't take the max session limit into account when adding sessions here,
 * as this could result in orphaned sessions.
 */
static int
add_sesexec_fd_to_session_list(const char *filename)
{
    int status = 0;
    struct trans *t = NULL;

    // Check filename is a socket
    if (g_socket_exist(filename))
    {
        // Try to connect to the session
        if ((t = ercp_connect(filename, sesman_is_term)) != NULL)
        {
            int sesexec_pid;
            int sesexec_uid;
            int sesexec_gid;

            // Find the credentials of the sesexec process on the other end
            if (g_sck_get_peer_cred(t->sck, &sesexec_pid,
                                    &sesexec_uid, &sesexec_gid) == 0)
            {
                // Don't talk to unprivileged processes. It's a big concern
                // if we find one.
                if (sesexec_uid != 0 || sesexec_gid != 0)
                {
                    LOG(LOG_LEVEL_ALWAYS,
                        "Unexpected sesexec owner %d:%d"
                        " for PID %d listening on %s",
                        sesexec_uid, sesexec_gid, sesexec_pid, filename);
                }
                else
                {
                    struct session_item *s_item;
                    if ((s_item = session_list_new()) != NULL)
                    {
                        // Finalise the session for I/O
                        t->trans_data_in = sesman_ercp_data_in;
                        t->callback_data = (void *)s_item;

                        // Complete the session fields for the
                        // E_SESSION_STARTING state
                        s_item->sesexec_trans = t;
                        s_item->sesexec_pid = sesexec_pid;
                        s_item->display[0] = '\0';

                        // Tell the caller we've added one
                        status = 1;
                    }
                }
            }
        }
    }

    // Clean up an unused transport
    if (status == 0)
    {
        trans_delete(t);
    }

    return 1;
}

/******************************************************************************/
static int
discover_sessions(const char *restart_dir)
{
    int rv = 0;
    struct list *dirnames = g_readdir(restart_dir);
    unsigned int start_time = g_get_elapsed_ms();
    unsigned int session_count;
    unsigned int elapsed;
    int robjs_count;
    intptr_t robjs[1024];
    int timeout;

    if (dirnames == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't read restart directory to discover sessions [%s]",
            g_get_strerror());
    }
    else
    {
        // Iterate over the restart directory, and add any sesexec
        // processes we discover to the session list, in
        // E_SESSION_STARTING state.
        char filename[XRDP_SOCKETS_MAXPATH];
        int i;

        for (i = 0 ; i < dirnames->count; ++i)
        {
            g_snprintf(filename, sizeof(filename),  "%s/%s",
                       restart_dir, (const char *)dirnames->items[i]);

            (void)add_sesexec_fd_to_session_list(filename);
        }

        list_delete(dirnames);
        dirnames = NULL;
    }

    // Process session list messages until either all sessions have
    // started (or failed), or we hit a timeout.
    while (1)
    {
        session_count = session_list_get_count_by_state(E_SESSION_STARTING);
        elapsed = (g_get_elapsed_ms() - start_time);
        if (session_count == 0 || elapsed >= MAX_DISCOVERY_WAIT_TIME)
        {
            break;
        }

        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        (void)session_list_get_wait_objs(robjs, &robjs_count);

        timeout = MAX_DISCOVERY_WAIT_TIME - elapsed; // > 0
        if (g_obj_wait(robjs, robjs_count, NULL, 0, timeout) != 0)
        {
            /* should not get here */
            g_sleep(100);
            continue;
        }

        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
            LOG(LOG_LEVEL_INFO, "discover_sessions: sesman asked to terminate");
            rv = 1;
            break;
        }

        (void)session_list_check_wait_objs();
    }

    if (rv == 0)
    {
        LOG(LOG_LEVEL_INFO,
            "Session discovery took %d secs and loaded %u sessions",
            elapsed, session_list_get_count_by_state(E_SESSION_RUNNING));

        if (session_count > 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "%u sessions have not responded at end of discovery",
                session_count);
        }
    }

    return rv;
}

/******************************************************************************/
int
sesman_restart_discover_sessions(void)
{
    int rv = 1;
    // The restart directory contains Unix Domain sockets, so can't
    // exceed XRDP_SOCKETS_MAXPATH in length
    char restart_dir[XRDP_SOCKETS_MAXPATH];

    // sizeof(g_cfg->listen_port) is guaranteed to be smaller than
    // XRDP_SOCKETS_MAXPATH
    g_snprintf(restart_dir, sizeof(restart_dir),
               "%s.r", g_cfg->listen_port);
    switch (init_restart_directory(restart_dir))
    {
        case E_RESTART_DIR_CREATED_OK:
            // Nothing to discover
            rv = 0;
            break;

        case E_RESTART_DIR_ALREADY_EXISTS:
            // Look for sessions from previous run
            rv = discover_sessions(restart_dir);
            break;

        default:
            ;
    }

    return rv;
}
