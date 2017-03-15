/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file tcp.c
 * @brief Tcp stream functions
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "sesman.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/
int
tcp_force_recv(int sck, char *data, int len)
{
    int rcvd;

    //#ifndef LIBSCP_CLIENT
    //  int block;
    //  block = lock_fork_critical_section_start();
    //#endif

    while (len > 0)
    {
        rcvd = g_tcp_recv(sck, data, len, 0);

        if (rcvd == -1)
        {
            if (g_tcp_last_error_would_block(sck))
            {
                g_sleep(1);
            }
            else
            {
                //#ifndef LIBSCP_CLIENT
                //        lock_fork_critical_section_end(block);
                //#endif
                return 1;
            }
        }
        else if (rcvd == 0)
        {
            //#ifndef LIBSCP_CLIENT
            //      lock_fork_critical_section_end(block);
            //#endif
            return 1;
        }
        else
        {
            data += rcvd;
            len -= rcvd;
        }
    }

    //#ifndef LIBSCP_CLIENT
    //  lock_fork_critical_section_end(block);
    //#endif

    return 0;
}

/*****************************************************************************/
int
tcp_force_send(int sck, char *data, int len)
{
    int sent;

    //#ifndef LIBSCP_CLIENT
    //  int block;
    //  block = lock_fork_critical_section_start();
    //#endif

    while (len > 0)
    {
        sent = g_tcp_send(sck, data, len, 0);

        if (sent == -1)
        {
            if (g_tcp_last_error_would_block(sck))
            {
                g_sleep(1);
            }
            else
            {
                //#ifndef LIBSCP_CLIENT
                //        lock_fork_critical_section_end(block);
                //#endif
                return 1;
            }
        }
        else if (sent == 0)
        {
            //#ifndef LIBSCP_CLIENT
            //      lock_fork_critical_section_end(block);
            //#endif
            return 1;
        }
        else
        {
            data += sent;
            len -= sent;
        }
    }

    //#ifndef LIBSCP_CLIENT
    //  lock_fork_critical_section_end(block);
    //#endif

    return 0;
}

/*****************************************************************************/
int
tcp_bind(int sck, char *addr, char *port)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons(atoi(port));
    s.sin_addr.s_addr = inet_addr(addr);
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
}
