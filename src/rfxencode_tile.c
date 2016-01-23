/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Encode
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
#include "rfxencode_alpha.h"

#ifdef RFX_USE_ACCEL_X86
#include "x86/funcs_x86.h"
#endif

#ifdef RFX_USE_ACCEL_AMD64
#include "amd64/funcs_amd64.h"
#endif

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/******************************************************************************/
static int
rfx_encode_format_rgb(char *rgb_data, int width, int height,
                      int stride_bytes, int pixel_format,
                      uint8 *r_buf, uint8 *g_buf, uint8 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 *lr_buf;
    uint8 *lg_buf;
    uint8 *lb_buf;

    LLOGLN(10, ("rfx_encode_format_rgb: pixel_format %d", pixel_format));
    b = 0;
    g = 0;
    r = 0;
    switch (pixel_format)
    {
        case RFX_FORMAT_BGRA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                    src++;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = r;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGBA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                    src++;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_BGR:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGB:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (rgb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
    }
    return 0;
}

/******************************************************************************/
static int
rfx_encode_format_argb(char *argb_data, int width, int height,
                       int stride_bytes, int pixel_format,
                       uint8 *a_buf, uint8 *r_buf, uint8 *g_buf, uint8 *b_buf)
{
    int x;
    int y;
    const uint8 *src;
    uint8 a;
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 *la_buf;
    uint8 *lr_buf;
    uint8 *lg_buf;
    uint8 *lb_buf;

    LLOGLN(10, ("rfx_encode_format_argb: pixel_format %d", pixel_format));
    b = 0;
    g = 0;
    r = 0;
    a = 0;
    switch (pixel_format)
    {
        case RFX_FORMAT_BGRA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (argb_data + y * stride_bytes);
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                    a = *src++;
                    *la_buf++ = a;
                }
                while (x < 64)
                {
                    *la_buf++ = a;
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = r;
                    x++;
                }
            }
            while (y < 64)
            {
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(la_buf, la_buf - 64, 64);
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGBA:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (argb_data + y * stride_bytes);
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                    a = *src++;
                    *la_buf++ = a;
                }
                while (x < 64)
                {
                    *la_buf++ = a;
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                la_buf = a_buf + y * 64;
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(la_buf, la_buf - 64, 64);
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_BGR:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (argb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    b = *src++;
                    *lb_buf++ = b;
                    g = *src++;
                    *lg_buf++ = g;
                    r = *src++;
                    *lr_buf++ = r;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
            }
            break;
        case RFX_FORMAT_RGB:
            for (y = 0; y < height; y++)
            {
                src = (uint8*) (argb_data + y * stride_bytes);
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                for (x = 0; x < width; x++)
                {
                    r = *src++;
                    *lr_buf++ = r;
                    g = *src++;
                    *lg_buf++ = g;
                    b = *src++;
                    *lb_buf++ = b;
                }
                while (x < 64)
                {
                    *lr_buf++ = r;
                    *lg_buf++ = g;
                    *lb_buf++ = b;
                    x++;
                }
            }
            while (y < 64)
            {
                lr_buf = r_buf + y * 64;
                lg_buf = g_buf + y * 64;
                lb_buf = b_buf + y * 64;
                memcpy(lr_buf, lr_buf - 64, 64);
                memcpy(lg_buf, lg_buf - 64, 64);
                memcpy(lb_buf, lb_buf - 64, 64);
                y++;
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
rfx_encode_rgb_to_yuv(uint8 *y_r_buf, uint8 *u_g_buf, uint8 *v_b_buf)
{
    int i;
    sint32 r, g, b;
    sint32 y, u, v;

    for (i = 0; i < 4096; i++)
    {
        r = y_r_buf[i];
        g = u_g_buf[i];
        b = v_b_buf[i];

        y = (r *  19595 + g *  38470 + b *   7471) >> 16;
        u = (r * -11071 + g * -21736 + b *  32807) >> 16;
        v = (r *  32756 + g * -27429 + b *  -5327) >> 16;

        y_r_buf[i] = MINMAX(y, 0, 255);
        u_g_buf[i] = MINMAX(u + 128, 0, 255);
        v_b_buf[i] = MINMAX(v + 128, 0, 255);

    }
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1(struct rfxencode *enc, const char *qtable,
                           uint8 *data, uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1:"));
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr1_encode(enc->dwt_buffer1, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3(struct rfxencode *enc, const char *qtable,
                           uint8 *data, uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3:"));
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(enc->dwt_buffer1, buffer, buffer_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1_x86_sse2:"));
#if defined(RFX_USE_ACCEL_X86)
    //if (rfxcodec_encode_dwt_shift_x86_sse2(qtable, data, enc->dwt_buffer1,
    //                                       enc->dwt_buffer) != 0)
    //{
    //    return 1;
    //}
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    //*size = rfxcodec_encode_diff_rlgr1_x86_sse2(enc->dwt_buffer1,
    //                                            buffer, buffer_size);
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr1_encode(enc->dwt_buffer1, buffer, buffer_size);
#endif
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3_x86_sse2:"));
#if defined(RFX_USE_ACCEL_X86)
    //if (rfxcodec_encode_dwt_shift_x86_sse2(qtable, data, enc->dwt_buffer1,
    //                                       enc->dwt_buffer) != 0)
    //{
    //    return 1;
    //}
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    //*size = rfxcodec_encode_diff_rlgr3_x86_sse2(enc->dwt_buffer1,
    //                                            buffer, buffer_size);
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(enc->dwt_buffer1, buffer, buffer_size);
#endif
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr1_amd64_sse2(struct rfxencode *enc, const char *qtable,
                                      uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr1_amd64_sse2:"));
#if defined(RFX_USE_ACCEL_AMD64)
    //if (rfxcodec_encode_dwt_shift_amd64_sse2(qtable, data, enc->dwt_buffer,
    //                                         enc->dwt_buffer1) != 0)
    //{
    //    return 1;
    //}
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    //*size = rfxcodec_encode_diff_rlgr1_amd64_sse2(enc->dwt_buffer1,
    //                                              buffer, buffer_size);
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr1_encode(enc->dwt_buffer1, buffer, buffer_size);
#endif
    return 0;
}

/******************************************************************************/
int
rfx_encode_component_rlgr3_amd64_sse2(struct rfxencode *enc, const char *qtable,
                                      uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size)
{
    LLOGLN(10, ("rfx_encode_component_rlgr3_amd64_sse2:"));
#if defined(RFX_USE_ACCEL_AMD64)
    //if (rfxcodec_encode_dwt_shift_amd64_sse2(qtable, data, enc->dwt_buffer1,
    //                                         enc->dwt_buffer) != 0)
    //{
    //    return 1;
    //}
    if (rfx_dwt_2d_encode(data, enc->dwt_buffer1, enc->dwt_buffer) != 0)
    {
        return 1;
    }
    if (rfx_quantization_encode(enc->dwt_buffer1, qtable) != 0)
    {
        return 1;
    }
    //*size = rfxcodec_encode_diff_rlgr3_amd64_sse2(enc->dwt_buffer1,
    //                                              buffer, buffer_size);
    if (rfx_differential_encode(enc->dwt_buffer1 + 4032, 64) != 0)
    {
        return 1;
    }
    *size = rfx_rlgr3_encode(enc->dwt_buffer1, buffer, buffer_size);
#endif
    return 0;
}

/******************************************************************************/
int
rfx_encode_rgb(struct rfxencode *enc, char *rgb_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size)
{
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;
    if (rfx_encode_format_rgb(rgb_data, width, height, stride_bytes,
                              enc->format,
                              y_r_buffer, u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_rgb_to_yuv(y_r_buffer, u_g_buffer, v_b_buffer) != 0)
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
    if (enc->rfx_encode(enc, u_quants, u_g_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        u_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: u_size %d", *u_size));
    stream_seek(data_out, *u_size);
    if (enc->rfx_encode(enc, v_quants, v_b_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        v_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: v_size %d", *v_size));
    stream_seek(data_out, *v_size);
    return 0;
}

/******************************************************************************/
int
rfx_encode_argb(struct rfxencode *enc, char *rgb_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *u_quants,
                const char *v_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size)
{
    uint8 *a_buffer;
    uint8 *y_r_buffer;
    uint8 *u_g_buffer;
    uint8 *v_b_buffer;

    LLOGLN(10, ("rfx_encode_argb:"));
    a_buffer = enc->a_buffer;
    y_r_buffer = enc->y_r_buffer;
    u_g_buffer = enc->u_g_buffer;
    v_b_buffer = enc->v_b_buffer;
    if (rfx_encode_format_argb(rgb_data, width, height, stride_bytes,
                               enc->format,
                               a_buffer, y_r_buffer,
                               u_g_buffer, v_b_buffer) != 0)
    {
        return 1;
    }
    if (rfx_encode_rgb_to_yuv(y_r_buffer, u_g_buffer, v_b_buffer) != 0)
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
    if (enc->rfx_encode(enc, u_quants, u_g_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        u_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: u_size %d", *u_size));
    stream_seek(data_out, *u_size);
    if (enc->rfx_encode(enc, v_quants, v_b_buffer,
                        stream_get_tail(data_out),
                        stream_get_left(data_out),
                        v_size) != 0)
    {
        return 1;
    }
    LLOGLN(10, ("rfx_encode_rgb: v_size %d", *v_size));
    stream_seek(data_out, *v_size);
    *a_size = rfx_encode_plane(enc, a_buffer, 64, 64, data_out);
    return 0;
}

/******************************************************************************/
int
rfx_encode_yuv(struct rfxencode *enc, char *yuv_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size)
{
    uint8 *y_buffer;
    uint8 *u_buffer;
    uint8 *v_buffer;

    y_buffer = (uint8 *) yuv_data;
    u_buffer = (uint8 *) (yuv_data + RFX_YUV_BTES);
    v_buffer = (uint8 *) (yuv_data + RFX_YUV_BTES * 2);
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

/******************************************************************************/
int
rfx_encode_yuva(struct rfxencode *enc, char *yuva_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *u_quants,
                const char *v_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size)
{
    uint8 *y_buffer;
    uint8 *u_buffer;
    uint8 *v_buffer;
    uint8 *a_buffer;

    y_buffer = (uint8 *) yuva_data;
    u_buffer = (uint8 *) (yuva_data + RFX_YUV_BTES);
    v_buffer = (uint8 *) (yuva_data + RFX_YUV_BTES * 2);
    a_buffer = (uint8 *) (yuva_data + RFX_YUV_BTES * 3);
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
    *a_size = rfx_encode_plane(enc, a_buffer, 64, 64, data_out);
    return 0;
}

