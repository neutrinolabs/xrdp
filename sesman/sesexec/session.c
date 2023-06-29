/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * @brief Session management code
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <errno.h>

#include "arch.h"
#include "session.h"

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
#include "xauth.h"
#include "xwait.h"
#include "xrdp_sockets.h"

struct session_data
{
    pid_t x_server; ///< PID of X server
    pid_t win_mgr; ///< PID of window manager
    pid_t chansrv; //< PID of chansrv
    time_t start_time;
    struct session_parameters params;
    // Flexible array member used to store strings in params and ip_addr;
#ifdef __cplusplus
    char strings[1];
#else
    char strings[];
#endif
};

/******************************************************************************/
/**
 * Create a new session_data structure from a session_parameters object
 *
 * @param sp Session parameters passed to session_start()
 * @return semi-initialised session_data struct
 */
static struct session_data *
session_data_new(const struct session_parameters *sp)
{
    unsigned int string_length = 0;
    // What string length do we need?
    string_length += g_strlen(sp->shell) + 1;
    string_length += g_strlen(sp->directory) + 1;

    struct session_data *sd = (struct session_data *)g_malloc(sizeof(*sd) + string_length, 0);

    if (sd == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory allocating session data struct");
    }
    else
    {
        sd->win_mgr = -1;
        sd->x_server = -1;
        sd->chansrv = -1;
        sd->start_time = 0;

        /* Copy all the non-string session parameters... */
        sd->params = *sp;

        /* ...and then the strings */
        char *memptr = sd->strings;

#define COPY_STRING(dest,src) \
    (dest) = memptr; \
    strcpy(memptr, src); \
    memptr += strlen(memptr) + 1

        COPY_STRING(sd->params.shell, sp->shell);
        COPY_STRING(sd->params.directory, sp->directory);

#undef COPY_STRING
    }

    return sd;
}

/******************************************************************************/
void
session_data_free(struct session_data *session_data)
{
    if (session_data != NULL)
    {
#ifdef USE_DEVEL_LOGGING
        if (session_data->win_mgr > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid window manager PID %d",
                      session_data->win_mgr);
        }
        if (session_data->x_server > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid X server PID %d",
                      session_data->x_server);
        }
        if (session_data->chansrv > 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Freeing session data with valid chansrv PID %d",
                      session_data->chansrv);
        }
#endif

        free(session_data);
    }
}

/******************************************************************************/
/**
 * Creates a string consisting of all parameters that is hosted in the param list
 * @param self
 * @param outstr, allocate this buffer before you use this function
 * @param len the allocated len for outstr
 * @return
 */
