/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Emmanuel Blindauer 2017
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
 * @file sessionrecord.h
 * @brief utmp handling code
 *
 */

#ifndef SESSIONRECORD_H
#define SESSIONRECORD_H

struct login_info;
struct proc_exit_status;

/**
 * @brief Record login in utmp
 *
 * @param pid PID of window manager
 * @param display Display number
 * @param login_info Information about logged in user
 */
void
utmp_login(int pid, int display, const struct login_info *login_info);

/**
 * @brief Record logout in utmp
 *
 * @param pid PID of window manager
 * @param display Display number
 * @param exit_status Exit status of process
 */
void
utmp_logout(int pid, int display, const struct proc_exit_status *exit_status);

#endif
