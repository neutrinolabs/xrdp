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

int  g_loc_io_count = 0;  // bytes read from local port
int  g_rem_io_count = 0;  // bytes read from remote port

static int g_terminated = 0;
static char g_buf[1024 * 32];

#define
#define

typedef unsigned short tui16;

/*****************************************************************************/
static void
g_memset(void *ptr, int val, int size)
{
    memset(ptr, val, size);
}

/*****************************************************************************/
static void
g_printf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

/*****************************************************************************/
static void
g_writeln(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    g_printf("\n");
}

/*****************************************************************************/
static void
g_hexdump(char *p, int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    line = (unsigned char *)p;
    offset = 0;

    while (offset < len)
    {
        g_printf("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            g_printf("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            g_printf("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            g_printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        g_writeln("%s", "");
        offset += thisline;
        line += thisline;
    }
}

/*****************************************************************************/
static int
g_tcp_socket(void)
{
    int rv;
    int option_value;
    socklen_t option_len;

    rv = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (rv < 0)
    {
        return -1;
    }
    option_len = sizeof(option_value);
    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value,
                       option_len);
        }
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value < (1024 * 32))
        {
            option_value = 1024 * 32;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                       option_len);
        }
    }

    return rv;
}

/*****************************************************************************/
static int
g_tcp_set_non_blocking(int sck)
{
    unsigned long i;

    i = fcntl(sck, F_GETFL);
    i = i | O_NONBLOCK;
    fcntl(sck, F_SETFL, i);
    return 0;
}

