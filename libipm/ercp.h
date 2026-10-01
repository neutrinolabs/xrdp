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
 * @file libipm/ercp.h
 * @brief ERCP declarations
 * @author Matt Burt
 *
 * Functions in this file use the following naming conventions:-
 *
 * E_ERCP_{msg}_REQUEST is sent by ercp_send_{msg}_request()
 * E_ERCP_{msg}_REQUEST is parsed by ercp_get_{msg}_request()
 * E_ERCP_{msg}_RESPONSE is sent by ercp_send_{msg}_response()
 * E_ERCP_{msg}_RESPONSE is parsed by ercp_get_{msg}_response()
 * E_ERCP_{msg}_EVENT is sent by ercp_send_{msg}_event()
 * E_ERCP_{msg}_EVENT is parsed by ercp_get_{msg}_event()
 */

#ifndef ERCP_H
#define ERCP_H

#include "arch.h"
#include "scp_application_types.h"
#include "trans.h"

struct guid;

/* Message codes */
enum ercp_msg_code
{
    E_ERCP_SESSION_ANNOUNCE_EVENT, // sesexec -> sesman
    E_ERCP_SESSION_FINISHED_EVENT, // sesexec -> sesman

    // A connect session request has no matching response, but if
    // successful, a client connect event will be generated.
    E_ERCP_CONNECT_SESSION_REQUEST, // sesman -> sesexec
    // No E_ERCP_CONNECT_SESSION_RESPONSE

