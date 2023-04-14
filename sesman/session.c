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

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
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
#include "os_calls.h"
#include "sesman.h"
#include "string_calls.h"
#include "xauth.h"
#include "xwait.h"
#include "xrdp_sockets.h"

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define USE_EXTRA_SESSION_FORK
#define USE_BSD_SETLOGIN
#endif

/* Module globals */

/* Currently, these duplicate module names in sesman.c. This is fine, and
 * will be fully resolved when sesman is split into two separate excutables */
static tintptr g_term_event = 0;
static tintptr g_sigchld_event = 0;
static int g_pid = 0; // PID of sesexec process (sesman sub-process)

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
set_term_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_term_event);
    }
}

/******************************************************************************/
static void
set_sigchld_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_sigchld_event);
    }
}

/******************************************************************************/
static void
start_chansrv(struct auth_info *auth_info,
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
        env_set_user(s->uid, 0, s->display,
                     g_cfg->env_names,
                     g_cfg->env_values);

        /* executing chansrv */
        g_execvp_list(exe_path, chansrv_params);

        /* should not get here */
        list_delete(chansrv_params);
    }
}

/******************************************************************************/
static int
cleanup_sockets(int display)
{
    LOG(LOG_LEVEL_INFO, "cleanup_sockets:");
    char file[256];
    int error;

    error = 0;

    g_snprintf(file, 255, CHANSRV_PORT_OUT_STR, display);
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

    g_snprintf(file, 255, CHANSRV_PORT_IN_STR, display);
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

    g_snprintf(file, 255, XRDP_CHANSRV_STR, display);
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

    g_snprintf(file, 255, CHANSRV_API_STR, display);
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

    g_snprintf(file, 255, XRDP_X11RDP_STR, display);
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

    g_snprintf(file, 255, XRDP_DISCONNECT_STR, display);
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
start_window_manager(struct auth_info *auth_info,
                     const struct session_parameters *s)
{
    char text[256];

    env_set_user(s->uid,
                 0,
                 s->display,
                 g_cfg->env_names,
                 g_cfg->env_values);

    auth_set_env(auth_info);
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

#ifdef HAVE_SYS_PRCTL_H
        /*
         * Make sure Xorg doesn't run setuid root. Root access is not
         * needed. Xorg can fail when run as root and the user has no
         * console permissions.
         * PR_SET_NO_NEW_PRIVS requires Linux kernel 3.5 and newer.
         */
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "[session start] (display %u): Failed to disable "
                "setuid on X server: %s",
                s->display, g_get_strerror());
        }
