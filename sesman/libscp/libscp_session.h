/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
 * @file libscp_session.h
 * @brief SCP_SESSION handling code
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_SESSION_H
#define LIBSCP_SESSION_H

#include "libscp.h"

/**
 *
 * @brief creates a new connection
 * @param sck the connection socket
 *
 * @return a struct SCP_SESSION* object on success, NULL otherwise
 *
 */
struct SCP_SESSION *
scp_session_create(void);

/*
 * Makes a copy of a struct SCP_SESSION object
 * @param s Object to clone
 * @return a copy of s, or NULL if no memory
 */
struct SCP_SESSION *
scp_session_clone(const struct SCP_SESSION *s);

int
scp_session_set_type(struct SCP_SESSION *s, tui8 type);

int
scp_session_set_version(struct SCP_SESSION *s, tui32 version);

int
scp_session_set_height(struct SCP_SESSION *s, tui16 h);

int
scp_session_set_width(struct SCP_SESSION *s, tui16 w);

int
scp_session_set_bpp(struct SCP_SESSION *s, tui8 bpp);

int
scp_session_set_rsr(struct SCP_SESSION *s, tui8 rsr);

int
scp_session_set_locale(struct SCP_SESSION *s, const char *str);

int
scp_session_set_username(struct SCP_SESSION *s, const char *str);

int
scp_session_set_password(struct SCP_SESSION *s, const char *str);

int
scp_session_set_domain(struct SCP_SESSION *s, const char *str);

int
scp_session_set_program(struct SCP_SESSION *s, const char *str);

int
scp_session_set_directory(struct SCP_SESSION *s, const char *str);

int
scp_session_set_client_ip(struct SCP_SESSION *s, const char *str);

int
scp_session_set_hostname(struct SCP_SESSION *s, const char *str);

int
scp_session_set_addr(struct SCP_SESSION *s, int type, const void *addr);

int
scp_session_set_display(struct SCP_SESSION *s, SCP_DISPLAY display);

int
scp_session_set_errstr(struct SCP_SESSION *s, const char *str);

int
scp_session_set_guid(struct SCP_SESSION *s, const tui8 *guid);

/**
 *
 * @brief destroys a session object
 * @param s the object to be destroyed
 *
 */
void
scp_session_destroy(struct SCP_SESSION *s);

#endif