    E_ERCP_CLIENT_CONNECT_EVENT,    // sesexec -> sesman
    E_ERCP_CLIENT_DISCONNECT_EVENT  // sesexec -> sesman
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
ercp_msgno_to_str(enum ercp_msg_code n, char *buff, unsigned int buff_size);

/* Connection management facilities */

/**
 * Converts a standard trans connected to an ERCP endpoint to an ERCP transport
 *
 * @param trans connected endpoint
 * @return != 0 for error
 */
int
ercp_init_trans(struct trans *trans);

/**
 * Converts an EICP transport to an ERCP transport.
 *
 * This is done following successful transmission or receipt of an
 * E_EICP_CREATE_SESSION_REQUEST.
 *
 * @param trans connected endpoint
 * @param callback_func New callback function for ERCP messages.
 * @param callback_data New argument for callback function
 */
void
ercp_trans_from_eicp_trans(struct trans *trans,
                           ttrans_data_in callback_func,
                           void *callback_data);

/**
 * Connects to an ERCP endpoint.
 *
 * @param port Path to UDS object in filesystem
 * @param term_func Function to poll during connection for program
 *         termination, or NULL for none.
 */
struct trans *
ercp_connect(const char *port,
             int (*term_func)(void));

/**
 * Checks an ERCP transport to see if a complete message is
 * available for parsing
 *
 * @param trans ERCP transport
 * @param[out] available != 0 if a complete message is available
 * @return != 0 for error
 */
int
ercp_msg_in_check_available(struct trans *trans, int *available);

/**
 * Waits on a single transport for an ERCP message to be available for
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
 * ercp_msg_in_check_available()
 */
int
ercp_msg_in_wait_available(struct trans *trans);


/**
 * Gets the ERCP message number of an incoming message
 *
 * @param trans ERCP transport
 * @return message in the buffer
 *
 * The results of calling this routine before ercp_msg_in_check_available()
 * states a message is available are undefined.
 */
enum ercp_msg_code
ercp_msg_in_get_msgno(const struct trans *trans);

/**
 * Resets an ERCP message buffer ready to receive the next message
 *
 * @param trans libipm transport
 */
void
ercp_msg_in_reset(struct trans *trans);

/* -------------------- Session event messages--------------------  */
/**
 * Send an E_ERCP_SESSION_ANNOUNCE_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This event contains all the information known about a session
 *
 * @param trans ERCP transport
 * @param display Display used by session
 * @param uid UID of user logged in to session
 * @param type Session type
 * @param start_width Starting width of seenio
 * @param start_height Starting height of session
 * @param bpp Bits-per-pixel for session
 * @param guid Session GUID
 * @param start_ip_addr Starting IP address of client
 * @param start_time Session start time
 * @param instance_name Name of xrdp instance
 * @param start_client_name Name of the client which requested the session
 * * @return != 0 for error
 */
int
ercp_send_session_announce_event(struct trans *trans,
                                 const char *display,
                                 uid_t uid,
                                 enum scp_session_type type,
                                 unsigned short start_width,
                                 unsigned short start_height,
                                 unsigned char bpp,
                                 const struct guid *guid,
                                 const char *start_ip_addr,
                                 time_t start_time,
                                 const char *instance_name,
                                 const char *start_client_name);


/**
 * Parse an incoming E_ERCP_SESSION_ANNOUNCE_EVENT
 *
 * This event contains all the information known about a session
 *
 * @param trans ERCP transport
 * @param[out] display Display used by session.
 * @param[out] uid UID of user logged in to session
 * @param[out] type Session type
 * @param[out] start_width Starting width of seenio
 * @param[out] start_height Starting height of session
 * @param[out] bpp Bits-per-pixel for session
 * @param[out] guid Session GUID
 * @param[out] start_ip_addr Starting IP address of client
 * @param[out] start_time Session start time
 * @param[out] instance_name Name of xrdp instance
 * @param[out] start_client_name Name of the client which requested the
 *  session
 * @return != 0 for error
 */
int
ercp_get_session_announce_event(struct trans *trans,
                                const char **display,
                                uid_t *uid,
                                enum scp_session_type *type,
                                unsigned short *start_width,
                                unsigned short *start_height,
                                unsigned char *bpp,
                                struct guid *guid,
                                const char **start_ip_addr,
                                time_t *start_time,
                                const char **instance_name,
                                const char **start_client_name);


/**
 * Send an E_ERCP_SESSION_FINISHED_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This event simply states the attached session has finished and can be
 * removed from any data structures held by sesman
 *
 * @param trans ERCP transport
 * @return != 0 for error
 */
int
ercp_send_session_finished_event(struct trans *trans);

/**
 * Send an E_ERCP_CONNECT_SESSION_REQUEST
 *
 * Direction : sesman -> sesexec
 *
 * This request tells sesexec to handle a session connection
 *
 * A response is sent directly to the SCP client, rather than back
 * to sesman
 *
 * @param trans ERCP transport
 * @param scp_fd SCP file descriptor for a response
 * @param client_ip IP address of connecting client
 * @param client_name Name of connecting client (from RDP client core info)
 * @param scp_flags Flags from scp_send_connect_session_request()
 * @return != 0 for error
 */
int
ercp_send_connect_session_request(struct trans *trans,
                                  int scp_fd,
                                  const char *client_ip,
                                  const char *client_name,
                                  unsigned int scp_flags);

/**
 * Get an E_ERCP_CONNECT_SESSION_REQUEST
 *
 * Direction : sesman -> sesexec
 *
 * This request tells sesexec to handle a session connection
 *
 * A response is sent directly to the SCP client, rather than back
 * to sesman
 *
 * @param trans ERCP transport
 * @param[out] scp_fd SCP file descriptor for a response
 * @param[out] client_ip IP address of connecting client
 * @param[out] client_name Name of connecting client
 * @param[out] scp_flags Flags from scp_send_connect_session_request()
 * @return != 0 for error
 */
int
ercp_get_connect_session_request(struct trans *trans,
                                 int *scp_fd,
                                 const char **client_ip,
                                 const char **client_name,
                                 unsigned int *scp_flags);

/**
 * Send an E_ERCP_CLIENT_CONNECT_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This request tells sesman to update its connected client information
 *
 * @param trans ERCP transport
 * @param client_ip IP address of connecting client
 * @param client_name Name of connecting client (from RDP client core info)
 * @param connect_time Time at which the connect event occurred
 * @return != 0 for error
 */
int
ercp_send_client_connect_event(struct trans *trans,
                               const char *client_ip,
                               const char *client_name,
                               time_t connect_time);

/**
 * Get an E_ERCP_CLIENT_CONNECT_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This request tells sesman to update its connected client information
 *
 * @param trans ERCP transport
 * @param[out] client_ip IP address of connecting client
 * @param[out] client_name Name of connecting client (from RDP client core info)
 * @param[out] connect_time Time at which the connect event occurred
 * @return != 0 for error
 */
int
ercp_get_client_connect_event(struct trans *trans,
                              const char **client_ip,
                              const char **client_name,
                              time_t *connect_time);

/**
 * Send an E_ERCP_CLIENT_DISCONNECT_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This request tells sesman to update its connected client information
 *
 * @param trans ERCP transport
 * @param disconnect_time Time at which the disconnect event occurred
 * @return != 0 for error
 */
int
ercp_send_client_disconnect_event(struct trans *trans,
                                  time_t disconnect_time);

/**
 * Get an E_ERCP_CLIENT_DISCONNECT_EVENT
 *
 * Direction : sesexec -> sesman
 *
 * This request tells sesman to update its connected client information
 *
 * @param trans ERCP transport
 * @param[out] disconnect_time Time at which the disconnect event occurred
 * @return != 0 for error
 */
int
ercp_get_client_disconnect_event(struct trans *trans,
                                 time_t *disconnect_time);

#endif /* ERCP_H */
