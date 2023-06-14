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
 * @file libipm/scp.h
 * @brief scp declarations
 * @author Simone Fedele/ Matt Burt
 *
 * Functions in this file use the following naming conventions:-
 *
 * E_SCP_{msg}_REQUEST is sent by scp_send_{msg}_request()
 * E_SCP_{msg}_REQUEST is parsed by scp_get_{msg}_request()
 * E_SCP_{msg}_RESPONSE is sent by scp_send_{msg}_response()
 * E_SCP_{msg}_RESPONSE is parsed by scp_get_{msg}_response()
 */

#ifndef SCP_H
#define SCP_H

#include "arch.h"

struct guid;
struct trans;

#include "scp_application_types.h"

/* Message codes */
enum scp_msg_code
{
    E_SCP_SET_PEERNAME_REQUEST = 1,
    // No E_SCP_SET_PEERNAME_RESPONSE

    E_SCP_SYS_LOGIN_REQUEST,
    E_SCP_UDS_LOGIN_REQUEST,
    E_SCP_LOGIN_RESPONSE, /* Shared between login request types */

    E_SCP_LOGOUT_REQUEST,
    // No S_SCP_LOGOUT_RESPONSE

    E_SCP_CREATE_SESSION_REQUEST,
    E_SCP_CREATE_SESSION_RESPONSE,

    E_SCP_LIST_SESSIONS_REQUEST,
    E_SCP_LIST_SESSIONS_RESPONSE,

