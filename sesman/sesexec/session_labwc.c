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
 * @file session_labwc.h
 * @brief Derived class for labwc session objects
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <errno.h>

#include "arch.h"
#include "session_labwc.h"
#include "session_parameters.h"

#include "sesman_auth.h"
#include "sesman_config.h"
#include "display_server_util.h"
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
                    "display :%d with embedded arguments using a shell: %s",
                    s->x11_display, s->shell);
                const char *argv[] = {"sh", "-c", s->shell, NULL};
                g_execvp("/bin/sh", (char **)argv);
            }
            else
            {
                LOG(LOG_LEVEL_INFO,
                    "Using user requested window manager on "
                    "display :%d : %s", s->x11_display, s->shell);
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
        LOG(LOG_LEVEL_DEBUG, "The user session on display :%d did not"
            " request a specific window manager", s->x11_display);
    }

    /* try to execute user window manager if enabled */
    if (g_cfg->enable_user_wm)
    {
        g_snprintf(text, sizeof(text), "%s/%s",
                   g_getenv("HOME"), g_cfg->user_wm);
        if (g_file_exist(text))
        {
            LOG(LOG_LEVEL_INFO,
                "Using window manager on display :%d from user"
                " home directory: %s", s->x11_display, text);
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
        "Using the default window manager on display :%d : %s",
        s->x11_display, g_cfg->default_wm);
    g_execlp3(g_cfg->default_wm, g_cfg->default_wm, 0);

    /* still a problem starting window manager just start xterm */
    LOG(LOG_LEVEL_WARNING,
        "No window manager on display :%d started, "
        "so falling back to starting xterm for user debugging",
        s->x11_display);
    g_execlp3("xterm", "xterm", 0);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
        "the window manager on display :%d, aborting connection",
        s->x11_display);
}

/*****************************************************************************/
/*
 * Opens a log file on stdout and stderr
 * @param name Unqualified name
 * @return != 0 for error
 *
 * @pre Current directory is $HOME
 *
 * The file is either opened or not. If not, an error is logged on the
 * standard mechanisms
 */
static int
redirect_stdout_stderr(const char *name)
{
    char path[256];
    char *log_path;
    int rv;

    rv = 1;
    // Use the log path if it exists
    if ((log_path = g_cfg->labwc.log_file_path) != NULL && log_path[0] != '\0')
    {
        char uidstr[64];
        char username[64];
        const struct info_string_tag map[] =
        {
            {'u', uidstr},
            {'U', username},
            INFO_STRING_END_OF_LIST
        };

        int uid = g_getuid();
        g_snprintf(uidstr, sizeof(uidstr), "%d", uid);
        if (g_getlogin(username, sizeof(username)) != 0)
        {
            /* Fall back to UID */
            g_strncpy(username, uidstr, sizeof(username) - 1);
        }

        (void)g_format_info_string(path, sizeof(path), log_path, map);
        if (g_create_path(path))
        {
            rv = 0;
        }
    }
    else if ((log_path = g_getenv("XDG_DATA_HOME")) != NULL &&
             log_path[0] != '\0')
    {
        g_snprintf(path, sizeof(path), "%s/xrdp", log_path);
        if (g_create_path(path))
        {
            rv = 0;
        }
    }

    // Always fall back to the home directory
    if (rv != 0)
    {
        strlcpy(path, ".local/share/xrdp", sizeof(path));
        if (g_create_path(path))
        {
            rv = 0;
        }
    }

    if (rv == 0)
    {
        /* Add the name to the end */
        unsigned int len = strlen(path);
        if (len < sizeof(path))
        {
            g_snprintf(path + len, sizeof(path) - len, "/%s", name);
        }

        int fd = g_file_open_rw(path);
        if (fd >= 0)
        {
            g_file_duplicate_on(fd, 1);
            g_file_duplicate_on(fd, 2);
            g_file_close(fd);
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "Can't open %s for writing [%s]",
                path, g_get_strerror());
            rv = 1;
        }
    }
    return rv;
}

