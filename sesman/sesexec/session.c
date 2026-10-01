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

#include <stdio.h>
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
#include "ssl_calls.h"
#include "string_calls.h"
#include "trans.h"
#include "xauth.h"
#include "xwait.h"
#include "xrdp_sockets.h"

struct session_data
{
    pid_t x_server; ///< PID of X server
    pid_t win_mgr; ///< PID of window manager
    pid_t chansrv; ///< PID of chansrv
    time_t start_time;
    unsigned int connect_count;
    char display[MAX_DISPLAY_NAME_SIZE]; // Set by session_start()
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
    string_length += g_strlen(sp->instance_name) + 1;
    string_length += g_strlen(sp->client_name) + 1;

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
        sd->connect_count = 0;

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
        COPY_STRING(sd->params.instance_name, sp->instance_name);
        COPY_STRING(sd->params.client_name, sp->client_name);

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
 * @param outstr allocate this buffer before you use this function
 * @param len the allocated len for outstr
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
start_chansrv(const struct login_info *login_info,
              const struct session_data *sd,
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
        env_set_user(login_info->uid,
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
start_window_manager(const struct login_info *login_info,
                     const struct session_data *sd,
                     void *closure /* unused */)
{
    char text[256];
    const struct session_parameters *sp = &sd->params;

    env_set_user(login_info->uid,
                 g_cfg->env_names,
                 g_cfg->env_values);

    auth_set_env(login_info->auth_info);
    LOG_DEVEL_LEAKING_FDS("window manager", 3, -1);

    if (sp->directory[0] != '\0')
    {
        if (g_cfg->sec.allow_alternate_shell)
        {
            g_set_current_dir(sp->directory);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Directory change to %s requested, but not "
                "allowed by AllowAlternateShell config value.",
                sp->directory);
        }
    }

    if (sp->shell[0] != '\0')
    {
        if (g_cfg->sec.allow_alternate_shell)
        {
            if (g_cfg->sec.pass_shell_as_env != NULL &&
                    g_cfg->sec.pass_shell_as_env[0] != '\0')
            {
                // Pass the shell in to the standard startwm scripts
                // in an environment variable
                LOG(LOG_LEVEL_INFO,
                    "Setting variable '%s' to the specified shell of '%s'",
                    g_cfg->sec.pass_shell_as_env,
                    sp->shell);
                g_setenv_log(g_cfg->sec.pass_shell_as_env, sp->shell, 1);
            }
            else
            {
                // Try to execute the shell directly (if permitted)
                if (g_strchr(sp->shell, ' ') != 0 ||
                        g_strchr(sp->shell, '\t') != 0)
                {
                    LOG(LOG_LEVEL_INFO,
                        "Using user requested window manager on "
                        "display %s with embedded arguments using a shell: %s",
                        sd->display, sp->shell);
                    const char *argv[] = {"sh", "-c", sp->shell, NULL};
                    g_execvp("/bin/sh", (char **)argv);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO,
                        "Using user requested window manager on "
                        "display %s %s", sd->display, sp->shell);
                    g_execlp3(sp->shell, sp->shell, 0);
                }
            }
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Shell %s requested by user, but not allowed by "
                "AllowAlternateShell config value.",
                sp->shell);
        }
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "The user session on display %s did "
            "not request a specific window manager", sd->display);
    }

    /* try to execute user window manager if enabled */
    if (g_cfg->enable_user_wm)
    {
        g_snprintf(text, sizeof(text), "%s/%s",
                   g_getenv("HOME"), g_cfg->user_wm);
        if (g_file_exist(text))
        {
            LOG(LOG_LEVEL_INFO,
                "Using window manager on display %s"
                " from user home directory: %s", sd->display, text);
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
        "Using the default window manager on display %s: %s",
        sd->display, g_cfg->default_wm);
    g_execlp3(g_cfg->default_wm, g_cfg->default_wm, 0);

    /* still a problem starting window manager just start xterm */
    LOG(LOG_LEVEL_WARNING,
        "No window manager on display %s started, "
        "so falling back to starting xterm for user debugging",
        sd->display);
    g_execlp3("xterm", "xterm", 0);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR, "A fatal error has occurred attempting to start "
        "the window manager on display %s, aborting connection",
        sd->display);
}

