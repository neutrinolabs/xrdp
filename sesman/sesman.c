/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * @file sesman.c
 * @brief Main program file
 * @author Jay Sorg
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdarg.h>

#include "arch.h"
#include "sesman.h"

#include "sesman_auth.h"
#include "sesman_config.h"
#include "eicp.h"
#include "eicp_process.h"
#include "ercp.h"
#include "ercp_process.h"
#include "pre_session_list.h"
#include "session_list.h"
#include "lock_uds.h"
#include "os_calls.h"
#include "scp.h"
#include "scp_process.h"
#include "sesexec_control.h"
#include "sig.h"
#include "string_calls.h"
#include "trans.h"
#include "xrdp_configure_options.h"
#include "xrdp_sockets.h"

/**
 * Maximum number of pre-session items
 */
#define MAX_PRE_SESSION_ITEMS 16

/**
 * Define the mode of operation of the program
 */
enum sesman_mode
{
    SSM_NORMAL = 0,
    SSM_KILL_DAEMON,
    SSM_RELOAD_DAEMON
};

struct sesman_startup_params
{
    const char *sesman_ini;
    enum sesman_mode mode;
    int no_daemon;
    int help;
    int version;
    int dump_config;
};

struct config_sesman *g_cfg;
static tintptr g_term_event = 0;
static tintptr g_sigchld_event = 0;
static tintptr g_reload_event = 0;

static struct trans *g_list_trans;

/* Variables used to lock g_list_trans */
static struct lock_uds *g_list_trans_lock;

static struct list *g_con_list = NULL;
static int g_pid;

/*****************************************************************************/
/**
 * @brief looks for a case-insensitive match of a string in a list
 * @param candidate  String to match
 * @param ... NULL-terminated list of strings to compare the candidate with
 * @return !=0 if the candidate is found in the list
 */
static int nocase_matches(const char *candidate, ...)
{
    va_list vl;
    const char *member;
    int result = 0;

    va_start(vl, candidate);
    while ((member = va_arg(vl, const char *)) != NULL)
    {
        if (g_strcasecmp(candidate, member) == 0)
        {
            result = 1;
            break;
        }
    }

    va_end(vl);
    return result;
}

/*****************************************************************************/
/**
 *
 * @brief  Command line argument parser
 * @param[in] argc number of command line arguments
 * @param[in] argv pointer array of commandline arguments
 * @param[out] sesman_startup_params Returned startup parameters
 * @return 0 on success, n on nth argument is unknown
 *
 */
static int
sesman_process_params(int argc, char **argv,
                      struct sesman_startup_params *startup_params)
{
    int index;
    const char *option;
    const char *value;

    startup_params->mode = SSM_NORMAL;

    index = 1;

    while (index < argc)
    {
        option = argv[index];

        if (index + 1 < argc)
        {
            value = argv[index + 1];
        }
        else
        {
            value = "";
        }

        if (nocase_matches(option, "-help", "--help", "-h", NULL))
        {
            startup_params->help = 1;
        }
        else if (nocase_matches(option, "-kill", "--kill", "-k", NULL))
        {
            startup_params->mode = SSM_KILL_DAEMON;
        }
        else if (nocase_matches(option, "-reload", "--reload", "-r", NULL))
        {
            startup_params->mode = SSM_RELOAD_DAEMON;
        }
        else if (nocase_matches(option, "-nodaemon", "--nodaemon", "-n",
                                "-nd", "--nd", "-ns", "--ns", NULL))
        {
            startup_params->no_daemon = 1;
        }
        else if (nocase_matches(option, "-v", "--version", NULL))
        {
            startup_params->version = 1;
        }
        else if (nocase_matches(option, "--dump-config", NULL))
        {
            startup_params->dump_config = 1;
        }
        else if (nocase_matches(option, "-c", "--config", NULL))
        {
            index++;
            startup_params->sesman_ini = value;
        }
        else /* unknown option */
        {
            return index;
        }

        index++;
    }

    return 0;
}

/******************************************************************************/
static int sesman_listen_test(struct config_sesman *cfg)
{
    int status = sesman_create_listening_transport(cfg);
    sesman_delete_listening_transport();
    return status;
}

/******************************************************************************/
int
sesman_close_all(void)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "sesman_close_all:");

    pre_session_list_cleanup();
    session_list_cleanup();

    g_delete_wait_obj(g_reload_event);
    g_delete_wait_obj(g_sigchld_event);
    g_delete_wait_obj(g_term_event);

    sesman_delete_listening_transport();

    return 0;
}

