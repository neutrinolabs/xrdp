/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Emmanuel Blindauer 2016
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
 * @file xauth.c
 * @brief XAUTHORITY handling code
 *
 */

#ifndef XAUTH_H
#define XAUTH_H

/**
 *
 * @brief create the XAUTHORITY file for the user according to the display and the cookie
 *        xauth uses XAUTHORITY if defined, ~/.Xauthority otherwise
 * @param display The session display
 * @param file If not NULL, write the authorization in the file instead of default location
 * @return 0 if adding the cookie is ok
 */

int
add_xauth_cookie(int display, const char *file);

#endif