/******************************************************************************/
static struct list *
prepare_xorg_xserver_params(const struct session_data *sd,
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
                "[session start] (display :%d): Failed to disable "
                "setuid on X server: %s",
                sd->params.x11_display, g_get_strerror());
        }

        g_snprintf(screen, sizeof(screen), ":%d", sd->params.x11_display);

        /* some args are passed via env vars */
        g_snprintf(text, sizeof(text), "%d", sd->params.width);
        g_setenv_log("XRDP_START_WIDTH", text, 1);

        g_snprintf(text, sizeof(text), "%d", sd->params.height);
        g_setenv_log("XRDP_START_HEIGHT", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_idle_time);
        g_setenv_log("XRDP_SESMAN_MAX_IDLE_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.max_disc_time);
        g_setenv_log("XRDP_SESMAN_MAX_DISC_TIME", text, 1);

        g_snprintf(text, sizeof(text), "%d", g_cfg->sess.kill_disconnected);
        g_setenv_log("XRDP_SESMAN_KILL_DISCONNECTED", text, 1);

        g_snprintf(text, sizeof(text), XRDP_X11RDP_BASE_STR, sd->display);
        g_setenv_log("XRDP_X11RDP_SOCKET", text, 1);

        g_snprintf(text, sizeof(text), XRDP_DISCONNECT_BASE_STR, sd->display);
        g_setenv_log("XRDP_DISCONNECT_SOCKET", text, 1);

        /* get path of Xorg from config */
        xserver = (const char *)list_get_item(g_cfg->xorg_params, 0);

        /* these are the must have parameters */
        list_add_strdup_multi(params,
                              xserver, screen,
                              "-auth", authfile,
                              LIST_ADD_STRDUP_TERM);

        /* additional parameters from sesman.ini file */
        list_append_list_strdup(g_cfg->xorg_params, params, 1);
    }

    return params;
}

/******************************************************************************/
/**
 * Create an Xvnc password file
 *
 * @param x11_display X11 display number
 * @return Name of passwd file, or NULL if no memory.
 *
 * env_set_user() must be called before calling this function
 */
static char *
get_xvnc_passwd_file_name(int x11_display)
{
    char *result = NULL;
    int len;

    char *pw_username = NULL;
    char *pw_dir = NULL;
    char hostname[256];
    int error;

    /* Get parameters needed for VNC filename */
    hostname[sizeof(hostname) - 1] = '\0';
    g_gethostname(hostname, sizeof(hostname));
    error = g_getuser_info_by_uid(g_getuid(), &pw_username, 0, 0, &pw_dir, 0);

    if (error != 0 || pw_username == NULL || pw_dir == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't get parameters for XVnc passwd file");
    }
    else
    {
        if (0 == g_cfg->auth_file_path)
        {
            /* if no auth_file_path is set, then we go for
             $HOME/.vnc/sesman_passwd-USERNAME@HOSTNAME:DISPLAY */
            if (!g_directory_exist(".vnc"))
            {
                if (g_mkdir(".vnc") < 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Error creating .vnc directory: %s",
                        g_get_strerror());
                }
            }

            len = g_snprintf(NULL, 0, "%s/.vnc/sesman_passwd-%s@%s:%d",
                             pw_dir, pw_username, hostname, x11_display);
            ++len; // Allow for terminator

            result = (char *) g_malloc(len, 1);
            if (result != NULL)
            {
                /* Try legacy names first, remove if found */
                g_snprintf(result, len,
                           "%s/.vnc/sesman_%s_passwd:%d",
                           pw_dir, pw_username, x11_display);
                if (g_file_exist(result))
                {
                    LOG(LOG_LEVEL_WARNING, "Removing old "
                        "password file %s", result);
                    g_file_delete(result);
                }
                g_snprintf(result, len,
                           "%s/.vnc/sesman_%s_passwd",
                           pw_dir, pw_username);
                if (g_file_exist(result))
                {
                    LOG(LOG_LEVEL_WARNING, "Removing insecure "
                        "password file %s", result);
                    g_file_delete(result);
                }
                g_snprintf(result, len,
                           "%s/.vnc/sesman_passwd-%s@%s:%d",
                           pw_dir, pw_username, hostname, x11_display);
            }
        }
        else
        {
            /* we use auth_file_path as requested */
            len = g_snprintf(NULL, 0, g_cfg->auth_file_path, pw_username);

            ++len; // Allow for terminator
            result = (char *) g_malloc(len, 1);
            if (result != NULL)
            {
                g_snprintf(result, len,
                           g_cfg->auth_file_path, pw_username);
            }
        }

        if (result == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Can't allocate memory for Xvnc passwd file name");
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "pass file: %s", result);
        }
    }
    g_free(pw_username);
    g_free(pw_dir);

    return result;
}

