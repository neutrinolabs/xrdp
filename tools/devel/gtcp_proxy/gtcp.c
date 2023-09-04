/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2013
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

#include "gtcp.h"

/**
 * Return a newly created socket or -1 on error
 *****************************************************************************/

int tcp_socket_create(void)
{
    int rv;
    int option_value;
    socklen_t option_len;

    /* in win32 a socket is an unsigned int, in linux, it's an int */
    if ((rv = (int) socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *) &option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *) &option_value,
                       option_len);
        }
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *) &option_value,
                   &option_len) == 0)
    {
        if (option_value < (1024 * 32))
        {
            option_value = 1024 * 32;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *) &option_value,
                       option_len);
        }
    }

    return rv;
}

/**
 * Place specified socket in non blocking mode
 *****************************************************************************/

void tcp_set_non_blocking(int skt)
{
    unsigned long i;

#if defined(_WIN32)
    i = 1;
    ioctlsocket(skt, FIONBIO, &i);
#else
    i = fcntl(skt, F_GETFL);
    i = i | O_NONBLOCK;
    fcntl(skt, F_SETFL, i);
#endif
}

/**
 * Assign name to socket
 *
 * @param  skt  the socket to bind
 * @param  port  the port to bind to
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

int tcp_bind(int skt, char *port)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((uint16_t) atoi(port));
    s.sin_addr.s_addr = INADDR_ANY;

    return bind(skt, (struct sockaddr *) &s, sizeof(struct sockaddr_in));
}

/**
 * Listen for incoming connections
 *
 * @param  skt  the socket to listen on
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

int tcp_listen(int skt)
{
    return listen(skt, 2);
}

/**
 * Accept incoming connection
 *
 * @param  skt  socket to accept incoming connection on
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

int tcp_accept(int skt)
{
    struct sockaddr_in s;
    socklen_t i;

    i = sizeof(struct sockaddr_in);
    memset(&s, 0, i);
    return accept(skt, (struct sockaddr *)&s, &i);
}

/**
 * Check if the socket would block
 *
 * @return TRUE if would block, else FALSE
 *****************************************************************************/

int tcp_last_error_would_block()
{
#if defined(_WIN32)
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINPROGRESS);
#endif
}

/**
 * Close specified socket
 *****************************************************************************/

void tcp_close(int skt)
{
    if (skt <= 0)
    {
        return;
    }

#if defined(_WIN32)
    closesocket(skt);
#else
    close(skt);
#endif
}

/**
 * Create a new socket
 *
 * @return new socket or -1 on error
 *****************************************************************************/

int tcp_socket(void)
{
    int rv;
    int option_value;
    socklen_t option_len;

    /* in win32 a socket is an unsigned int, in linux, it's an int */
    if ((rv = (int) socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *) &option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *) &option_value,
                       option_len);
        }
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *) &option_value,
                   &option_len) == 0)
    {
        if (option_value < (1024 * 32))
        {
            option_value = 1024 * 32;
            option_len = sizeof(option_value);
            setsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *) &option_value,
                       option_len);
        }
    }

    return rv;
}

/**
 * Connect to a server
 *
 * @param  skt      opaque socket obj
 * @param  address  connect to this server
 * @param  port     using this port
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/

int tcp_connect(int skt, const char *hostname, const char *port)
{
    struct sockaddr_in  s;
    struct hostent     *h;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((uint16_t) atoi(port));
    s.sin_addr.s_addr = inet_addr(hostname);

    if (s.sin_addr.s_addr == INADDR_NONE)
    {
        h = gethostbyname(hostname);

        if (h != 0)
        {
            if (h->h_name != 0)
            {
                if (h->h_addr_list != 0)
                {
                    if ((*(h->h_addr_list)) != 0)
                    {
                        s.sin_addr.s_addr = *((int *)(*(h->h_addr_list)));
                    }
                }
            }
        }
    }

    return connect(skt, (struct sockaddr *) &s, sizeof(struct sockaddr_in));
}

/**
 * Return 1 if we can write to the socket, 0 otherwise
 *****************************************************************************/

int tcp_can_send(int skt, int millis)
{
    int rv = 0;
    if (skt > 0)
    {
        struct pollfd pollfd;

        pollfd.fd = skt;
        pollfd.events = POLLOUT;
        pollfd.revents = 0;
        if (poll(&pollfd, 1, millis) > 0)
        {
            if ((pollfd.revents & POLLOUT) != 0)
            {
                rv = 1;
            }
        }
    }

    return rv;
}

/**
 * Return 1 if socket is OK, 0 otherwise
 *****************************************************************************/

int tcp_socket_ok(int skt)
{
    int opt;
    socklen_t opt_len = sizeof(opt);

    if (getsockopt(skt, SOL_SOCKET, SO_ERROR, (char *) (&opt), &opt_len) == 0)
    {
        if (opt == 0)
        {
            return 1;
        }
    }

    return 0;
}

/**
 * Check if specified sockets can be operated on without blocking
 *
 * @return 1 if they can be operated on or 0 if blocking would occur
 *****************************************************************************/

int tcp_select(int sck1, int sck2)
{
    struct pollfd pollfd[2] = {0};
    int rvmask[2] = {0}; /* Output masks corresponding to fds in pollfd */

    unsigned int i = 0;
    int rv = 0;

    if (sck1 > 0)
    {
        pollfd[i].fd = sck1;
        pollfd[i].events = POLLIN;
        rvmask[i] = 1;
        ++i;
    }

    if (sck2 > 0)
    {
        pollfd[i].fd = sck2;
        pollfd[i].events = POLLIN;
        rvmask[i] = 2;
        ++i;
    }

    if (poll(pollfd, i, 0) > 0)
    {
        if ((pollfd[0].revents & (POLLIN | POLLHUP)) != 0)
        {
            rv |= rvmask[0];
        }

        if ((pollfd[1].revents & (POLLIN | POLLHUP)) != 0)
        {
            rv |= rvmask[1];
        }
    }

    return rv;
}

int tcp_recv(int skt, void *ptr, int len, int flags)
{
#if defined(_WIN32)
    return recv(skt, (char *) ptr, len, flags);
#else
    return recv(skt, ptr, len, flags);
#endif
}

int tcp_send(int skt, const void *ptr, int len, int flags)
{
#if defined(_WIN32)
    return send(skt, (const char *)ptr, len, flags);
#else
    return send(skt, ptr, len, flags);
#endif
}
