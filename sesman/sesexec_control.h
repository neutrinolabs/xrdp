/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2023 Matt Burt
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
 * @file sesexec_control.h
 * @brief Start/stop session executive process
 * @author Matt Burt
 *
 */


#ifndef SESEXEC_H
#define SESEXEC_H

#include <sys/types.h>

struct trans;
struct pre_session_item;

/**
 * Start a session executive
 * @param psi Pre-session item to allocate EICP transport to
 * @result 0 for success
 *
 * If non-zero is returned, all errors have been logged.
 * If zero is returned, the sesexec_trans and sesexec_pid fields of
 * the pre-session-item have been initialised.
 */

int
sesexec_start(struct pre_session_item *psi);

#endif // SESEXEC_H
