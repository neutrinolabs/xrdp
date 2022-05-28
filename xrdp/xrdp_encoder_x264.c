/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2016
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
 * x264 Encoder
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <x264.h>

#include "xrdp.h"
#include "arch.h"
#include "os_calls.h"
#include "xrdp_encoder_x264.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do \
    { \
        if (_level < LLOG_LEVEL) \
        { \
            g_write("xrdp:xrdp_encoder_x264 [%10.10u]: ", g_time3()); \
            g_writeln _args ; \
        } \
    } \
    while (0)

struct x264_encoder
{
    x264_t *x264_enc_han;
    char *yuvdata;
    x264_param_t x264_params;
    int width;
    int height;
};

struct x264_global
{
    struct x264_encoder encoders[16];
};

/*****************************************************************************/
void *
xrdp_encoder_x264_create(void)
{
    struct x264_global *xg;

    LLOGLN(0, ("xrdp_encoder_x264_create:"));
    xg = (struct x264_global *) g_malloc(sizeof(struct x264_global), 1);
    if (xg == 0)
    {
        return 0;
    }
    return xg;
}

/*****************************************************************************/
int
xrdp_encoder_x264_delete(void *handle)
{
    struct x264_global *xg;
    struct x264_encoder *xe;
    int index;

    if (handle == 0)
    {
        return 0;
    }
    xg = (struct x264_global *) handle;
    for (index = 0; index < 16; index++)
    {
        xe = &(xg->encoders[index]);
        if (xe->x264_enc_han != 0)
        {
            x264_encoder_close(xe->x264_enc_han);
        }
        g_free(xe->yuvdata);
    }
    g_free(xg);
    return 0;
}

/*****************************************************************************/
int
xrdp_encoder_x264_encode(void *handle, int session,
                         int width, int height, int format, const char *data,
                         char *cdata, int *cdata_bytes)
{
    struct x264_global *xg;
    struct x264_encoder *xe;
    const char *src8;
    char *dst8;
    int index;
    x264_nal_t *nals;
    int num_nals;
    int frame_size;
    int frame_area;

    x264_picture_t pic_in;
    x264_picture_t pic_out;

    LOG(LOG_LEVEL_TRACE, "xrdp_encoder_x264_encode:");
    xg = (struct x264_global *) handle;
    xe = &(xg->encoders[session]);
    if ((xe->x264_enc_han == 0) || (xe->width != width) || (xe->height != height))
    {
        if (xe->x264_enc_han != 0)
        {
            x264_encoder_close(xe->x264_enc_han);
            xe->x264_enc_han = 0;
            g_free(xe->yuvdata);
            xe->yuvdata = 0;
        }
        if ((width > 0) && (height > 0))
        {
            //x264_param_default_preset(&(xe->x264_params), "superfast", "zerolatency");
            x264_param_default_preset(&(xe->x264_params), "ultrafast", "zerolatency");
            xe->x264_params.i_threads = 1;
            xe->x264_params.i_width = width;
            xe->x264_params.i_height = height;
            xe->x264_params.i_fps_num = 24;
            xe->x264_params.i_fps_den = 1;
            //xe->x264_params.b_cabac = 1;
            //xe->x264_params.i_bframe = 0;
            //xe->x264_params.rc.i_rc_method = X264_RC_CQP;
            //xe->x264_params.rc.i_qp_constant = 23;
            //x264_param_apply_profile(&(xe->x264_params), "high");
            x264_param_apply_profile(&(xe->x264_params), "main");
            //x264_param_apply_profile(&(xe->x264_params), "baseline");
            xe->x264_enc_han = x264_encoder_open(&(xe->x264_params));
            if (xe->x264_enc_han == 0)
            {
                return 1;
            }
            xe->yuvdata = (char *) g_malloc(width * height * 2, 0);
            if (xe->yuvdata == 0)
            {
                x264_encoder_close(xe->x264_enc_han);
                xe->x264_enc_han = 0;
                return 2;
            }
        }
        xe->width = width;
        xe->height = height;
    }

    if ((data != 0) && (xe->x264_enc_han != 0))
    {
        src8 = data;
        dst8 = xe->yuvdata;
        for (index = 0; index < height; index++)
        {
            g_memcpy(dst8, src8, width);
            src8 += width;
            dst8 += xe->x264_params.i_width;
        }

        src8 = data;
        src8 += width * height;
        dst8 = xe->yuvdata;

        frame_area = xe->x264_params.i_width * xe->x264_params.i_height - 1;
        dst8 += frame_area;
        for (index = 0; index < height; index += 2)
        {
            g_memcpy(dst8, src8, width);
            src8 += width;
            dst8 += xe->x264_params.i_width;
        }

        g_memset(&pic_in, 0, sizeof(pic_in));
        pic_in.img.i_csp = X264_CSP_NV12;
        pic_in.img.i_plane = 2;
        pic_in.img.plane[0] = (unsigned char *) (xe->yuvdata);
        pic_in.img.plane[1] = (unsigned char *) (xe->yuvdata + frame_area);
        pic_in.img.i_stride[0] = width;
        pic_in.img.i_stride[1] = xe->x264_params.i_width;
        num_nals = 0;
        frame_size = x264_encoder_encode(xe->x264_enc_han, &nals, &num_nals,
                                         &pic_in, &pic_out);
        LLOGLN(10, ("i_type %d", pic_out.i_type));
        if (frame_size < 1)
        {
            return 3;
        }
        if (*cdata_bytes < frame_size)
        {
            return 4;
        }
        g_memcpy(cdata, nals[0].p_payload, frame_size);
        *cdata_bytes = frame_size;
    }
    return 0;
}
