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

#include "auth.h"
#include "config.h"
#include "lock_uds.h"
#include "os_calls.h"
#include "scp.h"
#include "scp_process.h"
#include "sig.h"
#include "string_calls.h"
#include "trans.h"
#include "xrdp_configure_options.h"

/**
 * Maximum number of short-lived connections to sesman
 *
 * At the moment, all connections to sesman are short-lived. This may change
 * in the future
 */
#define MAX_SHORT_LIVED_CONNECTIONS 16

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
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
tintptr g_term_event = 0;
tintptr g_sigchld_event = 0;
tintptr g_reload_event = 0;

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

/**
 * Allocates a sesman_con struct
 *
 * @param trans Pointer to  newly-allocated transport
 * @return struct sesman_con pointer
 */
static struct sesman_con *
alloc_connection(struct trans *t)
{
    struct sesman_con *result;

    if ((result = g_new0(struct sesman_con, 1)) != NULL)
    {
        g_snprintf(result->peername, sizeof(result->peername), "%s", "unknown");
        result->t = t;
        result->auth_retry_count = g_cfg->sec.login_retry;
    }

    return result;
}

/**
 * Deletes a sesman_con struct, freeing resources
 *
 * After this call, the passed-in pointer is invalid and must not be
 * referenced.
 *
 * Any auth_info struct found in the sesman_con is also deallocated.
 *
 * @param sc struct to de-allocate
 */
static void
delete_connection(struct sesman_con *sc)
{
    if (sc != NULL)
    {
        trans_delete(sc->t);
        if (sc->auth_info != NULL)
        {
            auth_end(sc->auth_info);
        }
        g_free(sc->username);
        g_free(sc->ip_addr);
        g_free(sc);
    }
}

/*****************************************************************************/
int
sesman_set_connection_peername(struct sesman_con *sc, const char *name)
{
    int rv = 1;

    if (sc != NULL && name != NULL)
    {
        g_snprintf(sc->peername, sizeof(sc->peername), "%s", name);
        rv = 0;
    }

    return rv;
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
static int
create_sesman_runtime_dir(void)
{
    int rv = -1;
    /* Make sure if we create the directory, there's no gap where it
    * may have the wrong permissions */
    int entry_umask = g_umask_hex(0x755);

    if (!g_directory_exist(SESMAN_RUNTIME_PATH) &&
            !g_create_dir(SESMAN_RUNTIME_PATH))
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't create runtime directory '"
            SESMAN_RUNTIME_PATH "' [%s]", g_get_strerror());
    }
    else if (g_chown(SESMAN_RUNTIME_PATH, g_getuid(), g_getuid()) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't set ownership of sesman runtime directory [%s]",
            g_get_strerror());
    }
    else if (g_chmod_hex(SESMAN_RUNTIME_PATH, 0x755) != 0)
    {
        /* This might seem redundant, but there's a chance the
         * directory already exists */
        LOG(LOG_LEVEL_ERROR,
            "Can't set permissions of sesman runtime directory [%s]",
            g_get_strerror());
    }
    else
    {
        rv = 0;
    }
    g_umask_hex(entry_umask);

    return rv;
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
sesman_close_all(unsigned int flags)
{
    int index;
    struct sesman_con *sc;

    LOG_DEVEL(LOG_LEVEL_TRACE, "sesman_close_all:");


    sesman_delete_listening_transport();
    for (index = 0; index < g_con_list->count; index++)
    {
        sc = (struct sesman_con *) list_get_item(g_con_list, index);
        if (sc != NULL && (flags & SCA_CLOSE_AUTH_INFO) == 0)
        {
            // Prevent delete_connection() closing the auth_info down
            sc->auth_info = NULL;
        }
        delete_connection(sc);
    }
    return 0;
}

/******************************************************************************/
static int
sesman_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = scp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        struct sesman_con *sc = (struct sesman_con *)self->callback_data;
        if ((rv = scp_process(sc)) != 0)
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
    struct sesman_con *sc;
    if (g_con_list->count >= MAX_SHORT_LIVED_CONNECTIONS)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_data_in: error, too many "
            "connections, rejecting");
        trans_delete(new_self);
    }
    else if ((sc = alloc_connection(new_self)) == NULL ||
             scp_init_trans(new_self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_data_in: No memory to allocate "
            "new connection");
        delete_connection(sc);
    }
    else
    {
        new_self->callback_data = (void *)sc;
        new_self->trans_data_in = sesman_data_in;
        list_add_item(g_con_list, (intptr_t) sc);
    }

    return 0;
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

