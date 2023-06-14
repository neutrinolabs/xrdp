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

/* XRDP_SOCKET_ROOT_PATH must be defined to include this file */
#ifdef __cppcheck__
/*   avoid syntax errors */
#  define XRDP_SOCKET_ROOT_PATH "/dummy"
#elif !defined(XRDP_SOCKET_ROOT_PATH)
#  error "XRDP_SOCKET_ROOT_PATH must be defined"
#endif

/* Buffer size for code for fullpath declarations
 *
 * This needs to fit in the sun_path field of a sockaddr_un. POSIX
 * does not define this size, so the value below is the lower of
 * the FreeBSD/OpenBSD/NetBSD(104) and Linux(108) values */
#define XRDP_SOCKETS_MAXPATH 104

/* The socketdir is rooted at XRDP_SOCKET_ROOT_PATH. User-specific
 * sockets live in a user-specific sub-directory of this called
 * XRDP_SOCKET_PATH. The sub-directory is the UID of the user */
#define XRDP_SOCKET_PATH      XRDP_SOCKET_ROOT_PATH "/%d"

/* Sockets in XRDP_SOCKET_ROOT_PATH */
#define SCP_LISTEN_PORT_BASE_STR   "sesman.socket"

/* names of socket files within XRDP_SOCKET_PATH, qualified by
 * display number */
#define XRDP_CHANSRV_BASE_STR      "xrdp_chansrv_socket_%d"
#define CHANSRV_PORT_OUT_BASE_STR  "xrdp_chansrv_audio_out_socket_%d"
#define CHANSRV_PORT_IN_BASE_STR   "xrdp_chansrv_audio_in_socket_%d"
#define CHANSRV_API_BASE_STR       "xrdpapi_%d"
#define XRDP_X11RDP_BASE_STR       "xrdp_display_%d"
#define XRDP_DISCONNECT_BASE_STR   "xrdp_disconnect_display_%d"

/* fullpath declarations */
#define XRDP_CHANSRV_STR      XRDP_SOCKET_PATH "/" XRDP_CHANSRV_BASE_STR
#define CHANSRV_PORT_OUT_STR  XRDP_SOCKET_PATH "/" CHANSRV_PORT_OUT_BASE_STR
#define CHANSRV_PORT_IN_STR   XRDP_SOCKET_PATH "/" CHANSRV_PORT_IN_BASE_STR
#define CHANSRV_API_STR       XRDP_SOCKET_PATH "/" CHANSRV_API_BASE_STR
#define XRDP_X11RDP_STR       XRDP_SOCKET_PATH "/" XRDP_X11RDP_BASE_STR
#define XRDP_DISCONNECT_STR   XRDP_SOCKET_PATH "/" XRDP_DISCONNECT_BASE_STR

#endif
