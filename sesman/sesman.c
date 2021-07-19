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

#include "sesman.h"
#include "xrdp_configure_options.h"
#include "string_calls.h"

/**
 * Maximum number of short-lived connections to sesman
 *
 * At the moment, all connections to sesman are short-lived. This may change
 * in the future
 */
#define MAX_SHORT_LIVED_CONNECTIONS 16

struct sesman_startup_params
{
    const char *sesman_ini;
    int kill;
    int no_daemon;
    int help;
    int version;
    int dump_config;
};

int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct config_sesman *g_cfg; /* defined in config.h */

tintptr g_term_event = 0;

/**
 * Items stored on the g_con_list
 */
struct sesman_con
{
    struct trans *t;
    struct SCP_SESSION *s;
};

static struct trans *g_list_trans = NULL;
static struct list *g_con_list = NULL;

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
    struct SCP_SESSION *s;

    if ((result = g_new(struct sesman_con, 1)) != NULL)
    {
        if ((s = scp_session_create()) != NULL)
        {
            result->t = t;
            result->s = s;
            /* Ensure we can find the connection easily from a callback */
            t->callback_data = (void *)result;
        }
        else
        {
            g_free(result);
            result = NULL;
        }
    }

    return result;
}

/**
 * Deletes a sesman_con struct, freeing resources
 *
 * After this call, the passed-in pointer is invalid and must not be
 * referenced.
 *
 * @param sc struct to de-allocate
 */
static void
delete_connection(struct sesman_con *sc)
{
    trans_delete(sc->t);
    scp_session_destroy(sc->s);
    g_free(sc);
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
            startup_params->kill = 1;
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
    int error;
    int sck;
    int rv = 0;

    sck = g_tcp_socket();
    if (sck < 0)
    {
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "Testing if xrdp-sesman can listen on %s port %s.",
        cfg->listen_address, cfg->listen_port);
    g_tcp_set_non_blocking(sck);
    error = scp_tcp_bind(sck, cfg->listen_address, cfg->listen_port);
    if (error == 0)
    {
        /* try to listen */
        error = g_tcp_listen(sck);

        if (error == 0)
        {
            /* if listen succeeded, stop listen immediately */
            g_sck_close(sck);
        }
        else
        {
            rv = 1;
        }
    }
    else
    {
        rv = 1;
    }

    return rv;
}

/******************************************************************************/
int
sesman_close_all(void)
{
    int index;
    struct sesman_con *sc;

    LOG_DEVEL(LOG_LEVEL_TRACE, "sesman_close_all:");
    trans_delete(g_list_trans);
    for (index = 0; index < g_con_list->count; index++)
    {
        sc = (struct sesman_con *) list_get_item(g_con_list, index);
        delete_connection(sc);
    }
    return 0;
}

/******************************************************************************/
static int
sesman_data_in(struct trans *self)
{
    int version;
    int size;

    if (self->extra_flags == 0)
    {
        in_uint32_be(self->in_s, version);
        in_uint32_be(self->in_s, size);
        if (size > self->in_s->size)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_data_in: bad message size");
            return 1;
        }
        self->header_size = size;
        self->extra_flags = 1;
    }
    else
    {
        /* process message */
        struct sesman_con *sc = (struct sesman_con *)self->callback_data;
        self->in_s->p = self->in_s->data;
        if (scp_process(self, sc->s) != SCP_SERVER_STATE_OK)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_data_in: scp_process_msg failed");
            return 1;
        }
        /* reset for next message */
        self->header_size = 8;
        self->extra_flags = 0;
        init_stream(self->in_s, 0); /* Reset input stream pointers */
    }
    return 0;
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
    else if ((sc = alloc_connection(new_self)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_data_in: No memory to allocate "
            "new connection");
        trans_delete(new_self);
    }
    else
    {
        new_self->header_size = 8;
        new_self->trans_data_in = sesman_data_in;
        new_self->no_stream_init_on_data_in = 1;
        new_self->extra_flags = 0;
        list_add_item(g_con_list, (intptr_t) sc);
    }

    return 0;
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
    int cont;
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
    g_list_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
    if (g_list_trans == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_main_loop: trans_create failed");
        list_delete(g_con_list);
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "sesman_main_loop: address %s port %s",
        g_cfg->listen_address, g_cfg->listen_port);
    error = trans_listen_address(g_list_trans, g_cfg->listen_port,
                                 g_cfg->listen_address);
    if (error != 0)
    {
        LOG(LOG_LEVEL_ERROR, "sesman_main_loop: trans_listen_address "
            "failed");
        trans_delete(g_list_trans);
        list_delete(g_con_list);
        return 1;
    }
    g_list_trans->trans_conn_in = sesman_listen_conn_in;
    cont = 1;
    while (cont)
    {
        timeout = -1;
        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
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
        error = trans_get_wait_objs_rw(g_list_trans, robjs, &robjs_count,
                                       wobjs, &wobjs_count, &timeout);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "trans_get_wait_objs_rw failed");
            break;
        }

        error = g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout);
        if (error != 0)
        {
            /* error, should not get here */
            g_sleep(100);
        }

        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
            break;
        }

        for (index = 0; index < g_con_list->count; index++)
        {
            scon = (struct sesman_con *)list_get_item(g_con_list, index);
            if (scon != NULL)
            {
                error = trans_check_wait_objs(scon->t);
                if (error != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                        "trans_check_wait_objs failed, removing trans");
                    delete_connection(scon);
                    list_remove_item(g_con_list, index);
                    index--;
                    continue;
                }
            }
        }
        error = trans_check_wait_objs(g_list_trans);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesman_main_loop: "
                "trans_check_wait_objs failed");
            break;
        }
    }
    for (index = 0; index < g_con_list->count; index++)
    {
        scon = (struct sesman_con *) list_get_item(g_con_list, index);
        delete_connection(scon);
    }
    list_delete(g_con_list);
    trans_delete(g_list_trans);
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
    g_printf("   -h, --help        show help\n");
    g_printf("   -v, --version     show version\n");
    g_printf("   -n, --nodaemon    don't fork into background\n");
    g_printf("   -c, --config      specify new path to sesman.ini\n");
    g_writeln("      --dump-config display config on stdout on startup");
    g_deinit();
}

