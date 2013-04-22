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

#include "gtcp.h"

/**
 * Return a newly created socket or -1 on error
 *****************************************************************************/

int tcp_socket_create(void)
{
    int rv;
    int option_value;

#if defined(_WIN32)
    int option_len;
#else
    unsigned int option_len;
#endif

    /* in win32 a socket is an unsigned int, in linux, its an int */
    if ((rv = (int) socket(PF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

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
 * Place specifed socket in non blocking mode
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
    int ret ;
    char ipAddr[256] ;
    struct sockaddr_in s;

#if defined(_WIN32)
    int i;
#else
    unsigned int i;
#endif

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
        return;

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

#if defined(_WIN32)
    int option_len;
#else
    unsigned int option_len;
#endif

    /* in win32 a socket is an unsigned int, in linux, its an int */
    if ((rv = (int) socket(PF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

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
    fd_set         wfds;
    struct timeval time;
    int            rv;

    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;
    FD_ZERO(&wfds);

    if (skt > 0)
    {
        FD_SET(((unsigned int) skt), &wfds);
        rv = select(skt + 1, 0, &wfds, 0, &time);

        if (rv > 0)
        {
            return tcp_socket_ok(skt);
        }
    }

    return 0;
}

/**
 * Return 1 if socket is OK, 0 otherwise
 *****************************************************************************/

int tcp_socket_ok(int skt)
{
    int opt;

#if defined(_WIN32)
    int opt_len;
#else
    unsigned int opt_len;
#endif

    opt_len = sizeof(opt);

    if (getsockopt(skt, SOL_SOCKET, SO_ERROR, (char *) (&opt), &opt_len) == 0)
    {
        if (opt == 0)
            return 1;
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
    fd_set         rfds;
    struct timeval time;

    int            max = 0;
    int            rv  = 0;

    memset(&rfds, 0, sizeof(fd_set));
    memset(&time, 0, sizeof(struct timeval));

    time.tv_sec = 0;
    time.tv_usec = 0;
    FD_ZERO(&rfds);

    if (sck1 > 0)
        FD_SET(((unsigned int) sck1), &rfds);

    if (sck2 > 0)
        FD_SET(((unsigned int) sck2), &rfds);

    max = sck1;

    if (sck2 > max)
        max = sck2;

    rv = select(max + 1, &rfds, 0, 0, &time);

    if (rv > 0)
    {
        rv = 0;

        if (FD_ISSET(((unsigned int) sck1), &rfds))
            rv = rv | 1;

        if (FD_ISSET(((unsigned int)sck2), &rfds))
            rv = rv | 2;
    }
    else
    {
        rv = 0;
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
