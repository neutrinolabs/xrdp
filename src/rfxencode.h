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

#ifndef __RFXENCODE_H
#define __RFXENCODE_H

struct rfxencode;

typedef int (*rfx_encode_proc)(struct rfxencode *enc, const char *qtable,
                               uint8 *data, uint8 *buffer,
                               int buffer_size, int *size);

struct rfxencode
{
    int width;
    int height;
    int frame_idx;
    int header_processed;
    int mode;
    int properties;
    int flags;
    int bits_per_pixel;
    int format;
    int pad0[7];

    uint8 a_buffer[4096];
    uint8 y_r_buffer[4096];
    uint8 u_g_buffer[4096];
    uint8 v_b_buffer[4096];

    sint16 dwt_buffer[4096];
    sint16 dwt_buffer1[4096];

    rfx_encode_proc rfx_encode;

    int got_sse2;
    int got_sse3;
    int got_sse41;
    int got_sse42;
    int got_sse4a;
    int got_popcnt;
    int got_lzcnt;
    int got_neon;
};

#endif