/******************************************************************************/
static int
set_xvnc_passwd(const char *filename, const char *passwd)
{
    char encryptedPasswd[16];
    char key[24];
    char passwd_hash[20];
    char passwd_hash_text[40];
    int fd;
    int passwd_bytes;
    void *des;
    void *sha1;

    if (filename == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "Cannot write VNC password hash to NULL file");
        return 1;
    }

    /*
     * If we're in FIPS mode, do not write the GUID to disk after it's
     * been encrypted with an insecure algorithm.
     */
    if (g_fips_mode_enabled())
    {
        LOG(LOG_LEVEL_ERROR, "Can't create VNC password file in FIPS mode");
        return 1;
    }
    /* create password hash from password */
    passwd_bytes = (passwd == NULL) ? 0 : strlen(passwd);
    sha1 = ssl_sha1_info_create();
    ssl_sha1_clear(sha1);
    ssl_sha1_transform(sha1, "xrdp_vnc", 8);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_complete(sha1, passwd_hash);
    ssl_sha1_info_delete(sha1);
    g_snprintf(passwd_hash_text, sizeof(passwd_hash_text),
               "%2.2x%2.2x%2.2x%2.2x",
               (tui8)passwd_hash[0], (tui8)passwd_hash[1],
               (tui8)passwd_hash[2], (tui8)passwd_hash[3]);
    passwd = passwd_hash_text;

    /* create file from password */
    g_memset(encryptedPasswd, 0, sizeof(encryptedPasswd));
    g_strncpy(encryptedPasswd, passwd, 8);
    g_memset(key, 0, sizeof(key));
    g_mirror_memcpy(key, g_fixedkey, 8);
    des = ssl_des3_encrypt_info_create(key, 0);
    ssl_des3_encrypt(des, 8, encryptedPasswd, encryptedPasswd);
    ssl_des3_info_delete(des);
    fd = g_file_open_ex(filename, 0, 1, 1, 1);
    if (fd == -1)
    {
        LOG(LOG_LEVEL_WARNING,
            "Cannot write VNC password hash to file %s: %s",
            filename, g_get_strerror());
        return 1;
    }
    g_file_write(fd, encryptedPasswd, 8);
    g_file_close(fd);
    return 0;
}

/******************************************************************************/
/**
 * Prepare a list of parameters for the Xvnc X server
 * @param sd Session data
 * @param authfile XAUTHORITY file
 * @param passwd_file VNC password file, or NULL
 * @param port UDS port to connect to, or NULL
 * @return parameters list
 *
 * One of passwd_file and port must be set
 */
