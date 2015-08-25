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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxcompose.h"
#include "rfxconstants.h"
#include "rfxencode_tile.h"

#ifdef RFX_USE_ACCEL_X86
#include "x86/funcs_x86.h"
#endif

#ifdef RFX_USE_ACCEL_AMD64
#include "amd64/funcs_amd64.h"
#endif

/******************************************************************************/
int
rfxcodec_encode_create(int width, int height, int format, int flags,
                       void **handle)
{
    struct rfxencode *enc;
    int ax;
    int bx;
    int cx;
    int dx;

    enc = (struct rfxencode *) malloc(sizeof(struct rfxencode));
    if (enc == 0)
    {
        return 1;
    }
    memset(enc, 0, sizeof(struct rfxencode));
#if defined(RFX_USE_ACCEL_X86)
    cpuid_x86(1, 0, &ax, &bx, &cx, &dx);
#elif defined(RFX_USE_ACCEL_AMD64)
    cpuid_amd64(1, 0, &ax, &bx, &cx, &dx);
#else
    ax = 0;
    bx = 0;
    cx = 0;
    dx = 0;
#endif
    if (dx & (1 << 26)) /* SSE 2 */
    {
        printf("rfxcodec_encode_create: got sse2\n");
        enc->got_sse2 = 1;
    }
    if (cx & (1 << 0)) /* SSE 3 */
    {
        printf("rfxcodec_encode_create: got sse3\n");
        enc->got_sse3 = 1;
    }
    if (cx & (1 << 19)) /* SSE 4.1 */
    {
        printf("rfxcodec_encode_create: got sse4.1\n");
        enc->got_sse41 = 1;
    }
    if (cx & (1 << 20)) /* SSE 4.2 */
    {
        printf("rfxcodec_encode_create: got sse4.2\n");
        enc->got_sse42 = 1;
    }
    if (cx & (1 << 23)) /* popcnt */
    {
        printf("rfxcodec_encode_create: got popcnt\n");
        enc->got_popcnt = 1;
    }
#if defined(RFX_USE_ACCEL_X86)
    cpuid_x86(0x80000001, 0, &ax, &bx, &cx, &dx);
#elif defined(RFX_USE_ACCEL_AMD64)
    cpuid_amd64(0x80000001, 0, &ax, &bx, &cx, &dx);
#else
    ax = 0;
    bx = 0;
    cx = 0;
    dx = 0;
#endif
    if (cx & (1 << 5)) /* lzcnt */
    {
        printf("rfxcodec_encode_create: got lzcnt\n");
        enc->got_lzcnt = 1;
    }
    if (cx & (1 << 6)) /* SSE 4.a */
    {
        printf("rfxcodec_encode_create: got sse4.a\n");
        enc->got_sse4a = 1;
    }

    enc->width = width;
    enc->height = height;
    enc->mode = RLGR3;
    if (flags & RFX_FLAGS_RLGR1)
    {
        enc->mode = RLGR1;
    }
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
            return 2;
    }
    enc->format = format;
    /* assign encoding functions */
    if (flags & RFX_FLAGS_NOACCEL)
    {
        if (enc->mode == RLGR3)
        {
            enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
        }
        else
        {
            enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
        }
    }
    else
    {
#if defined(RFX_USE_ACCEL_X86_NOT) // turned off for now
        if (enc->mode == RLGR3)
        {
            enc->rfx_encode = rfx_encode_component_rlgr3_x86_sse2; /* rfxencode_tile.c */
        }
        else
        {
            enc->rfx_encode = rfx_encode_component_rlgr1_x86_sse2; /* rfxencode_tile.c */
        }
#elif defined(RFX_USE_ACCEL_AMD64_NOT) // turned off for now
        if (enc->mode == RLGR3)
        {
            enc->rfx_encode = rfx_encode_component_rlgr3_amd64_sse2; /* rfxencode_tile.c */
        }
        else
        {
            enc->rfx_encode = rfx_encode_component_rlgr1_amd64_sse2; /* rfxencode_tile.c */
        }
#else
        if (enc->mode == RLGR3)
        {
            enc->rfx_encode = rfx_encode_component_rlgr3; /* rfxencode_tile.c */
        }
        else
        {
            enc->rfx_encode = rfx_encode_component_rlgr1; /* rfxencode_tile.c */
        }
#endif
    }
    if (ax == 0)
    {
    }
    if (bx == 0)
    {
    }
    *handle = enc;
    return 0;
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
                const struct rfx_rect *regions, int num_regions,
                const struct rfx_tile *tiles, int num_tiles,
                const char *quants, int num_quants, int flags)
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
                                 tiles, num_tiles, quants, num_quants,
                                 flags) != 0)
    {
        return 1;
    }
    *cdata_bytes = (int) (s.p - s.data);
    return 0;
}
