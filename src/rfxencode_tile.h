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

#ifndef __RFXTILE_H
#define __RFXTILE_H

#include "rfxcommon.h"


#define RFX_YUV_BTES (64 * 64)

int
rfx_encode_component_rlgr1(struct rfxencode *enc, const char *qtable,
                           uint8 *data,
                           uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr3(struct rfxencode *enc, const char *qtable,
                           uint8 *data,
                           uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_rgb(struct rfxencode *enc, char *rgb_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *cb_size, int *cr_size);
int
rfx_encode_argb(struct rfxencode *enc, char *argb_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *cb_quants,
                const char *cr_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size);
int
rfx_encode_yuv(struct rfxencode *enc, char *yuv_data,
               int width, int height, int stride_bytes,
               const char *y_quants, const char *u_quants,
               const char *v_quants,
               STREAM *data_out, int *y_size, int *u_size, int *v_size);
int
rfx_encode_yuva(struct rfxencode *enc, char *yuv_data,
                int width, int height, int stride_bytes,
                const char *y_quants, const char *u_quants,
                const char *v_quants,
                STREAM *data_out, int *y_size, int *u_size,
                int *v_size, int *a_size);

int
rfx_encode_component_rlgr1_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr3_x86_sse2(struct rfxencode *enc, const char *qtable,
                                    uint8 *data,
                                    uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr1_x86_sse41(struct rfxencode *enc, const char *qtable,
                                     uint8 *data,
                                     uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr3_x86_sse41(struct rfxencode *enc, const char *qtable,
                                     uint8 *data,
                                     uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr1_amd64_sse2(struct rfxencode *enc, const char *qtable,
                                      uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr3_amd64_sse2(struct rfxencode *enc, const char *qtable,
                                      uint8 *data,
                                      uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr1_amd64_sse41(struct rfxencode *enc, const char *qtable,
                                       uint8 *data,
                                       uint8 *buffer, int buffer_size, int *size);
int
rfx_encode_component_rlgr3_amd64_sse41(struct rfxencode *enc, const char *qtable,
                                       uint8 *data,
                                       uint8 *buffer, int buffer_size, int *size);

#endif
