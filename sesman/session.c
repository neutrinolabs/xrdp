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

static struct session_chain *g_sessions;
static int g_session_count;

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
struct session_item *
session_get_bydata(const struct session_parameters *sp)
{
    char policy_str[64];
    struct session_chain *tmp;
    int policy = g_cfg->sess.policy;

    if ((policy & SESMAN_CFG_SESS_POLICY_DEFAULT) != 0)
    {
        /* In the past (i.e. xrdp before v0.9.14), the default
         * session policy varied by sp->type. If this is needed again
         * in the future, here is the place to add it */
        policy = SESMAN_CFG_SESS_POLICY_U | SESMAN_CFG_SESS_POLICY_B;
    }

    config_output_policy_string(policy, policy_str, sizeof(policy_str));

    LOG(LOG_LEVEL_DEBUG,
        "%s: search policy=%s type=%s U=%d B=%d D=(%dx%d) I=%s",
        __func__,
        policy_str, SCP_SESSION_TYPE_TO_STR(sp->type),
        sp->uid, sp->bpp, sp->width, sp->height,
        sp->ip_addr);

    /* 'Separate' policy never matches */
    if (policy & SESMAN_CFG_SESS_POLICY_SEPARATE)
    {
        LOG(LOG_LEVEL_DEBUG, "%s: No matches possible", __func__);
        return NULL;
    }

    for (tmp = g_sessions ; tmp != 0 ; tmp = tmp->next)
    {
        struct session_item *item = tmp->item;

        LOG(LOG_LEVEL_DEBUG,
            "%s: try %p type=%s U=%d B=%d D=(%dx%d) I=%s",
            __func__,
            item,
            SCP_SESSION_TYPE_TO_STR(item->type),
            item->uid,
            item->bpp,
            item->width, item->height,
            item->start_ip_addr);

        if (item->type != sp->type)
        {
            LOG(LOG_LEVEL_DEBUG, "%s: Type doesn't match", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_U) && sp->uid != item->uid)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: UID doesn't match for 'U' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_B) && item->bpp != sp->bpp)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: bpp doesn't match for 'B' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_D) &&
                (item->width != sp->width || item->height != sp->height))
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: Dimensions don't match for 'D' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_I) &&
                g_strcmp(item->start_ip_addr, sp->ip_addr) != 0)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: IPs don't match for 'I' policy", __func__);
            continue;
        }

        LOG(LOG_LEVEL_DEBUG,
            "%s: Got match, display=%d", __func__, item->display);
        return item;
    }

    LOG(LOG_LEVEL_DEBUG, "%s: No matches found", __func__);
    return 0;
}

/******************************************************************************/
/**
 *
 * @brief checks if there's a server running on a display
 * @param display the display to check
 * @return 0 if there isn't a display running, nonzero otherwise
 *
 */
static int
x_server_running_check_ports(int display)
{
    char text[256];
    int x_running;
    int sck;

    g_sprintf(text, "/tmp/.X11-unix/X%d", display);
    x_running = g_file_exist(text);

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, "/tmp/.X%d-lock", display);
        x_running = g_file_exist(text);
    }

    if (!x_running) /* check 59xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "59%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 60xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "60%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 62xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "62%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, XRDP_CHANSRV_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_PORT_OUT_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_PORT_IN_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_API_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, XRDP_X11RDP_STR, display);
        x_running = g_file_exist(text);
    }

    if (x_running)
    {
        LOG(LOG_LEVEL_INFO, "Found X server running at %s", text);
    }

    return x_running;
}

/******************************************************************************/
/* called with the main thread
   returns boolean */
static int
session_is_display_in_chain(int display)
{
    struct session_chain *chain;
    struct session_item *item;

    chain = g_sessions;

    while (chain != 0)
    {
        item = chain->item;

        if (item->display == display)
        {
            return 1;
        }

        chain = chain->next;
    }

    return 0;
}

/******************************************************************************/
/* called with the main thread */
static int
session_get_avail_display_from_chain(void)
{
    int display;

    display = g_cfg->sess.x11_display_offset;

    while ((display - g_cfg->sess.x11_display_offset) <= g_cfg->sess.max_sessions)
    {
        if (!session_is_display_in_chain(display))
        {
            if (!x_server_running_check_ports(display))
            {
                return display;
            }
        }

        display++;
    }

    LOG(LOG_LEVEL_ERROR, "X server -- no display in range (%d to %d) is available",
        g_cfg->sess.x11_display_offset,
        g_cfg->sess.x11_display_offset + g_cfg->sess.max_sessions);
    return 0;
}