/******************************************************************************/
static int
kill_running_sesman(const char *pid_file)
{
    int error;
    int fd;
    int pid;
    char pid_s[32] = {0};

    /* check if sesman is running */
    if (!g_file_exist(pid_file))
    {
        g_printf("sesman is not running (pid file not found - %s)\n", pid_file);
        g_deinit();
        return 1;
    }

    fd = g_file_open(pid_file);

    if (-1 == fd)
    {
        g_printf("error opening pid file[%s]: %s\n", pid_file, g_get_strerror());
        return 1;
    }

    error = g_file_read(fd, pid_s, sizeof(pid_s) - 1);

    if (-1 == error)
    {
        g_printf("error reading pid file: %s\n", g_get_strerror());
        g_file_close(fd);
        g_deinit();
        return 1;
    }

    g_file_close(fd);
    pid = g_atoi(pid_s);

    error = g_sigterm(pid);

    if (0 != error)
    {
        g_printf("error killing sesman: %s\n", g_get_strerror());
    }
    else
    {
        g_file_delete(pid_file);
    }

    g_deinit();
    return error;
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


    if (startup_params.kill)
    {
        g_exit(kill_running_sesman(pid_file));
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

    /* reading config */
    if ((g_cfg = config_read(startup_params.sesman_ini)) == NULL)
    {
        g_printf("error reading config %s: %s\nquitting.\n",
                 startup_params.sesman_ini, g_get_strerror());
        g_deinit();
        g_exit(1);
    }

    if (startup_params.dump_config)
    {
        config_dump(g_cfg);
    }

    /* starting logging subsystem */
    log_error = log_start(startup_params.sesman_ini, "xrdp-sesman",
                          startup_params.dump_config);

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
                g_writeln("error");
                break;
        }

        config_free(g_cfg);
        g_deinit();
        g_exit(1);
    }

    LOG(LOG_LEVEL_TRACE, "config loaded in %s at %s:%d", __func__, __FILE__, __LINE__);
    LOG(LOG_LEVEL_TRACE, "    sesman_ini        = %s", g_cfg->sesman_ini);
    LOG(LOG_LEVEL_TRACE, "    listen_address    = %s", g_cfg->listen_address);
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

    /* libscp initialization */
    scp_init();


    if (daemon)
    {
        /* start of daemonizing code */
        if (sesman_listen_test(g_cfg) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Failed to start xrdp-sesman daemon, "
                "possibly address already in use.");
            config_free(g_cfg);
            g_deinit();
            g_exit(1);
        }

        if (0 != g_fork())
        {
            config_free(g_cfg);
            g_deinit();
            g_exit(0);
        }

    }

    /* signal handling */
    g_pid = g_getpid();
    /* old style signal handling is now managed synchronously by a
     * separate thread. uncomment this block if you need old style
     * signal handling and comment out thread_sighandler_start()
     * going back to old style for the time being
     * problem with the sigaddset functions in sig.c - jts */
#if 1
    g_signal_hang_up(sig_sesman_reload_cfg); /* SIGHUP  */
    g_signal_user_interrupt(sig_sesman_shutdown); /* SIGINT  */
    g_signal_terminate(sig_sesman_shutdown); /* SIGTERM */
    g_signal_child_stop(sig_sesman_session_end); /* SIGCHLD */
#endif
#if 0
    thread_sighandler_start();
#endif

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

    g_snprintf(text, 255, "xrdp_sesman_%8.8x_main_term", g_pid);
    g_term_event = g_create_wait_obj(text);

    error = sesman_main_loop();

    /* clean up PID file on exit */
    if (daemon)
    {
        g_file_delete(pid_file);
    }

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
