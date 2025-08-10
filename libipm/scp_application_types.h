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

#include <sys/types.h>

/**
 * Select the desktop application session type
 */
enum scp_session_type
{
    SCP_SESSION_TYPE_XVNC = 0,  ///< Session used Xvnc
    SCP_SESSION_TYPE_XVNC_UDS,  ///< Session used Xvnc with UDS connection
    SCP_SESSION_TYPE_XORG,  ///< Session used Xorg + xorgxrdp
    SCP_SESSION_TYPE_LABWC, ///< Labwc compositor session
    SCP_SESSION_TYPE_LABWC_OVER_VNC  ///< Labwc over VNC compositor session
};

#define SCP_SESSION_TYPE_TO_STR(t) \
    ((t) == SCP_SESSION_TYPE_XVNC ? "Xvnc" : \
     (t) == SCP_SESSION_TYPE_XVNC_UDS ? "Xvnc-UDS" : \
     (t) == SCP_SESSION_TYPE_XORG ? "Xorg" : \
     (t) == SCP_SESSION_TYPE_LABWC ? "labwc" : \
     (t) == SCP_SESSION_TYPE_LABWC_OVER_VNC ? "labwc(vnc)" : \
     "unknown" \
    )

#define SCP_SESSION_TYPE_IS_X11(t) \
    ((t) >= SCP_SESSION_TYPE_XVNC && (t) <= SCP_SESSION_TYPE_XORG)

// Other constants
enum
{
    /*
     * Storage space to allocate for a display name
     */
    SCP_DISPLAY_NAME_SIZE = 32
};

/**
 * @brief Information to display about a particular sesman session
 */
struct scp_session_info
{
    int sid; ///< Session ID
    char *display; ///< Display name (":n" or "wayland-n")
    enum scp_session_type type; ///< Session type
    unsigned short width; ///< Initial session width
    unsigned short height; ///< Initial session height
    unsigned char bpp;  ///< Session bits-per-pixel
    time_t start_time;  ///< When session was created
    uid_t uid;     ///< Username for session
    char *start_ip_addr; ///< IP address of starting client
    char *client_ip;  ///< Current client IP
    char *client_name; ///< Current client name
    time_t last_connect_disconnect; ///< Time of last client connect/disconnect
};

/**
 * Status of a login request
 */
enum scp_login_status
{
    E_SCP_LOGIN_OK = 0, ///< The connection is now loggned in
    E_SCP_LOGIN_ALREADY_LOGGED_IN, //< A user is currently logged in
    E_SCP_LOGIN_NO_MEMORY, ///< Memory allocation failure
    /**
     * User couldn't be authenticated, or user doesn't exist */
    E_SCP_LOGIN_NOT_AUTHENTICATED,
    E_SCP_LOGIN_NOT_AUTHORIZED, ///< User is authenticated, but not authorized
    E_SCP_LOGIN_GENERAL_ERROR ///< An unspecific error has occurred
};

/**
 * Convert an scp_login_status code to a readable string for output
 * @param n Message code
 * @param buff to contain string
 * @param buff_size length of buff
 * @return buff is returned for convenience.
 */
const char *
scp_login_status_to_str(enum scp_login_status n,
                        char *buff, unsigned int buff_size);

/**
 * Status of a session creation request
 */
enum scp_screate_status
{
    E_SCP_SCREATE_OK = 0, ///< Session created
    E_SCP_SCREATE_NO_MEMORY, ///< Memory allocation failure
    E_SCP_SCREATE_NOT_LOGGED_IN, ///< Connection is not logged in
    E_SCP_SCREATE_MAX_REACHED, ///< Max number of sessions already reached
    E_SCP_SCREATE_NO_DISPLAY, ///< No X server display number is available
    E_SCP_SCREATE_X_SERVER_FAIL, ///< X server could not be started
    E_SCP_SCREATE_SESSION_FAIL, ///< The session failed quickly
    E_SCP_SCREATE_IN_PROGRESS, ///< A create session request is in progress
    E_SCP_SCREATE_GENERAL_ERROR ///< An unspecific error has occurred
};

/**
 * Convert an scp_session creation code to a readable string for output
 * @param n Message code
 * @param buff to contain string
 * @param buff_size length of buff
 * @return buff is returned for convenience.
 */
const char *
scp_screate_status_to_str(enum scp_screate_status n,
                          char *buff, unsigned int buff_size);

/*
 * Flags passed to scp_send_connect_session_request()
 */

/**
 * Set this to get an FD for chansrv
 */
#define E_SCP_SCONNECT_FLAG_NEED_CHANSRV (1<<0)

/**
 * Status of a session connection request
 */
enum scp_sconnect_status
{
    E_SCP_SCONNECT_OK = 0, ///< Session created
    E_SCP_SCONNECT_NOT_LOGGED_IN, ///< Connection is not logged in
    E_SCP_SCONNECT_NO_SUCH_GUID, ///< GUID does not exist for this user
    E_SCP_SCONNECT_NO_MEMORY, ///< Memory allocation failure
    E_SCP_SCONNECT_SERVER_FAIL, ///< Can't connect to X server
    E_SCP_SCONNECT_GENERAL_ERROR ///< An unspecific error has occurred
};

/**
 * Convert an scp_session connection code to a readable string for output
 * @param n Message code
 * @param buff to contain string
 * @param buff_size length of buff
 * @return buff is returned for convenience.
 */
const char *
scp_sconnect_status_to_str(enum scp_sconnect_status n,
                           char *buff, unsigned int buff_size);

/**
 * Status of an list sessions message
 */
enum scp_list_sessions_status
{
    /**
     * This message contains a valid session, and other messages
     * will be sent
     */
    E_SCP_LS_SESSION_INFO = 0,

    /**
     * This message indicates the end of a list of sessions. No session
     * is contained in the message */
    E_SCP_LS_END_OF_LIST,

    /**
     * Client hasn't logged in yet
     */
    E_SCP_LS_NOT_LOGGED_IN = 100,
    /**
     * A client-side error occurred allocating memory for the session
     */
    E_SCP_LS_NO_MEMORY
};

/**
 * Status of a create sockdir message
 */
enum scp_create_sockdir_status
{
    E_SCP_CS_OK = 0,

    /**
     * Client hasn't logged in yet
     */
    E_SCP_CS_NOT_LOGGED_IN = 100,

    /**
     * sesman failed to create the directory
     */
    E_SCP_CS_OTHER_ERROR
};

#endif /* SCP_APPLICATION_TYPES_H */