static struct list *
prepare_xvnc_xserver_params(const struct session_data *sd,
                            const char *authfile,
                            const char *passwd_file,
                            const char *port)
{
    char screen[32] = {0}; /* display number */
    char geometry[32] = {0};
    char depth[32] = {0};
    const char *xserver;
    const struct session_parameters *sp = &sd->params;

    struct list *params = list_create();
    if (params != NULL)
    {
        params->auto_free = 1;

        g_snprintf(screen, sizeof(screen), ":%d", sp->x11_display);
        g_snprintf(geometry, sizeof(geometry), "%dx%d", sp->width, sp->height);
        g_snprintf(depth, sizeof(depth), "%d", sp->bpp);

        /* get path of Xvnc from config */
        xserver = (const char *)list_get_item(g_cfg->vnc_params, 0);

        /* these are the must have parameters */
        list_add_strdup_multi(params,
                              xserver, screen,
                              "-auth", authfile,
                              "-geometry", geometry,
                              "-depth", depth,
                              LIST_ADD_STRDUP_TERM);

        if (passwd_file != NULL)
        {
            /* RFB authorization */
            list_add_strdup_multi(params,
                                  "-rfbauth", passwd_file,
                                  LIST_ADD_STRDUP_TERM);
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
                                  "-rfbport", "-1",
                                  "-rfbunixpath", port,
                                  "-rfbunixmode", sock_mode,
                                  "-SecurityTypes", "None",
                                  LIST_ADD_STRDUP_TERM);
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
start_x_server(const struct login_info *login_info,
               const struct session_data *sd,
               void *closure /* unused */)
{
    char authfile[256]; /* The filename for storing xauth information */
    char execvpparams[2048];
    char *passwd_file = NULL;
    struct list *xserver_params = NULL;
    int unknown_session_type = 0;
    const struct session_parameters *sp = &sd->params;

    env_set_user(login_info->uid,
                 g_cfg->env_names,
                 g_cfg->env_values);

    /* Allocate the passwd_file if required */
    if (sp->type == SCP_SESSION_TYPE_XVNC &&
            (passwd_file = get_xvnc_passwd_file_name(sp->x11_display)) == NULL)
    {
        /* An error has been logged */
        return;
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
    if (sp->x11_display >= 0 &&
            add_xauth_cookie(sp->x11_display, authfile) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Error setting the xauth cookie for display %s in file %s",
            sd->display, authfile);
    }
    else
    {
        switch (sp->type)
        {
                char guid_str[GUID_STR_SIZE];
                char port[256];

            case SCP_SESSION_TYPE_XORG:
                xserver_params = prepare_xorg_xserver_params(sd, authfile);
                break;

            case SCP_SESSION_TYPE_XVNC:
                guid_to_str(&sp->guid, guid_str);
                set_xvnc_passwd(passwd_file, guid_str);
                xserver_params = prepare_xvnc_xserver_params(sd, authfile,
                                 passwd_file, NULL);
                break;

            case SCP_SESSION_TYPE_XVNC_UDS:
                g_snprintf(port, sizeof(port), XRDP_X11RDP_STR,
                           login_info->uid, sd->display);
                xserver_params = prepare_xvnc_xserver_params(sd, authfile,
                                 NULL, port);
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
                sp->type);
        }
        else
        {
            /* fire up X server */
            LOG(LOG_LEVEL_INFO, "Starting X server on display %s: %s",
                sd->display,
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
        "to start the X server on display %s, aborting connection",
        sd->display);
}

/******************************************************************************/
/*
 * Simple helper process to fork a child and log errors */
static int
fork_child(
    void (*runproc)(const struct login_info *,
                    const struct session_data *,
                    void *closure),
    const struct login_info *login_info,
    const struct session_data *sd,
    pid_t group_pid,
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
        runproc(login_info, sd, closure);
        g_exit(0);
    }

    if (pid < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Fork failed [%s]", g_get_strerror());
    }

    return pid;
}

/******************************************************************************/
static int
process_startup_wait_time(struct session_data *sd)
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
            session_process_sigchld_event(sd);
            if (sd->win_mgr < 0)
            {
                // Session has failed in the StartupWaitTime
                // Wait for the rest of the session to finish
                rv = 1;
                session_send_term(sd, 1);
                break;
            }
        }
    }

    return rv;
}

/******************************************************************************/
static enum scp_screate_status
session_start_wrapped(struct login_info *login_info,
                      const struct session_parameters *s,
                      struct session_data *sd)
{
    int chansrv_pid;
    int display_pid;
    int window_manager_pid;
    enum scp_screate_status status = E_SCP_SCREATE_GENERAL_ERROR;

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

    if (auth_start_session(login_info->auth_info, sd->display) != 0)
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
            "[session start] (display %s): setsid failed - pid %d",
            sd->display, g_getpid());
    }

    if (g_setlogin(login_info->username) < 0)
    {
        LOG(LOG_LEVEL_WARNING,
            "[session start] (display %s): setlogin failed for user %s - pid %d",
            sd->display, login_info->username, g_getpid());
    }
