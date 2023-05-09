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
 * @file libipm/eicp.h
 * @brief EICP declarations
 * @author Matt Burt
 *
 * Functions in this file use the following naming conventions:-
 *
 * E_EICP_{msg}_REQUEST is sent by eicp_send_{msg}_request()
 * E_EICP_{msg}_REQUEST is parsed by eicp_get_{msg}_request()
 * E_EICP_{msg}_RESPONSE is sent by eicp_send_{msg}_response()
 * E_EICP_{msg}_RESPONSE is parsed by eicp_get_{msg}_response()
 */

#ifndef EICP_H
#define EICP_H

#include "arch.h"
#include "scp_application_types.h"

struct trans;
struct guid;

/* Message codes */
enum eicp_msg_code
{
    E_EICP_SYS_LOGIN_REQUEST,
    E_EICP_SYS_LOGIN_RESPONSE,

    E_EICP_LOGOUT_REQUEST,
    // No E_EICP_LOGOUT_RESPONSE

    E_EICP_CREATE_SESSION_REQUEST
    // No E_EICP_CREATE_SESSION_RESPONSE
};

/* Common facilities */

/**
 * Convert a message code to a string for output
 * @param n Message code
 * @param buff to contain string
 * @param buff_size length of buff
 * @return buff is returned for convenience.
 */
const char *
eicp_msgno_to_str(enum eicp_msg_code n, char *buff, unsigned int buff_size);

/* Connection management facilities */

/**
 * Converts a standard trans connected to an EICP endpoint to an EICP transport
 *
 * @param trans connected endpoint
 * @return != 0 for error
 */
int
eicp_init_trans(struct trans *trans);

/**
 * Creates an EICP transport from a file descriptor
 *
 * @param fd file descriptor
 * @param trans_type TRANS_TYPE_SERVER or TRANS_TYPE_CLIENT
 * @param term_func Function to poll during connection for program
 *         termination, or NULL for none.
 * @return SCP transport, or NULL
 */
struct trans *
eicp_init_trans_from_fd(int fd, int trans_type, int (*term_func)(void));


/**
 * Checks an EICP transport to see if a complete message is
 * available for parsing
 *
 * @param trans EICP transport
 * @param[out] available != 0 if a complete message is available
 * @return != 0 for error
 */
int
eicp_msg_in_check_available(struct trans *trans, int *available);

/**
 * Waits on a single transport for an EICP message to be available for
 * parsing
 *
 * @param trans libipm transport
 * @return != 0 for error
 *
 * While the call is active, data-in callbacks for the transport are
 * disabled.
 *
 * Only use this call if you have nothing to do until a message
 * arrives on the transport. If you have other transports to service, use
 * eicp_msg_in_check_available()
 */
int
eicp_msg_in_wait_available(struct trans *trans);


/**
 * Gets the EICP message number of an incoming message
 *
 * @param trans EICP transport
 * @return message in the buffer
 *
 * The results of calling this routine before eicp_msg_in_check_available()
 * states a message is available are undefined.
 */
enum eicp_msg_code
eicp_msg_in_get_msgno(const struct trans *trans);

/**
 * Resets an EICP message buffer ready to receive the next message
 *
 * @param trans libipm transport
 */
void
eicp_msg_in_reset(struct trans *trans);

/* -------------------- Connect messages--------------------  */

/**
 * Send an E_EICP_SYS_LOGIN_REQUEST
 *
 * @param trans EICP transport
 * @param username Username
 * @param password Password
 * @param ip_addr IP address for the client (or "" if not known)
 * @param scp_fd SCP file descriptor from sesman client
 * @return != 0 for error
 *
 * sesexec replies (eventually) with E_EICP_SYS_LOGIN_RESPONSE
 *
 * Once this message has been sent, sesman can close its own SCP transport
 * down, as sesexec is responsible for client communication. When sesexec
 * responds, sesman can recreate the SCP transport if necessary.
 *
 * While E_EICP_SYS_LOGIN_REQUEST is being processed, sesman must assume
 * sesexec will be unresponsive to other EICP messages (although a
 * SIGTERM should be effective).
 */
int
eicp_send_sys_login_request(struct trans *trans,
                            const char *username,
                            const char *password,
                            const char *ip_addr,
                            int scp_fd);