/******************************************************************************/
int
sesman_scp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = scp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        struct pre_session_item *psi;
        psi = (struct pre_session_item *)self->callback_data;

        if ((rv = scp_process(psi)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_data_in: scp_process_msg failed");
        }
        scp_msg_in_reset(self);
    }

    return rv;
}

/******************************************************************************/
static int
sesman_listen_conn_in(struct trans *self, struct trans *new_self)
{
    struct pre_session_item *psi;
    if (pre_session_list_get_count() >= MAX_PRE_SESSION_ITEMS)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_listen_conn_in: error, too many "
            "connections, rejecting");
        trans_delete(new_self);
    }
    else if ((psi = pre_session_list_new()) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_data_in: No memory to allocate "
            "new connection");
        trans_delete(new_self);
    }
    else if (scp_init_trans(new_self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_data_in: Can't init SCP connection");
        trans_delete(new_self);
    }
    else
    {
        new_self->callback_data = (void *)psi;
        new_self->trans_data_in = sesman_scp_data_in;
        psi->client_trans = new_self;
    }

    return 0;
}

/******************************************************************************/
int
sesman_eicp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = eicp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        struct pre_session_item *psi;
        psi = (struct pre_session_item *)self->callback_data;
        if ((rv = eicp_process(psi)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_eicp_data_in: eicp_process_msg failed");
        }
        eicp_msg_in_reset(self);
    }

    return rv;
}

/******************************************************************************/
int
sesman_ercp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = ercp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        struct session_item *si = (struct session_item *)self->callback_data;
        if ((rv = ercp_process(si)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_ercp_data_in: ercp_process_msg failed");
        }
        ercp_msg_in_reset(self);
    }

    return rv;
}

/******************************************************************************/
/**
 * Informs the main loop a termination signal has been received
 */
static void
set_term_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_term_event);
    }
}

/*****************************************************************************/
/* No-op signal handler.
 */
static void
sig_no_op(int sig)
{
    /* no-op */
}

/******************************************************************************/
/**
 * Catch a SIGCHLD and ignore the main loop
 *
 * In theory we could use waitpid() in the signal handler, but that
 * would prevent us adding any logging
 */
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
/**
 * Informs the main loop a SIGHUP has been received
 */
static void
set_reload_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_reload_event);
    }
}

/******************************************************************************/
void
sesman_delete_listening_transport(void)
{
    if (g_getpid() == g_pid)
    {
        trans_delete(g_list_trans);
    }
    else
    {
        trans_delete_from_child(g_list_trans);
    }
    g_list_trans = NULL;

    unlock_uds(g_list_trans_lock); // Won't unlock anything for a child process
    g_list_trans_lock = NULL;
}

/******************************************************************************/
int
sesman_create_listening_transport(const struct config_sesman *cfg)
{
    int rv = 1;
    g_list_trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
    if (g_list_trans == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "%s: trans_create failed", __func__);
    }
    else if ((g_list_trans_lock = lock_uds(cfg->listen_port)) != NULL)
    {
        /* Make sure the file is always created with the correct
         * permissions, if it's not there */
        int entry_umask = g_umask_hex(0x666);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "%s: port %s", __func__, cfg->listen_port);
        rv = trans_listen_address(g_list_trans, cfg->listen_port, NULL);
        if (rv != 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s: trans_listen_address failed", __func__);
        }
        else if (g_chown(cfg->listen_port, g_getuid(), g_getuid()) != 0)
        {
            LOG(LOG_LEVEL_ERROR,
                "Can't set ownership of '%s' [%s]",
                cfg->listen_port, g_get_strerror());
        }
        else if ((rv = g_chmod_hex(cfg->listen_port, 0x666)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s: Can't set permissions on '%s' [%s]",
                __func__, cfg->listen_port, g_get_strerror());
        }
        else
        {
            g_list_trans->trans_conn_in = sesman_listen_conn_in;
        }
        g_umask_hex(entry_umask);
    }

    if (rv != 0)
    {
        sesman_delete_listening_transport();
    }

    return rv;
}

/******************************************************************************/
int
sesman_is_term(void)
{
    return g_is_wait_obj_set(g_term_event);
}

/******************************************************************************/
/**
 *
 * @brief Starts sesman main loop
 *
 */
