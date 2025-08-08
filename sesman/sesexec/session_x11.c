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
 * @file session_x11.c
 * @brief Base class for X11 session objects
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <errno.h>

#include "arch.h"
#include "session_x11.h"
#include "session_parameters.h"

#include "sesman_auth.h"
#include "sesman_config.h"
#include "env.h"
#include "guid.h"
#include "list.h"
#include "log.h"
#include "login_info.h"
#include "os_calls.h"
#include "sesexec.h"
#include "sessionrecord.h"
#include "string_calls.h"
#include "trans.h"
#include "xauth.h"
#include "xwait.h"
#include "xrdp_sockets.h"

/******************************************************************************/
/**
 * Creates a string consisting of all parameters that is hosted in the param list
 * @param self
 * @param outstr, allocate this buffer before you use this function
 * @param len the allocated len for outstr
 * @return
 */
static char *
dumpItemsToString(struct list *self, char *outstr, int len)
{
    int index;
    int totalLen = 0;

    g_memset(outstr, 0, len);
    if (self->count == 0)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "List is empty");
    }

    for (index = 0; index < self->count; index++)
    {
        /* +1 = one space*/
        totalLen = totalLen + g_strlen((char *)list_get_item(self, index)) + 1;

        if (len > totalLen)
        {
            g_strcat(outstr, (char *)list_get_item(self, index));
            g_strcat(outstr, " ");
        }
    }

    return outstr ;
}

/******************************************************************************/
static void
start_window_manager(struct session_data *baseobj,
                     const struct login_info *login_info,
                     void *closure /* unused */)
{
    char text[256];
    const struct session_parameters *s = baseobj->params;

    env_set_user(login_info->uid,
                 0,
                 baseobj->params->display,
                 g_cfg->env_names,
                 g_cfg->env_values);

    auth_set_env(login_info->auth_info);
    LOG_DEVEL_LEAKING_FDS("window manager", 3, -1);

    if (s->directory[0] != '\0')
    {
        if (g_cfg->sec.allow_alternate_shell)
        {
            g_set_current_dir(s->directory);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Directory change to %s requested, but not "
                "allowed by AllowAlternateShell config value.",
                s->directory);
        }
    }

    if (s->shell[0] != '\0')
    {
        if (g_cfg->sec.allow_alternate_shell)
        {
            if (g_strchr(s->shell, ' ') != 0 || g_strchr(s->shell, '\t') != 0)
            {
                LOG(LOG_LEVEL_INFO,
                    "Using user requested window manager on "
                    "display %u with embedded arguments using a shell: %s",
                    s->display, s->shell);
                const char *argv[] = {"sh", "-c", s->shell, NULL};
                g_execvp("/bin/sh", (char **)argv);
            }
            else
            {
                LOG(LOG_LEVEL_INFO,
                    "Using user requested window manager on "
                    "display %d: %s", s->display, s->shell);
                g_execlp3(s->shell, s->shell, 0);
            }
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Shell %s requested by user, but not allowed by "
                "AllowAlternateShell config value.",
                s->shell);
        }
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "The user session on display %u did "
            "not request a specific window manager", s->display);
    }

    /* try to execute user window manager if enabled */
    if (g_cfg->enable_user_wm)
    {
        g_snprintf(text, sizeof(text), "%s/%s",
                   g_getenv("HOME"), g_cfg->user_wm);
        if (g_file_exist(text))
        {
            LOG(LOG_LEVEL_INFO,
                "Using window manager on display %u"
                " from user home directory: %s", s->display, text);
            g_execlp3(text, g_cfg->user_wm, 0);
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG,
                "The user home directory window manager configuration "
                "is enabled but window manager program does not exist: %s",
                text);
        }
    }

    LOG(LOG_LEVEL_INFO,
        "Using the default window manager on display %u: %s",
        s->display, g_cfg->default_wm);
    g_execlp3(g_cfg->default_wm, g_cfg->default_wm, 0);

    /* still a problem starting window manager just start xterm */
    LOG(LOG_LEVEL_WARNING,
        "No window manager on display %u started, "
        "so falling back to starting xterm for user debugging",
        s->display);
    g_execlp3("xterm", "xterm", 0);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
        "the window manager on display %u, aborting connection",
        s->display);
}

