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
 * @file libipm/scp_sync.h
 * @brief scp declarations (synchronous calls)
 * @author Simone Fedele/ Matt Burt
 *
 * This module places a synchronous wrapper on top of some of the
 * calls in scp.h. It is intended to be used for simple SCP applications
 * which do not need to handle SCP messages along with other messages
 * using the xrdp transport mechanism.
 *
 */

#ifndef SCP_SYNC_H
#define SCP_SYNC_H

#include <scp.h>

/**
 * Waits on a single transport for a specific SCP message to be available for
 * parsing
 *
 * @param trans libipm transport
 * @param wait_msgno Message number to wait for
 * @return != 0 for error
 *
 * This is a convenience function, used to implement synchronous calls.
 *
 * While the call is active, data-in callbacks for the transport are
 * disabled.
 *
 * Unexpected messages are ignored and logged.
 *
 * Only use this call if you have nothing to do until a message
 * arrives on the transport.
 * - If you have other transports to service, use
 *   scp_msg_in_check_available()
 * - If you can process any incoming message, use
 *   scp_msg_in_wait_available()
 */
int
scp_sync_wait_specific(struct trans *t, enum scp_msg_code wait_msgno);

/**
 * Send UDS login request to sesman and wait for answer
 *
 * @param t SCP transport
 * @return 0 for successful login
 *
 * If non-zero is returned, the scp_connection has been closed (if
 * appropriate) and simply remains to be deleted.
 */
int
scp_sync_uds_login_request(struct trans *t);

/**
 * Send list sessions request to sesman and wait for answer(s)
 *
 * @param t SCP transport
 * @return list of sessions
 *
 * If NULL is returned, the scp_connection has been closed (if
 * appropriate) and simply remains to be deleted.
 *
 * If NULL is not returned, the caller must call list_delete() to
 * free it.
 */
struct list *
scp_sync_list_sessions_request(struct trans *t);

/**
 * Send sockdir creation request to sesman and wait for answer
 *
 * @param t SCP transport
 * @return 0 for successful response from sesman
 *
 * If non-zero is returned, the scp_connection has been closed (if
 * appropriate) and simply remains to be deleted.
 */
int
scp_sync_create_sockdir_request(struct trans *t);

#endif /* SCP_SYNC_H */