static int
sesman_main_loop(void)
{
    int error;
    int robjs_count;
    intptr_t robjs[1024];

    g_con_list = list_create();
    if (g_con_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_main_loop: list_create failed");
        return 1;
    }
    if (sesman_create_listening_transport(g_cfg) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "sesman_main_loop: sesman_create_listening_transport failed");
        list_delete(g_con_list);
        return 1;
    }
    LOG(LOG_LEVEL_INFO, "Sesman now listening on %s", g_cfg->listen_port);

    error = 0;
    while (!error)
    {
        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;
        robjs[robjs_count++] = g_reload_event;

        if (g_list_trans != NULL)
        {
            /* g_list_trans might be NULL on a reconfigure if sesman
             * is unable to listen again */
            error = trans_get_wait_objs(g_list_trans, robjs, &robjs_count);
            if (error != 0)
            {
                LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                    "trans_get_wait_objs failed");
                break;
            }
        }

        error = pre_session_list_get_wait_objs(robjs, &robjs_count);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "pre_session_list_get_wait_objs failed");
            break;
        }

        error = session_list_get_wait_objs(robjs, &robjs_count);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "session_list_get_wait_objs failed");
            break;
        }

        if (g_obj_wait(robjs, robjs_count, NULL, 0, -1) != 0)
        {
            /* should not get here */
            LOG(LOG_LEVEL_WARNING, "sesman_main_loop: "
                "Unexpected error from g_obj_wait()");
            g_sleep(100);
        }

        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
            LOG(LOG_LEVEL_INFO, "sesman_main_loop: "
                "sesman asked to terminate");
            break;
        }

        if (g_is_wait_obj_set(g_sigchld_event)) /* term */
        {
            g_reset_wait_obj(g_sigchld_event);
            // Prevent any zombies from hanging around
            while (g_waitchild(NULL) > 0)
            {
                ;
            }
        }

        if (g_is_wait_obj_set(g_reload_event)) /* We're asked to reload */
        {
            g_reset_wait_obj(g_reload_event);
            sig_sesman_reload_cfg();
        }

        if (g_list_trans != NULL)
        {
            error = trans_check_wait_objs(g_list_trans);
            if (error != 0)
            {
                LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                    "trans_check_wait_objs failed");
                break;
            }
        }

        error = pre_session_list_check_wait_objs();
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "pre_session_list_check_wait_objs failed");
            break;
        }

        error = session_list_check_wait_objs();
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "session_list_check_wait_objs failed");
            break;
        }
    }

    return error;
}

/*****************************************************************************/
static void
print_version(void)
{
    g_writeln("xrdp-sesman %s", PACKAGE_VERSION);
    g_writeln("  The xrdp session manager");
    g_writeln("  Copyright (C) 2004-%d Jay Sorg, "
              "Neutrino Labs, and all contributors.",
              VERSION_YEAR);
    g_writeln("  See https://github.com/neutrinolabs/xrdp for more information.");
    g_writeln("%s", "");

#if defined(XRDP_CONFIGURE_OPTIONS)
    g_writeln("  Configure options:");
    g_writeln("%s", XRDP_CONFIGURE_OPTIONS);
#endif
}

/******************************************************************************/
static void
print_help(void)
{
    g_printf("Usage: xrdp-sesman [options]\n");
    g_printf("   -k, --kill        shut down xrdp-sesman\n");
    g_printf("   -r, --reload      reload xrdp-sesman\n");
    g_printf("   -h, --help        show help\n");
    g_printf("   -v, --version     show version\n");
    g_printf("   -n, --nodaemon    don't fork into background\n");
    g_printf("   -c, --config      specify new path to sesman.ini\n");
    g_printf("       --dump-config display config on stdout on startup\n");
    g_deinit();
}

/******************************************************************************/
/**
 * Reads the PID file
 */
static int
read_pid_file(const char *pid_file, int *pid)
{
    int rv = 1;
    int fd;

    /* check if sesman is running */
    if (!g_file_exist(pid_file))
    {
        g_printf("sesman is not running (pid file not found - %s)\n", pid_file);
    }
    else if ((fd = g_file_open_ro(pid_file)) < 0)
    {
        g_printf("error opening pid file[%s]: %s\n", pid_file, g_get_strerror());
    }
    else
    {
        char pid_s[32] = {0};
        int error = g_file_read(fd, pid_s, sizeof(pid_s) - 1);
        g_file_close(fd);

        if (error < 0)
        {
            g_printf("error reading pid file: %s\n", g_get_strerror());
        }
        else
        {
            *pid = g_atoi(pid_s);
            rv = 0;
        }
    }

    return rv;
}

