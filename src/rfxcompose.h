/**
 * RFX codec encoder
 *
 * Copyright 2014-2015 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXCOMPOSE_H
#define __RFXCOMPOSE_H

#include "rfxcommon.h"

int
rfx_compose_message_header(struct rfxencode* enc, STREAM* s);
int
rfx_compose_message_data(struct rfxencode* enc, STREAM* s,
                         struct rfx_rect *regions, int num_regions,
                         char *buf, int width, int height, int stride_bytes,
                         struct rfx_tile *tiles, int num_tiles,
                         const char *quants, int num_quants, int flags);

#endif