/******************************************************************************/
/* Either execs wayvnc, or returns */
static void
start_wayvnc(struct session_data *baseobj,
             const struct login_info *login_info,
             void *closure /* unused */)
{
    char execvpparams[2048];
    char sockname[XRDP_SOCKETS_MAXPATH];

    env_set_user(login_info->uid,
                 0,
                 g_cfg->env_names,
                 g_cfg->env_values);

    /* Add environment variables for wayvnc */
    auth_set_env(login_info->auth_info);

    /* get path of wayvnc from config */
    const char *wayvnc = g_cfg->labwc.wayvnc_exe;
    if (wayvnc == NULL || wayvnc[0] == '\0')
    {
        wayvnc = "wayvnc";
    }

    /* Get the VNC socket name */
    g_snprintf(sockname, sizeof(sockname), XRDP_X11RDP_STR,
               login_info->uid, baseobj->display);
    if (g_file_exist(sockname))
    {
        (void)g_file_delete(sockname);
    }

    struct list *params = list_create();
    if (params == NULL ||
            (!(params->auto_free = 1)) || // Just set 'auto_free' inline
            !list_add_strdup_multi(params,
                                   wayvnc,
                                   "-u",
                                   sockname,
                                   NULL))
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory allocating wayvnc params");
    }
    else
    {
        /* fire up wayvnc */
        LOG(LOG_LEVEL_INFO, "Starting wayvnc: %s",
            dumpItemsToString(params, execvpparams, 2048));
        LOG_DEVEL_LEAKING_FDS("wayvnc", 3, -1);

        /* Logging? */
        if (g_cfg->labwc.enable_wayvnc_log)
        {
            char name[64];
            g_snprintf(name, sizeof(name),
                       "wayvnc.%s.log", baseobj->display);
            redirect_stdout_stderr(name);
            (void)list_add_strdup_multi(params, "-L", "debug", NULL);
        }

        g_execvp_list((const char *)params->items[0],
                      params);

        /* should not get here */
        LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
            "to start wayvnc [%s]", g_get_strerror());
    }
    list_delete(params);
}

/******************************************************************************/
/* Either execs labwc, or returns */
static void
start_labwc(struct session_data *baseobj,
            const struct login_info *login_info,
            void *closure /* unused */)
{
    char execvpparams[2048];

    env_set_user(login_info->uid,
                 0,
                 g_cfg->env_names,
                 g_cfg->env_values);

    /* Add basic environment variables for labwc */
    auth_set_env(login_info->auth_info);

    // XDG_RUNTIME_DIR is needed to store the WAYLAND_DISPLAY name
    const char *xdg_runtime_dir = g_getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir == NULL || xdg_runtime_dir[0] == '\0')
    {
        LOG(LOG_LEVEL_ERROR, "XDG_RUNTIME_DIR is not defined");
    }
    else if (!g_directory_exist(xdg_runtime_dir))
    {
        LOG(LOG_LEVEL_ERROR, "XDG_RUNTIME_DIR [%s] is not valid",
            xdg_runtime_dir);
    }
    else
    {
        /* Add other environment variables */
        // ...Run with no backing hardware
        g_setenv("WLR_BACKENDS", "headless", 1);
        // ...Always have an output
        g_setenv("LABWC_FALLBACK_OUTPUT", "NOOP-fallback", 1);
        // ...Update dbus-daemon session variables
        // (needed if a session is active)
        g_setenv("LABWC_UPDATE_ACTIVATION_ENV", "1", 1);

        /* get path of labwc from config */
        const char *labwc = g_cfg->labwc.labwc_exe;
        if (labwc == NULL || labwc[0] == '\0')
        {
            labwc = "labwc";
        }
        struct list *params = list_create();
        if (params == NULL ||
                (!(params->auto_free = 1)) || // Just set 'auto_free' inline
                !list_add_strdup_multi(params,
                                       labwc,
                                       "--startup",
                                       XRDP_LIBEXEC_PATH "/labwc-getenv",
                                       NULL))
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory allocating labwc params");
        }
        else
        {
            /* fire up labwc */
            LOG(LOG_LEVEL_INFO, "Starting labwc: %s",
                dumpItemsToString(params, execvpparams, 2048));
            LOG_DEVEL_LEAKING_FDS("labwc", 3, -1);

            /* Logging? */
            if (g_cfg->labwc.enable_labwc_log)
            {
                redirect_stdout_stderr("labwc.log");
                (void)list_add_strdup_multi(params, "-d", NULL);
            }

            g_execvp_list((const char *)params->items[0],
                          params);

            /* should not get here */
            LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
                "to start labwc [%s]", g_get_strerror());
        }
        list_delete(params);
    }
}

