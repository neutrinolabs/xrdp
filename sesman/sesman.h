/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file sesman.h
 * @brief Main include file
 * @author Jay Sorg
 *
 */

#ifndef SESMAN_H
#define SESMAN_H

/**
 * Type for managing sesman connections from xrdp (etc)
 */
struct sesman_con
{
    struct trans *t;
    char peername[15 + 1]; /* Name of peer, if known, for logging */
    int    close_requested; /* Set to close the connection normally */
    unsigned int auth_retry_count;
    struct auth_info *auth_info; /* non-NULL for an authenticated connection */
    int    uid; /* User */
    char *username; /* Username from UID (at time of logon) */
    char  *ip_addr; /* Connecting IP address */
};

/* Globals */
extern struct config_sesman *g_cfg;
extern unsigned char g_fixedkey[8];
extern tintptr g_term_event;
extern tintptr g_sigchld_event;
extern tintptr g_reload_event;

/**
 * Set the peername of a connection
 *
 * @param name Name to set
 * @result 0 for success
 */
int
sesman_set_connection_peername(struct sesman_con *sc, const char *name);

/*
 * Close all file descriptors used by sesman.
 *
 * This is generally used after forking, to make sure the
 * file descriptors used by the main process are not disturbed
 *
 * This call will also release all trans and SCP_SESSION objects
 * held by sesman
 *
 * @param flags Set SCA_CLOSE_AUTH_INFO to close any open auth_info
 *              objects. By default these are not cleared, and should
 *              only be done so when exiting sesman.
 */
#define SCA_CLOSE_AUTH_INFO (1<<0)
int
sesman_close_all(unsigned int flags);

/*
 * Remove the listening transport
 *
 * Needed if reloading the config and the listener has changed
 */
void
sesman_delete_listening_transport(void);

/*
 * Create the listening socket transport
 *
 * @return 0 for success
 */
int
sesman_create_listening_transport(const struct config_sesman *cfg);

#endif
