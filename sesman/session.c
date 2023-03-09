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

#include "arch.h"
#include "session.h"

#include "auth.h"
#include "config.h"
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
static int
session_start_chansrv(int uid, int display)
{
    struct list *chansrv_params;
    const char *exe_path = XRDP_SBIN_PATH "/xrdp-chansrv";
    int chansrv_pid;

    chansrv_pid = g_fork();
    if (chansrv_pid == 0)
    {
        chansrv_params = list_create();
        chansrv_params->auto_free = 1;

        /* building parameters */

        list_add_strdup(chansrv_params, exe_path);

        env_set_user(uid, 0, display,
                     g_cfg->env_names,
                     g_cfg->env_values);

        LOG(LOG_LEVEL_INFO,
            "Starting the xrdp channel server for display %d", display);

        /* executing chansrv */
        g_execvp_list(exe_path, chansrv_params);

        /* should not get here */
        list_delete(chansrv_params);
        g_exit(1);
    }
    return chansrv_pid;
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
void
session_start(struct auth_info *auth_info,
              const struct session_parameters *s)
{
    char geometry[32];
    char depth[32];
    char screen[32]; /* display number */
    char text[256];
    char username[256];
    char execvpparams[2048];
    char *xserver = NULL; /* absolute/relative path to Xorg/Xvnc */
    char *passwd_file = NULL;
    struct list *xserver_params = (struct list *)NULL;
    char authfile[256]; /* The filename for storing xauth information */
    int chansrv_pid;
    int display_pid;
    int window_manager_pid;

    /* initialize (zero out) local variables: */
    g_memset(geometry, 0, sizeof(char) * 32);
    g_memset(depth, 0, sizeof(char) * 32);
    g_memset(screen, 0, sizeof(char) * 32);
    g_memset(text, 0, sizeof(char) * 256);

    /* Get the username for display purposes */
    username_from_uid(s->uid, username, sizeof(username));

    LOG(LOG_LEVEL_INFO,
        "[session start] (display %d): calling auth_start_session from pid %d",
        s->display, g_getpid());

    /* Set the secondary groups before starting the session to prevent
     * problems on PAM-based systems (see Linux pam_setcred(3)).
     * If we have *BSD setusercontext() this is not done here */
#ifndef HAVE_SETUSERCONTEXT
    if (g_initgroups(username) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Failed to initialise secondary groups for %s: %s",
            username, g_get_strerror());
        g_exit(1);
    }
#endif

    auth_start_session(auth_info, s->display);
    g_sprintf(geometry, "%dx%d", s->width, s->height);
    g_sprintf(depth, "%d", s->bpp);
    g_sprintf(screen, ":%d", s->display);
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    /*
     * FreeBSD bug
     * ports/157282: effective login name is not set by xrdp-sesman
     * http://www.freebsd.org/cgi/query-pr.cgi?pr=157282
     *
     * from:
     *  $OpenBSD: session.c,v 1.252 2010/03/07 11:57:13 dtucker Exp $
     *  with some ideas about BSD process grouping to xrdp
     */
    pid_t bsdsespid = g_fork();

    if (bsdsespid == -1)
    {
    }
    else if (bsdsespid == 0) /* BSD session leader */
    {
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
    }

    g_waitpid(bsdsespid);

    if (bsdsespid > 0)
    {
        g_exit(0);
        /*
         * intermediate sesman should exit here after WM exits.
         * do not execute the following codes.
         */
    }
#endif
    window_manager_pid = g_fork(); /* parent becomes X,
                         child forks wm, and waits, todo */
    if (window_manager_pid == -1)
    {
        LOG(LOG_LEVEL_ERROR,
            "Failed to fork for the window manager on display %d", s->display);
    }
    else if (window_manager_pid == 0)
    {
        enum xwait_status xws;

        env_set_user(s->uid,
                     0,
                     s->display,
                     g_cfg->env_names,
                     g_cfg->env_values);
        xws = wait_for_xserver(s->display);

        if (xws == XW_STATUS_OK)
        {
            auth_set_env(auth_info);
            if (s->directory != 0)
            {
                if (s->directory[0] != 0)
                {
                    g_set_current_dir(s->directory);
                }
            }
            if (s->shell != 0 && s->shell[0] != 0)
            {
                if (g_strchr(s->shell, ' ') != 0 || g_strchr(s->shell, '\t') != 0)
                {
                    LOG(LOG_LEVEL_INFO,
                        "Starting user requested window manager on "
                        "display %d with embedded arguments using a shell: %s",
                        s->display, s->shell);
                    const char *argv[] = {"sh", "-c", s->shell, NULL};
                    g_execvp("/bin/sh", (char **)argv);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO,
                        "Starting user requested window manager on "
                        "display %d: %s", s->display, s->shell);
                    g_execlp3(s->shell, s->shell, 0);
                }
            }
            else
            {
                LOG(LOG_LEVEL_DEBUG, "The user session on display %d did "
                    "not request a specific window manager", s->display);
            }

            /* try to execute user window manager if enabled */
            if (g_cfg->enable_user_wm)
            {
                g_sprintf(text, "%s/%s", g_getenv("HOME"), g_cfg->user_wm);
                if (g_file_exist(text))
                {
                    LOG(LOG_LEVEL_INFO,
                        "Starting window manager on display %d"
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
                "Starting the default window manager on display %d: %s",
                s->display, g_cfg->default_wm);
            g_execlp3(g_cfg->default_wm, g_cfg->default_wm, 0);

            /* still a problem starting window manager just start xterm */
            LOG(LOG_LEVEL_WARNING,
                "No window manager on display %d started, "
                "so falling back to starting xterm for user debugging",
                s->display);
            g_execlp3("xterm", "xterm", 0);

            /* should not get here */
        }
        else
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
        }

        LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
            "the window manager on display %d, aborting connection",
            s->display);
        g_exit(0);
    }
    else
    {
        display_pid = g_fork(); /* parent becomes scp,
                            child becomes X */
        if (display_pid == -1)
        {
            LOG(LOG_LEVEL_ERROR,
                "Failed to fork for the X server on display %d", s->display);
        }
        else if (display_pid == 0) /* child */
        {
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

            /* setting Xserver environment variables */
            g_snprintf(text, 255, "%d", g_cfg->sess.max_idle_time);
            g_setenv("XRDP_SESMAN_MAX_IDLE_TIME", text, 1);
            g_snprintf(text, 255, "%d", g_cfg->sess.max_disc_time);
            g_setenv("XRDP_SESMAN_MAX_DISC_TIME", text, 1);
            g_snprintf(text, 255, "%d", g_cfg->sess.kill_disconnected);
            g_setenv("XRDP_SESMAN_KILL_DISCONNECTED", text, 1);
            g_setenv("XRDP_SOCKET_PATH", XRDP_SOCKET_PATH, 1);

            /* prepare the Xauthority stuff */
            if (g_getenv("XAUTHORITY") != NULL)
            {
                g_snprintf(authfile, 255, "%s", g_getenv("XAUTHORITY"));
            }
            else
            {
                g_snprintf(authfile, 255, "%s", ".Xauthority");
            }

            /* Add the entry in XAUTHORITY file or exit if error */
            if (add_xauth_cookie(s->display, authfile) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Error setting the xauth cookie for display %d in file %s",
                    s->display, authfile);

                LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
                    "the X server on display %d, aborting connection",
                    s->display);
                g_exit(1);
            }

            if (s->type == SCP_SESSION_TYPE_XORG)
            {
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
                        "[session start] (display %d): Failed to disable "
                        "setuid on X server: %s",
                        s->display, g_get_strerror());
                }