/******************************************************************************/
static struct list *
prepare_xorg_xserver_params(const struct session_parameters *s,
                            const char *authfile)
{

    char screen[32]; /* display number */
    char text[128];
    const char *xserver;

    struct list *params = list_create();
    if (params != NULL)
    {
        params->auto_free = 1;

        /*
         * Make sure Xorg doesn't run setuid root. Root access is not
         * needed. Xorg can fail when run as root and the user has no
         * console permissions.
         */
        if (g_cfg->sec.xorg_no_new_privileges && g_no_new_privs() != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "[session start] (display %u): Failed to disable "
                "setuid on X server: %s",
                s->display, g_get_strerror());
        }

        g_snprintf(screen, sizeof(screen), ":%u", s->display);

        /* some args are passed via env vars */
        g_snprintf(text, sizeof(text), "%d", s->width);
        g_setenv_log("XRDP_START_WIDTH", text, 1);

        g_snprintf(text, sizeof(text), "%d", s->height);
        g_setenv_log("XRDP_START_HEIGHT", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_idle_time);
        g_setenv_log("XRDP_SESMAN_MAX_IDLE_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_disc_time);
        g_setenv_log("XRDP_SESMAN_MAX_DISC_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.kill_disconnected);
        g_setenv_log("XRDP_SESMAN_KILL_DISCONNECTED", text, 1);

        /* get path of Xorg from config */
        xserver = (const char *)list_get_item(g_cfg->xorg_params, 0);

        /* these are the must have parameters */
        list_add_strdup_multi(params,
                              xserver, screen,
                              "-auth", authfile,
                              NULL);

        /* additional parameters from sesman.ini file */
        list_append_list_strdup(g_cfg->xorg_params, params, 1);
    }

    return params;
}

/******************************************************************************/
/**
 * Prepare a list of parameters for the Xvnc X server
 * @param s Session parameters
 * @params authfile XAUTHORITY file
 * @params passwd_file VNC password file, or NULL
 * @params port UDS port to connect to, or NULL
 * @return parameters list
 *
 * One of passwd_file and port must be set
 */
static struct list *
prepare_xvnc_xserver_params(const struct session_parameters *s,
                            const char *authfile,
                            const char *passwd_file,
                            const char *port)
{
    char screen[32] = {0}; /* display number */
    char geometry[32] = {0};
    char depth[32] = {0};
    char guid_str[GUID_STR_SIZE];
    const char *xserver;

    struct list *params = list_create();
    if (params != NULL)
    {
        params->auto_free = 1;

        g_snprintf(screen, sizeof(screen), ":%u", s->display);
        g_snprintf(geometry, sizeof(geometry), "%dx%d", s->width, s->height);
        g_snprintf(depth, sizeof(depth), "%d", s->bpp);

        guid_to_str(&s->guid, guid_str);
        env_check_password_file(passwd_file, guid_str);

        /* get path of Xvnc from config */
        xserver = (const char *)list_get_item(g_cfg->vnc_params, 0);

        /* these are the must have parameters */
        list_add_strdup_multi(params,
                              xserver, screen,
                              "-auth", authfile,
                              "-geometry", geometry,
                              "-depth", depth,
                              NULL);

        if (passwd_file != NULL)
        {
            /* RFB authorization */
            list_add_strdup_multi(params,
                                  "-rfbauth", passwd_file,
                                  NULL);
        }
        else if (port != NULL)
        {
            /* UDS connection. Authorization is handled by standard socket
             * permissions, so we do not need to authorize within the
             * VNC protocol exchange as well */
            char sock_mode[16];

            /* Convert a standard permissions mask into decimal
             * for the -rfbunixmode switch argument
             */
            g_snprintf(sock_mode, sizeof(sock_mode),
                       "%d", 0660); /* rw-rw---- */

            list_add_strdup_multi(params,
                                  "-rfbunixpath", port,
                                  "-rfbunixmode", sock_mode,
                                  "-SecurityTypes", "None",
                                  NULL);
        }

        /* additional parameters from sesman.ini file */
        //config_read_xserver_params(SCP_SESSION_TYPE_XVNC,
        //                           xserver_params);
        list_append_list_strdup(g_cfg->vnc_params, params, 1);
    }
    return params;
}

/******************************************************************************/
/* Either execs the X server, or returns */
static void
start_x_server(struct session_data *baseobj,
               const struct login_info *login_info,
               void *closure /* unused */)
{
    char authfile[256]; /* The filename for storing xauth information */
    char execvpparams[2048];
    char *passwd_file = NULL;
    struct list *xserver_params = NULL;
    int unknown_session_type = 0;

    if (baseobj->params->type == SCP_SESSION_TYPE_XVNC)
    {
        env_set_user(login_info->uid,
                     &passwd_file,
                     baseobj->params->display,
                     g_cfg->env_names,
                     g_cfg->env_values);
    }
    else
    {
        env_set_user(login_info->uid,
                     0,
                     baseobj->params->display,
                     g_cfg->env_names,
                     g_cfg->env_values);
    }

    /* prepare the Xauthority stuff */
    if (g_getenv("XAUTHORITY") != NULL)
    {
        g_snprintf(authfile, sizeof(authfile), "%s",
                   g_getenv("XAUTHORITY"));
    }
    else
    {
        g_snprintf(authfile, sizeof(authfile), "%s", ".Xauthority");
    }

    /* Add the entry in XAUTHORITY file or exit if error */
    if (add_xauth_cookie(baseobj->params->display, authfile) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Error setting the xauth cookie for display %u in file %s",
            baseobj->params->display, authfile);
    }
    else
    {
        switch (baseobj->params->type)
        {
                char port[256];

            case SCP_SESSION_TYPE_XORG:
                xserver_params = prepare_xorg_xserver_params(baseobj->params,
                                 authfile);
                break;

            case SCP_SESSION_TYPE_XVNC:
                xserver_params = prepare_xvnc_xserver_params(baseobj->params,
                                 authfile, passwd_file, NULL);
                break;

            case SCP_SESSION_TYPE_XVNC_UDS:
                g_snprintf(port, sizeof(port), XRDP_X11RDP_STR,
                           login_info->uid, baseobj->params->display);
                xserver_params = prepare_xvnc_xserver_params(baseobj->params,
                                 authfile, NULL, port);
                break;

            default:
                unknown_session_type = 1;
        }

        if (xserver_params == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory allocating X server params");
        }
        else if (unknown_session_type)
        {
            LOG(LOG_LEVEL_ERROR, "Unknown session type: %d",
                baseobj->params->type);
        }
        else
        {
            /* fire up X server */
            LOG(LOG_LEVEL_INFO, "Starting X server on display %u: %s",
                baseobj->params->display,
                dumpItemsToString(xserver_params, execvpparams, 2048));
            LOG_DEVEL_LEAKING_FDS("X server", 3, -1);
            g_execvp_list((const char *)xserver_params->items[0],
                          xserver_params);
        }
    }

    /* should not get here */
    g_free(passwd_file);
    list_delete(xserver_params);
    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
        "to start the X server on display %u, aborting connection",
        baseobj->params->display);
}

