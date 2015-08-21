/**
 * librfxcodec: A Remote Desktop Protocol client.
 * RemoteFX Codec Library
 *
 * Copyright 2015 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXCODEC_ENCODE_ALPHA_H
#define __RFXCODEC_ENCODE_ALPHA_H

int
rfx_encode_plane(struct rfxencode *enc, uint8 *plane, int cx, int cy,
                 STREAM *s);

#endif

