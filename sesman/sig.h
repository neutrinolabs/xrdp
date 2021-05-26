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
 * @file sig.h
 * @brief Signal handling function declarations
 * @author Jay Sorg, Simone Fedele
 *
 */

#ifndef SIG_H
#define SIG_H

/**
 *
 * @brief Shutdown signal code
 * @param sig The received signal
 *
 */
void
sig_sesman_shutdown(int sig);

/**
 *
 * @brief SIGHUP handling code
 * @param sig The received signal
 *
 */
void
sig_sesman_reload_cfg(int sig);

/**
 *
 * @brief SIGCHLD handling code
 * @param sig The received signal
 *
 */
void
sig_sesman_session_end(int sig);

/**
 *
 * @brief signal handling thread
 *
 */
void *
sig_handler_thread(void *arg);

#endif
