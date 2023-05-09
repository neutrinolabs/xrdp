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
 * @file ercp_process.h
 * @brief ERCP (executive run-time control protocol) handler function
 * @author Matt Burt
 *
 */

#ifndef ERCP_PROCESS_H
#define ERCP_PROCESS_H

struct session_item;

/**
 *
 * @brief Processes an ERCP message
 * @param sc the sesman connection
 *
 */
int
ercp_process(struct session_item *si);

#endif // ERCP_PROCESS_H
