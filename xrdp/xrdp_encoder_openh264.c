/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2016-2024
 * Copyright (C) Christopher Pitstick 2023-2024
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
 *
 * openh264 Encoder
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <wels/codec_api.h>
#include <wels/codec_def.h>

#include "xrdp.h"
#include "arch.h"
#include "os_calls.h"
#include "xrdp_encoder_openh264.h"
#include "xrdp_tconfig.h"

#define OPENH264_MAX_ENCODERS 16

struct openh264_encoder
{
    ISVCEncoder *openh264_enc_han;
    char *yuvdata;
    int width;
    int height;
};

struct openh264_global
{
    struct openh264_encoder encoders[OPENH264_MAX_ENCODERS];
    struct xrdp_tconfig_gfx_openh264_param
        openh264_param[NUM_CONNECTION_TYPES];
};

/* The method invocations on ISVCEncoder are different for C and C++, as
   ISVCEncoder is a true class in C++, but an emulated one in C */
#ifdef __cplusplus /* compiling with g++  */
#define ENC_GET_DEFAULT_PARAMS(obj, pParam) (obj)->GetDefaultParams(pParam)
#define ENC_INITIALIZE_EXT(obj, pParam)     (obj)->InitializeExt(pParam)
#define ENC_ENCODE_FRAME(obj, kpSrcPic, pBsInfo) \
    (obj)->EncodeFrame(kpSrcPic, pBsInfo)
#else
#define ENC_GET_DEFAULT_PARAMS(obj, pParam) (*obj)->GetDefaultParams(obj, pParam)
#define ENC_INITIALIZE_EXT(obj, pParam)     (*obj)->InitializeExt(obj, pParam)
#define ENC_ENCODE_FRAME(obj, kpSrcPic, pBsInfo) \
    (*obj)->EncodeFrame(obj, kpSrcPic, pBsInfo)
#endif

/*****************************************************************************/
void *
xrdp_encoder_openh264_create(void)
{
    struct openh264_global *og;
    struct xrdp_tconfig_gfx gfxconfig;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_encoder_openh264_create:");
    og = g_new0(struct openh264_global, 1);
    tconfig_load_gfx(GFX_CONF, &gfxconfig);
    memcpy(&og->openh264_param, &gfxconfig.openh264_param,
           sizeof(struct xrdp_tconfig_gfx_openh264_param) *
           NUM_CONNECTION_TYPES);
    return og;

}

/*****************************************************************************/
int
xrdp_encoder_openh264_delete(void *handle)
{
    struct openh264_global *og;
    struct openh264_encoder *oe;
    int index;

    if (handle == NULL)
    {
        return 0;
    }
    og = (struct openh264_global *) handle;
    for (index = 0; index < 16; index++)
    {
        oe = &(og->encoders[index]);
        if (oe->openh264_enc_han != NULL)
        {
            WelsDestroySVCEncoder(oe->openh264_enc_han);
        }
        g_free(oe->yuvdata);
    }
    g_free(og);
    return 0;
}

