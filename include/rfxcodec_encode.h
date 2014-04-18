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

struct rfx_rect
{
    int x;
    int y;
    int cx;
    int cy;
};

void *
rfxcodec_encode_create(int width, int height, int format);
int
rfxcodec_encode_destroy(void * handle);
int
rfxcodec_encode(void *handle, char *cdata, int *cdata_bytes,
                char *buf, int width, int height, int stride_bytes,
                struct rfx_rect *region, int num_region,
                struct rfx_rect *tiles, int num_tiles,
                int *quant);

#endif