static int
session_start_chansrv(int uid, int display)
{
    struct list *chansrv_params;
    const char *exe_path = XRDP_SBIN_PATH "/xrdp-chansrv";
    int chansrv_pid;

    chansrv_pid = g_fork();
    if (chansrv_pid == 0)
    {
        LOG(LOG_LEVEL_INFO,
            "Starting the xrdp channel server for display %d", display);

        chansrv_params = list_create();
        chansrv_params->auto_free = 1;

        /* building parameters */

        list_add_strdup(chansrv_params, exe_path);

        env_set_user(uid, 0, display,
                     g_cfg->env_names,
                     g_cfg->env_values);

        /* executing chansrv */
        g_execvp_list(exe_path, chansrv_params);

        /* should not get here */
        list_delete(chansrv_params);
        g_exit(1);
    }
    return chansrv_pid;
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

enum scp_screate_status
session_start(struct auth_info *auth_info,
              const struct session_parameters *s,
              int *returned_display,
              struct guid *guid)
{
    int display = 0;
    int pid = 0;
    char geometry[32];
    char depth[32];
    char screen[32]; /* display number */
    char text[256];
    char username[256];
    char execvpparams[2048];
    char *xserver = NULL; /* absolute/relative path to Xorg/Xvnc */
    char *passwd_file;
    struct session_chain *temp = (struct session_chain *)NULL;
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

    passwd_file = 0;

    /* Get the username for display purposes */
    username_from_uid(s->uid, username, sizeof(username));

    /* check to limit concurrent sessions */
    if (g_session_count >= g_cfg->sess.max_sessions)
    {
        LOG(LOG_LEVEL_ERROR, "max concurrent session limit "
            "exceeded. login for user %s denied", username);
        return E_SCP_SCREATE_MAX_REACHED;
    }

    temp = (struct session_chain *)g_malloc(sizeof(struct session_chain), 0);

    if (temp == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory error: cannot create new session "
            "chain element - user %s", username);
        return E_SCP_SCREATE_NO_MEMORY;
    }

    temp->item = (struct session_item *)g_malloc(sizeof(struct session_item), 0);

    if (temp->item == 0)
    {
        g_free(temp);
        LOG(LOG_LEVEL_ERROR, "Out of memory error: cannot create new session "
            "item - user %s", username);
        return E_SCP_SCREATE_NO_MEMORY;
    }

    display = session_get_avail_display_from_chain();

    if (display == 0)
    {
        g_free(temp->item);
        g_free(temp);
        return E_SCP_SCREATE_NO_DISPLAY;
    }

    /* Create a GUID for the new session before we fork */
    *guid = guid_new();
    *returned_display = display;

    pid = g_fork(); /* parent is fork from tcp accept,
                       child forks X and wm, then becomes scp */

    if (pid == -1)
    {
        LOG(LOG_LEVEL_ERROR,
            "[session start] (display %d): Failed to fork for scp with "
            "errno: %d, description: %s",
            display, g_get_errno(), g_get_strerror());
        g_free(temp->item);
        g_free(temp);
        return E_SCP_SCREATE_GENERAL_ERROR;
    }

    if (pid == 0)
    {
        LOG(LOG_LEVEL_INFO,
            "[session start] (display %d): calling auth_start_session from pid %d",
            display, g_getpid());

        /* Clone the session object, as the passed-in copy will be
         * deleted by sesman_close_all() */
        if ((s = clone_session_params(s)) == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Failed to clone the session data - out of memory");
            g_exit(1);
        }

        /* Wait objects created in a parent are not valid in a child */
        g_delete_wait_obj(g_reload_event);
        g_delete_wait_obj(g_sigchld_event);
        g_delete_wait_obj(g_term_event);

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

        sesman_close_all(0);
        auth_start_session(auth_info, display);
        g_sprintf(geometry, "%dx%d", s->width, s->height);
        g_sprintf(depth, "%d", s->bpp);
        g_sprintf(screen, ":%d", display);
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
                    display, g_getpid());
            }

            if (g_setlogin(username) < 0)
            {
                LOG(LOG_LEVEL_WARNING,
                    "[session start] (display %d): setlogin failed for user %s - pid %d",
                    display, username, g_getpid());
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
                "Failed to fork for the window manager on display %d", display);
        }
        else if (window_manager_pid == 0)
        {
            env_set_user(s->uid,
                         0,
                         display,
                         g_cfg->env_names,
                         g_cfg->env_values);
            if (wait_for_xserver(display))
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
                            display, s->shell);
                        const char *argv[] = {"sh", "-c", s->shell, NULL};
                        g_execvp("/bin/sh", (char **)argv);
                    }
                    else
                    {
                        LOG(LOG_LEVEL_INFO,
                            "Starting user requested window manager on "
                            "display %d: %s", display, s->shell);
                        g_execlp3(s->shell, s->shell, 0);
                    }
                }
                else
                {
                    LOG(LOG_LEVEL_DEBUG, "The user session on display %d did "
                        "not request a specific window manager", display);
                }

                /* try to execute user window manager if enabled */
                if (g_cfg->enable_user_wm)
                {
                    g_sprintf(text, "%s/%s", g_getenv("HOME"), g_cfg->user_wm);
                    if (g_file_exist(text))
                    {
                        LOG(LOG_LEVEL_INFO,
                            "Starting window manager on display %d"
                            " from user home directory: %s", display, text);
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
                    display, g_cfg->default_wm);
                g_execlp3(g_cfg->default_wm, g_cfg->default_wm, 0);

                /* still a problem starting window manager just start xterm */
                LOG(LOG_LEVEL_WARNING,
                    "No window manager on display %d started, "
                    "so falling back to starting xterm for user debugging",
                    display);
                g_execlp3("xterm", "xterm", 0);

                /* should not get here */
            }
            else
            {
                LOG(LOG_LEVEL_ERROR,
                    "There is no X server active on display %d", display);
            }

            LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
                "the window manager on display %d, aborting connection",
                display);
            g_exit(0);
        }
        else
        {
            display_pid = g_fork(); /* parent becomes scp,
                                child becomes X */
            if (display_pid == -1)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Failed to fork for the X server on display %d", display);
            }
            else if (display_pid == 0) /* child */
            {
                if (s->type == SCP_SESSION_TYPE_XVNC)
                {
                    env_set_user(s->uid,
                                 &passwd_file,
                                 display,
                                 g_cfg->env_names,
                                 g_cfg->env_values);
                }
                else
                {
                    env_set_user(s->uid,
                                 0,
                                 display,
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
                if (add_xauth_cookie(display, authfile) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Error setting the xauth cookie for display %d in file %s",
                        display, authfile);

                    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
                        "the X server on display %d, aborting connection",
                        display);
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
                            display, g_get_strerror());
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
                    guid_to_str(guid, guid_str);
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
                        display);
                    g_exit(1);
                }

                /* fire up X server */
                LOG(LOG_LEVEL_INFO, "Starting X server on display %d: %s",
                    display, dumpItemsToString(xserver_params, execvpparams, 2048));
                g_execvp_list(xserver, xserver_params);

                /* should not get here */
                LOG(LOG_LEVEL_ERROR,
                    "Error starting X server on display %d", display);
                LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting "
                    "to start the X server on display %d, aborting connection",
                    display);

                list_delete(xserver_params);
                g_exit(1);
            }
            else
            {
                int wm_wait_time;
                struct exit_status wm_exit_status;
                struct exit_status xserver_exit_status;
                struct exit_status chansrv_exit_status;

                chansrv_pid = session_start_chansrv(s->uid, display);

                LOG(LOG_LEVEL_INFO,
                    "Session started successfully for user %s on display %d",
                    username, display);

                /* Monitor the amount of time we wait for the
                 * window manager. This is approximately how long the window
                 * manager was running for */
                LOG(LOG_LEVEL_INFO, "Session in progress on display %d, waiting "
                    "until the window manager (pid %d) exits to end the session",
                    display, window_manager_pid);
                wm_wait_time = g_time1();
                wm_exit_status = g_waitpid_status(window_manager_pid);
                wm_wait_time = g_time1() - wm_wait_time;
                if (wm_exit_status.exit_code > 0)
                {
                    LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                        "exited with non-zero exit code %d and signal %d. This "
                        "could indicate a window manager config problem",
                        window_manager_pid, display, wm_exit_status.exit_code,
                        wm_exit_status.signal_no);
                }
                if (wm_wait_time < 10)
                {
                    /* This could be a config issue. Log a significant error */
                    LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %d) "
                        "exited quickly (%d secs). This could indicate a window "
                        "manager config problem",
                        window_manager_pid, display, wm_wait_time);
                }
                else
                {
                    LOG(LOG_LEVEL_DEBUG, "Window manager (pid %d, display %d) "
                        "was running for %d seconds.",
                        window_manager_pid, display, wm_wait_time);
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
                    display_pid, display);
                g_sigterm(display_pid);

                LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                    "on display %d", chansrv_pid, display);
                g_sigterm(chansrv_pid);

                /* make sure socket cleanup happen after child process exit */
                xserver_exit_status = g_waitpid_status(display_pid);
                LOG(LOG_LEVEL_INFO,
                    "X server on display %d (pid %d) returned exit code %d "
                    "and signal number %d",
                    display, display_pid, xserver_exit_status.exit_code,
                    xserver_exit_status.signal_no);

                chansrv_exit_status = g_waitpid_status(chansrv_pid);
                LOG(LOG_LEVEL_INFO,
                    "xrdp channel server for display %d (pid %d) "
                    "exit code %d and signal number %d",
                    display, chansrv_pid, chansrv_exit_status.exit_code,
                    chansrv_exit_status.signal_no);

                cleanup_sockets(display);
                g_deinit();
                g_exit(0);
            }
        }
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "Starting session: session_pid %d, "
            "display :%d.0, width %d, height %d, bpp %d, client ip %s, "
            "UID %d",
            pid, display, s->width, s->height, s->bpp, s->ip_addr, s->uid);
        temp->item->pid = pid;
        temp->item->display = display;
        temp->item->width = s->width;
        temp->item->height = s->height;
        temp->item->bpp = s->bpp;
        temp->item->auth_info = auth_info;
        g_strncpy(temp->item->start_ip_addr, s->ip_addr,
                  sizeof(temp->item->start_ip_addr) - 1);
        temp->item->uid = s->uid;
        temp->item->guid = *guid;

        temp->item->start_time = g_time1();

        temp->item->type = s->type;
        temp->item->status = SESMAN_SESSION_STATUS_ACTIVE;

        temp->next = g_sessions;
        g_sessions = temp;
        g_session_count++;

        return E_SCP_SCREATE_OK;
    }

    /* Shouldn't get here */
    g_free(temp->item);
    g_free(temp);
    return E_SCP_SCREATE_GENERAL_ERROR;
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

