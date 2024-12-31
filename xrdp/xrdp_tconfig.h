/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Koichiro Iwao
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

/**
 *
 * @file xrdp_tconfig.c
 * @brief TOML config loader and structures
 * @author Koichiro Iwao
 *
 */
#ifndef _XRDP_TCONFIG_H_
#define _XRDP_TCONFIG_H_

#include "arch.h"

/* The number of connection types in MS-RDPBCGR 2.2.1.3.2 */
#define NUM_CONNECTION_TYPES 7
#define GFX_CONF XRDP_CFG_PATH "/gfx.toml"

/* nc stands for new config */
struct xrdp_tconfig_gfx_x264_param
{
    char preset[16];
    char tune[16];
    char profile[16];
    int vbv_max_bitrate;
    int vbv_buffer_size;
    int fps_num;
    int fps_den;
    int threads;
};

struct xrdp_tconfig_gfx_openh264_param
{
    bool_t EnableFrameSkip;
    int TargetBitrate;
    int MaxBitrate;
    float MaxFrameRate;
};

enum xrdp_tconfig_codecs
{
    XTC_H264,
    XTC_RFX
};

enum xrdp_tconfig_h264_encoders
{
    XTC_H264_X264,
    XTC_H264_OPENH264
};

struct xrdp_tconfig_gfx_codec_order
{
    enum xrdp_tconfig_codecs codecs[2];
    unsigned short codec_count;
};

struct xrdp_tconfig_gfx
{
    struct xrdp_tconfig_gfx_codec_order codec;
    enum xrdp_tconfig_h264_encoders h264_encoder;
    /* store x264 parameters for each connection type */
    struct xrdp_tconfig_gfx_x264_param x264_param[NUM_CONNECTION_TYPES];
    struct xrdp_tconfig_gfx_openh264_param
        openh264_param[NUM_CONNECTION_TYPES];
};

static const char *const rdpbcgr_connection_type_names[] =
{
    "default", /* for xrdp internal use */
    "modem",
    "broadband_low",
    "satellite",
    "broadband_high",
    "wan",
    "lan",
    "autodetect",
    0
};

/**
 * Provide a string representation of a codec order
 *
 * @param codec_order Codec order struct
 * @param buff Buffer for result
 * @param bufflen Length of above
 * @return Convenience copy of buff
 */
const char *
tconfig_codec_order_to_str(
    const struct xrdp_tconfig_gfx_codec_order *codec_order,
    char *buff,
    unsigned int bufflen);

/**
 * Loads the GFX config from the specified file
 *
 * @param filename Name of file to load
 * @param config Struct to receive result
 * @return 0 for success
 *
 * In the event of failure, an error is logged. A minimal
 * useable configuration is always returned
 */
int
tconfig_load_gfx(const char *filename, struct xrdp_tconfig_gfx *config);

#endif