    E_SCP_CLOSE_CONNECTION_REQUEST
    // No E_SCP_CLOSE_CONNECTION_RESPONSE
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
scp_msgno_to_str(enum scp_msg_code n, char *buff, unsigned int buff_size);

/* Connection management facilities */

/**
 * Maps a port definition to a UNIX domain socket path
 * @param port Port definition (e.g. from sesman.ini). Can be "" or NULL
 * @param buff Buffer for result
 * @param bufflen Length of buff
 *
 * @return Number of chars needed for result, excluding the '\0'
 */
int
scp_port_to_unix_domain_path(const char *port, char *buff,
                             unsigned int bufflen);

/**
 * Maps a port definition to a displayable string
 * @param port Port definition (e.g. from sesman.ini). Can be "" or NULL
 * @param buff Buffer for result
 * @param bufflen Length of buff
 *
 * @return Number of chars needed for result, excluding the '\0'
 *
 * This differs from scp_port_to_unix_domain_path() in that the result is
 * for displaying to the user (i.e. in a status message), rather than for
 * connecting to. For log messages, use the result of
 * scp_port_to_unix_domain_path()
 */
int
scp_port_to_display_string(const char *port, char *buff, unsigned int bufflen);

/**
 * Connect to an SCP server
 *
 * @param port Port definition (e.g. from sesman.ini)
 * @param peername Name of this program or object (e.g. "xrdp-sesadmin")
 * @param term_func Function to poll during connection for program
 *         termination, or NULL for none.
 * @return Initialised SCP transport
 *
 * The returned transport has the is_term member set to term_func.
 */
struct trans *
scp_connect(const char *port,
            const char *peername,
            int (*term_func)(void));

/**
 * Converts a standard trans connected to an SCP endpoint to an SCP transport
 *
 * If you are running on a client, you may wish to use
 * scp_send_set_peername_request() after the connect to inform the
 * server who you are.
 *
 * @param trans connected endpoint
 * @return != 0 for error
 */
int
scp_init_trans(struct trans *trans);

/**
 * Creates an SCP transport from a file descriptor
 *
 * If you are running on a client, you may wish to use
 * scp_send_set_peername_request() after the connect to inform the
 * server who you are.
 *
 * @param fd file descriptor
 * @param trans_type TRANS_TYPE_SERVER or TRANS_TYPE_CLIENT
 * @param term_func Function to poll during connection for program
 *         termination, or NULL for none.
 * @return SCP transport, or NULL
 */
struct trans *
scp_init_trans_from_fd(int fd, int trans_type, int (*term_func)(void));

/**
 * Checks an SCP transport to see if a complete message is
 * available for parsing
 *
 * @param trans SCP transport
 * @param[out] available != 0 if a complete message is available
 * @return != 0 for error
 */
int
scp_msg_in_check_available(struct trans *trans, int *available);

/**
 * Waits on a single transport for an SCP message to be available for
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
 * scp_msg_in_check_available()
 */
int
scp_msg_in_wait_available(struct trans *trans);


/**
 * Gets the SCP message number of an incoming message
 *
 * @param trans SCP transport
 * @return message in the buffer
 *
 * The results of calling this routine before scp_msg_in_check_available()
 * states a message is available are undefined.
 */
enum scp_msg_code
scp_msg_in_get_msgno(const struct trans *trans);

/**
 * Resets an SCP message buffer ready to receive the next message
 *
 * @param trans libipm transport
 */
void
scp_msg_in_reset(struct trans *trans);

/* -------------------- Setup messages--------------------  */

/**
 * Send an E_SCP_SET_PEERNAME_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @param peername Peername
 * @return != 0 for error
 *
 * Server does not send a response
 *
 * This message is sent automatically by scp_connect(), but it can
 * be sent at any time.
 */
int
scp_send_set_peername_request(struct trans *trans,
                              const char *peername);

/**
 * Parse an incoming E_SCP_SET_PEERNAME_REQUEST message (SCP server)
 *
 * @param trans SCP transport
 * @param[out] peername peername
 * @return != 0 for error
 */
int
scp_get_set_peername_request(struct trans *trans,
                             const char **peername);

/* -------------------- Login messages--------------------  */

/**
 * Send an E_SCP_UDS_LOGIN_REQUEST (SCP client)
 *
 * User is logged in using their socket details
 *
 * @param trans SCP transport
 * @return != 0 for error
 *
 * Server replies with E_SCP_LOGIN_RESPONSE
 */
int
scp_send_uds_login_request(struct trans *trans);

/**
 * Send an E_SCP_SYS_LOGIN_REQUEST (SCP client)
 *
 * User is logged in using explicit credentials
 *
 * @param trans SCP transport
 * @param username Username
 * @param password Password
 * @param ip_addr IP address for the client (or "" if not known)
 * @return != 0 for error
 *
 * Server replies with E_SCP_LOGIN_RESPONSE
 */
int
scp_send_sys_login_request(struct trans *trans,
                           const char *username,
                           const char *password,
                           const char *ip_addr);

/**
 * Parse an incoming E_SCP_SYS_LOGIN_REQUEST message (SCP server)
 *
 * @param trans SCP transport
 * @param[out] username Username
 * @param[out] password Password
 * @param[out] ip_addr IP address for the client. May be ""
 * @return != 0 for error
 */
int
scp_get_sys_login_request(struct trans *trans,
                          const char **username,
                          const char **password,
                          const char **ip_addr);

/**
 * Send an E_SCP_LOGIN_RESPONSE (SCP server)
 *
 * @param trans SCP transport
 * @param login_result What happened to the login
 * @param server_closed If login fails, whether server has closed connection.
 *        If not, a retry can be made.
 * @param uid UID for a successful login
 * @return != 0 for error
 */
int
scp_send_login_response(struct trans *trans,
                        enum scp_login_status login_result,
                        int server_closed,
                        int uid);

/**
 * Parses an incoming E_SCP_LOGIN_RESPONSE (SCP client)
 *
 * @param trans SCP transport
 * @param[out] login_result 0 for success, PAM error code otherwise
 * @param[out] server_closed If login fails, whether server has closed
 *             connection. If not a retry can be made.
 * @param[out] uid UID for a successful login
 *
 * server_closed and uid can be passed NULL if the caller isn't interested.
 *
 * @return != 0 for error
 */
int
scp_get_login_response(struct trans *trans,
                       enum scp_login_status *login_result,
                       int *server_closed,
                       int *uid);

/**
 * Send an E_SCP_LOGOUT_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @return != 0 for error
 *
 * Logs the user out (if they are logged in), so that another
 * login request can be sent, maybe with a different user.
 *
 * A reply is not sent
 */
int
scp_send_logout_request(struct trans *trans);

/* -------------------- Session messages--------------------  */

/**
 * Send an E_SCP_CREATE_SESSION_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @param type Session type
 * @param width Initial session width
 * @param height Initial session height
 * @param bpp Session bits-per-pixel (ignored for Xorg sessions)
 * @param shell User program to run. May be ""
 * @param directory Directory to run the program in. May be ""
 * @return != 0 for error
 *
 * Server replies with E_SCP_CREATE_SESSION_RESPONSE
 */
int
scp_send_create_session_request(struct trans *trans,
                                enum scp_session_type type,
                                unsigned short width,
                                unsigned short height,
                                unsigned char bpp,
                                const char *shell,
                                const char *directory);


/**
 * Parse an incoming E_SCP_CREATE_SESSION_REQUEST (SCP server)
 *
 * @param trans SCP transport
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
scp_get_create_session_request(struct trans *trans,
                               enum scp_session_type *type,
                               unsigned short *width,
                               unsigned short *height,
                               unsigned char *bpp,
                               const char **shell,
                               const char **directory);

/**
 * Send an E_SCP_CREATE_SESSION_RESPONSE (SCP server)
 *
 * @param trans SCP transport
 * @param status Status of creation request
 * @param display Should be zero if create session failed.
 * @param guid Guid for session. Should be all zeros if create session failed
 *
 * @return != 0 for error
 */
int
scp_send_create_session_response(struct trans *trans,
                                 enum scp_screate_status status,
                                 int display,
                                 const struct guid *guid);


/**
 * Parse an incoming E_SCP_CREATE_SESSION_RESPONSE (SCP client)
 *
 * @param trans SCP transport
 * @param[out] status Status of creation request
 * @param[out] display Should be zero if create session failed.
 * @param[out] guid Guid for session. Should be all zeros if create session
 *                  failed
 *
 * @return != 0 for error
 */
int
scp_get_create_session_response(struct trans *trans,
                                enum scp_screate_status *status,
                                int *display,
                                struct guid *guid);

/**
 * Send an E_LIST_SESSIONS_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @return != 0 for error
 *
 * Server replies with one or more E_SCP_LIST_SESSIONS_RESPONSE
 */
int
scp_send_list_sessions_request(struct trans *trans);

/**
 * Send an E_LIST_SESSIONS_RESPONSE (SCP server)
 *
 * @param trans SCP transport
 * @param status Status of request
 * @param info Session to send if status == E_SCL_LS_SESSION_INFO
 * @return != 0 for error
 */
int
scp_send_list_sessions_response(
    struct trans *trans,
    enum scp_list_sessions_status status,
    const struct scp_session_info *info);

/**
 * Parse an incoming E_LIST_SESSIONS_RESPONSE (SCP client)
 *
 * @param trans SCP transport
 * @param[out] status Status of request
 * @param[out] info Session if status == E_SCL_LS_SESSION_INFO
 * @return != 0 for error
 *
 * The session info is returned as a dynamically allocated structure.
 * After use the structure (and its members) can be de-allocated with
 * a single call to g_free()
 *
 * The info structures can be added to a 'struct list' with auto_free set.
 * When the list is de-allocated, all the structures will be de-allocated too.
 */
int
scp_get_list_sessions_response(
    struct trans *trans,
    enum scp_list_sessions_status *status,
    struct scp_session_info **info);

/**
 * Send an E_CLOSE_CONNECTION_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @return != 0 for error
 *
 * Server closes the connection quietly.
 */
int
scp_send_close_connection_request(struct trans *trans);


#endif /* SCP_H */
