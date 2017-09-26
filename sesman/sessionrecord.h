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
 * @brief utmp/wtmp handling code
 *
 */

#ifndef SESSIONRECORD_H
#define SESSIONRECORD_H


#ifdef HAVE_UTMPX_H
#include <utmpx.h>
typedef struct utmpx _utmp;
#else
#include <utmpx.h>
typedef struct utmp _utmp;
#endif




#define XRDP_LINE_FORMAT "xrdp:%d"
/**
 *
 * @brief
 *
 * @param pid
 * @return 0
 */

int add_xtmp_entry(int pid, const char *line, const char *user, const char *rhostname, short state);

int utmp_login(int pid, int display, const char *user, const char *rhostname);

int utmp_logout(int pid, int display, const char *user, const char *rhostname);

#endif
