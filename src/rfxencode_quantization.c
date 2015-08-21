/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Quantization
 *
 * Copyright 2011 Vic Lee
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

#include <math.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"

#if 0
/******************************************************************************/
/* return round(d1 / d2) as if d1 ad d2 are float */
static int divround(int d1, int d2)
{
    if ((d1 < 0) ^ (d2 < 0))
    {
        return (d1 - (d2 / 2)) / d2;
    }
    return (d1 + (d2 / 2)) / d2;
}
#endif

#if 0
#define DIVROUND(_d1, _d2) \
    (_d1) < 0 ? \
        ((_d1) - ((_d2) / 2)) / (_d2) : \
        ((_d1) + ((_d2) / 2)) / (_d2)

/******************************************************************************/
static int
rfx_quantization_encode_block(sint16* buffer, int buffer_size, uint32 factor)
{
    sint16* dst;
    sint16 scale_value;

    if (factor == 0)
    {
        return 1;
    }
    scale_value = 1 << factor;
    for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
    {
        *dst = DIVROUND(*dst, scale_value);
    }
    return 0;
}
#endif

#if 0
/******************************************************************************/
static int
rfx_quantization_encode_block(sint16* buffer, int buffer_size, uint32 factor)
{
    sint16* dst;

    if (factor == 0)
    {
        return 1;
    }
    for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
    {
        *dst = *dst >> factor;
    }
    return 0;
}
#endif

#if 0
/******************************************************************************/
static int
rfx_quantization_encode_block(sint16* buffer, int buffer_size, uint32 factor)
{
    sint16* dst;
    sint16 half;

    if (factor == 0)
    {
        return 1;
    }

    half = (1 << (factor - 1));
    for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
    {
        *dst = (*dst + half) >> factor;
    }
    return 0;
}
#endif

#if 1
/******************************************************************************/
static int
rfx_quantization_encode_block(sint16* buffer, int buffer_size, uint32 factor)
{
    sint16* dst;
    sint16 half;

    factor += DWT_FACTOR;
    if (factor == 0)
    {
        return 1;
    }
    half = (1 << (factor - 1));
    for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
    {
        *dst = (*dst + half) >> factor;
    }
    return 0;
}
#endif

/******************************************************************************/
int
rfx_quantization_encode(sint16* buffer, const char* qtable)
{
    uint32 factor;

    factor = ((qtable[4] >> 0) & 0xf) - 6;
    rfx_quantization_encode_block(buffer, 1024, factor); /* HL1 */
    factor = ((qtable[3] >> 4) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 1024, 1024, factor); /* LH1 */
    factor = ((qtable[4] >> 4) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 2048, 1024, factor); /* HH1 */
    factor = ((qtable[2] >> 4) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3072, 256, factor); /* HL2 */
    factor = ((qtable[2] >> 0) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3328, 256, factor); /* LH2 */
    factor = ((qtable[3] >> 0) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3584, 256, factor); /* HH2 */
    factor = ((qtable[1] >> 0) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3840, 64, factor); /* HL3 */
    factor = ((qtable[0] >> 4) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3904, 64, factor); /* LH3 */
    factor = ((qtable[1] >> 4) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 3968, 64, factor); /* HH3 */
    factor = ((qtable[0] >> 0) & 0xf) - 6;
    rfx_quantization_encode_block(buffer + 4032, 64, factor); /* LL3 */
    return 0;
}