/******************************************************************************/
/**
 * Used by the labwc waiter to set environment variables
 * @param name Name of environment variable to set
 * @param value Value to give 'name'
 * @param closure Used to locate the labwc session object
 */
static void
labwc_setenvvar(const char *name,
                const char *value,
                void *closure)
{
    // Cast the closure to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)closure;

    /* Add variables to the configured environment */
    if (!list_add_strdup(g_cfg->env_names, name) ||
            !list_add_strdup(g_cfg->env_values, value))
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory adding %s to env", name);
    }
    else
    {
        // Check to set the global display name for the object
        if (strcmp(name, "WAYLAND_DISPLAY") == 0)
        {
            strlcpy(self->base.display, value, sizeof(self->base.display));
        }
    }
}

/******************************************************************************/
/**
 * Waits for labwc to start using a helper prog
 *
 * @param self session_data_labwc object
 * @param login_info Login information
 * @param env_names List of environment variables for the helper (names)
 * @param env_values List of environment variables for the helper (values)
 *
 * @post If DS_STATUS_OK is returned, the environment will be modified, and
 *       self->base.name should be set to the WAYLAND_DISPLAY
 *
 */
static enum display_server_status
wait_for_labwc(struct session_data_labwc *self,
               struct login_info *login_info,
               const struct list *env_names,
               const struct list *env_values)
{
    enum display_server_status rv = DS_STATUS_MISC_ERROR;
    struct list *cmd = list_create();
    if (cmd == NULL ||
            (!(cmd->auto_free = 1)) ||  // Just set 'auto_free' inline
            !list_add_strdup_multi(cmd,
                                   XRDP_LIBEXEC_PATH "/waitforw",
                                   NULL))
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory running waitforw");
    }
    else
    {
        rv = wait_for_display_server(login_info,
                                     env_names, env_values, cmd,
                                     labwc_setenvvar, self);
    }

    list_delete(cmd);
    return rv;
}

