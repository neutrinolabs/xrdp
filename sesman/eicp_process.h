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
 * @file eicp_process.h
 * @brief eicp (executive initialisation control protocol) handler function
 * @author Matt Burt
 *
 */

#ifndef EICP_PROCESS_H
#define EICP_PROCESS_H

struct pre_session_item;

/**
 *
 * @brief Processes an EICP message
 * @param sc the sesman connection
 *
 */
int
eicp_process(struct pre_session_item *psi);

#endif
