/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022, all xrdp contributors
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
 * @file libipm/scp_application_types.h
 * @brief scp type declarations intended for use in the application
 * @author Simone Fedele/ Matt Burt
 */

#ifndef SCP_APPLICATION_TYPES_H
#define SCP_APPLICATION_TYPES_H

#include <time.h>

/**
 * Select the desktop application session type
 */
enum scp_session_type
{
    SCP_SESSION_TYPE_XVNC = 0,  ///< Session used Xvnc
    SCP_SESSION_TYPE_XRDP, ///< Session uses X11rdp
    SCP_SESSION_TYPE_XORG  ///< Session used Xorg + xorgxrdp
};

#define SCP_SESSION_TYPE_TO_STR(t) \
    ((t) == SCP_SESSION_TYPE_XVNC ? "Xvnc" : \
     (t) == SCP_SESSION_TYPE_XRDP ? "Xrdp" : \
     (t) == SCP_SESSION_TYPE_XORG ? "Xorg" : \
     "unknown" \
    )

/**
 * @brief Information to display about a particular sesman session
 */
struct scp_session_info
{
    int sid; ///< Session ID
    unsigned int display; ///< Display number
    enum scp_session_type type; ///< Session type
    unsigned short width; ///< Initial session width
    unsigned short height; ///< Initial session height
    unsigned char bpp;  ///< Session bits-per-pixel
    time_t start_time;  ///< When sesion was created
    char *username;     ///< Username for session
    char *start_ip_addr; ///< IP address of starting client
};


#endif /* SCP_APPLICATION_TYPES_H */