/**
 * Parse an incoming E_EICP_SYS_LOGIN_REQUEST message (sesexec)
 *
 * @param trans EICP transport
 * @param[out] username Username
 * @param[out] password Password
 * @param[out] ip_addr IP address for the client (or "" if not known)
 * @param [out] scp_fd SCP file descriptor from sesman client
 * @return != 0 for error
 */
int
eicp_get_sys_login_request(struct trans *trans,
                           const char **username,
                           const char **password,
                           const char **ip_addr,
                           int *scp_fd);

/**
 * Send an E_EICP_SYS_LOGIN_RESPONSE (sesexec)
 *
 * @param trans EICP transport
 * @param is_logged_in true if the SCP client is logged in
 * @param uid UID of connected user
 * @param scp_fd File descriptor of sesman client
 * @return != 0 for error
 *
 * The uid and scp_fd are ignored unless is_logged_in is true.
 *
 * If is_logged_in is false, it is assumed that sesexec has properly
 * closed the connection to the SCP client.
 */
int
eicp_send_sys_login_response(struct trans *trans,
                             int is_logged_in,
                             uid_t uid,
                             int scp_fd);

/**
 * Parses an incoming E_EICP_SYS_LOGIN_RESPONSE (sesexec)
 *
 * @param trans EICP transport
 * @param[out] is_logged_in true if the SCP client is logged in
 * @param[out] uid UID of connected user
 * @param[out] scp_fd File descriptor of sesman client
 * @return != 0 for error
 *
 * The uid and client_fd are returned as (uid_t)-1 and -1 respectively
 * unless is_logged_in is true
 */
int
eicp_get_sys_login_response(struct trans *trans,
                            int *is_logged_in,
                            uid_t *uid,
                            int *scp_fd);

/**
 * Send an E_EICP_LOGOUT_REQUEST (sesexec)
 *
 * @param trans EICP transport
 * @return != 0 for error
 *
 * The sesexec process will exit normally
 */
int
eicp_send_logout_request(struct trans *trans);

/* -------------------- Session messages--------------------  */

/**
 * Send an E_EICP_CREATE_SESSION_REQUEST (sesman)
 *
 * @param trans EICP transport
 * @param scp_fd SCP file descriptor from sesman client
 * @param display X display number to use
 * @param type Session type
 * @param width Initial session width
 * @param height Initial session height
 * @param bpp Session bits-per-pixel (ignored for Xorg sessions)
 * @param shell User program to run. May be ""
 * @param directory Directory to run the program in. May be ""
 * @return != 0 for error
 *
 * The UID for the session comes from one of two places:-
 * - The UID for a sys login request is used if one has successfully
 *   been executed.
 * - If no sys login request is used, the UID is taken from the scp_fd
 *
 * Following a successful request, the session creation can be
 * considered to be underway. The result of this operation is
 * conveyed back to the caller as an ERCP event. The caller must use
 * ercp_trans_from_eicp_trans() on 'trans' to convert the transport to
 * an ERCP transport to receive this (and other) session run-time events.
 */
int
eicp_send_create_session_request(struct trans *trans,
                                 int scp_fd,
                                 unsigned int display,
                                 enum scp_session_type type,
                                 unsigned short width,
                                 unsigned short height,
                                 unsigned char bpp,
                                 const char *shell,
                                 const char *directory);


/**
 * Parse an incoming E_EICP_CREATE_SESSION_REQUEST (sesexec)
 *
 * @param trans EICP transport
 * @param[out] scp_fd SCP file descriptor from sesman client
 * @param[out] display X display number to use
 * @param[out] type Session type
 * @param[out] width Initial session width
 * @param[out] height Initial session height
 * @param[out] bpp Session bits-per-pixel (ignored for Xorg sessions)
 * @param[out] shell User program to run. May be ""
 * @param[out] directory Directory to run the program in. May be ""
 * @return != 0 for error
 *
 * Returned string pointers are valid until scp_msg_in_reset() is
 * called for the transport
 */
int
eicp_get_create_session_request(struct trans *trans,
                                int *scp_fd,
                                unsigned int *display,
                                enum scp_session_type *type,
                                unsigned short *width,
                                unsigned short *height,
                                unsigned char *bpp,
                                const char **shell,
                                const char **directory);

#endif /* EICP_H */