/******************************************************************************/
static enum scp_screate_status
start(struct session_data *baseobj,
      struct login_info *login_info,
      const struct session_parameters *s)
{
    int chansrv_pid;
    int labwc_pid;
    int wayvnc_pid;
    int window_manager_pid;
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;

    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    /* start labwc in a new process group.
     *
     * We group all the session processes in a single process group, as
     * it allows signals to be sent to the user session without affecting
     * sesexec (and vice-versa). This is particularly important when
     * debugging sesexec as we don't want a SIGINT in the debugger to
     * be passed to the children */
    labwc_pid = session_base_fork_child(&self->base, login_info, 0,
                                        start_labwc, NULL);
    if (labwc_pid > 0)
    {
        enum display_server_status dss;
        dss = wait_for_labwc(self, login_info,
                             g_cfg->env_names, g_cfg->env_values);

        if (dss != DS_STATUS_OK)
        {
            switch (dss)
            {
                case DS_STATUS_TIMED_OUT:
                    LOG(LOG_LEVEL_ERROR, "Timed out waiting for labwc");
                    break;
                case DS_STATUS_FAILED_TO_START:
                    LOG(LOG_LEVEL_ERROR, "labwc failed to start");
                    break;
                default:
                    LOG(LOG_LEVEL_ERROR,
                        "An error occurred waiting for labwc");
            }
            status = E_SCP_SCREATE_LABWC_FAIL;
            /* Kill it anyway in case it did start and we just failed to
             * pick up on it */
            g_sigterm(labwc_pid);
            g_waitpid(labwc_pid);
        }
        else if (self->base.display[0] == '\0')
        {
            LOG(LOG_LEVEL_ERROR, "labwc failed to set WAYLAND_DISPLAY");
            status = E_SCP_SCREATE_LABWC_FAIL;
            g_sigterm(labwc_pid);
            g_waitpid(labwc_pid);
        }
        else
        {
            LOG(LOG_LEVEL_INFO, "labwc is working on %s", self->base.display);

            LOG(LOG_LEVEL_INFO, "Starting wayvnc for display %s",
                self->base.display);

            wayvnc_pid = session_base_fork_child(baseobj, login_info,
                                                 labwc_pid, start_wayvnc,
                                                 NULL);
            if (wayvnc_pid < 0)
            {
                status = E_SCP_SCREATE_WAYVNC_FAIL;
                g_sigterm(labwc_pid);
                g_waitpid(labwc_pid);
            }
            else
            {
                LOG(LOG_LEVEL_INFO, "Starting window manager for display %s",
                    self->base.display);

                window_manager_pid = session_base_fork_child(baseobj,
                                     login_info,
                                     labwc_pid, start_window_manager,
                                     NULL);
                if (window_manager_pid < 0)
                {
                    g_sigterm(labwc_pid);
                    g_waitpid(labwc_pid);
                    g_sigterm(wayvnc_pid);
                    g_waitpid(wayvnc_pid);
                }
                else
                {
                    utmp_login(window_manager_pid, self->base.display,
                               login_info);
                    LOG(LOG_LEVEL_INFO,
                        "Starting the xrdp channel server for display %s",
                        self->base.display);

                    chansrv_pid = session_base_fork_child(
                                      baseobj, login_info, labwc_pid,
                                      session_base_start_chansrv, NULL);

                    self->win_mgr_pid = window_manager_pid;
                    self->labwc_pid = labwc_pid;
                    self->wayvnc_pid = wayvnc_pid;

                    // Set the rest of the base class member variables we
                    // are responsible for
                    self->base.chansrv_pid = chansrv_pid;
                    self->base.start_time = time(NULL);

                    if (session_base_process_startup_wait_time(&self->base) == 0)
                    {
                        // Tell the caller we've started
                        LOG(LOG_LEVEL_INFO,
                            "Session in progress on display %s. Waiting until the "
                            "window manager (pid %d) exits to end the session",
                            self->base.display, window_manager_pid);

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
    }

    return status;
}

/******************************************************************************/
static void
sess_free(struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    if (self != NULL)
    {
#ifdef USE_DEVEL_LOGGING
        if (self->labwc_pid > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid labwc PID %d",
                      self->labwc_pid);
        }
        if (self->wayvnc_pid > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid wayvnc PID %d",
                      self->wayvnc_pid);
        }
#endif
    }
}


/******************************************************************************/
static int
cleanup_sockets(struct session_data_labwc *self)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "cleanup_sockets:");

    char file[XRDP_SOCKETS_MAXPATH];

    int uid = g_login_info->uid;
    char *display = STRIP_COLON(self->base.display);

    // Cleanup sockets used by the base display class
    int error = session_base_cleanup_sockets(&self->base);

    if (self->base.params->type == SCP_SESSION_TYPE_LABWC_OVER_VNC)
    {
        /* Clean up the VNC socket */
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
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    return (self->base.chansrv_pid > 0) + (self->labwc_pid > 0) +
           (self->wayvnc_pid > 0) + (self->win_mgr_pid > 0);
}

/******************************************************************************/
/**
 * Processes an exited child
 *
 * The PID of the child process is removed from the session_data.
 *
 * @param baseobj session_data object
 * @param pid PID of exited process
 * @param e Exit status of the exited process
 */
static void
process_child_exit(struct session_data *baseobj,
                   int pid,
                   const struct proc_exit_status *e)
{
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    if (pid == self->labwc_pid)
    {
        // Don't log the exit if we haven't got as far as registering
        // the display name
        if (baseobj->display[0] != '\0')
        {
            LOG(LOG_LEVEL_INFO, "labwc pid %d on display %s finished",
                self->labwc_pid, baseobj->display);
        }
        self->labwc_pid = -1;
        // No other action - window manager should be going soon
    }
    else if (pid == self->wayvnc_pid)
    {
        LOG(LOG_LEVEL_INFO, "wayvnc pid %d on display %s finished",
            self->wayvnc_pid, baseobj->display);
        self->wayvnc_pid = -1;
        // No other action - window manager should be going soon
    }
    else if (pid == self->win_mgr_pid)
    {
        int wm_wait_time = time(NULL) - self->base.start_time;

        if (e->reason == E_PXR_STATUS_CODE && e->val == 0)
        {
            LOG(LOG_LEVEL_INFO,
                "Window manager (pid %d, display %s) "
                "finished normally in %d secs",
                self->win_mgr_pid, baseobj->display, wm_wait_time);
        }
        else
        {
            char reason[128];
            exit_status_to_str(e, reason, sizeof(reason));

            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %s) "
                "exited with %s. This "
                "could indicate a window manager config problem",
                self->win_mgr_pid, baseobj->display, reason);
        }
        if (wm_wait_time < 10)
        {
            /* This could be a config issue. Log a significant error */
            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %s) "
                "exited quickly (%d secs). This could indicate a window "
                "manager config problem",
                self->win_mgr_pid, baseobj->display, wm_wait_time);
        }

        utmp_logout(self->win_mgr_pid, self->base.display, e);
        self->win_mgr_pid = -1;

        if (self->labwc_pid > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating labwc (pid %d) on display %s",
                self->labwc_pid, baseobj->display);
            g_sigterm(self->labwc_pid);
        }

        if (self->wayvnc_pid > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating wayvnc (pid %d) on display %s",
                self->wayvnc_pid, baseobj->display);
            g_sigterm(self->wayvnc_pid);
        }

        if (self->base.chansrv_pid > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                "on display %s",
                self->base.chansrv_pid, baseobj->display);
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
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    // For labwc at present, check the wayvnc server too
    return (self->win_mgr_pid > 0 && self->labwc_pid > 0);
}

