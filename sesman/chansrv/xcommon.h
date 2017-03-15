/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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

#if !defined(XCOMMON_H)
#define XCOMMON_H

#include "arch.h"
#include "parse.h"

/* 32 implies long */
#define FORMAT_TO_BYTES(_format) \
    (_format) == 32 ? sizeof(long) : (_format) / 8

int
xcommon_get_local_time(void);
int
xcommon_init(void);
int
xcommon_get_wait_objs(tbus* objs, int* count, int* timeout);
int
xcommon_check_wait_objs(void);

#endif