#endif

                xserver_params = list_create();
                xserver_params->auto_free = 1;

                /* get path of Xorg from config */
                xserver = g_strdup((const char *)list_get_item(g_cfg->xorg_params, 0));

                /* these are the must have parameters */
                list_add_strdup_multi(xserver_params,
                                      xserver, screen,
                                      "-auth", authfile,
                                      NULL);

                /* additional parameters from sesman.ini file */
                list_append_list_strdup(g_cfg->xorg_params, xserver_params, 1);

                /* some args are passed via env vars */
                g_sprintf(geometry, "%d", s->width);
                g_setenv("XRDP_START_WIDTH", geometry, 1);

                g_sprintf(geometry, "%d", s->height);
                g_setenv("XRDP_START_HEIGHT", geometry, 1);
            }
            else if (s->type == SCP_SESSION_TYPE_XVNC)
            {
                char guid_str[GUID_STR_SIZE];
                guid_to_str(&s->guid, guid_str);
                env_check_password_file(passwd_file, guid_str);
                xserver_params = list_create();
                xserver_params->auto_free = 1;

                /* get path of Xvnc from config */
                xserver = g_strdup((const char *)list_get_item(g_cfg->vnc_params, 0));

                /* these are the must have parameters */
                list_add_strdup_multi(xserver_params,
                                      xserver, screen,
                                      "-auth", authfile,
                                      "-geometry", geometry,
                                      "-depth", depth,
                                      "-rfbauth", passwd_file,
                                      NULL);
                g_free(passwd_file);

                /* additional parameters from sesman.ini file */
                //config_read_xserver_params(SCP_SESSION_TYPE_XVNC,
                //                           xserver_params);
                list_append_list_strdup(g_cfg->vnc_params, xserver_params, 1);
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "Unknown session type: %d",
                    s->type);
                LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
                    "to start the X server on display %d, aborting connection",
                    s->display);
                g_exit(1);
            }

            /* fire up X server */
            LOG(LOG_LEVEL_INFO, "Starting X server on display %d: %s",
                s->display, dumpItemsToString(xserver_params, execvpparams, 2048));
            g_execvp_list(xserver, xserver_params);

            /* should not get here */
            LOG(LOG_LEVEL_ERROR,
                "Error starting X server on display %d", s->display);
            LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
                "to start the X server on display %d, aborting connection",
                s->display);

            list_delete(xserver_params);
            g_exit(1);
        }
        else
        {
            int wm_wait_time;
            struct exit_status wm_exit_status;
            struct exit_status xserver_exit_status;
            struct exit_status chansrv_exit_status;
            char reason[128];

            chansrv_pid = session_start_chansrv(s->uid, s->display);

            LOG(LOG_LEVEL_INFO,
                "Session started successfully for user %s on display %d",
                username, s->display);

            /* Monitor the amount of time we wait for the
             * window manager. This is approximately how long the window
             * manager was running for */
            LOG(LOG_LEVEL_INFO, "Session in progress on display %d, waiting "
                "until the window manager (pid %d) exits to end the session",
                s->display, window_manager_pid);
            wm_wait_time = g_time1();
            wm_exit_status = g_waitpid_status(window_manager_pid);
            wm_wait_time = g_time1() - wm_wait_time;
            if (wm_exit_status.reason == E_XR_STATUS_CODE &&
                    wm_exit_status.val == 0)
            {
                // Normal exit
            }
            else
            {
                exit_status_to_str(&wm_exit_status, reason, sizeof(reason));

                LOG(LOG_LEVEL_WARNING,
                    "Window manager (pid %d, display %d) "
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
            else
            {
                LOG(LOG_LEVEL_DEBUG, "Window manager (pid %d, display %d) "
                    "was running for %d seconds.",
                    window_manager_pid, s->display, wm_wait_time);
            }
            LOG(LOG_LEVEL_INFO,
                "Calling auth_stop_session from pid %d",
                g_getpid());
            auth_stop_session(auth_info);
            // auth_end() is called from the main process currently,
            // as this called auth_start()
            //auth_end(auth_info);

            LOG(LOG_LEVEL_INFO,
                "Terminating X server (pid %d) on display %d",
                display_pid, s->display);
            g_sigterm(display_pid);

            LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                "on display %d", chansrv_pid, s->display);
            g_sigterm(chansrv_pid);

            /* make sure socket cleanup happen after child process exit */
            xserver_exit_status = g_waitpid_status(display_pid);
            exit_status_to_str(&xserver_exit_status, reason, sizeof(reason));
            LOG(LOG_LEVEL_INFO,
                "X server on display %d (pid %d) exited with %s",
                s->display, display_pid, reason);

            chansrv_exit_status = g_waitpid_status(chansrv_pid);
            exit_status_to_str(&chansrv_exit_status, reason, sizeof(reason));
            LOG(LOG_LEVEL_INFO,
                "xrdp channel server for display %d (pid %d)"
                " exited with %s",
                s->display, chansrv_pid, reason);

            cleanup_sockets(s->display);
            g_deinit();
            g_exit(0);
        }
    }
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
