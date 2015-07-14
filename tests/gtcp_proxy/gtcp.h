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

#ifndef _GTCP_H
#define _GTCP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

int  tcp_socket_create(void);
void tcp_set_non_blocking(int skt);
int  tcp_bind(int skt, char *port);
int  tcp_listen(int skt);
int  tcp_accept(int skt);
int  tcp_last_error_would_block();
void tcp_close(int skt);
int  tcp_socket(void);
int  tcp_connect(int skt, const char *hostname, const char *port);
int  tcp_can_send(int skt, int millis);
int  tcp_socket_ok(int skt);
int  tcp_select(int sck1, int sck2);
int  tcp_recv(int skt, void *ptr, int len, int flags);
int  tcp_send(int skt, const void *ptr, int len, int flags);

#endif