/*****************************************************************************/
int
xrdp_encoder_openh264_encode(void *handle, int session, int left, int top,
                             int width, int height, int twidth, int theight,
                             int format, const char *data,
                             short *crects, int num_crects,
                             char *cdata, int *cdata_bytes,
                             int connection_type, int *flags_ptr)
{
    struct openh264_global *og;
    struct openh264_encoder *oe;
    const char *src8;
    const char *src8a;
    char *dst8;
    char *dst8a;
    char *dst8b;
    char *dst8c;
    int index;
    int jndex;
    int flags;
    int x;
    int y;
    int cx;
    int cy;
    int ct; /* connection_type */
    SSourcePicture pic1;
    SFrameBSInfo info;
    SLayerBSInfo *layer;
    SEncParamExt encParamExt;
    SSpatialLayerConfig *slc;
    int status;
    int layer_position;
    char *write_location;
    unsigned char *payload;
    int size;
    int lcdata_bytes;

    LOG(LOG_LEVEL_TRACE, "xrdp_encoder_openh264_encode:");
    flags = 0;
    og = (struct openh264_global *) handle;
    oe = &(og->encoders[session % OPENH264_MAX_ENCODERS]);
    /* validate connection type */
    ct = connection_type;
    if (ct > CONNECTION_TYPE_LAN || ct < CONNECTION_TYPE_MODEM)
    {
        ct = CONNECTION_TYPE_LAN;
    }
    if ((oe->openh264_enc_han == NULL) ||
            (oe->width != width) || (oe->height != height))
    {
        if (oe->openh264_enc_han != NULL)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_encoder_openh264_encode: "
                "WelsDestroySVCEncoder %p", oe->openh264_enc_han);
            WelsDestroySVCEncoder(oe->openh264_enc_han);
            oe->openh264_enc_han = NULL;
            g_free(oe->yuvdata);
            oe->yuvdata = NULL;
            flags |= 2;
        }
        if ((width > 0) && (height > 0))
        {
            status = WelsCreateSVCEncoder(&oe->openh264_enc_han);
            if ((status != 0) || (oe->openh264_enc_han == NULL))
            {
                LOG(LOG_LEVEL_ERROR, "Failed to create H.264 encoder");
                return 1;
            }
            LOG(LOG_LEVEL_INFO, "xrdp_encoder_openh264_encode: "
                "WelsCreateSVCEncoder rv %p for width %d height %d",
                oe->openh264_enc_han, width, height);
            status = ENC_GET_DEFAULT_PARAMS(
                         oe->openh264_enc_han, &encParamExt);
            LOG(LOG_LEVEL_INFO, "xrdp_encoder_openh264_encode: "
                "GetDefaultParams rv %d", status);
            if (status == 0)
            {
                encParamExt.iUsageType = CAMERA_VIDEO_REAL_TIME;
                encParamExt.iPicWidth = (width + 15) & ~15;
                encParamExt.iPicHeight = (height + 15) & ~15;
                encParamExt.iRCMode = RC_BITRATE_MODE;
                encParamExt.iSpatialLayerNum = 1;
                /* Set encode parameters from config */
                encParamExt.bEnableFrameSkip = og->openh264_param[ct].EnableFrameSkip;
                encParamExt.iTargetBitrate = og->openh264_param[ct].TargetBitrate;
                encParamExt.iMaxBitrate = og->openh264_param[ct].MaxBitrate;
                encParamExt.fMaxFrameRate = og->openh264_param[ct].MaxFrameRate;
                /* defaults to INCREASING_ID, Mac client needs CONSTANT_ID */
                encParamExt.eSpsPpsIdStrategy = CONSTANT_ID;
                slc = encParamExt.sSpatialLayers + 0;
                slc->fFrameRate = encParamExt.fMaxFrameRate;
                slc->iVideoWidth = encParamExt.iPicWidth;
                slc->iVideoHeight = encParamExt.iPicHeight;
                slc->iSpatialBitrate = encParamExt.iTargetBitrate;
                slc->iMaxSpatialBitrate = encParamExt.iMaxBitrate;
                status = ENC_INITIALIZE_EXT(
                             oe->openh264_enc_han, &encParamExt);
                LOG(LOG_LEVEL_INFO, "xrdp_encoder_openh264_encode: "
                    "InitializeExt rv %d", status);
            }
            oe->yuvdata = g_new(char, (width + 16) * (height + 16) * 2);
            if (oe->yuvdata == NULL)
            {
                WelsDestroySVCEncoder(oe->openh264_enc_han);
                oe->openh264_enc_han = NULL;
                return 2;
            }
            flags |= 1;
        }
        oe->width = width;
        oe->height = height;
    }

    if ((data != NULL) && (oe->openh264_enc_han != NULL))
    {
        g_memset(&pic1, 0, sizeof(pic1));
        pic1.iPicWidth = (width + 15) & ~15;
        pic1.iPicHeight = (height + 15) & ~15;
        pic1.iColorFormat = videoFormatI420;
        pic1.iStride[0] = pic1.iPicWidth;
        pic1.iStride[1] = pic1.iPicWidth / 2;
        pic1.iStride[2] = pic1.iPicWidth / 2;
        pic1.pData[0] = (unsigned char *) (oe->yuvdata);
        pic1.pData[1] = pic1.pData[0] + pic1.iPicWidth * pic1.iPicHeight;
        pic1.pData[2] = pic1.pData[1] + (pic1.iPicWidth / 2) *
                        (pic1.iPicHeight / 2);
        for (index = 0; index < num_crects; index++)
        {
            src8 = data;
            dst8 = (char *) (pic1.pData[0]);
            x = crects[index * 4 + 0];
            y = crects[index * 4 + 1];
            cx = crects[index * 4 + 2];
            cy = crects[index * 4 + 3];
            LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_openh264_encode: "
                      "x %d y %d cx %d cy %d", x, y, cx, cy);
            src8 += twidth * y + x;
            dst8 += pic1.iStride[0] * (y - top) + (x - left);
            for (; cy > 0; cy -= 1)
            {
                g_memcpy(dst8, src8, cx);
                src8 += twidth;
                dst8 += pic1.iStride[0];
            }
        }
        for (index = 0; index < num_crects; index++)
        {
            src8 = data; /* uv */
            src8 += twidth * theight;
            dst8 = (char *) (pic1.pData[1]); /* u */
            dst8a = (char *) (pic1.pData[2]); /* v */
            x = crects[index * 4 + 0];
            y = crects[index * 4 + 1];
            cx = crects[index * 4 + 2];
            cy = crects[index * 4 + 3];
            src8 += twidth * (y / 2) + x;
            dst8 += pic1.iStride[1] * ((y - top) / 2) + ((x - left) / 2);
            dst8a += pic1.iStride[2] * ((y - top) / 2) + ((x - left) / 2);
            for (; cy > 0; cy -= 2)
            {
                src8a = src8; /* uv */
                dst8b = dst8; /* u */
                dst8c = dst8a; /* v */
                for (jndex = 0; jndex < cx; jndex += 2)
                {
                    *(dst8b++) = *(src8a++); /* u */
                    *(dst8c++) = *(src8a++); /* v */
                }
                src8 += twidth; /* uv */
                dst8 += pic1.iStride[1]; /* u */
                dst8a += pic1.iStride[2]; /* v */
            }
        }
        g_memset(&info, 0, sizeof(info));
        status = ENC_ENCODE_FRAME(oe->openh264_enc_han, &pic1, &info);
        if (status != 0)
        {
            LOG(LOG_LEVEL_TRACE, "OpenH264: Failed to encode frame");
            return 3;
        }
        if (info.eFrameType == videoFrameTypeSkip)
        {
            LOG(LOG_LEVEL_TRACE, "OpenH264: frame was skipped!");
            return 4;
        }
        lcdata_bytes = 0;
        for (index = 0; index < info.iLayerNum; index++)
        {
            layer_position = 0;
            layer = info.sLayerInfo + index;
            for (jndex = 0; jndex < layer->iNalCount; jndex++)
            {
                write_location = cdata + lcdata_bytes;
                payload = layer->pBsBuf + layer_position;
                size = layer->pNalLengthInByte[jndex];
                if (lcdata_bytes + size > *cdata_bytes)
                {
                    LOG(LOG_LEVEL_INFO, "out of room");
                    return 5;
                }
                g_memcpy(write_location, payload, size);
                layer_position += size;
                lcdata_bytes += size;
            }
        }
        *cdata_bytes = lcdata_bytes;
    }
    if (flags_ptr != NULL)
    {
        *flags_ptr = flags;
    }
    return 0;
}

/*****************************************************************************/
int
xrdp_encoder_openh264_install_ok(void)
{
    int rv;

    // Declare something with maximal alignment we can take the address
    // of to pass to WelsCreateSVCEncoder. This object is not directly
    // accessed.
    //
    // Note we can't use the ISVCEncoder type directly, as in C++ this
    // is an abstract class.
    long double dummy;

    ISVCEncoder *p = (ISVCEncoder *)&dummy;

    // The real OpenH264 library will ALWAYS change the value of the
    // passed-in pointer
    // The noopenh264 library will NEVER change the value of the passed-in
    // pointer
    // For both libraries, the relevant source is in
    // codec/encoder/plus/src/welsEncoderExt.cpp
    WelsCreateSVCEncoder(&p);
    rv = (p != (ISVCEncoder *)&dummy); // Did the passed-in value change
    // If p is &dummy or NULL, this call does nothing, otherwise resources
    // are deallocated.
    WelsDestroySVCEncoder(p);

    return rv;
}
