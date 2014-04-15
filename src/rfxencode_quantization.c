/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Quantization
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"

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

/******************************************************************************/
int
rfx_quantization_encode(sint16* buffer, const int* quantization_values)
{
    rfx_quantization_encode_block(buffer, 1024, quantization_values[8] - 6); /* HL1 */
    rfx_quantization_encode_block(buffer + 1024, 1024, quantization_values[7] - 6); /* LH1 */
    rfx_quantization_encode_block(buffer + 2048, 1024, quantization_values[9] - 6); /* HH1 */
    rfx_quantization_encode_block(buffer + 3072, 256, quantization_values[5] - 6); /* HL2 */
    rfx_quantization_encode_block(buffer + 3328, 256, quantization_values[4] - 6); /* LH2 */
    rfx_quantization_encode_block(buffer + 3584, 256, quantization_values[6] - 6); /* HH2 */
    rfx_quantization_encode_block(buffer + 3840, 64, quantization_values[2] - 6); /* HL3 */
    rfx_quantization_encode_block(buffer + 3904, 64, quantization_values[1] - 6); /* LH3 */
    rfx_quantization_encode_block(buffer + 3968, 64, quantization_values[3] - 6); /* HH3 */
    rfx_quantization_encode_block(buffer + 4032, 64, quantization_values[0] - 6); /* LL3 */

    /* The coefficients are scaled by << 5 at RGB->YCbCr phase, so we round it back here */
    rfx_quantization_encode_block(buffer, 4096, 5);
    return 0;
}