/******************************************************************************/
enum session_kill_status
session_kill(int pid)
{
    struct session_chain *tmp;
    struct session_chain *prev;

    tmp = g_sessions;
    prev = 0;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "session descriptor for "
                "pid %d is null!", pid);

            if (prev == 0)
            {
                /* prev does no exist, so it's the first element - so we set
                   g_sessions */
                g_sessions = tmp->next;
            }
            else
            {
                prev->next = tmp->next;
            }

            return SESMAN_SESSION_KILL_NULLITEM;
        }

        if (tmp->item->pid == pid)
        {
            char username[256];
            username_from_uid(tmp->item->uid, username, sizeof(username));

            /* deleting the session */
            if (tmp->item->auth_info != NULL)
            {
                LOG(LOG_LEVEL_INFO,
                    "Calling auth_end for pid %d from pid %d",
                    pid, g_getpid());
                auth_end(tmp->item->auth_info);
                tmp->item->auth_info = NULL;
            }
            LOG(LOG_LEVEL_INFO,
                "++ terminated session: UID %d (%s), display :%d.0, "
                "session_pid %d, ip %s",
                tmp->item->uid, username, tmp->item->display,
                tmp->item->pid, tmp->item->start_ip_addr);
            g_free(tmp->item);

            if (prev == 0)
            {
                /* prev does no exist, so it's the first element - so we set
                   g_sessions */
                g_sessions = tmp->next;
            }
            else
            {
                prev->next = tmp->next;
            }

            g_free(tmp);
            g_session_count--;
            return SESMAN_SESSION_KILL_OK;
        }

        /* go on */
        prev = tmp;
        tmp = tmp->next;
    }

    return SESMAN_SESSION_KILL_NOTFOUND;
}