#endif

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

        g_setenv("XRDP_SOCKET_PATH", XRDP_SOCKET_PATH, 1);

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
start_x_server(struct auth_info *auth_info,
               const struct session_parameters *s)
{
    char authfile[256]; /* The filename for storing xauth information */
    char execvpparams[2048];
    char *passwd_file = NULL;
    struct list *xserver_params = NULL;
    int unknown_session_type = 0;

    if (s->type == SCP_SESSION_TYPE_XVNC)
    {
        env_set_user(s->uid,
                     &passwd_file,
                     s->display,
                     g_cfg->env_names,
                     g_cfg->env_values);
    }
    else
    {
        env_set_user(s->uid,
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
/**
 * Convert a UID to a username
 *
 * @param uid UID
 * @param uname pointer to output buffer
 * @param uname_len Length of output buffer
 * @return 0 for success.
 */
static int
username_from_uid(int uid, char *uname, int uname_len)
{
    char *ustr;
    int rv = g_getuser_info_by_uid(uid, &ustr, NULL, NULL, NULL, NULL);

    if (rv == 0)
    {
        g_snprintf(uname, uname_len, "%s", ustr);
        g_free(ustr);
    }
    else
    {
        g_snprintf(uname, uname_len, "<unknown>");
    }
    return rv;
}

/******************************************************************************/
static void
exit_status_to_str(const struct exit_status *e, char buff[], int bufflen)
{
    switch (e->reason)
    {
        case E_XR_STATUS_CODE:
            if (e->val == 0)
            {
                g_snprintf(buff, bufflen, "exit code zero");
            }
            else
            {
                g_snprintf(buff, bufflen, "non-zero exit code %d", e->val);
            }
            break;

        case E_XR_SIGNAL:
            g_snprintf(buff, bufflen, "signal %d", e->val);
            break;

        default:
            g_snprintf(buff, bufflen, "an unexpected error");
            break;
    }
}

/******************************************************************************/
static void
run_xrdp_session(const struct session_parameters *s,
                 struct auth_info *auth_info,
                 int window_manager_pid,
                 int display_pid,
                 int chansrv_pid)
{
    int wm_wait_time;
    struct exit_status wm_exit_status = {.reason = E_XR_UNEXPECTED, .val = 0};
    int wm_running = 1;

    /* Monitor the amount of time we wait for the
     * window manager. This is approximately how long the window
     * manager was running for */
    LOG(LOG_LEVEL_INFO, "Session in progress on display :%d. Waiting "
        "until the window manager (pid %d) exits to end the session",
        s->display, window_manager_pid);
    wm_wait_time = g_time1();

    /* Wait for the window manager to terminate
     *
     * We can't use g_waitpid() variants for this, as these aren't
     * interruptible by a SIGTERM */
    while (wm_running)
    {
        int robjs_count;
        intptr_t robjs[32];
        int pid;
        struct exit_status e;

        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;

        if (g_obj_wait(robjs, robjs_count, NULL, 0, 0) != 0)
        {
            /* should not get here */
            LOG(LOG_LEVEL_WARNING, "run_xrdp_session: "
                "Unexpected error from g_obj_wait()");
            g_sleep(100);
            continue;
        }

        if (g_is_wait_obj_set(g_term_event))
        {
            g_reset_wait_obj(g_term_event);
            LOG(LOG_LEVEL_INFO, "Received SIGTERM");
            // Pass it on to the window manager
            g_sigterm(window_manager_pid);
        }

        // Check for any finished children
        g_reset_wait_obj(g_sigchld_event);
        while ((pid = g_waitchild(&e)) > 0)
        {
            if (pid == window_manager_pid)
            {
                wm_running = 0;
                wm_exit_status = e;
            }
            else if (pid == display_pid)
            {
                LOG(LOG_LEVEL_INFO, "X server pid %d on display :%d finished",
                    display_pid, s->display);
                display_pid = -1;
                // No other action - window manager should be going soon
            }
            else if (pid == chansrv_pid)
            {
                LOG(LOG_LEVEL_INFO,
                    "xrdp channel server pid %d on display :%d finished",
                    chansrv_pid, s->display);
                chansrv_pid = -1;
            }
        }
    }
    wm_wait_time = g_time1() - wm_wait_time;

    if (wm_exit_status.reason == E_XR_STATUS_CODE && wm_exit_status.val == 0)
    {
        LOG(LOG_LEVEL_INFO,
            "Window manager (pid %d, display %d) finished normally in %d secs",
            window_manager_pid, s->display, wm_wait_time);
    }
    else
    {
        char reason[128];
        exit_status_to_str(&wm_exit_status, reason, sizeof(reason));

        LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
            "exited with %s. This "
            "could indicate a window manager config problem",
            window_manager_pid, s->display, reason);
    }
    if (wm_wait_time < 10)
    {
        /* This could be a config issue. Log a significant error */
        LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
            "exited quickly (%d secs). This could indicate a window "
            "manager config problem",
            window_manager_pid, s->display, wm_wait_time);
    }

    if (display_pid > 0)
    {
        LOG(LOG_LEVEL_INFO, "Terminating X server (pid %d) on display :%d",
            display_pid, s->display);
        g_sigterm(display_pid);
    }

    if (chansrv_pid > 0)
    {
        LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
            "on display :%d", chansrv_pid, s->display);
        g_sigterm(chansrv_pid);
    }

    /* make sure all children are gone before socket cleanup happens */
    if (display_pid > 0)
    {
        g_waitpid(display_pid);
        LOG(LOG_LEVEL_INFO, "X server pid %d on display :%d finished",
            display_pid, s->display);
    }

    if (chansrv_pid > 0)
    {
        g_waitpid(chansrv_pid);
        LOG(LOG_LEVEL_INFO,
            "xrdp channel server pid %d on display :%d finished",
            chansrv_pid, s->display);
    }

    cleanup_sockets(s->display);
    g_deinit();
}

/******************************************************************************/
/*
 * Simple helper process to fork a child and log errors */
static int
fork_child(
    void (*runproc)(struct auth_info *, const struct session_parameters *),
    struct auth_info *auth_info,
    const struct session_parameters *s)
{
    int pid = g_fork();
    if (pid == 0)
    {
        /* Child process */
        runproc(auth_info, s);
        g_exit(0);
    }

    if (pid < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Fork failed [%s]", g_get_strerror());
    }

    return pid;
}

