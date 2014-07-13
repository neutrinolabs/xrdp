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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxcompose.h"
#include "rfxconstants.h"

/******************************************************************************/
void *
rfxcodec_encode_create(int width, int height, int format, int flags)
{
    struct rfxencode *enc;

    enc = malloc(sizeof(struct rfxencode));
    if (enc == 0)
    {
        return 0;
    }
    memset(enc, 0, sizeof(struct rfxencode));
    enc->width = width;
    enc->height = height;
    enc->mode = RLGR3;
    switch (format)
    {
        case RFX_FORMAT_BGRA:
            enc->bits_per_pixel = 32;
            break;
        case RFX_FORMAT_RGBA:
            enc->bits_per_pixel = 32;
            break;
        case RFX_FORMAT_BGR:
            enc->bits_per_pixel = 24;
            break;
        case RFX_FORMAT_RGB:
            enc->bits_per_pixel = 24;
            break;
        case RFX_FORMAT_YUV:
            enc->bits_per_pixel = 32;
            break;
        default:
            free(enc);
            return NULL;
    }
    enc->format = format;
    return enc;
}

/******************************************************************************/
int
rfxcodec_encode_destroy(void * handle)
{
    struct rfxencode *enc;

    enc = (struct rfxencode *) handle;
    if (enc == 0)
    {
        return 0;
    }
    free(enc);
    return 0;
}

/******************************************************************************/
int
rfxcodec_encode(void *handle, char *cdata, int *cdata_bytes,
                char *buf, int width, int height, int stride_bytes,
                struct rfx_rect *regions, int num_regions,
                struct rfx_tile *tiles, int num_tiles,
                const int *quants, int num_quants)
{
    struct rfxencode *enc;
    STREAM s;

    enc = (struct rfxencode *) handle;

    s.data = (uint8 *) cdata;
    s.p = s.data;
    s.size = *cdata_bytes;

    /* Only the first frame should send the RemoteFX header */
    if ((enc->frame_idx == 0) && (enc->header_processed == 0))
    {
        if (rfx_compose_message_header(enc, &s) != 0)
        {
            return 1;
        }
    }
    if (rfx_compose_message_data(enc, &s, regions, num_regions,
                                 buf, width, height, stride_bytes,
                                 tiles, num_tiles, quants, num_quants) != 0)
    {
        return 1;
    }
    *cdata_bytes = (int) (s.p - s.data);
    return 0;
}