/******************************************************************************/
void
session_sigkill_all(void)
{
    struct session_chain *tmp;

    tmp = g_sessions;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "found null session descriptor!");
        }
        else
        {
            g_sigterm(tmp->item->pid);
        }

        /* go on */
        tmp = tmp->next;
    }
}

/******************************************************************************/
struct session_item *
session_get_bypid(int pid)
{
    struct session_chain *tmp;
    struct session_item *dummy;

    dummy = g_new0(struct session_item, 1);

    if (0 == dummy)
    {
        LOG(LOG_LEVEL_ERROR, "session_get_bypid: out of memory");
        return 0;
    }

    tmp = g_sessions;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!", pid);
            g_free(dummy);
            return 0;
        }

        if (tmp->item->pid == pid)
        {
            g_memcpy(dummy, tmp->item, sizeof(struct session_item));
            return dummy;
        }

        /* go on */
        tmp = tmp->next;
    }

    g_free(dummy);
    return 0;
}

/******************************************************************************/
struct scp_session_info *
session_get_byuid(int uid, unsigned int *cnt, unsigned char flags)
{
    struct session_chain *tmp;
    struct scp_session_info *sess;
    int count;
    int index;

    count = 0;

    tmp = g_sessions;

    LOG(LOG_LEVEL_DEBUG, "searching for session by UID: %d", uid);
    while (tmp != 0)
    {
        if (uid == tmp->item->uid)
        {
            LOG(LOG_LEVEL_DEBUG, "session_get_byuser: status=%d, flags=%d, "
                "result=%d", (tmp->item->status), flags,
                ((tmp->item->status) & flags));

            if ((tmp->item->status) & flags)
            {
                count++;
            }
        }

        /* go on */
        tmp = tmp->next;
    }

    if (count == 0)
    {
        (*cnt) = 0;
        return 0;
    }

    /* malloc() an array of disconnected sessions */
    sess = g_new0(struct scp_session_info, count);

    if (sess == 0)
    {
        (*cnt) = 0;
        return 0;
    }

    tmp = g_sessions;
    index = 0;

    while (tmp != 0 && index < count)
    {
        if (uid == tmp->item->uid)
        {
            if ((tmp->item->status) & flags)
            {
                (sess[index]).sid = tmp->item->pid;
                (sess[index]).display = tmp->item->display;
                (sess[index]).type = tmp->item->type;
                (sess[index]).height = tmp->item->height;
                (sess[index]).width = tmp->item->width;
                (sess[index]).bpp = tmp->item->bpp;
                (sess[index]).start_time = tmp->item->start_time;
                (sess[index]).uid = tmp->item->uid;
                (sess[index]).start_ip_addr = g_strdup(tmp->item->start_ip_addr);

                /* Check for string allocation failures */
                if ((sess[index]).start_ip_addr == NULL)
                {
                    free_session_info_list(sess, *cnt);
                    (*cnt) = 0;
                    return 0;
                }
                index++;
            }
        }

        /* go on */
        tmp = tmp->next;
    }

    (*cnt) = count;
    return sess;
}