/******************************************************************************/
/**
 * Sub-process to start a session
 *
 * @param auth_info Authentication info
 * @param s Session parameters
 * @param success_fd File descriptor to write to on success
 *
 * @return status
 *
 * This routine returns a status on failure. On success, a character is
 * written to the file descriptor to indicate a success, and then the
 * routine runs for the lifetime of the session.
 */
enum scp_screate_status
session_start_subprocess(struct auth_info *auth_info,
                         const struct session_parameters *s,
                         int success_fd)
{
    char username[256];
    int chansrv_pid;
    int display_pid;
    int window_manager_pid;
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;
    char text[64];

    /* Set up wait objects so we can detect signals */
    g_pid = g_getpid();
    g_snprintf(text, sizeof(text), "xrdp_sesexec_%8.8x_main_term",
               g_pid);
    g_term_event = g_create_wait_obj(text);
    g_signal_terminate(set_term_event);
    g_snprintf(text, sizeof(text), "xrdp_sesexec_%8.8x_sigchld",
               g_pid);
    g_sigchld_event = g_create_wait_obj(text);
    g_signal_child_stop(set_sigchld_event);

    /* Get the username for display purposes */
    username_from_uid(s->uid, username, sizeof(username));

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

    if (g_setlogin(username) < 0)
    {
        LOG(LOG_LEVEL_WARNING,
            "[session start] (display %d): setlogin failed for user %s - pid %d",
            s->display, username, g_getpid());
    }
#endif

    /* Set the secondary groups before starting the session to prevent
     * problems on PAM-based systems (see Linux pam_setcred(3)).
     * If we have *BSD setusercontext() this is not done here */
#ifndef HAVE_SETUSERCONTEXT
    if (g_initgroups(username) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Failed to initialise secondary groups for %s: %s",
            username, g_get_strerror());
        return E_SCP_SCREATE_GENERAL_ERROR;
    }
#endif

    display_pid = fork_child(start_x_server, auth_info, s);
    if (display_pid > 0)
    {
        enum xwait_status xws;
        xws = wait_for_xserver(s->uid,
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
                                            auth_info, s);
            if (window_manager_pid < 0)
            {
                g_sigterm(display_pid);
                g_waitpid(display_pid);
            }
            else
            {
                LOG(LOG_LEVEL_INFO,
                    "Starting the xrdp channel server for display :%d",
                    s->display);

                chansrv_pid = fork_child(start_chansrv, auth_info, s);

                // Tell the caller we've started
                char zero = 0;
                g_file_write(success_fd, &zero, 1);
                status = E_SCP_SCREATE_OK;

                /* This call does not return  until the session is done */
                run_xrdp_session(s, auth_info, window_manager_pid,
                                 display_pid, chansrv_pid);
            }
        }
    }

    return status;
}

#ifdef USE_EXTRA_SESSION_FORK
/*
 * FreeBSD bug
 * ports/157282: effective login name is not set by xrdp-sesman
 * http://www.freebsd.org/cgi/query-pr.cgi?pr=157282
 *
 * from:
 *  $OpenBSD: session.c,v 1.252 2010/03/07 11:57:13 dtucker Exp $
 *  with some ideas about BSD process grouping to xrdp
 */
static int
run_extra_fork(void)
{
    char text[64];
    struct exit_status e;
    int stat;

    pid_t bsdsespid = g_fork();

    if (bsdsespid <= 0)
    {
        /* Error, or child */
        return bsdsespid;
    }
    /*
     * intermediate sesman should return the status of its own child, and
     * kill the child if we get a sigterm
     */

    /* Set up wait objects so we can detect signals */
    g_pid = g_getpid();
    g_snprintf(text, sizeof(text), "xrdp_intermediate_%8.8x_main_term",
               g_pid);
    g_term_event = g_create_wait_obj(text);
    g_signal_terminate(set_term_event);
    g_snprintf(text, sizeof(text), "xrdp_intermediate_%8.8x_sigchld",
               g_pid);
    g_sigchld_event = g_create_wait_obj(text);
    g_signal_child_stop(set_sigchld_event);

    // Wait for a SIGCHLD event. We've only got one child so we know where
    // it's come from!
    while (!g_is_wait_obj_set(g_sigchld_event))
    {
        int robjs_count;
        intptr_t robjs[4];

        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;

        if (g_obj_wait(robjs, robjs_count, NULL, 0, 0) != 0)
        {
            /* should not get here */
            LOG(LOG_LEVEL_WARNING, "run_extra_fork: "
                "Unexpected error from g_obj_wait()");
            g_sleep(100);
        }
        else if (g_is_wait_obj_set(g_term_event))
        {
            g_reset_wait_obj(g_term_event);
            LOG(LOG_LEVEL_INFO, "Received SIGTERM");
            // Pass it on to the BSD session leader
            g_sigterm(bsdsespid);
        }
    }

    e = g_waitpid_status(bsdsespid);
    stat = (e.reason == E_XR_STATUS_CODE) ? e.val : E_SCP_SCREATE_GENERAL_ERROR;
    g_exit(stat);
    return -1;
}
#endif

