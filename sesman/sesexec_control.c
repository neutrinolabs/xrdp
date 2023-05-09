/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2023 Matt Burt
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
 * @file sesexec_control.c
 * @brief Start/stop session executive process
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "sesman_config.h"
#include "eicp.h"
#include "log.h"
#include "os_calls.h"
#include "pre_session_list.h"
#include "string_calls.h"
#include "sesexec_control.h"
#include "sesman.h"
#include "trans.h"
#include "xrdp_sockets.h"

#define SESEXEC_SHORTNAME "xrdp-sesexec"
#define SESEXEC_LONGNAME XRDP_LIBEXEC_PATH "/" SESEXEC_SHORTNAME

// Some platforms using setsid() benefit from sesexec being
// forked again, so the UNIX session can be created cleanly
// See FreeBSD bug
//     ports/157282: effective login name is not set by xrdp-sesman
//     https://www.freebsd.org/cgi/query-pr.cgi?pr=157282

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define USE_BSD_SETLOGIN 1
#endif

/*****************************************************************************/
/**
 * Adds entries to the args list for running sesexec
 *
 * @param args Argument list
 * @return 0 for memory allocation failure, 1 for success
 */
static int
create_exec_args_add_entries(struct list *args)
{
    if (!list_add_strdup(args, SESEXEC_SHORTNAME))
    {
        return 0;
    }

    if (g_strcmp(g_cfg->sesman_ini, DEFAULT_SESMAN_INI) != 0)
    {
        if (!list_add_strdup_multi(args, "-c", g_cfg->sesman_ini, NULL))
        {
            return 0;
        }
    }

    return 1;
}

/*****************************************************************************/
/**
 * Create an args list for sesexec
 * @return NULL if memory could not be allocated
 *
 * The result must be freed with list_delete() after use
 */
static struct list *
create_exec_args(void)
{
    struct list *result = list_create();
    if (result != NULL)
    {
        result->auto_free = 1;
        if (!create_exec_args_add_entries(result))
        {
            list_delete(result);
            result = NULL;
        }
    }

    return result;
}

/*****************************************************************************/
int
sesexec_start(struct pre_session_item *psi)
{
    // Local socket pair used to set up the EICP channel for sesexec
    // We also use the socket pair to communicate the PID of sesexec back
    // to sesman. Technically we only need this if USE_BSD_SETLOGIN
    // is set, but removing this variable complicates the code so
    // much it isn't worth it.
    int sck[2] = {-1, -1};
    int rv = -1;
    int size;
    const char *exe = SESEXEC_LONGNAME;
    struct list *args = NULL;

    if (!g_executable_exist(exe))
    {
        LOG(LOG_LEVEL_ERROR, "Can't execute %s", exe);
    }
    else if ((args = create_exec_args()) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory running sesexec");
    }
    else if (g_sck_local_socketpair(sck) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't create sesexec EICP socket [%s]",
            g_get_strerror());
    }
    else
    {
        int pid = g_fork();
        if (pid == -1)
        {
            // Error already logged
            g_file_close(sck[0]);
            g_file_close(sck[1]);
        }
        else if (pid == 0)
        {
            /* Sesexec process */
            g_file_close(sck[0]);

#if USE_BSD_SETLOGIN
            if (g_fork() != 0)
            {
                g_exit(0);
            }
#endif
            // Send our pid back to sesman
            pid = g_getpid();
            size = g_file_write(sck[1], (const char *)&pid, sizeof(pid));

            if (size != sizeof(pid))
            {
                LOG(LOG_LEVEL_ERROR, "Can't write to PID socket [%s]",
                    g_get_strerror());
            }
            else
            {
                /* Put the number of the file descriptor in EICP_FD
                 * in the environment */
                char buff[64];
                g_snprintf(buff, sizeof(buff), "%d", sck[1]);
                g_setenv("EICP_FD", buff, 1);

                /* [Development] Log all file descriptors not marked cloexec
                 * other than stdin, stdout, stderr, and the EICP fd in sck[1].
                 */
                if (sck[1] < 3)
                {
                    // EICP fd has overwritten one of the standard descriptors
                    LOG_DEVEL_LEAKING_FDS("xrdp-sesexec", 3, -1);
                }
                else
                {
                    LOG_DEVEL_LEAKING_FDS("xrdp-sesexec", 3, sck[1]);
                    LOG_DEVEL_LEAKING_FDS("xrdp-sesexec", sck[1] + 1, -1);
                }

                g_execvp_list(exe, args);

                // Shouldn't get here. Errors are logged if we do.
            }
            g_exit(1);
        }
        else
        {
            g_file_close(sck[1]);

            // Get the PID from the child (or the grancdchild)
            int size = g_file_read(sck[0], (char *)&pid, sizeof(pid));

            if (size != sizeof(pid))
            {
                LOG(LOG_LEVEL_ERROR, "Can't read PID of sesexec process [%s]",
                    g_get_strerror());
                g_file_close(sck[0]);
            }
            else
            {
                struct trans *t = eicp_init_trans_from_fd(sck[0],
                                  TRANS_TYPE_CLIENT,
                                  sesman_is_term);
                if (t == NULL)
                {
                    LOG(LOG_LEVEL_ERROR, "Can't create sesexec transport [%s]",
                        g_get_strerror());
                    g_file_close(sck[0]);
                }
                else
                {
                    t->trans_data_in = sesman_eicp_data_in;
                    t->callback_data = (void *)psi;
                    psi->sesexec_trans = t;
                    psi->sesexec_pid = pid;
                    rv = 0;
                }
            }
        }
    }

    list_delete(args);
    return rv;
}