#endif

    /* start the X server in a new process group.
     *
     * We group the X server, window manager and chansrv in a single
     * process group, as it allows signals to be sent to the user session
     * without affecting sesexec (and vice-versa). This is particularly
     * important when debugging sesexec as we don't want a SIGINT in
     * the debugger to be passed to the children */
    display_pid = fork_child(start_x_server, login_info, sd, 0, NULL);
    if (display_pid > 0)
    {
        enum xwait_status xws;
        xws = wait_for_xserver(login_info->uid,
                               g_cfg->env_names,
                               g_cfg->env_values,
                               s->x11_display);

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
            LOG(LOG_LEVEL_INFO, "Display %s is working", sd->display);
            LOG(LOG_LEVEL_INFO, "Starting window manager for display %s",
                sd->display);

            window_manager_pid = fork_child(start_window_manager,
                                            login_info, sd, display_pid, NULL);
            if (window_manager_pid < 0)
            {
                g_sigterm(display_pid);
                g_waitpid(display_pid);
            }
            else
            {
                utmp_login(window_manager_pid, sd->display, login_info);
                LOG(LOG_LEVEL_INFO,
                    "Starting the xrdp channel server for display %s",
                    sd->display);

                chansrv_pid = fork_child(start_chansrv, login_info,
                                         sd, display_pid, NULL);

                sd->win_mgr = window_manager_pid;
                sd->x_server = display_pid;
                sd->chansrv = chansrv_pid;
                sd->start_time = time(NULL);

                if (process_startup_wait_time(sd) == 0)
                {
                    // Tell the caller we've started
                    LOG(LOG_LEVEL_INFO,
                        "Session in progress on display %s. Waiting until the "
                        "window manager (pid %d) exits to end the session",
                        sd->display, window_manager_pid);

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
enum scp_screate_status
session_start(struct login_info *login_info,
              const struct session_parameters *sp,
              struct session_data **session_data)
{
    enum scp_screate_status status = E_SCP_SCREATE_OK;

    /* Create the session_data struct first */
    struct session_data *sd = session_data_new(sp);
    if (sd == NULL)
    {
        status = E_SCP_SCREATE_NO_MEMORY;
    }
    else
    {
        if (sp->x11_display >= 0)
        {
            /* Initialise the display name for logging purposes */
            g_get_display_string_from_x11_display(sp->x11_display,
                                                  sd->display,
                                                  MAX_DISPLAY_NAME_SIZE);
            /* Add the DISPLAY to the list of environment variables we
             * set for all the sub-processes */
            char displayname[32];
            snprintf(displayname, sizeof(displayname), ":%d",
                     sp->x11_display);

            if (!list_add_strdup(g_cfg->env_names, "DISPLAY") ||
                    !list_add_strdup(g_cfg->env_values, displayname))
            {
                session_data_free(sd);
                status = E_SCP_SCREATE_NO_MEMORY;
            }
        }

        if (status == E_SCP_SCREATE_OK)
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
    }

    return status;
}

/******************************************************************************/
static int
cleanup_sockets(struct session_data *sd)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "cleanup_sockets:");

    char file[XRDP_SOCKETS_MAXPATH];
    int error = 0;

    int uid = g_login_info->uid;

    g_snprintf(file, sizeof(file), CHANSRV_PORT_OUT_STR, uid, sd->display);
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

    g_snprintf(file, sizeof(file), CHANSRV_PORT_IN_STR, uid, sd->display);
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

    g_snprintf(file, sizeof(file), XRDP_CHANSRV_STR, uid, sd->display);
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

    g_snprintf(file, sizeof(file), CHANSRV_API_STR, uid, sd->display);
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

    g_snprintf(file, sizeof(file), XRDP_X11RDP_STR, uid, sd->display);
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

    g_snprintf(file, sizeof(file), XRDP_DISCONNECT_STR, uid, sd->display);
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
/**
 * Processes an exited child
 *
 * The PID of the child process is removed from the session_data.
 *
 * @param sd session_data for this session
 * @param pid PID of exited process
 * @param e Exit status of the exited process
 */
static void
process_child_exit(struct session_data *sd,
                   int pid,
                   const struct proc_exit_status *e)
{
    if (pid == sd->x_server)
    {
        LOG(LOG_LEVEL_INFO, "X server pid %d on display %s finished",
            sd->x_server, sd->display);
        sd->x_server = -1;
        // No other action - window manager should be going soon
    }
    else if (pid == sd->chansrv)
    {
        LOG(LOG_LEVEL_INFO,
            "xrdp channel server pid %d on display %s finished",
            sd->chansrv, sd->display);
        sd->chansrv = -1;
    }
    else if (pid == sd->win_mgr)
    {
        int wm_wait_time = time(NULL) - sd->start_time;

        if (e->reason == E_PXR_STATUS_CODE && e->val == 0)
        {
            LOG(LOG_LEVEL_INFO,
                "Window manager (pid %d, display %s) "
                "finished normally in %d secs",
                sd->win_mgr, sd->display, wm_wait_time);
        }
        else
        {
            char reason[128];
            exit_status_to_str(e, reason, sizeof(reason));

            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %s) "
                "exited with %s. This "
                "could indicate a window manager config problem",
                sd->win_mgr, sd->display, reason);
        }
        if (wm_wait_time < 10)
        {
            /* This could be a config issue. Log a significant error */
            LOG(LOG_LEVEL_WARNING, "Window manager (pid %d, display %s) "
                "exited quickly (%d secs). This could indicate a window "
                "manager config problem",
                sd->win_mgr, sd->display, wm_wait_time);
        }

        utmp_logout(sd->win_mgr, sd->display, e);
        sd->win_mgr = -1;

        if (sd->x_server > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating X server (pid %d) on display %s",
                sd->x_server, sd->display);
            g_sigterm(sd->x_server);
        }

        if (sd->chansrv > 0)
        {
            LOG(LOG_LEVEL_INFO, "Terminating the xrdp channel server (pid %d) "
                "on display %s", sd->chansrv, sd->display);
            g_sigterm(sd->chansrv);
        }
    }

    if (!session_active(sd))
    {
        cleanup_sockets(sd);
    }
}