char *
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
start_chansrv(struct login_info *login_info,
              const struct session_parameters *s)
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
        env_set_user(login_info->uid, 0, s->display,
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
static void
start_window_manager(struct login_info *login_info,
                     const struct session_parameters *s)
{
    char text[256];

    env_set_user(login_info->uid,
                 0,
                 s->display,
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
        g_setenv("XRDP_START_WIDTH", text, 1);

        g_snprintf(text, sizeof(text), "%d", s->height);
        g_setenv("XRDP_START_HEIGHT", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_idle_time);
        g_setenv("XRDP_SESMAN_MAX_IDLE_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_disc_time);
        g_setenv("XRDP_SESMAN_MAX_DISC_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.kill_disconnected);
        g_setenv("XRDP_SESMAN_KILL_DISCONNECTED", text, 1);

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
static struct list *
prepare_xvnc_xserver_params(const struct session_parameters *s,
                            const char *authfile,
                            const char *passwd_file)
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
                              "-rfbauth", passwd_file,
                              NULL);

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
start_x_server(struct login_info *login_info,
               const struct session_parameters *s)
{
    char authfile[256]; /* The filename for storing xauth information */
    char execvpparams[2048];
    char *passwd_file = NULL;
    struct list *xserver_params = NULL;
    int unknown_session_type = 0;

    if (s->type == SCP_SESSION_TYPE_XVNC)
    {
        env_set_user(login_info->uid,
                     &passwd_file,
                     s->display,
                     g_cfg->env_names,
                     g_cfg->env_values);
    }
    else
    {
        env_set_user(login_info->uid,
                     0,
                     s->display,
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
    if (add_xauth_cookie(s->display, authfile) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Error setting the xauth cookie for display %u in file %s",
            s->display, authfile);
    }
    else
    {
        switch (s->type)
        {
            case SCP_SESSION_TYPE_XORG:
                xserver_params = prepare_xorg_xserver_params(s, authfile);
                break;

            case SCP_SESSION_TYPE_XVNC:
                xserver_params = prepare_xvnc_xserver_params(s, authfile,
                                 passwd_file);
                break;

            default:
                unknown_session_type = 1;
        }

        g_free(passwd_file);
        passwd_file = NULL;

        if (xserver_params == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory allocating X server params");
        }
        else if (unknown_session_type)
        {
            LOG(LOG_LEVEL_ERROR, "Unknown session type: %d",
                s->type);
        }
        else
        {
            /* fire up X server */
            LOG(LOG_LEVEL_INFO, "Starting X server on display %u: %s",
                s->display,
                dumpItemsToString(xserver_params, execvpparams, 2048));
            LOG_DEVEL_LEAKING_FDS("X server", 3, -1);
            g_execvp_list((const char *)xserver_params->items[0],
                          xserver_params);
        }
    }

    /* should not get here */
    list_delete(xserver_params);
    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
        "to start the X server on display %u, aborting connection",
        s->display);
}

/******************************************************************************/
/*
 * Simple helper process to fork a child and log errors */
static int
fork_child(
    void (*runproc)(struct login_info *, const struct session_parameters *),
    struct login_info *login_info,
    const struct session_parameters *s,
    pid_t group_pid)
{
    int pid = g_fork();
    if (pid == 0)
    {
        /* Child process */
        if (group_pid >= 0)
        {
            (void)g_setpgid(0, group_pid);
        }
        runproc(login_info, s);
        g_exit(0);
    }

    if (pid < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Fork failed [%s]", g_get_strerror());
    }

    return pid;
}

/******************************************************************************/
enum scp_screate_status
session_start_wrapped(struct login_info *login_info,
                      const struct session_parameters *s,
                      struct session_data *sd)
{
    int chansrv_pid;
    int display_pid;
    int window_manager_pid;
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;

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

    /* start the X server in a new process group.
     *
     * We group the X server, window manager and chansrv in a single
     * process group, as it allows signals to be sent to the user session
     * without affecting sesexec (and vice-versa). This is particularly
     * important when debugging sesexec as we don't want a SIGINT in
     * the debugger to be passed to the children */
    display_pid = fork_child(start_x_server, login_info, s, 0);
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

            window_manager_pid = fork_child(start_window_manager,
                                            login_info, s, display_pid);
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

                chansrv_pid = fork_child(start_chansrv, login_info,
                                         s, display_pid);

                // Tell the caller we've started
                LOG(LOG_LEVEL_INFO,
                    "Session in progress on display :%d. Waiting until the "
                    "window manager (pid %d) exits to end the session",
                    s->display, window_manager_pid);

                sd->win_mgr = window_manager_pid;
                sd->x_server = display_pid;
                sd->chansrv = chansrv_pid;
                sd->start_time = g_time1();
                status = E_SCP_SCREATE_OK;
            }
        }
    }

    return status;
}


/******************************************************************************/
enum scp_screate_status
session_start(struct login_info *login_info,
              const struct session_parameters *sp,
              struct session_data **session_data)
{
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;
    /* Create the session_data struct first */
    struct session_data *sd = session_data_new(sp);
    if (sd == NULL)
    {
        status = E_SCP_SCREATE_NO_MEMORY;
    }
    else
    {
        status = session_start_wrapped(login_info, sp, sd);
        if (status == E_SCP_SCREATE_OK)
        {
            *session_data = sd;
        }
        else
        {
            *session_data = NULL;
            session_data_free(sd);
        }
    }

    return status;
}

/******************************************************************************/
static int
cleanup_sockets(int uid, int display)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "cleanup_sockets:");

    char file[XRDP_SOCKETS_MAXPATH];
    int error = 0;

    g_snprintf(file, sizeof(file), CHANSRV_PORT_OUT_STR, uid, display);
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

    g_snprintf(file, sizeof(file), CHANSRV_PORT_IN_STR, uid, display);
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

    g_snprintf(file, sizeof(file), XRDP_CHANSRV_STR, uid, display);
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

    g_snprintf(file, sizeof(file), CHANSRV_API_STR, uid, display);
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
void
session_process_child_exit(struct session_data *sd,
                           int pid,
                           const struct proc_exit_status *e)
{
    if (pid == sd->x_server)
    {
        LOG(LOG_LEVEL_INFO, "X server pid %d on display :%d finished",
            sd->x_server, sd->params.display);
        sd->x_server = -1;
        // No other action - window manager should be going soon
    }
    else if (pid == sd->chansrv)
    {
        LOG(LOG_LEVEL_INFO,
            "xrdp channel server pid %d on display :%d finished",
            sd->chansrv, sd->params.display);
        sd->chansrv = -1;
    }
    else if (pid == sd->win_mgr)
    {
        int wm_wait_time = g_time1() - sd->start_time;

        if (e->reason == E_PXR_STATUS_CODE && e->val == 0)
        {
            LOG(LOG_LEVEL_INFO,
                "Window manager (pid %d, display %d) "
                "finished normally in %d secs",
                sd->win_mgr, sd->params.display, wm_wait_time);
        }
        else
        {
            char reason[128];
            exit_status_to_str(e, reason, sizeof(reason));

            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                "exited with %s. This "
                "could indicate a window manager config problem",
                sd->win_mgr, sd->params.display, reason);
        }
        if (wm_wait_time < 10)
        {
            /* This could be a config issue. Log a significant error */
            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                "exited quickly (%d secs). This could indicate a window "
                "manager config problem",
                sd->win_mgr, sd->params.display, wm_wait_time);
        }

        utmp_logout(sd->win_mgr, sd->params.display, e);
        sd->win_mgr = -1;

        if (sd->x_server > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating X server (pid %d) on display :%d",
                sd->x_server, sd->params.display);
            g_sigterm(sd->x_server);
        }

        if (sd->chansrv > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                "on display :%d", sd->chansrv, sd->params.display);
            g_sigterm(sd->chansrv);
        }
    }

    if (!session_active(sd))
    {
        cleanup_sockets(g_login_info->uid, sd->params.display);
    }
}