/******************************************************************************/
/**
 * Informs the main loop a SIGCHLD has been received
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

    unlock_uds(g_list_trans_lock);
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
    int wobjs_count;
    int timeout;
    int index;
    intptr_t robjs[32];
    intptr_t wobjs[32];
    struct sesman_con *scon;

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
        timeout = -1;
        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;
        robjs[robjs_count++] = g_reload_event;
        wobjs_count = 0;
        for (index = 0; index < g_con_list->count; index++)
        {
            scon = (struct sesman_con *)list_get_item(g_con_list, index);
            if (scon != NULL)
            {
                error = trans_get_wait_objs_rw(scon->t,
                                               robjs, &robjs_count,
                                               wobjs, &wobjs_count, &timeout);
                if (error != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                        "trans_get_wait_objs_rw failed");
                    break;
                }
            }
        }
        if (error != 0)
        {
            break;
        }
        if (g_list_trans != NULL)
        {
            /* g_list_trans might be NULL on a reconfigure if sesman
             * is unable to listen again */
            error = trans_get_wait_objs_rw(g_list_trans, robjs, &robjs_count,
                                           wobjs, &wobjs_count, &timeout);
            if (error != 0)
            {
                LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                    "trans_get_wait_objs_rw failed");
                break;
            }
        }

        if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
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

        if (g_is_wait_obj_set(g_sigchld_event)) /* A child has exited */
        {
            g_reset_wait_obj(g_sigchld_event);
            sig_sesman_session_end();
        }

        if (g_is_wait_obj_set(g_reload_event)) /* We're asked to reload */
        {
            g_reset_wait_obj(g_reload_event);
            sig_sesman_reload_cfg();
        }

        index = 0;
        while (index < g_con_list->count)
        {
            int remove_con = 0;
            scon = (struct sesman_con *)list_get_item(g_con_list, index);
            if (trans_check_wait_objs(scon->t) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                    "trans_check_wait_objs failed, removing trans");
                remove_con = 1;
            }
            else if (scon->close_requested)
            {
                remove_con = 1;
            }

            if (remove_con)
            {
                delete_connection(scon);
                list_remove_item(g_con_list, index);
            }
            else
            {
                ++index;
            }
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
    }

    sesman_close_all(SCA_CLOSE_AUTH_INFO);
    list_delete(g_con_list);
    return 0;
}

/*****************************************************************************/
static void
print_version(void)
{
    g_writeln("xrdp-sesman %s", PACKAGE_VERSION);
    g_writeln("  The xrdp session manager");
    g_writeln("  Copyright (C) 2004-2020 Jay Sorg, "
              "Neutrino Labs, and all contributors.");
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
    else if ((fd = g_file_open(pid_file)) < 0)
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
int
main(int argc, char **argv)
{
    int error;
    enum logReturns log_error;
    char text[256];
    char pid_file[256];
    char default_sesman_ini[256];
    struct sesman_startup_params startup_params = {0};
    int errored_argc;
    int daemon;

    g_init("xrdp-sesman");
    g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
    g_snprintf(default_sesman_ini, 255, "%s/sesman.ini", XRDP_CFG_PATH);

    startup_params.sesman_ini = default_sesman_ini;

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

        if (g_file_open("/dev/null") < 0)
        {
        }

        if (g_file_open("/dev/null") < 0)
        {
        }

        if (g_file_open("/dev/null") < 0)
        {
        }
    }

    /* Create the runtime directory before we try to listen (or
     * test-listen), so there's somewhere for the default socket to live */
    if (create_sesman_runtime_dir() != 0)
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

    g_signal_hang_up(set_reload_event); /* SIGHUP  */
    g_signal_user_interrupt(set_term_event); /* SIGINT  */
    g_signal_terminate(set_term_event); /* SIGTERM */
    g_signal_child_stop(set_sigchld_event); /* SIGCHLD */

    if (daemon)
    {
        /* writing pid file */
        char pid_s[32];
        int fd = g_file_open(pid_file);

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

    /* make sure the socket directory exists */
    g_mk_socket_path("xrdp-sesman");

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

    error = sesman_main_loop();

    /* clean up PID file on exit */
    if (daemon)
    {
        g_file_delete(pid_file);
    }

    g_delete_wait_obj(g_reload_event);
    g_delete_wait_obj(g_sigchld_event);
    g_delete_wait_obj(g_term_event);

    if (!daemon)
    {
        log_end();
    }

    config_free(g_cfg);
    g_deinit();
    g_exit(error);
    return 0;
}