/******************************************************************************/
static enum scp_screate_status
start(struct session_data *baseobj,
      struct login_info *login_info,
      const struct session_parameters *s)
{
    int chansrv_pid;
    int display_pid;
    int window_manager_pid;
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;

    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    /* start the X server in a new process group.
     *
     * We group the X server, window manager and chansrv in a single
     * process group, as it allows signals to be sent to the user session
     * without affecting sesexec (and vice-versa). This is particularly
     * important when debugging sesexec as we don't want a SIGINT in
     * the debugger to be passed to the children */
    display_pid = session_base_fork_child(&self->base, login_info, 0,
                                          start_x_server, NULL);
    if (display_pid > 0)
    {
        enum xwait_status xws;
        xws = wait_for_xserver(login_info->uid,
                               g_cfg->env_names,
                               g_cfg->env_values,
                               s->display);

        if (xws != XW_STATUS_OK)
        {
            switch (xws)
            {
                case XW_STATUS_TIMED_OUT:
                    LOG(LOG_LEVEL_ERROR, "Timed out waiting for X server");
                    break;
                case XW_STATUS_FAILED_TO_START:
                    LOG(LOG_LEVEL_ERROR, "X server failed to start");
                    break;
                default:
                    LOG(LOG_LEVEL_ERROR,
                        "An error occurred waiting for the X server");
            }
            status = E_SCP_SCREATE_X_SERVER_FAIL;
            /* Kill it anyway in case it did start and we just failed to
             * pick up on it */
            g_sigterm(display_pid);
            g_waitpid(display_pid);
        }
        else
        {
            LOG(LOG_LEVEL_INFO, "X server :%d is working", s->display);
            LOG(LOG_LEVEL_INFO, "Starting window manager for display :%d",
                s->display);

            window_manager_pid = session_base_fork_child(baseobj, login_info,
                                 display_pid, start_window_manager,
                                 NULL);
            if (window_manager_pid < 0)
            {
                g_sigterm(display_pid);
                g_waitpid(display_pid);
            }
            else
            {
                utmp_login(window_manager_pid, s->display, login_info);
                LOG(LOG_LEVEL_INFO,
                    "Starting the xrdp channel server for display :%d",
                    s->display);

                chansrv_pid = session_base_fork_child(
                                  baseobj, login_info, display_pid,
                                  session_base_start_chansrv, NULL);

                self->win_mgr_pid = window_manager_pid;
                self->x_server_pid = display_pid;

                // Set the base class member variables we are responsible for
                self->base.chansrv_pid = chansrv_pid;
                self->base.start_time = time(NULL);

                if (session_base_process_startup_wait_time(&self->base) == 0)
                {
                    // Tell the caller we've started
                    LOG(LOG_LEVEL_INFO,
                        "Session in progress on display :%d. Waiting until the "
                        "window manager (pid %d) exits to end the session",
                        s->display, window_manager_pid);

                    status = E_SCP_SCREATE_OK;
                }
                else
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Session failed during startup wait time");
                    status = E_SCP_SCREATE_SESSION_FAIL;
                }
            }
        }
    }

    return status;
}

