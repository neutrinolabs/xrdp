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
 * @file scp_v1.h
 * @brief scp version 1 declarations - management
 * @author Simone Fedele
 *
 */

#ifndef SCP_V1_MNG_H
#define SCP_V1_MNG_H

/**
 *
 * @brief processes the stream using scp version 1
 * @param in_sck connection socket
 * @param in_s input stream
 * @param out_s output stream
 *
 */
void
scp_v1_mng_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

#endif