/******************************************************************************/
void
session_process_sigchld_event(struct session_data *sd)
{
    struct proc_exit_status e;
    int pid;

    // Check for any finished children
    while ((pid = g_waitchild(&e)) > 0)
    {
        process_child_exit(sd, pid, &e);
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
unsigned int
session_get_connect_count(const struct session_data *sd)
{
    return (sd == NULL) ? 0 : sd->connect_count;
}

/******************************************************************************/
const char *
session_get_display(const struct session_data *sd)
{
    return (sd == NULL) ? "" : sd->display;
}

/******************************************************************************/
unsigned int
session_increment_connect_count(struct session_data *sd)
{
    return (sd == NULL) ? 0 : sd->connect_count++;
}

/******************************************************************************/
const struct session_parameters *
session_get_parameters(const struct session_data *sd)
{
    return (sd == NULL) ? NULL : &sd->params;
}

/******************************************************************************/
void
session_send_term(struct session_data *sd, int wait_for_all)
{
    if (sd != NULL)
    {
        if (sd->win_mgr > 0)
        {
            // Killing the window manager only is appropriate here.
            // When we process SIGCHLD for the window manager, we
            // will kill other processes as appropriate
            g_sigterm(sd->win_mgr);
        }

        if (wait_for_all)
        {
            while (session_active(sd))
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
                    session_process_sigchld_event(sd);
                }
            }
        }
    }
}

/******************************************************************************/
static void
start_reconnect_script(const struct login_info *login_info,
                       const struct session_data *sd,
                       void *closure)
{
    env_set_user(login_info->uid,
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
            "Starting session reconnection script on display %s: %s",
            sd->display, g_cfg->reconnect_sh);
        g_execlp3(g_cfg->reconnect_sh, g_cfg->reconnect_sh, 0);

        /* should not get here */
        LOG(LOG_LEVEL_ERROR,
            "Error starting session reconnection script on display %s: %s",
            sd->display, g_cfg->reconnect_sh);
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
session_run_reconnect_script(const struct login_info *login_info,
                             const struct session_data *sd,
                             const char *vars[])
{
    if (fork_child(start_reconnect_script,
                   login_info, sd, sd->x_server, (void *)vars) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to fork for session reconnection script");
    }
}

/******************************************************************************/
int
session_get_display_server_fd(const struct login_info *login_info,
                              const struct session_data *sd)
{
    char portname[XRDP_SOCKETS_MAXPATH];
    const char *localhost = "localhost"; // Ignored for TRANS_MODE_UNIX
    int socket_mode;

    int rv = -1;

    if (sd->x_server <= 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Request to connect to display server %s"
            " which has exited", sd->display);
    }
    else
    {
        switch (sd->params.type)
        {
            case SCP_SESSION_TYPE_XVNC:
                socket_mode = TRANS_MODE_TCP;
                snprintf(portname, sizeof(portname), "%d",
                         5900 + sd->params.x11_display);
                break;

            case SCP_SESSION_TYPE_XVNC_UDS:
            case SCP_SESSION_TYPE_XORG:
                socket_mode = TRANS_MODE_UNIX;
                snprintf(portname, sizeof(portname), XRDP_X11RDP_STR,
                         login_info->uid, sd->display);

                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Unsupported session type %d for connect",
                    sd->params.type);
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
                LOG(LOG_LEVEL_ERROR, "Can't connect to display server %s [%s]",
                    sd->display,
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
int
session_get_chansrv_fd(const struct login_info *login_info,
                       const struct session_data *sd)
{
    char portname[XRDP_SOCKETS_MAXPATH];

    int rv = -1;

    if (sd->chansrv <= 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Request to connect to chansrv %s"
            " which has exited", sd->display);
    }
    else
    {
        snprintf(portname, sizeof(portname),
                 XRDP_CHANSRV_STR, login_info->uid, sd->display);

        // Use the transport library to get the fd
        struct trans *t = trans_create(TRANS_MODE_UNIX, 8192, 8192);
        if (t == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Out of memory creating transport");
        }
        else if (trans_connect(t, NULL, portname, 10 * 1000) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't connect to chansrv %s [%s]",
                sd->display,
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
