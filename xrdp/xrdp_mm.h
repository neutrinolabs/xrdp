/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2023
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
 *
 * module manager
 */

#ifndef _XRDP_MM_H
#define _XRDP_MM_H

#include "arch.h"
#include "trans.h"
#include "list16.h"
#include "libxrdpinc.h"
#include "xrdp_types.h"

int
advance_resize_state_machine(struct xrdp_mm *mm,
                             enum display_resize_state new_state);

#endif
