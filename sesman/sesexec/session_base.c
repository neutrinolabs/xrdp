/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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
 * @file session_base.c
 * @brief Session object base class and utilities
 * @author Jay Sorg, Simone Fedele
 *
 */

//#include <stdio.h>

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "session_base.h"
#include "session_x11.h"
#include "session_parameters.h"

#include "sesman_config.h"
#include "env.h"
#include "log.h"
#include "login_info.h"
#include "os_calls.h"
#include "sesexec.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

/******************************************************************************/
struct session_data *
session_base_new(const struct session_parameters *sp)
{
    struct session_data *self = NULL;

    // Make a copy of the session parameters for the new object
    struct session_parameters *sp_copy = copy_session_parameters(sp);
    if (sp_copy == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Out of memory allocating session parameter block");
    }
    else
    {
        struct session_data_x11 *self_x11 = session_x11_new();
        if (self_x11 == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory allocating X11 session object");
        }
        else
        {
            self = &self_x11->base;
        }
    }

    if (self != NULL)
    {
        self->params = sp_copy;
        self->chansrv_pid = -1;
        self->start_time = 0;
        self->connect_count = 0;
    }
    else
    {
        free(sp_copy);
    }

    return self;
}

/******************************************************************************/
void
session_base_destroy(struct session_data *self)
{
    if (self != NULL)
    {
        // Call the (optional) derived class destructor
        if (self->vtable->free != NULL)
        {
            self->vtable->free(self);
        }
#ifdef USE_DEVEL_LOGGING
        if (self->chansrv_pid > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid chansrv PID %d",
                      self->chansrv_pid);
        }
#endif

        free(self->params);
        free(self);
    }
}

/******************************************************************************/
void
session_base_start_chansrv(struct session_data *self,
                           const struct login_info *login_info,
                           void *closure /* unused */)
{
    struct list *chansrv_params = list_create();
    const char *exe_path = XRDP_SBIN_PATH "/xrdp-chansrv";

    if (chansrv_params != NULL)
    {
        chansrv_params->auto_free = 1;
        if (!list_add_strdup(chansrv_params, exe_path))
        {
            list_delete(chansrv_params);
            chansrv_params = NULL;
        }
    }

    if (chansrv_params == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory starting chansrv");
    }
    else
    {
        env_set_user(login_info->uid, 0, self->params->display,
                     g_cfg->env_names,
                     g_cfg->env_values);

        LOG_DEVEL_LEAKING_FDS("chansrv", 3, -1);

        /* executing chansrv */
        g_execvp_list(exe_path, chansrv_params);

        /* should not get here */
        list_delete(chansrv_params);
    }
}

/******************************************************************************/
/*
 * Simple helper process to fork a child and log errors */
int
session_base_fork_child(
    struct session_data *self,
    const struct login_info *login_info,
    pid_t group_pid,
    void (*runproc)(struct session_data *self,
                    const struct login_info *,
                    void *closure),
    void *closure)
{
    int pid = g_fork();
    if (pid == 0)
    {
        /* Child process */
        if (group_pid >= 0)
        {
            (void)g_setpgid(0, group_pid);
        }
        runproc(self, login_info, closure);
        g_exit(0);
    }

    if (pid < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Fork failed [%s]", g_get_strerror());
    }

    return pid;
}

/******************************************************************************/
int
session_base_process_startup_wait_time(struct session_data *self)
{
    int rv = 0;
    int robjs_count;
    intptr_t robjs[10];
    unsigned int start = g_get_elapsed_ms();

    LOG(LOG_LEVEL_INFO, "Waiting for %u ms for session to start",
        g_cfg->sess.startup_wait_time);
    while (1)
    {
        unsigned int elapsed = g_get_elapsed_ms() - start;
        if (elapsed >= g_cfg->sess.startup_wait_time)
        {
            break;
        }

        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;

        if (g_obj_wait(robjs, robjs_count, NULL, 0,
                       g_cfg->sess.startup_wait_time - elapsed) != 0)
        {
            /* should not get here */
            LOG(LOG_LEVEL_WARNING, "process_startup_wait_time: "
                "Unexpected error from g_obj_wait()");
            g_sleep(100);
            continue;
        }

        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
            // Simulate success for now, but leave g_term_event set. The
            // main loop will also pick up the terminate event and the
            // session will be closed normally
            break;
        }

        if (g_is_wait_obj_set(g_sigchld_event)) /* SIGCHLD */
        {
            g_reset_wait_obj(g_sigchld_event);
            session_base_process_sigchld_event(self);
            if (!self->vtable->main_sess_proc_active(self))
            {
                // Session has started to fail in the StartupWaitTime
                // Wait for the rest of the session to finish
                rv = 1;
                session_base_send_term(self, 1);
                break;
            }
        }
    }

    return rv;
}

/******************************************************************************/
int
session_base_cleanup_sockets(struct session_data *self)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "session_base_cleanup_sockets:");

    char file[XRDP_SOCKETS_MAXPATH];
    int error = 0;

    int uid = g_login_info->uid;
    int display = self->params->display;

    g_snprintf(file, sizeof(file), CHANSRV_PORT_OUT_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "session_base_cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "session_base_cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    g_snprintf(file, sizeof(file), CHANSRV_PORT_IN_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "session_base_cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "session_base_cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    g_snprintf(file, sizeof(file), XRDP_CHANSRV_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "session_base_cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "session_base_cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    g_snprintf(file, sizeof(file), CHANSRV_API_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "session_base_cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "session_base_cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    return error;
}

/******************************************************************************/
void
session_base_process_sigchld_event(struct session_data *self)
{
    struct proc_exit_status e;
    int pid;

    // Check for any finished children
    while ((pid = g_waitchild(&e)) > 0)
    {
        if (pid == self->chansrv_pid)
        {
            LOG(LOG_LEVEL_INFO,
                "xrdp channel server pid %d on display :%d finished",
                self->chansrv_pid, self->params->display);
            self->chansrv_pid = -1;
        }
        else
        {
            self->vtable->process_child_exit(self, pid, &e);
        }
    }
}

/******************************************************************************/
void
session_base_send_term(struct session_data *self, int wait_for_all)
{
    self->vtable->send_term(self);

    if (wait_for_all)
    {
        while (self->vtable->active_processes(self) > 0)
        {
            /* Don't check SIGTERM - we shouldn't be here long */
            if (g_obj_wait(&g_sigchld_event, 1, NULL, 0, -1) != 0)
            {
                /* should not get here */
                LOG(LOG_LEVEL_WARNING, "session_send_term: "
                    "Unexpected error from g_obj_wait()");
                g_sleep(100);
            }
            else
            {
                g_reset_wait_obj(g_sigchld_event);
                session_base_process_sigchld_event(self);
            }
        }
    }
}
