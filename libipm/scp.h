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
 * E_SCP_<msg>_REQUEST is sent by scp_send_<msg>_request()
 * E_SCP_<msg>_REQUEST is parsed by scp_get_<msg>_request()
 * E_SCP_<msg>_RESPONSE is sent by scp_send_<msg>_response()
 * E_SCP_<msg>_RESPONSE is parsed by scp_get_<msg>_response()
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
    E_SCP_GATEWAY_REQUEST = 1,
    E_SCP_GATEWAY_RESPONSE,
    E_SCP_CREATE_SESSION_REQUEST,
    E_SCP_CREATE_SESSION_RESPONSE,
    E_SCP_LIST_SESSIONS_REQUEST,
    E_SCP_LIST_SESSIONS_RESPONSE
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
 * Connect to an SCP server
 *
 * @param host Host providing SCP service
 * @param port TCP port for SCP service
 * @param term_func Function to poll during connection for program
 *         termination, or NULL for none.
 * @return Initialised SCP transport
 *
 * The returned tranport has the is_term member set to term_func.
 */
struct trans *
scp_connect(const char *host, const  char *port,
            int (*term_func)(void));

/**
 * Converts a standard trans connected to an SCP endpoint to an SCP transport
 *
 * @param trans connected endpoint
 * @return != 0 for error
 *
 * The returned tranport has the is_term member set to term_func.
 */
int
scp_init_trans(struct trans *trans);

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
 * Start parsing an SCP message
 *
 * @param trans SCP transport
 * @return message in the buffer
 *
 * The results of calling this routine before scp_msg_in_check_available()
 * states a message is available are undefined.
 *
 * Calling this function rests the message parsing pointer to the start
 * of the message
 */
enum scp_msg_code
scp_msg_in_start(struct trans *trans);

/**
 * Resets an SCP message buffer ready to receive the next message
 *
 * @param trans libipm transport
 */
void
scp_msg_in_reset(struct trans *trans);


/**
 * Send an E_SCP_GATEWAY_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @param username Username
 * @param password Password
 * @param connection_description Description of the connection
 * @return != 0 for error
 *
 * Server replies with E_SCP_GATEWAY_RESPONSE
 */
int
scp_send_gateway_request(struct trans *trans,
                         const char *username,
                         const char *password,
                         const char *connection_description);

/**
 * Parse an incoming E_SCP_GATEWAY_REQUEST message (SCP server)
 *
 * @param trans SCP transport
 * @param[out] username Username
 * @param[out] password Password
 * @param[out] connection_description Description of the connection
 * @return != 0 for error
 */
int
scp_get_gateway_request(struct trans *trans,
                        const char **username,
                        const char **password,
                        const char **connection_description);

/**
 * Send an E_SCP_GATEWAY_RESPONSE (SCP server)
 *
 * @param trans SCP transport
 * @param auth_result 0 for success, PAM error code otherwise
 * @return != 0 for error
 */
int
scp_send_gateway_response(struct trans *trans,
                          int auth_result);

/**
 * Parses an incoming E_SCP_GATEWAY_RESPONSE (SCP client)
 *
 * @param trans SCP transport
 * @param[out] auth_result 0 for success, PAM error code otherwise
 * @return != 0 for error
 */
int
scp_get_gateway_response(struct trans *trans,
                         int *auth_result);

/* Session messages */

/**
 * Send an E_SCP_CREATE_SESSION_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @param username Username of session to create or re-connect to
 * @param password Password for user
 * @param type Session type
 * @param width Initial session width
 * @param height Initial session height
 * @param bpp Session bits-per-pixel (ignored for Xorg sessions)
 * @param shell User program to run. May be ""
 * @param directory Directory to run the program in. May be ""
 * @param connection_description Description of the connection
 * @return != 0 for error
 *
 * Server replies with E_SCP_CREATE_SESSION_RESPONSE
 */
int
scp_send_create_session_request(struct trans *trans,
                                const char *username,
                                const char *password,
                                enum scp_session_type type,
                                unsigned short width,
                                unsigned short height,
                                unsigned char bpp,
                                const char *shell,
                                const char *directory,
                                const char *connection_description);


/**
 * Parse an incoming E_SCP_CREATE_SESSION_REQUEST (SCP server)
 *
 * @param trans SCP transport
 * @param[out] username Username of session to create or re-connect to
 * @param[out] password Password for user
 * @param[out] type Session type
 * @param[out] width Initial session width
 * @param[out] height Initial session height
 * @param[out] bpp Session bits-per-pixel (ignored for Xorg sessions)
 * @param[out] shell User program to run. May be ""
 * @param[out] directory Directory to run the program in. May be ""
 * @param[out] connection_description Description of the connection
 * @return != 0 for error
 *
 * Returned string pointers are valid until scp_msg_in_reset() is
 * called for the transport
 */
int
scp_get_create_session_request(struct trans *trans,
                               const char **username,
                               const char **password,
                               enum scp_session_type *type,
                               unsigned short *width,
                               unsigned short *height,
                               unsigned char *bpp,
                               const char **shell,
                               const char **directory,
                               const char **connection_description);

/**
 * Send an E_SCP_CREATE_SESSION_RESPONSE (SCP server)
 *
 * @param trans SCP transport
 * @param auth_result 0 for success, PAM error code otherwise
 * @param display Should be zero if authentication failed.
 * @param guid Guid for session. Should be all zeros if authentication failed
 *
 * @return != 0 for error
 */
int
scp_send_create_session_response(struct trans *trans,
                                 int auth_result,
                                 int display,
                                 const struct guid *guid);


/**
 * Parse an incoming E_SCP_CREATE_SESSION_RESPONSE (SCP client)
 *
 * @param trans SCP transport
 * @param[out] auth_result 0 for success, PAM error code otherwise
 * @param[out] display Should be zero if authentication failed.
 * @param[out] guid Guid for session. Should be all zeros if authentication
 *                  failed
 *
 * @return != 0 for error
 */
int
scp_get_create_session_response(struct trans *trans,
                                int *auth_result,
                                int *display,
                                struct guid *guid);

/**
 * Status of an E_SCP_LIST_SESSIONS_RESPONSE message
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
     * Authentication failed for the user
     */
    E_SCP_LS_AUTHENTICATION_FAIL = 100,

    /**
     * A client-side error occurred allocating memory for the session
     */
    E_SCP_LS_NO_MEMORY
};

/**
 * Send an E_LIST_SESSIONS_REQUEST (SCP client)
 *
 * @param trans SCP transport
 * @param username Username
 * @param password Password
 * @return != 0 for error
 *
 * Server replies with one or more E_SCP_LIST_SESSIONS_RESPONSE
 */
int
scp_send_list_sessions_request(struct trans *trans,
                               const char *username,
                               const char *password);

/**
 * Parse an incoming E_LIST_SESSIONS_REQUEST (SCP server)
 *
 * @param trans SCP transport
 * @param[out] username Username
 * @param[out] password Password
 * @return != 0 for error
 */
int
scp_get_list_sessions_request(struct trans *trans,
                              const char **username,
                              const char **password);

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


#endif /* SCP_H */
