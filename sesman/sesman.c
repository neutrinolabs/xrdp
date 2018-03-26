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

#include "sesman.h"

int g_sck;
int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct config_sesman *g_cfg; /* defined in config.h */

tintptr g_term_event = 0;

/******************************************************************************/
int sesman_listen_test(struct config_sesman *cfg)
{
    int error;
    int sck;
    int rv = 0;

    sck = g_tcp_socket();
    if (sck < 0)
    {
        return 1;
    }

    log_message(LOG_LEVEL_DEBUG, "Testing if xrdp-sesman can listen on %s port %s.",
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
/**
 *
 * @brief Starts sesman main loop
 *
 */
int
sesman_main_loop(void)
{
    int in_sck;
    int error;
    int robjs_count;
    int cont;
    int rv = 0;
    tbus sck_obj;
    tbus robjs[8];

    g_sck = g_tcp_socket();
    if (g_sck < 0)
    {
        log_message(LOG_LEVEL_ERROR, "error opening socket, g_tcp_socket() failed...");
        return 1;
    }

    g_tcp_set_non_blocking(g_sck);
    error = scp_tcp_bind(g_sck, g_cfg->listen_address, g_cfg->listen_port);

    if (error == 0)
    {
        error = g_tcp_listen(g_sck);

        if (error == 0)
        {
            log_message(LOG_LEVEL_INFO, "listening to port %s on %s",
                        g_cfg->listen_port, g_cfg->listen_address);
            sck_obj = g_create_wait_obj_from_socket(g_sck, 0);
            cont = 1;

            while (cont)
            {
                /* build the wait obj list */
                robjs_count = 0;
                robjs[robjs_count++] = sck_obj;
                robjs[robjs_count++] = g_term_event;

                /* wait */
                if (g_obj_wait(robjs, robjs_count, 0, 0, -1) != 0)
                {
                    /* error, should not get here */
                    g_sleep(100);
                }

                if (g_is_wait_obj_set(g_term_event)) /* term */
                {
                    break;
                }

                if (g_is_wait_obj_set(sck_obj)) /* incoming connection */
                {
                    in_sck = g_tcp_accept(g_sck);

                    if ((in_sck == -1) && g_tcp_last_error_would_block(g_sck))
                    {
                        /* should not get here */
                        g_sleep(100);
                    }
                    else if (in_sck == -1)
                    {
                        /* error, should not get here */
                        break;
                    }
                    else
                    {
                        /* we've got a connection, so we pass it to scp code */
                        LOG_DBG("new connection");
                        scp_process_start((void*)(tintptr)in_sck);
                        g_sck_close(in_sck);
                    }
                }
            }

            g_delete_wait_obj_from_socket(sck_obj);
        }
        else
        {
            log_message(LOG_LEVEL_ERROR, "listen error %d (%s)",
                        g_get_errno(), g_get_strerror());
            rv = 1;
        }
    }
    else
    {
        log_message(LOG_LEVEL_ERROR, "bind error on "
                    "port '%s': %d (%s)", g_cfg->listen_port,
                    g_get_errno(), g_get_strerror());
        rv = 1;
    }
    g_tcp_close(g_sck);
    return rv;
}

/******************************************************************************/
void
print_usage(int retcode)
{
    g_printf("xrdp-sesman - xrdp session manager\n\n");
    g_printf("Usage: xrdp-sesman [options]\n");
    g_printf("   -n, --nodaemon   run as foreground process\n");
    g_printf("   -k, --kill       kill running xrdp-sesman\n");
    g_printf("   -h, --help       show this help\n");
    g_deinit();
    g_exit(retcode);
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int fd;
    enum logReturns log_error;
    int error;
    int daemon = 1;
    int pid;
    char pid_s[32];
    char text[256];
    char pid_file[256];
    char cfg_file[256];

    g_init("xrdp-sesman");
    g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);

    if (1 == argc)
    {
        /* no options on command line. normal startup */
        g_printf("starting sesman...\n");
        daemon = 1;
    }
    else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--nodaemon")) ||
                             (0 == g_strcasecmp(argv[1], "-nodaemon")) ||
                             (0 == g_strcasecmp(argv[1], "-n")) ||
                             (0 == g_strcasecmp(argv[1], "-ns"))))
    {
        /* starts sesman not daemonized */
        g_printf("starting sesman in foreground...\n");
        daemon = 0;
    }
    else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--help")) ||
                             (0 == g_strcasecmp(argv[1], "-help")) ||
                             (0 == g_strcasecmp(argv[1], "-h"))))
    {
        print_usage(0);
    }
    else if ((2 == argc) && ((0 == g_strcasecmp(argv[1], "--kill")) ||
                             (0 == g_strcasecmp(argv[1], "-kill")) ||
                             (0 == g_strcasecmp(argv[1], "-k"))))
    {
        /* killing running sesman */
        /* check if sesman is running */
        if (!g_file_exist(pid_file))
        {
            g_printf("sesman is not running (pid file not found - %s)\n", pid_file);
            g_deinit();
            g_exit(1);
        }

        fd = g_file_open(pid_file);

        if (-1 == fd)
        {
            g_printf("error opening pid file[%s]: %s\n", pid_file, g_get_strerror());
            return 1;
        }

        g_memset(pid_s, 0, sizeof(pid_s));
        error = g_file_read(fd, pid_s, 31);

        if (-1 == error)
        {
            g_printf("error reading pid file: %s\n", g_get_strerror());
            g_file_close(fd);
            g_deinit();
            g_exit(error);
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
        g_exit(error);
    }
    else
    {
        /* there's something strange on the command line */
        g_printf("Error: invalid command line arguments\n\n");
        print_usage(1);
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
    g_cfg = g_new0(struct config_sesman, 1);

    if (0 == g_cfg)
    {
        g_printf("error creating config: quitting.\n");
        g_deinit();
        g_exit(1);
    }

    //g_cfg->log.fd = -1; /* don't use logging before reading its config */
    if (0 != config_read(g_cfg))
    {
        g_printf("error reading config: %s\nquitting.\n", g_get_strerror());
        g_deinit();
        g_exit(1);
    }

    g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);

    /* starting logging subsystem */
    log_error = log_start(cfg_file, "xrdp-sesman");

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

        g_deinit();
        g_exit(1);
    }

    /* libscp initialization */
    scp_init();

    if (daemon)
    {
        /* start of daemonizing code */
        if (sesman_listen_test(g_cfg) != 0)
        {

            log_message(LOG_LEVEL_ERROR, "Failed to start xrdp-sesman daemon, "
                                         "possibly address already in use.");
            g_deinit();
            g_exit(1);
        }

        if (0 != g_fork())
        {
            g_deinit();
            g_exit(0);
        }

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
        fd = g_file_open(pid_file);

        if (-1 == fd)
        {
            log_message(LOG_LEVEL_ERROR,
                        "error opening pid file[%s]: %s",
                        pid_file, g_get_strerror());
            log_end();
            g_deinit();
            g_exit(1);
        }

        g_sprintf(pid_s, "%d", g_pid);
        g_file_write(fd, pid_s, g_strlen(pid_s));
        g_file_close(fd);
    }

    /* start program main loop */
    log_message(LOG_LEVEL_INFO,
                "starting xrdp-sesman with pid %d", g_pid);

    /* make sure the socket directory exists */
    g_mk_socket_path("xrdp-sesman");

    /* make sure the /tmp/.X11-unix directory exists */
    if (!g_directory_exist("/tmp/.X11-unix"))
    {
        if (!g_create_dir("/tmp/.X11-unix"))
        {
            log_message(LOG_LEVEL_ERROR,
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

    g_deinit();
    g_exit(error);
    return 0;
}