/******************************************************************************/
static int
labwc_getpgid(const struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    return self->labwc_pid;
}

/******************************************************************************/
static void
send_term(struct session_data *baseobj)
{
    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;
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

    int rv = -1;

    // Downcast the base object pointer to a pointer to the labwc session object
    struct session_data_labwc *self = (struct session_data_labwc *)baseobj;

    portname[0] = '\0';

    switch (self->base.params->type)
    {
        case SCP_SESSION_TYPE_LABWC_OVER_VNC:
            if (self->wayvnc_pid <= 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Request to connect to VNC display server %s"
                    " which has exited", baseobj->display);
            }
            else
            {
                g_snprintf(portname, sizeof(portname), XRDP_X11RDP_STR,
                           login_info->uid, STRIP_COLON(self->base.display));
            }
            break;

        default:
            LOG(LOG_LEVEL_ERROR, "Unsupported session type %d for connect",
                self->base.params->type);
    }

    if (portname[0] != '\0')
    {
        // Use the transport library to get the fd
        struct trans *t = trans_create(TRANS_MODE_UNIX, 8 * 8192, 8192);
        if (t == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory creating transport");
        }
        else if (trans_connect(t, NULL, portname, 3000) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't connect to display server %s [%s]",
                baseobj->display,
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

/******************************************************************************/
static const struct session_data_vtable
    labwc_vtable =
{
    .start = start,
    .free = sess_free,
    .process_child_exit = process_child_exit,
    .active_processes = active_processes,
    .main_sess_proc_active = main_sess_proc_active,
    .getpgid = labwc_getpgid,
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
struct session_data_labwc *
session_labwc_new(void)
{
    struct session_data_labwc *self;
    self = (struct session_data_labwc *)g_malloc(sizeof(*self), 0);

    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory allocating session data struct");
    }
    else
    {
        self->base.vtable = &labwc_vtable;
        self->labwc_pid = -1;
        self->wayvnc_pid = -1;
        self->win_mgr_pid = -1;
    }

    return self;
}
