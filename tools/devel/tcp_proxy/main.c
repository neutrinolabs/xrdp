/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "os_calls.h"
#include "string_calls.h"

int  g_loc_io_count = 0;  // bytes read from local port
int  g_rem_io_count = 0;  // bytes read from remote port

static int g_terminated = 0;
static char g_buf[1024 * 32];


typedef unsigned short tui16;


/*****************************************************************************/
static int
g_tcp_socket_ok(int sck)
{
    int opt;
    socklen_t opt_len = sizeof(opt);

    if (getsockopt(sck, SOL_SOCKET, SO_ERROR, (char *)(&opt), &opt_len) == 0)
    {
        if (opt == 0)
        {
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************/
static int
main_loop(char *local_port, char *remote_ip, char *remote_port, int hexdump)
{
    int lis_sck;
    int acc_sck;
    int con_sck;
    int sel;
    int count;
    int sent;
    int error;
    int i;
    int acc_to_con;
    int con_to_acc;

    acc_to_con = 0;
    con_to_acc = 0;
    acc_sck = 0;

    /* create the listening socket and setup options */
    lis_sck = g_tcp_socket();
    g_tcp_set_non_blocking(lis_sck);
    error = g_tcp_bind(lis_sck, local_port);

    if (error != 0)
    {
        LOG(LOG_LEVEL_WARNING, "bind failed");
    }

    /* listen for an incoming connection */
    if (error == 0)
    {
        error = g_tcp_listen(lis_sck);

        if (error == 0)
        {
            LOG(LOG_LEVEL_INFO, "listening for connection");
        }
    }

    /* accept an incoming connection */
    if (error == 0)
    {
        while ((!g_terminated) && (error == 0))
        {
            acc_sck = g_sck_accept(lis_sck);

            if ((acc_sck == -1) && g_tcp_last_error_would_block(lis_sck))
            {
                g_sleep(100);
            }
            else if (acc_sck == -1)
            {
                error = 1;
            }
            else
            {
                break;
            }
        }

        if (error == 0)
        {
            error = g_terminated;
        }

        /* stop listening */
        g_tcp_close(lis_sck);
        lis_sck = 0;

        if (error == 0)
        {
            LOG(LOG_LEVEL_INFO, "got connection");
        }
    }

    /* connect outgoing socket */
    con_sck = 0;

    if (error == 0)
    {
        con_sck = g_tcp_socket();
        g_tcp_set_non_blocking(con_sck);
        error = g_tcp_connect(con_sck, remote_ip, remote_port);

        if ((error == -1) && g_tcp_last_error_would_block(con_sck))
        {
            error = 0;
            i = 0;

            while (!(g_tcp_can_send(con_sck, 100) && g_tcp_socket_ok(con_sck))
                    && (!g_terminated)
                    && (i < 100))
            {
                g_sleep(100);
                i++;
            }

            if (i > 99)
            {
                LOG(LOG_LEVEL_ERROR, "timeout connecting");
                error = 1;
            }

            if (g_terminated)
            {
                error = 1;
            }
        }

        if ((error != 0) && (!g_terminated))
        {
            LOG(LOG_LEVEL_ERROR, "error connecting to remote\r\n");
        }
    }

    while ((!g_terminated) && (error == 0))
    {
        sel = g_tcp_select(con_sck, acc_sck);

        if (sel == 0)
        {
            g_sleep(10);
            continue;
        }

        if (sel & 1)
        {
            // can read from con_sck w/o blocking
            count = g_tcp_recv(con_sck, g_buf, 1024 * 16, 0);
            error = count < 1;

            if (error == 0)
            {
                g_loc_io_count += count;
                con_to_acc += count;

                if (hexdump)
                {
                    LOG_HEXDUMP(LOG_LEVEL_INFO, "from remove, the socket from connect", g_buf, count);
                }

                LOG(LOG_LEVEL_DEBUG, "local_io_count: %d\tremote_io_count: %d",
                    g_loc_io_count, g_rem_io_count);
                sent = 0;

                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(acc_sck, g_buf + sent, count - sent, 0);

                    if ((i == -1) && g_tcp_last_error_would_block(acc_sck))
                    {
                        if (g_tcp_can_send(acc_sck, 1000))
                        {
                            g_tcp_socket_ok(acc_sck);
                        }
                    }
                    else if (i < 1)
                    {
                        error = 1;
                    }
                    else
                    {
                        sent += i;
                    }
                }
            }
        }

        if (sel & 2)
        {
            // can read from acc_sck w/o blocking
            count = g_tcp_recv(acc_sck, g_buf, 1024 * 16, 0);
            error = count < 1;

            if (error == 0)
            {
                g_rem_io_count += count;
                acc_to_con += count;

                if (hexdump)
                {
                    LOG_HEXDUMP(LOG_LEVEL_INFO, "from accepted, the socket from accept", g_buf, count);
                }

                LOG(LOG_LEVEL_DEBUG, "local_io_count: %d\tremote_io_count: %d",
                    g_loc_io_count, g_rem_io_count);
                sent = 0;

                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(con_sck, g_buf + sent, count - sent, 0);

                    if ((i == -1) && g_tcp_last_error_would_block(con_sck))
                    {
                        if (g_tcp_can_send(con_sck, 1000))
                        {
                            g_tcp_socket_ok(con_sck);
                        }
                    }
                    else if (i < 1)
                    {
                        error = 1;
                    }
                    else
                    {
                        sent += i;
                    }
                }
            }
        }
    }

    g_tcp_close(lis_sck);
    g_tcp_close(con_sck);
    g_tcp_close(acc_sck);
    LOG(LOG_LEVEL_INFO, "acc_to_con %d", acc_to_con);
    LOG(LOG_LEVEL_INFO, "con_to_acc %d", con_to_acc);
    return 0;
}


/*****************************************************************************/
static int
usage(void)
{
    g_writeln("tcp_proxy <local-port> <remote-ip> <remote-port> [dump]");
    return 0;
}


/*****************************************************************************/
void
proxy_shutdown(int sig)
{
    LOG(LOG_LEVEL_INFO, "shutting down");
    g_terminated = 1;
}

void
clear_counters(int sig)
{
    LOG(LOG_LEVEL_DEBUG, "cleared counters at: local_io_count: %d remote_io_count: %d",
        g_loc_io_count, g_rem_io_count);
    g_loc_io_count = 0;
    g_rem_io_count = 0;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    int dump;
    struct log_config *config;

    if (argc < 4)
    {
        usage();
        return 0;
    }

    g_init("tcp_proxy");
    g_signal_user_interrupt(proxy_shutdown); /* SIGINT  */
    g_signal_usr1(clear_counters);           /* SIGUSR1 */
    g_signal_terminate(proxy_shutdown);      /* SIGTERM */

    config = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(config);
    log_config_free(config);

    if (argc < 5)
    {
        while (!g_terminated)
        {
            g_loc_io_count = 0;
            g_rem_io_count = 0;
            main_loop(argv[1], argv[2], argv[3], 0);
        }
    }
    else
    {
        dump = g_strcasecmp(argv[4], "dump") == 0;

        while (!g_terminated)
        {
            main_loop(argv[1], argv[2], argv[3], dump);
        }
    }

    log_end();
    g_deinit();
    return 0;
}
