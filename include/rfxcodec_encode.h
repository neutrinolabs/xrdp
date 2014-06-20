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

#ifndef __RFXCODEC_ENCODE_H
#define __RFXCODEC_ENCODE_H

#define RFX_FORMAT_BGRA 0
#define RFX_FORMAT_RGBA 1
#define RFX_FORMAT_BGR  2
#define RFX_FORMAT_RGB  3
#define RFX_FORMAT_YUV  4 /* YUV444 linear tiled mode */

#define RFX_FLAGS_NONE  0 /* default RFX_FLAGS_RLGR3 and RFX_FLAGS_SAFE */

#define RFX_FLAGS_RLGR3 0 /* default */
#define RFX_FLAGS_RLGR1 1

#define RFX_FLAGS_SAFE  0 /* default */
#define RFX_FLAGS_OPT1  8
#define RFX_FLAGS_OPT2  16

struct rfx_rect
{
    int x;
    int y;
    int cx;
    int cy;
};

struct rfx_tile
{
    int x; /* multiple of 64 */
    int y; /* multiple of 64 */
    int cx; /* must be 64 */
    int cy; /* must be 64 */
    int quant_y;
    int quant_cb;
    int quant_cr;
};

void *
rfxcodec_encode_create(int width, int height, int format, int flags);
int
rfxcodec_encode_destroy(void * handle);
/* quants, 10 ints per set, should be num_quants * 10 ints in quants)
 * quantizer order is
 * 0 - LL3
 * 1 - LH3
 * 2 - HL3
 * 3 - HH3
 * 4 - LH2
 * 5 - HL2
 * 6 - HH2
 * 7 - LH1
 * 8 - HL1
 * 9 - HH1 */
int
rfxcodec_encode(void *handle, char *cdata, int *cdata_bytes,
                char *buf, int width, int height, int stride_bytes,
                struct rfx_rect *region, int num_region,
                struct rfx_tile *tiles, int num_tiles,
                const int *quants, int num_quants);

#endif
