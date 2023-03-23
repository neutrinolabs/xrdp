/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2023
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
 * @file sesexec.h
 * @brief Main include file
 * @author Jay Sorg
 *
 */

#ifndef SESEXEC_H
#define SESEXEC_H

#include <sys/types.h>

struct config_sesman;
struct trans;
struct login_info;
struct session_data;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define USE_BSD_SETLOGIN
#endif

/* Globals */
extern struct config_sesman *g_cfg;
extern unsigned char g_fixedkey[8];
extern struct login_info *g_login_info;
extern struct session_data *g_session_data;

extern tintptr g_term_event;
extern tintptr g_sigchld_event;
extern pid_t g_pid;


/**
 * Callback to process incoming ERCP data
 */
int
sesexec_ercp_data_in(struct trans *self);

/*
 * Check for termination
 */
int
sesexec_is_term(void);

/*
 * Terminate the sesexec main loop
 */
void
sesexec_terminate_main_loop(int status);

#endif // SESEXEC_H
