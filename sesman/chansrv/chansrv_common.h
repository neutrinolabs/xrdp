/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2009-2014
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

#ifndef _CHANSRV_COMMON_H
#define _CHANSRV_COMMON_H

#include "parse.h"
#include "os_calls.h"

/* Define bitmask values for restricting the clipboard */
#define CLIP_RESTRICT_NONE 0
#define CLIP_RESTRICT_TEXT (1<<0)
#define CLIP_RESTRICT_FILE (1<<1)
#define CLIP_RESTRICT_IMAGE (1<<2)
#define CLIP_RESTRICT_ALL 0x7fffffff

int read_entire_packet(struct stream *src, struct stream **dest, int chan_flags, int length, int total_length);

#endif

