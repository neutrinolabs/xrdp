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
#include "rfxencode_tile.h"

/******************************************************************************/
static void
cpuid(int func, int *eax, int *ebx, int *ecx, int *edx)
{
    *eax = 0;
    *ebx = 0;
    *ecx = 0;
    *edx = 0;
#ifdef __GNUC__
#if defined(__i386__) || defined(__x86_64__)
    *eax = func;
    __asm volatile
        (
            "mov %%ebx, %%edi;"
            "cpuid;"
            "mov %%ebx, %%esi;"
            "mov %%edi, %%ebx;"
            :"+a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
            : :"edi"
        );
#endif
#endif
}

#if 0
inline unsigned int get_cpu_feature_flags()
{
    unsigned int features;

    __asm
    {
        // Save registers
        push    eax
        push    ebx
        push    ecx
        push    edx

        // Get the feature flags (eax=1) from edx
        mov     eax, 1
        cpuid
        mov     features, edx

        // Restore registers
        pop     edx
        pop     ecx
        pop     ebx
        pop     eax
    }

    return features;
}

#define cpuid(func,a,b,c,d)\
	asm {\
	mov	eax, func\
	cpuid\
	mov	a, eax\
	mov	b, ebx\
	mov	c, ecx\
	mov	d, edx\
	}

#endif

// http://softpixel.com/~cwright/programming/simd/cpuid.php

#define SSE4_1_FLAG     0x080000
#define SSE4_2_FLAG     0x100000

/*
Function 0x80000001:
bit (edx) feature
22        AMD MMX Extensions
30        3DNow!2
31        3DNow! 
*/ 

#if 0
#define cpuid(_func, _ax, _bx, _cx, _dx) \
    __asm volatile ("cpuid": \
    "=a" (_ax), "=b" (_bx), "=c" (_cx), "=d" (_dx) : "a" (_func));
#endif

/******************************************************************************/
void *
rfxcodec_encode_create(int width, int height, int format, int flags)
{
    struct rfxencode *enc;
    int ax, bx, cx, dx;

    enc = (struct rfxencode *) malloc(sizeof(struct rfxencode));
    if (enc == 0)
    {
        return 0;
    }
    memset(enc, 0, sizeof(struct rfxencode));
    cpuid(1, &ax, &bx, &cx, &dx);
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
    cpuid(0x80000001, &ax, &bx, &cx, &dx);
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
    /* assign encoding functions */
    if (flags & RFX_FLAGS_NOACCEL)
    {
        enc->rfx_encode = rfx_encode_component; /* rfxencode_tile.c */
    }
    else
    {
#if defined(RFX_USE_ACCEL) && RFX_USE_ACCEL
        enc->rfx_encode = rfx_encode_component_x86_sse4; /* rfxencode_tile.c */
#else
        enc->rfx_encode = rfx_encode_component; /* rfxencode_tile.c */
#endif
    }
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
