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

#ifndef _RAIL_H_
#define _RAIL_H_

#include "arch.h"
#include "parse.h"

int APP_CC
rail_init(void);
int APP_CC
rail_deinit(void);
int APP_CC
rail_data_in(struct stream* s, int chan_id, int chan_flags,
             int length, int total_length);
int APP_CC
rail_xevent(void* xevent);

#endif
