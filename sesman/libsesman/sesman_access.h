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
 * @file sesman_access.h
 * @brief User access control definitions
 * @author Simone Fedele
 *
 */

#ifndef SESMAN_ACCESS_H
#define SESMAN_ACCESS_H

struct config_security;

/**
 *
 * @brief Checks if the user is allowed to access the terminal server
 * @param user the user to check
 * @return 0 if access is denied, !=0 if allowed
 *
 */
int
access_login_allowed(const struct config_security *cfg_sec,
                     const char *user);

/**
 *
 * @brief Checks if the user is allowed to access the terminal server for management
 * @param user the user to check
 * @return 0 if access is denied, !=0 if allowed
 *
 */
int
access_login_mng_allowed(const struct config_security *cfg_sec,
                         const char *user);

#endif