/******************************************************************************/
enum scp_screate_status
session_start(struct auth_info *auth_info,
              const struct session_parameters *s,
              int *pid)
{
    int fd[2];
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;

    if (g_pipe(fd) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Cant create a pipe [%s]", g_get_strerror());
    }
    else
    {
        *pid = g_fork();
        if (*pid == 0)
        {
            /**
             * We're now forked from the main sesman process, so we
             * can close file descriptors that we no longer need */

            g_file_close(fd[0]);

            sesman_close_all(0);

            /* Wait objects created in a parent are not valid in a child */
            sesman_delete_wait_objects();

            LOG(LOG_LEVEL_INFO,
                "calling auth_start_session for uid=%d from pid %d",
                s->uid, g_getpid());
            auth_start_session(auth_info, s->display);

            /* Run the child */
#ifdef USE_EXTRA_SESSION_FORK
            if (run_extra_fork() < 0)
            {
                return E_SCP_SCREATE_GENERAL_ERROR;
            }
#endif
            status = session_start_subprocess(auth_info, s, fd[1]);

            LOG(LOG_LEVEL_INFO,
                "Calling auth_stop_session from pid %d",
                g_getpid());
            auth_stop_session(auth_info);
            g_exit(status);
        }

        g_file_close(fd[1]);
        if (*pid == -1)
        {
            LOG(LOG_LEVEL_ERROR, "Cant fork [%s]", g_get_strerror());
        }
        else
        {
            /* Wait for the child to signal success, or return an error */
            int err;
            char buff;
            do
            {
                err = g_file_read(fd[0], &buff, 1);
            }
            while (err == -1 && g_get_errno() == EINTR);
            if (err < 0)
            {
                /* Read problem */
                LOG(LOG_LEVEL_ERROR, "Can't read pipe [%s]", g_get_strerror());
            }
            else if (err > 0)
            {
                /* Session is up and running */
                status = E_SCP_SCREATE_OK;
            }
            else
            {
                /* Process has failed. Get the exit status of the child */
                struct exit_status e;
                e = g_waitpid_status(*pid);
                if (e.reason == E_XR_STATUS_CODE)
                {
                    status = (enum scp_screate_status)e.val;
                }
                else
                {
                    char reason[128];
                    exit_status_to_str(&e, reason, sizeof(reason));
                    LOG(LOG_LEVEL_ERROR, "Child exited with %s", reason);
                }
            }
        }
        g_file_close(fd[0]);
    }

    return status;
}

/******************************************************************************/
int
session_reconnect(int display, int uid,
                  struct auth_info *auth_info)
{
    int pid;

    pid = g_fork();

    if (pid == -1)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to fork for session reconnection script");
    }
    else if (pid == 0)
    {
        env_set_user(uid,
                     0,
                     display,
                     g_cfg->env_names,
                     g_cfg->env_values);
        auth_set_env(auth_info);

        if (g_file_exist(g_cfg->reconnect_sh))
        {
            LOG(LOG_LEVEL_INFO,
                "Starting session reconnection script on display %d: %s",
                display, g_cfg->reconnect_sh);
            g_execlp3(g_cfg->reconnect_sh, g_cfg->reconnect_sh, 0);

            /* should not get here */
            LOG(LOG_LEVEL_ERROR,
                "Error starting session reconnection script on display %d: %s",
                display, g_cfg->reconnect_sh);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Session reconnection script file does not exist: %s",
                g_cfg->reconnect_sh);
        }

        /* TODO: why is this existing with a success error code when the
            reconnect script failed to be executed? */
        g_exit(0);
    }

    return display;
}
