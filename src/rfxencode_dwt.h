/**
 * RFX codec encoder
 *
 * Copyright 2014 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXENCODE_RFX_H
#define __RFXENCODE_RFX_H

int
rfx_dwt_2d_encode(sint16 *buffer, sint16 *dwt_buffer);
int
rfx_dwt_2d_encode8(sint8 *in_buffer, sint16 *buffer, sint16 *dwt_buffer);

#endif