/******************************************************************************/
/** Creates the socket path for sesman and session sockets
*/
static int
create_xrdp_socket_root_path(void)
{
    int uid = g_getuid();
    int gid = g_getgid();

    /* Create the path using 0755 permissions */
    int old_umask = g_umask_hex(0x22);
    (void)g_create_path(XRDP_SOCKET_ROOT_PATH"/");
    (void)g_umask_hex(old_umask);

    /* Check the ownership and permissions on the last path element
     * are as expected */
    if (g_chown(XRDP_SOCKET_ROOT_PATH, uid, gid) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_root_path: Can't set owner of %s to %d:%d",
            XRDP_SOCKET_ROOT_PATH, uid, gid);
        return 1;
    }

    if (g_chmod_hex(XRDP_SOCKET_ROOT_PATH, 0x755) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_root_path: Can't set perms of %s to 0x755",
            XRDP_SOCKET_ROOT_PATH);
        return 1;
    }

    return 0;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int error;
    enum logReturns log_error;
    char text[256];
    char pid_file[256];
    struct sesman_startup_params startup_params = {0};
    int errored_argc;
    int daemon;

    g_init("xrdp-sesman");
    g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);

    startup_params.sesman_ini = DEFAULT_SESMAN_INI;

    errored_argc = sesman_process_params(argc, argv, &startup_params);
    if (errored_argc > 0)
    {
        print_version();
        g_writeln("%s", "");
        print_help();
        g_writeln("%s", "");

        g_writeln("Unknown option: %s", argv[errored_argc]);
        g_deinit();
        g_exit(1);
    }

    if (startup_params.help)
    {
        print_help();
        g_exit(0);
    }

    if (startup_params.version)
    {
        print_version();
        g_exit(0);
    }


    if (startup_params.mode == SSM_KILL_DAEMON)
    {
        int pid;
        int error = 1;
        if (read_pid_file(pid_file, &pid) == 0)
        {
            if (g_sigterm(pid) != 0)
            {
                g_printf("error killing sesman: %s\n", g_get_strerror());
            }
            else
            {
                /* File is no longer required */
                g_file_delete(pid_file);
                error = 0;
            }
        }
        g_deinit();
        g_exit(error);
    }

    if (startup_params.mode == SSM_RELOAD_DAEMON)
    {
        int pid;
        int error = 1;
        if (read_pid_file(pid_file, &pid) == 0)
        {
            if (g_sighup(pid) != 0)
            {
                g_printf("error reloading sesman: %s\n", g_get_strerror());
            }
            else
            {
                error = 0;
            }
        }
        g_deinit();
        g_exit(error);
    }

    if (g_file_exist(pid_file))
    {
        g_printf("xrdp-sesman is already running.\n");
        g_printf("if it's not running, try removing ");
        g_printf("%s", pid_file);
        g_printf("\n");
        g_deinit();
        g_exit(1);
    }

    /* starting logging subsystem */
    if (!g_file_exist(startup_params.sesman_ini))
    {
        g_printf("Config file %s does not exist\n", startup_params.sesman_ini);
        g_deinit();
        g_exit(1);
    }
    log_error = log_start(
                    startup_params.sesman_ini, "xrdp-sesman",
                    (startup_params.dump_config) ? LOG_START_DUMP_CONFIG : 0);

    if (log_error != LOG_STARTUP_OK)
    {
        switch (log_error)
        {
            case LOG_ERROR_MALLOC:
                g_writeln("error on malloc. cannot start logging. quitting.");
                break;
            case LOG_ERROR_FILE_OPEN:
                g_writeln("error opening log file [%s]. quitting.",
                          getLogFile(text, 255));
                break;
            default:
                // Assume sufficient messages have already been generated
                break;
        }

        g_deinit();
        g_exit(1);
    }

    /* reading config */
    if ((g_cfg = config_read(startup_params.sesman_ini)) == NULL)
    {
        LOG(LOG_LEVEL_ALWAYS, "error reading config %s: %s",
            startup_params.sesman_ini, g_get_strerror());
        log_end();
        g_deinit();
        g_exit(1);
    }

    if (startup_params.dump_config)
    {
        config_dump(g_cfg);
    }

    LOG(LOG_LEVEL_TRACE, "config loaded in %s at %s:%d", __func__, __FILE__, __LINE__);
    LOG(LOG_LEVEL_TRACE, "    sesman_ini        = %s", g_cfg->sesman_ini);
    LOG(LOG_LEVEL_TRACE, "    listen_port       = %s", g_cfg->listen_port);
    LOG(LOG_LEVEL_TRACE, "    enable_user_wm    = %d", g_cfg->enable_user_wm);
    LOG(LOG_LEVEL_TRACE, "    default_wm        = %s", g_cfg->default_wm);
    LOG(LOG_LEVEL_TRACE, "    user_wm           = %s", g_cfg->user_wm);
    LOG(LOG_LEVEL_TRACE, "    reconnect_sh      = %s", g_cfg->reconnect_sh);
    LOG(LOG_LEVEL_TRACE, "    auth_file_path    = %s", g_cfg->auth_file_path);

    daemon = !startup_params.no_daemon;
    if (daemon)
    {
        /* not to spit on the console, shut up stdout/stderr before anything's logged */
        g_file_close(0);
        g_file_close(1);
        g_file_close(2);

        if (g_file_open_rw("/dev/null") < 0)
        {
        }

        if (g_file_open_rw("/dev/null") < 0)
        {
        }

        if (g_file_open_rw("/dev/null") < 0)
        {
        }
    }

    /* Create the socket directory before we try to listen (or
     * test-listen), so there's somewhere for the default socket to live */
    if (create_xrdp_socket_root_path() != 0)
    {
        config_free(g_cfg);
        log_end();
        g_deinit();
        g_exit(1);
    }

    if (daemon)
    {
        /* start of daemonizing code */
        if (sesman_listen_test(g_cfg) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Failed to start xrdp-sesman daemon, "
                "possibly address already in use.");
            config_free(g_cfg);
            log_end();
            g_deinit();
            g_exit(1);
        }

        if (0 != g_fork())
        {
            config_free(g_cfg);
            log_end();
            g_deinit();
            g_exit(0);
        }

    }

    /* Now we've forked (if necessary), we can get the program PID */
    g_pid = g_getpid();

    /* signal handling */
    g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_term", g_pid);
    g_term_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_sesman_%8.8x_sigchld", g_pid);
    g_sigchld_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_sesman_%8.8x_reload", g_pid);
    g_reload_event = g_create_wait_obj(text);

    g_signal_user_interrupt(set_term_event); /* SIGINT  */
    g_signal_terminate(set_term_event); /* SIGTERM */
    g_signal_pipe(sig_no_op);          /* SIGPIPE */
    g_signal_child_stop(set_sigchld_event); /* SIGCHLD */
    g_signal_hang_up(set_reload_event); /* SIGHUP  */

    if (daemon)
    {
        /* writing pid file */
        char pid_s[32];
        int fd = g_file_open_rw(pid_file);

        if (-1 == fd)
        {
            LOG(LOG_LEVEL_ERROR,
                "error opening pid file[%s]: %s",
                pid_file, g_get_strerror());
            log_end();
            config_free(g_cfg);
            g_deinit();
            g_exit(1);
        }

        g_sprintf(pid_s, "%d", g_pid);
        g_file_write(fd, pid_s, g_strlen(pid_s));
        g_file_close(fd);
    }

    /* start program main loop */
    LOG(LOG_LEVEL_INFO,
        "starting xrdp-sesman with pid %d", g_pid);

    /* make sure the /tmp/.X11-unix directory exists */
    if (!g_directory_exist("/tmp/.X11-unix"))
    {
        if (!g_create_dir("/tmp/.X11-unix"))
        {
            LOG(LOG_LEVEL_ERROR,
                "sesman.c: error creating dir /tmp/.X11-unix");
        }
        g_chmod_hex("/tmp/.X11-unix", 0x1777);
    }

    if ((error = pre_session_list_init(MAX_PRE_SESSION_ITEMS)) == 0 &&
            (error = session_list_init()) == 0)
    {
        error = sesman_main_loop();
    }

    /* clean up PID file on exit */
    if (daemon)
    {
        g_file_delete(pid_file);
    }

    sesman_close_all();

    log_end();

    config_free(g_cfg);
    g_deinit();
    g_exit(error);
    return 0;
}