/******************************************************************************/
unsigned int
session_active(const struct session_data *sd)
{
    return
        (sd == NULL)
        ? 0
        : (sd->win_mgr > 0) + (sd->x_server > 0) + (sd->chansrv > 0);
}

/******************************************************************************/
time_t
session_get_start_time(const struct session_data *sd)
{
    return (sd == NULL) ? 0 : sd->start_time;
}

/******************************************************************************/
void
session_send_term(struct session_data *sd)
{
    if (sd != NULL && sd->win_mgr > 0)
    {
        g_sigterm(sd->win_mgr);
    }
}

/******************************************************************************/
static void
start_reconnect_script(struct login_info *login_info,
                       const struct session_parameters *s)
{
    env_set_user(login_info->uid, 0, s->display,
                 g_cfg->env_names,
                 g_cfg->env_values);

    auth_set_env(login_info->auth_info);

    if (g_file_exist(g_cfg->reconnect_sh))
    {
        LOG_DEVEL_LEAKING_FDS("reconnect script", 3, -1);

        LOG(LOG_LEVEL_INFO,
            "Starting session reconnection script on display %d: %s",
            s->display, g_cfg->reconnect_sh);
        g_execlp3(g_cfg->reconnect_sh, g_cfg->reconnect_sh, 0);

        /* should not get here */
        LOG(LOG_LEVEL_ERROR,
            "Error starting session reconnection script on display %d: %s",
            s->display, g_cfg->reconnect_sh);
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
session_reconnect(struct login_info *login_info,
                  struct session_data *sd)
{
    if (fork_child(start_reconnect_script,
                   login_info, &sd->params, sd->x_server) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to fork for session reconnection script");
    }
}
