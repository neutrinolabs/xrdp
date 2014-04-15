/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Differential Encoding
 *
 * Copyright 2011 Vic Lee
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

#ifndef __RFXENCODE_DIFFERENTIAL_H
#define __RFXENCODE_DIFFERENTIAL_H

#include "rfxcommon.h"

int
rfx_differential_encode(sint16 *buffer, int buffer_size);

#endif /* __RFXENCODE_DIFFERENTIAL_H */