/******************************************************************************/
static void
sess_free(struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    if (self != NULL)
    {
#ifdef USE_DEVEL_LOGGING
        if (self->win_mgr_pid > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid window manager PID %d",
                      self->win_mgr_pid);
        }
        if (self->x_server_pid > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid X server PID %d",
                      self->x_server_pid);
        }
#endif
    }
}


/******************************************************************************/
static int
cleanup_sockets(struct session_data_x11 *self)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "cleanup_sockets:");

    char file[XRDP_SOCKETS_MAXPATH];

    int uid = g_login_info->uid;
    int display = self->base.params->display;

    // Cleanup sockets used by the base display class
    int error = session_base_cleanup_sockets(&self->base);

    /* the following files should be deleted by xorgxrdp
     * but just in case the deletion failed */

    g_snprintf(file, sizeof(file), XRDP_X11RDP_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    g_snprintf(file, sizeof(file), XRDP_DISCONNECT_STR, uid, display);
    if (g_file_exist(file))
    {
        LOG(LOG_LEVEL_DEBUG, "cleanup_sockets: deleting %s", file);
        if (g_file_delete(file) == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "cleanup_sockets: failed to delete %s (%s)",
                file, g_get_strerror());
            error++;
        }
    }

    return error;
}

/******************************************************************************/
static void
exit_status_to_str(const struct proc_exit_status *e, char buff[], int bufflen)
{
    switch (e->reason)
    {
        case E_PXR_STATUS_CODE:
            if (e->val == 0)
            {
                g_snprintf(buff, bufflen, "exit code zero");
            }
            else
            {
                g_snprintf(buff, bufflen, "non-zero exit code %d", e->val);
            }
            break;

        case E_PXR_SIGNAL:
        {
            char sigstr[MAXSTRSIGLEN];
            g_snprintf(buff, bufflen, "signal %s",
                       g_sig2text(e->val, sigstr));
        }
        break;

        default:
            g_snprintf(buff, bufflen, "an unexpected error");
            break;
    }
}

/******************************************************************************/
static unsigned int
active_processes(const struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    return (self->win_mgr_pid > 0) +
           (self->x_server_pid > 0) + (self->base.chansrv_pid > 0);
}

/******************************************************************************/
/**
 * Processes an exited child
 *
 * The PID of the child process is removed from the session_data.
 *
 * @param self session_data object
 * @param pid PID of exited process
 * @param e Exit status of the exited process
 */
