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

struct config_sesman;
struct trans;

/* Globals */
extern struct config_sesman *g_cfg;

/**
 * Close all file descriptors used by sesman.
 *
 * This is generally used after forking, to make sure the
 * file descriptors used by the main process are not disturbed
 *
 * This call will also :-
 * - release all trans objects held by sesman
 * - Delete sesman wait objects
 * - Call sesman_delete_listening_transport()
 */
int
sesman_close_all(void);

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

/**
 * Callback to process incoming SCP data
 */
int
sesman_scp_data_in(struct trans *self);

/**
 * Callback to process incoming EICP data
 */
int
sesman_eicp_data_in(struct trans *self);

/**
 * Callback to process incoming ERCP data
 */
int
sesman_ercp_data_in(struct trans *self);

/*
 * Check for termination
 */
int
sesman_is_term(void);

#endif
