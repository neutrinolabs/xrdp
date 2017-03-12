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
 * @file tcp.h
 * @brief Tcp stream functions declarations
 * @author Jay Sorg, Simone Fedele
 *
 */

#ifndef TCP_H
#define TCP_H

/**
 *
 * @brief Force receiving data from tcp stream
 * @param sck The socket to read from
 * @param data Data buffer
 * @param len Data buffer size
 * @return 0 on success, 1 on error
 *
 */
int
tcp_force_recv(int sck, char* data, int len);

/**
 *
 * @brief Force sending data to tcp stream
 * @param sck the socket to write to
 * @param data Data buffer
 * @param len Data buffer size
 * @return 0 on success, 1 on error
 *
 */
int
tcp_force_send(int sck, char* data, int len);

/**
 *
 * @brief Binds the listening socket
 * @param sck Listening socket
 * @param addr Listening address
 * @param port Listening port
 * @return 0 on success, -1 on error
 *
 */
int
tcp_bind(int sck, char* addr, char* port);

#endif
