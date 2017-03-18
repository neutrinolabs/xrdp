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
 *
 * names of UNIX sockets for inter-process communication
 */

#if !defined(XRDP_SOCKETS_H)
#define XRDP_SOCKETS_H

#define XRDP_CHANSRV_STR      XRDP_SOCKET_PATH "/xrdp_chansrv_socket_%d"
#define CHANSRV_PORT_OUT_STR  XRDP_SOCKET_PATH "/xrdp_chansrv_audio_out_socket_%d"
#define CHANSRV_PORT_IN_STR   XRDP_SOCKET_PATH "/xrdp_chansrv_audio_in_socket_%d"
#define CHANSRV_API_STR       XRDP_SOCKET_PATH "/xrdpapi_%d"
#define XRDP_X11RDP_STR       XRDP_SOCKET_PATH "/xrdp_display_%d"
#define XRDP_DISCONNECT_STR   XRDP_SOCKET_PATH "/xrdp_disconnect_display_%d"

#endif