static void
process_child_exit(struct session_data *baseobj,
                   int pid,
                   const struct proc_exit_status *e)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    if (pid == self->x_server_pid)
    {
        LOG(LOG_LEVEL_INFO, "X server pid %d on display :%d finished",
            self->x_server_pid, self->base.params->display);
        self->x_server_pid = -1;
        // No other action - window manager should be going soon
    }
    else if (pid == self->win_mgr_pid)
    {
        int wm_wait_time = time(NULL) - self->base.start_time;

        if (e->reason == E_PXR_STATUS_CODE && e->val == 0)
        {
            LOG(LOG_LEVEL_INFO,
                "Window manager (pid %d, display %d) "
                "finished normally in %d secs",
                self->win_mgr_pid, self->base.params->display, wm_wait_time);
        }
        else
        {
            char reason[128];
            exit_status_to_str(e, reason, sizeof(reason));

            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                "exited with %s. This "
                "could indicate a window manager config problem",
                self->win_mgr_pid, self->base.params->display, reason);
        }
        if (wm_wait_time < 10)
        {
            /* This could be a config issue. Log a significant error */
            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                "exited quickly (%d secs). This could indicate a window "
                "manager config problem",
                self->win_mgr_pid, self->base.params->display, wm_wait_time);
        }

        utmp_logout(self->win_mgr_pid, self->base.params->display, e);
        self->win_mgr_pid = -1;

        if (self->x_server_pid > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating X server (pid %d) on display :%d",
                self->x_server_pid, self->base.params->display);
            g_sigterm(self->x_server_pid);
        }

        if (self->base.chansrv_pid > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                "on display :%d",
                self->base.chansrv_pid, self->base.params->display);
            g_sigterm(self->base.chansrv_pid);
        }
    }

    if (active_processes(&self->base) == 0)
    {
        cleanup_sockets(self);
    }
}

/******************************************************************************/
static int
main_sess_proc_active(const struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    return (self->win_mgr_pid > 0);
}

/******************************************************************************/
static int
x11_getpgid(const struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    return self->x_server_pid;
}

/******************************************************************************/
static void
send_term(struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;
    if (self->win_mgr_pid > 0)
    {
        // Killing the window manager only is appropriate here.
        // When we process SIGCHLD for the window manager, we
        // will kill other processes as appropriate
        g_sigterm(self->win_mgr_pid);
    }
}

/******************************************************************************/
static int
get_display_server_fd(
    const struct session_data *baseobj,
    const struct login_info *login_info)
{
    char portname[XRDP_SOCKETS_MAXPATH];
    const char *localhost = "localhost"; // Ignored for TRANS_MODE_UNIX
    int socket_mode;

    int rv = -1;

    // Downcast the base object pointer to a pointer to the X11 session object
    struct session_data_x11 *self = (struct session_data_x11 *)baseobj;

    if (self->x_server_pid <= 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Request to connect to display server :%u"
            " which has exited", self->base.params->display);
    }
    else
    {
        switch (self->base.params->type)
        {
            case SCP_SESSION_TYPE_XVNC:
                socket_mode = TRANS_MODE_TCP;
                snprintf(portname, sizeof(portname), "%u",
                         5900 + self->base.params->display);
                break;

            case SCP_SESSION_TYPE_XVNC_UDS:
            case SCP_SESSION_TYPE_XORG:
                socket_mode = TRANS_MODE_UNIX;
                snprintf(portname, sizeof(portname), XRDP_X11RDP_STR,
                         login_info->uid, (int)self->base.params->display);

                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Unsupported session type %d for connect",
                    self->base.params->type);
                portname[0] = '\0';
        }

        if (portname[0] != '\0')
        {
            // Use the transport library to get the fd
            struct trans *t = trans_create(socket_mode, 8 * 8192, 8192);
            if (t == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Out of memory creating transport");
            }
            else if (trans_connect(t, localhost, portname, 3000) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Can't connect to display server :%u [%s]",
                    self->base.params->display,
                    g_get_strerror());
            }
            else
            {
                rv = t->sck;
                t->sck = -1;
            }
            trans_delete(t);
        }
    }

    return rv;
}

/******************************************************************************/
static const struct session_data_vtable
    x11_vtable =
{
    .start = start,
    .free = sess_free,
    .process_child_exit = process_child_exit,
    .active_processes = active_processes,
    .main_sess_proc_active = main_sess_proc_active,
    .getpgid = x11_getpgid,
    .send_term = send_term,
    .get_display_server_fd = get_display_server_fd
};

/******************************************************************************/
/**
 * Create a new session_data structure from a session_parameters object
 *
 * @param sp Session parameters passed to session_start()
 * @return semi-initialised session_data struct
 */
struct session_data_x11 *
session_x11_new(void)
{
    struct session_data_x11 *self;
    self = (struct session_data_x11 *)g_malloc(sizeof(*self), 0);

    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory allocating session data struct");
    }
    else
    {
        self->base.vtable = &x11_vtable;
        self->win_mgr_pid = -1;
        self->x_server_pid = -1;
    }

    return self;
}
