/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
 *
 * Copyright 2011 Vic Lee
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
#include "rfxconstants.h"
#include "rfxencode_tile.h"
#include "rfxencode_dwt.h"
#include "rfxencode_quantization.h"
#include "rfxencode_differential.h"
#include "rfxencode_rlgr1.h"
#include "rfxencode_rlgr3.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/******************************************************************************/
static int
rfx_encode_format_rgb(char *rgb_data, int width, int height,
                      int stride_bytes, int pixel_format,
                      sint8 *r_buf, sint8 *g_buf, sint8 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    sint8 r;
    sint8 g;
    sint8 b;

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
/* http://msdn.microsoft.com/en-us/library/ff635643.aspx
 * 0.299   -0.168935    0.499813
 * 0.587   -0.331665   -0.418531
 * 0.114    0.50059    -0.081282
   y = r *  0.299000 + g *  0.587000 + b *  0.114000;
   u = r * -0.168935 + g * -0.331665 + b *  0.500590;
   v = r *  0.499813 + g * -0.418531 + b * -0.081282; */
/* 19595  38470   7471
  -11071 -21736  32807
   32756 -27429  -5327 */
static int
rfx_encode_rgb_to_ycbcr(sint8 *y_r_buf, sint8 *cb_g_buf, sint8 *cr_b_buf)
{
    int i;
    sint32 r, g, b;
    sint32 y, cb, cr;

    for (i = 0; i < 4096; i++)
    {
        r = y_r_buf[i];
        g = cb_g_buf[i];
        b = cr_b_buf[i];

        y =  (r *  19595 + g *  38470 + b *   7471) >> 16;
        cb = (r * -11071 + g * -21736 + b *  32807) >> 16;
        cr = (r *  32756 + g * -27429 + b *  -5327) >> 16;

        y_r_buf[i] = MINMAX(y - 128, -128, 127);
        cb_g_buf[i] = MINMAX(cb, -128, 127);
        cr_b_buf[i] = MINMAX(cr, -128, 127);

    }
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1(struct rfxencode *enc, const int *quantization_values,
                           sint8 *data, uint8 *buffer, int buffer_size, int *size)
{
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, quantization_values) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr1_encode(enc->dwt_buffer1, 4096, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3(struct rfxencode *enc, const int *quantization_values,
                           sint8 *data, uint8 *buffer, int buffer_size, int *size)
{
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, quantization_values) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(enc->dwt_buffer1, 4096, buffer, buffer_size);
    return 0;
}

int
dwt_shift_x86_sse4(const int *qtable, sint8 *src, sint16 *dst, sint16 *temp);
int
diff_rlgr3_x86(sint16 *co, int num_co, uint8 *dst, int dst_bytes);

/******************************************************************************/
int
rfx_encode_component_x86_sse4(struct rfxencode *enc,
                              const int *quantization_values,
                              sint8 *data,
                              uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_x86_sse4:"));
    /* put asm calls here */
    if (dwt_shift_x86_sse4(quantization_values, data, enc->dwt_buffer1,
                           enc->dwt_buffer) != 0)
    {
        return 1;
    }
    *size = diff_rlgr3_x86(enc->dwt_buffer1, 4096, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_rgb(struct rfxencode *enc, char *rgb_data,
               int width, int height, int stride_bytes,
               const int *y_quants, const int *cb_quants, const int *cr_quants,
               STREAM *data_out, int *y_size, int *cb_size, int *cr_size)
{
    sint8 *y_r_buffer;
    sint8 *cb_g_buffer;
    sint8 *cr_b_buffer;

    y_r_buffer = enc->y_r_buffer;
    cb_g_buffer = enc->cb_g_buffer;
    cr_b_buffer = enc->cr_b_buffer;
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
    if (enc->rfx_encode(enc, y_quants, y_r_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        y_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: y_size %d", *y_size));
    stream_seek(data_out, *y_size);
    if (enc->rfx_encode(enc, cb_quants, cb_g_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        cb_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: cb_size %d", *cb_size));
    stream_seek(data_out, *cb_size);
    if (enc->rfx_encode(enc, cr_quants, cr_b_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        cr_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: cr_size %d", *cr_size));
    stream_seek(data_out, *cr_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_yuv(struct rfxencode *enc, char *yuv_data,
               int width, int height, int stride_bytes,
               const int *y_quants, const int *u_quants, const int *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size)
{
    sint8 *y_buffer;
    sint8 *u_buffer;
    sint8 *v_buffer;

    y_buffer = (sint8 *) yuv_data;
    u_buffer = (sint8 *) (yuv_data + RFX_YUV_BTES);
    v_buffer = (sint8 *) (yuv_data + RFX_YUV_BTES * 2);
    if (enc->rfx_encode(enc, y_quants, y_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        y_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *y_size);
    if (enc->rfx_encode(enc, u_quants, u_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        u_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *u_size);
    if (enc->rfx_encode(enc, v_quants, v_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        v_size) != 0)
    {
        return 1;
    }
    stream_seek(data_out, *v_size);
    return 0;
}