/*****************************************************************************/
static int
g_tcp_bind(int sck, const char* port)
{
  struct sockaddr_in s;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_port = htons((tui16)atoi(port));
  s.sin_addr.s_addr = INADDR_ANY;
  return bind(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

/*****************************************************************************/
static int
g_tcp_listen(int sck)
{
    return listen(sck, 2);
}

/*****************************************************************************/
static int
g_tcp_select(int sck1, int sck2)
{
    fd_set rfds;
    struct timeval time;
    int max = 0;
    int rv = 0;

    g_memset(&rfds, 0, sizeof(fd_set));
    g_memset(&time, 0, sizeof(struct timeval));

    time.tv_sec = 0;
    time.tv_usec = 0;
    FD_ZERO(&rfds);

    if (sck1 > 0)
    {
        FD_SET(((unsigned int)sck1), &rfds);
    }

    if (sck2 > 0)
    {
        FD_SET(((unsigned int)sck2), &rfds);
    }

    max = sck1;

    if (sck2 > max)
    {
        max = sck2;
    }

    rv = select(max + 1, &rfds, 0, 0, &time);

    if (rv > 0)
    {
        rv = 0;

        if (FD_ISSET(((unsigned int)sck1), &rfds))
        {
            rv = rv | 1;
        }

        if (FD_ISSET(((unsigned int)sck2), &rfds))
        {
            rv = rv | 2;
        }
    }
    else
    {
        rv = 0;
    }

    return rv;
}

/*****************************************************************************/
static int
g_tcp_recv(int sck, void *ptr, int len, int flags)
{
    return recv(sck, ptr, len, flags);
}

/*****************************************************************************/
static void
g_tcp_close(int sck)
{
    if (sck == 0)
    {
        return;
    }
    close(sck);
}

/*****************************************************************************/
static int
g_tcp_send(int sck, const void *ptr, int len, int flags)
{
    return send(sck, ptr, len, flags);
}

/*****************************************************************************/
void
g_sleep(int msecs)
{
    usleep(msecs * 1000);
}

/*****************************************************************************/
static int
g_tcp_last_error_would_block(int sck)
{
    return (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINPROGRESS);
}

/*****************************************************************************/
static int
g_tcp_accept(int sck)
{
    int ret ;
    struct sockaddr_in s;
    unsigned int i;

    i = sizeof(struct sockaddr_in);
    memset(&s, 0, i);
    ret = accept(sck, (struct sockaddr *)&s, &i);
    return ret ;
}

/*****************************************************************************/
static int
g_tcp_connect(int sck, const char* address, const char* port)
{
    struct sockaddr_in s;
    struct hostent* h;

    g_memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((tui16)atoi(port));
    s.sin_addr.s_addr = inet_addr(address);
    if (s.sin_addr.s_addr == INADDR_NONE)
    {
        h = gethostbyname(address);
        if (h != 0)
        {
            if (h->h_name != 0)
            {
                if (h->h_addr_list != 0)
                {
                    if ((*(h->h_addr_list)) != 0)
                    {
                        s.sin_addr.s_addr = *((int*)(*(h->h_addr_list)));
                    }
                }
            }
        }
    }
    return connect(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

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
static void
g_init(const char *app_name)
{
    setlocale(LC_CTYPE, "");
}

/*****************************************************************************/
static void
g_deinit(void)
{
}

/*****************************************************************************/
static int
g_tcp_can_send(int sck, int millis)
{
    fd_set wfds;
    struct timeval time;
    int rv;

    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;
    FD_ZERO(&wfds);

    if (sck > 0)
    {
        FD_SET(((unsigned int)sck), &wfds);
        rv = select(sck + 1, 0, &wfds, 0, &time);

        if (rv > 0)
        {
            return g_tcp_socket_ok(sck);
        }
    }

    return 0;
}

/*****************************************************************************/
static void
g_signal_user_interrupt(void (*func)(int))
{
    signal(SIGINT, func);
}

/*****************************************************************************/
static void
g_signal_terminate(void (*func)(int))
{
    signal(SIGTERM, func);
}

/*****************************************************************************/
static void
g_signal_usr1(void (*func)(int))
{
    signal(SIGUSR1, func);
}

/*****************************************************************************/
static int
g_strcasecmp(const char *c1, const char *c2)
{
    return strcasecmp(c1, c2);
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
        g_writeln("bind failed");
    }

    /* listen for an incoming connection */
    if (error == 0)
    {
        error = g_tcp_listen(lis_sck);

        if (error == 0)
        {
            g_writeln("listening for connection");
        }
    }

    /* accept an incoming connection */
    if (error == 0)
    {
        while ((!g_terminated) && (error == 0))
        {
            acc_sck = g_tcp_accept(lis_sck);

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
            g_writeln("got connection");
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

            while ((!g_tcp_can_send(con_sck, 100)) && (!g_terminated) && (i < 100))
            {
                g_sleep(100);
                i++;
            }

            if (i > 99)
            {
                g_writeln("timeout connecting");
                error = 1;
            }

            if (g_terminated)
            {
                error = 1;
            }
        }

        if ((error != 0) && (!g_terminated))
        {
            g_writeln("error connecting to remote\r\n");
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
                    g_writeln("from remove, the socket from connect");
                    g_hexdump(g_buf, count);
                }

#if 0
                g_writeln("local_io_count: %d\tremote_io_count: %d",
                          g_loc_io_count, g_rem_io_count);
#endif
                sent = 0;

                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(acc_sck, g_buf + sent, count - sent, 0);

                    if ((i == -1) && g_tcp_last_error_would_block(acc_sck))
                    {
                        g_tcp_can_send(acc_sck, 1000);
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
                    g_writeln("from accepted, the socket from accept");
                    g_hexdump(g_buf, count);
                }

#if 0
                g_writeln("local_io_count: %d\tremote_io_count: %d",
                          g_loc_io_count, g_rem_io_count);
#endif
                sent = 0;

                while ((sent < count) && (error == 0) && (!g_terminated))
                {
                    i = g_tcp_send(con_sck, g_buf + sent, count - sent, 0);

                    if ((i == -1) && g_tcp_last_error_would_block(con_sck))
                    {
                        g_tcp_can_send(con_sck, 1000);
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
    g_writeln("acc_to_con %d", acc_to_con);
    g_writeln("con_to_acc %d", con_to_acc);
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
    g_writeln("shutting down");
    g_terminated = 1;
}

void
clear_counters(int sig)
{
    g_writeln("cleared counters at: local_io_count: %d remote_io_count: %d",
              g_loc_io_count, g_rem_io_count);
    g_loc_io_count = 0;
    g_rem_io_count = 0;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    int dump;

    if (argc < 4)
    {
        usage();
        return 0;
    }

    g_init("tcp_proxy");
    g_signal_user_interrupt(proxy_shutdown); /* SIGINT  */
    g_signal_usr1(clear_counters);           /* SIGUSR1 */
    g_signal_terminate(proxy_shutdown);      /* SIGTERM */

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

    g_deinit();
    return 0;
}
