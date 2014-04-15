/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
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
#include "rfxtile.h"
#include "rfxencode_dwt.h"
#include "rfxencode_quantization.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr3.h"

/******************************************************************************/
static int
rfx_encode_format_rgb(char *rgb_data, int width, int height,
                      int stride_bytes, int pixel_format,
                      sint16 *r_buf, sint16 *g_buf, sint16 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    sint16 r;
    sint16 g;
    sint16 b;

    switch (pixel_format)
    {
        case RFX_FORMAT_BGRA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *b_buf++ = b;
                    g = *src++;
                    *g_buf++ = g;
                    r = *src++;
                    *r_buf++ = r;
                    src++;
                }
            }
            break;
        case RFX_FORMAT_RGBA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *r_buf++ = r;
                    g = *src++;
                    *g_buf++ = g;
                    b = *src++;
                    *b_buf++ = b;
                    src++;
                }
            }
            break;
        case RFX_FORMAT_BGR:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *b_buf++ = b;
                    g = *src++;
                    *g_buf++ = g;
                    r = *src++;
                    *r_buf++ = r;
                }
            }
            break;
        case RFX_FORMAT_RGB:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *r_buf++ = r;
                    g = *src++;
                    *g_buf++ = g;
                    b = *src++;
                    *b_buf++ = b;
                }
            }
            break;
    }
    return 0;
}

/******************************************************************************/
static int
rfx_encode_rgb_to_ycbcr(sint16 *y_r_buf, sint16 *cb_g_buf, sint16 *cr_b_buf)
{
    /* sint32 is used intentionally because we calculate with
     * shifted factors! */
    int i;
    sint32 r, g, b;
    sint32 y, cb, cr;

    /**
     * The encoded YCbCr coefficients are represented as 11.5
     * fixed-point numbers:
     *
     * 1 sign bit + 10 integer bits + 5 fractional bits
     *
     * However only 7 integer bits will be actually used since the value
     * range is [-128.0, 127.0].  In other words, the encoded coefficients
     * is scaled by << 5 when interpreted as sint16.
     * It will be scaled down to original during the quantization phase.
     */
    for (i = 0; i < 4096; i++)
    {
        r = y_r_buf[i];
        g = cb_g_buf[i];
        b = cr_b_buf[i];

        /*
         * We scale the factors by << 15 into 32-bit integers in order to
         * avoid slower floating point multiplications. Since the terms
         * need to be scaled by << 5 we simply scale the final sum by >> 10
         *
         * Y:  0.299000 << 15 = 9798,  0.587000 << 15 = 19235, 0.114000 << 15 = 3735
         * Cb: 0.168935 << 15 = 5535,  0.331665 << 15 = 10868, 0.500590 << 15 = 16403
         * Cr: 0.499813 << 15 = 16377, 0.418531 << 15 = 13714, 0.081282 << 15 = 2663
         */

        y  = (r *  9798 + g *  19235 + b *  3735) >> 10;
        cb = (r * -5535 + g * -10868 + b * 16403) >> 10;
        cr = (r * 16377 + g * -13714 + b * -2663) >> 10;

        y_r_buf[i] = MINMAX(y - 4096, -4096, 4095);
        cb_g_buf[i] = MINMAX(cb, -4096, 4095);
        cr_b_buf[i] = MINMAX(cr, -4096, 4095);
    }
    return 0;
}

/******************************************************************************/
static int
rfx_encode_component(struct rfxencode *enc, const int *quantization_values,
                     sint16 *data, uint8 *buffer, int buffer_size, int *size)
{
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(data, quantization_values) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(data + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(data, 4096, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_rgb(struct rfxencode *enc, char *rgb_data,
               int width, int height, int stride_bytes,
               const int *y_quants, const int *cb_quants, const int *cr_quants,
               STREAM *data_out, int *y_size, int *cb_size, int *cr_size)
{
    sint16 *y_r_buffer = enc->y_r_buffer;
    sint16 *cb_g_buffer = enc->cb_g_buffer;
    sint16 *cr_b_buffer = enc->cr_b_buffer;

    if (rfx_encode_format_rgb(rgb_data, width, height, stride_bytes,
                              enc->format,
                              y_r_buffer, cb_g_buffer, cr_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_rgb_to_ycbcr(y_r_buffer, cb_g_buffer, cr_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_component(enc, y_quants, y_r_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             y_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *y_size);
    if (rfx_encode_component(enc, cb_quants, cb_g_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             cb_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *cb_size);
    if (rfx_encode_component(enc, cr_quants, cr_b_buffer,
                             stream_get_tail(data_out),
                             stream_get_left(data_out),
                             cr_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *cr_size);
    return 0;
}