/******************************************************************************/
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt)
{
    if (sesslist != NULL && cnt > 0)
    {
        unsigned int i;
        for (i = 0 ; i < cnt ; ++i)
        {
            g_free(sesslist[i].start_ip_addr);
        }
    }

    g_free(sesslist);
}

/******************************************************************************/
int
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
struct session_parameters *
clone_session_params(const struct session_parameters *sp)
{
    struct session_parameters *result;
    char *strptr;

    /* Allocate a single block of memory big enough for the structure and
     * all the strings it points to */
    unsigned int len = sizeof(*result);
    len += g_strlen(sp->shell) + 1;
    len += g_strlen(sp->directory) + 1;
    len += g_strlen(sp->ip_addr) + 1;

    if ((result = (struct session_parameters *)g_malloc(len, 0)) != NULL)
    {
        *result = *sp; /* Copy all the scalar parameters */

        /* Initialise the string pointers in the result */
        strptr = (char *)result + sizeof(*result);

#define COPY_STRING_MEMBER(src,dest)\
    {\
        unsigned int len = g_strlen(src) + 1;\
        g_memcpy(strptr, (src), len);\
        (dest) = strptr;\
        strptr += len;\
    }

        COPY_STRING_MEMBER(sp->shell, result->shell);
        COPY_STRING_MEMBER(sp->directory, result->directory);
        COPY_STRING_MEMBER(sp->ip_addr, result->ip_addr);

#undef COPY_STRING_MEMBER
    }

    return result;
}
