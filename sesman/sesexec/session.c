/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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
 * @brief Session management definitions
 *
 * This module wraps the session management classes
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>

#include "session.h"
#include "session_base.h"
#include "session_parameters.h"

#include "sesman_auth.h"
#include "sesman_config.h"
#include "env.h"
#include "log.h"
#include "login_info.h"
#include "os_calls.h"
#include "sesexec.h"
#include "trans.h"
#include "xrdp_sockets.h"

/******************************************************************************/
/**
 * Starts a session from an operating system perspective
 * @param login_info login info for user
 * @param s session_parameters
 * @return Status
 */
static enum scp_screate_status
session_start_preamble(struct login_info *login_info,
                       const struct session_parameters *s)
{
    /* Set the secondary groups before starting the session to prevent
     * problems on PAM-based systems (see Linux pam_setcred(3)).
     * If we have *BSD setusercontext() this is not done here */
#ifndef HAVE_SETUSERCONTEXT
    if (g_initgroups(login_info->username) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Failed to initialise secondary groups for %s: %s",
            login_info->username, g_get_strerror());
        return E_SCP_SCREATE_GENERAL_ERROR;
    }
#endif

    if (auth_start_session(login_info->auth_info, s->display) != 0)
    {
        // Errors are logged by the auth module, as they are
        // specific to that module
        return E_SCP_SCREATE_GENERAL_ERROR;
    }
#ifdef USE_BSD_SETLOGIN
    /**
     * Create a new session and process group since the 4.4BSD
     * setlogin() affects the entire process group
     */
    if (g_setsid() < 0)
    {
        LOG(LOG_LEVEL_WARNING,
            "[session start] (display %d): setsid failed - pid %d",
            s->display, g_getpid());
    }

    if (g_setlogin(login_info->username) < 0)
    {
        LOG(LOG_LEVEL_WARNING,
            "[session start] (display %d): setlogin failed for user %s - pid %d",
            s->display, login_info->username, g_getpid());
    }
#endif

    return E_SCP_SCREATE_OK;
}

/******************************************************************************/
enum scp_screate_status
session_start(struct login_info *login_info,
              const struct session_parameters *sp,
              struct session_data **session_data)
{
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;
    /* Create the session_data struct first */
    struct session_data *self = session_base_new(sp);
    if (self == NULL)
    {
        status = E_SCP_SCREATE_NO_MEMORY;
    }
    else
    {
        status = session_start_preamble(login_info, sp);
        if (status == E_SCP_SCREATE_OK)
        {
            status = self->vtable->start(self, login_info, sp);
            if (status == E_SCP_SCREATE_OK)
            {
                *session_data = self;
            }
            else
            {
                *session_data = NULL;
                session_data_free(self);
            }
        }
    }

    return status;
}

/******************************************************************************/
void
session_data_free(struct session_data *self)
{
    session_base_destroy(self);
}

/******************************************************************************/
void
session_process_sigchld_event(struct session_data *self)
{
    // The base class does this, as the base class method needs to be called
    // from other base class methods.
    session_base_process_sigchld_event(self);
}

/******************************************************************************/
unsigned int
session_active(const struct session_data *self)
{
    return self->vtable->active_processes(self);
}

/******************************************************************************/
time_t
session_get_start_time(const struct session_data *self)
{
    return self->start_time;
}

/******************************************************************************/
unsigned int
session_increment_connect_count(struct session_data *self)
{
    return self->connect_count++;
}

/******************************************************************************/
static void
start_reconnect_script(struct session_data *self,
                       const struct login_info *login_info,
                       void *closure)
{
    env_set_user(login_info->uid, 0, self->params->display,
                 g_cfg->env_names,
                 g_cfg->env_values);

    auth_set_env(login_info->auth_info);

    if (g_file_exist(g_cfg->reconnect_sh))
    {
        /* The 'closure' parameter points to a list of strings
         * which need to be set in the environment for the reconnect script */
        if (closure != NULL)
        {
            const char **p = (const char **)closure;
            while (*p != NULL && *(p + 1) != NULL)
            {
                (void)g_setenv(*p, *(p + 1), 1);
                p += 2;
            }
        }
        LOG_DEVEL_LEAKING_FDS("reconnect script", 3, -1);

        LOG(LOG_LEVEL_INFO,
            "Starting session reconnection script on display %d: %s",
            self->params->display, g_cfg->reconnect_sh);
        g_execlp3(g_cfg->reconnect_sh, g_cfg->reconnect_sh, 0);

        /* should not get here */
        LOG(LOG_LEVEL_ERROR,
            "Error starting session reconnection script on display %d: %s",
            self->params->display, g_cfg->reconnect_sh);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING,
            "Session reconnection script file does not exist: %s",
            g_cfg->reconnect_sh);
    }
}

/******************************************************************************/
void
session_run_reconnect_script(struct session_data *self,
                             const struct login_info *login_info,
                             const char *vars[])
{
    if (session_base_fork_child(self,
                                login_info,
                                self->vtable->getpgid(self),
                                start_reconnect_script,
                                (void *)vars) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to fork for session reconnection script");
    }
}

/******************************************************************************/
const struct session_parameters *
session_get_parameters(const struct session_data *self)
{
    return self->params;
}

/******************************************************************************/
void
session_send_term(struct session_data *self, int wait_for_all)
{
    // The base class does this, as the base class method needs to be called
    // from other base class methods.
    session_base_send_term(self, wait_for_all);
}

/******************************************************************************/
int
session_get_display_server_fd(const struct session_data *self,
                              const struct login_info *login_info)
{
    return self->vtable->get_display_server_fd(self, login_info);
}

/******************************************************************************/
int
session_get_chansrv_fd(const struct session_data *self,
                       const struct login_info *login_info)
{
    char portname[XRDP_SOCKETS_MAXPATH];

    int rv = -1;

    if (self->chansrv_pid <= 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Request to connect to chansrv :%u"
            " which has exited", self->params->display);
    }
    else
    {
        snprintf(portname, sizeof(portname),
                 XRDP_CHANSRV_STR, login_info->uid, (int)self->params->display);

        // Use the transport library to get the fd
        struct trans *t = trans_create(TRANS_MODE_UNIX, 8192, 8192);
        if (t == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory creating transport");
        }
        else if (trans_connect(t, NULL, portname, 10 * 1000) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't connect to chansrv :%u [%s]",
                self->params->display,
                g_get_strerror());
        }
        else
        {
            rv = t->sck;
            t->sck = -1;
        }
        trans_delete(t);
    }

    return rv;
}
