/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2016-2024
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
#include "xrdp_tconfig.h"

#define X264_MAX_ENCODERS 16

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
    struct x264_encoder encoders[X264_MAX_ENCODERS];
    struct xrdp_tconfig_gfx_x264_param x264_param[NUM_CONNECTION_TYPES];
};

/*****************************************************************************/
void *
xrdp_encoder_x264_create(void)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_encoder_x264_create:");

    struct x264_global *xg;
    struct xrdp_tconfig_gfx gfxconfig;
    xg = g_new0(struct x264_global, 1);
    tconfig_load_gfx(GFX_CONF, &gfxconfig);

    memcpy(&xg->x264_param, &gfxconfig.x264_param,
           sizeof(struct xrdp_tconfig_gfx_x264_param) * NUM_CONNECTION_TYPES);

    return xg;

}

/*****************************************************************************/
int
xrdp_encoder_x264_delete(void *handle)
{
    struct x264_global *xg;
    struct x264_encoder *xe;
    int index;

    if (handle == NULL)
    {
        return 0;
    }
    xg = (struct x264_global *) handle;
    for (index = 0; index < 16; index++)
    {
        xe = &(xg->encoders[index]);
        if (xe->x264_enc_han != NULL)
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
xrdp_encoder_x264_encode(void *handle, int session, int left, int top,
                         int width, int height, int twidth, int theight,
                         int format, const char *data,
                         short *crects, int num_crects,
                         char *cdata, int *cdata_bytes, int connection_type,
                         int *flags_ptr)
{
    struct x264_global *xg;
    struct x264_encoder *xe;
    const char *src8;
    char *dst8;
    int index;
    x264_nal_t *nals;
    int num_nals;
    int frame_size;
    int x264_width_height;
    int flags;
    int x;
    int y;
    int cx;
    int cy;
    int ct; /* connection_type */

    x264_picture_t pic_in;
    x264_picture_t pic_out;

    LOG(LOG_LEVEL_TRACE, "xrdp_encoder_x264_encode:");
    flags = 0;
    xg = (struct x264_global *) handle;
    xe = &(xg->encoders[session % X264_MAX_ENCODERS]);

    /* validate connection type */
    ct = connection_type;
    if (ct > CONNECTION_TYPE_LAN || ct < CONNECTION_TYPE_MODEM)
    {
        ct = CONNECTION_TYPE_LAN;
    }

    if ((xe->x264_enc_han == NULL) ||
            (xe->width != width) || (xe->height != height))
    {
        if (xe->x264_enc_han != NULL)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_encoder_x264_encode: "
                "x264_encoder_close %p", xe->x264_enc_han);
            x264_encoder_close(xe->x264_enc_han);
            xe->x264_enc_han = NULL;
            g_free(xe->yuvdata);
            xe->yuvdata = NULL;
            flags |= 2;
        }
        if ((width > 0) && (height > 0))
        {
            x264_param_default_preset(&(xe->x264_params),
                                      xg->x264_param[ct].preset,
                                      xg->x264_param[ct].tune);
            xe->x264_params.i_threads = xg->x264_param[ct].threads;
            xe->x264_params.i_width = (width + 15) & ~15;
            xe->x264_params.i_height = (height + 15) & ~15;
            xe->x264_params.i_fps_num = xg->x264_param[ct].fps_num;
            xe->x264_params.i_fps_den = xg->x264_param[ct].fps_den;
            xe->x264_params.rc.i_rc_method = X264_RC_CRF;
            xe->x264_params.rc.i_vbv_max_bitrate = xg->x264_param[ct].vbv_max_bitrate;
            xe->x264_params.rc.i_vbv_buffer_size = xg->x264_param[ct].vbv_buffer_size;
            x264_param_apply_profile(&(xe->x264_params),
                                     xg->x264_param[ct].profile);
            xe->x264_enc_han = x264_encoder_open(&(xe->x264_params));
            LOG(LOG_LEVEL_INFO, "xrdp_encoder_x264_encode: "
                "x264_encoder_open rv %p for width %d height %d",
                xe->x264_enc_han, width, height);
            if (xe->x264_enc_han == NULL)
            {
                return 1;
            }
            xe->yuvdata = g_new(char, width * height * 2);
            if (xe->yuvdata == NULL)
            {
                x264_encoder_close(xe->x264_enc_han);
                xe->x264_enc_han = NULL;
                return 2;
            }
            flags |= 1;
        }
        xe->width = width;
        xe->height = height;
    }

    if ((data != NULL) && (xe->x264_enc_han != NULL))
    {
        x264_width_height = xe->x264_params.i_width * xe->x264_params.i_height;
        for (index = 0; index < num_crects; index++)
        {
            src8 = data;
            dst8 = xe->yuvdata;
            x = crects[index * 4 + 0];
            y = crects[index * 4 + 1];
            cx = crects[index * 4 + 2];
            cy = crects[index * 4 + 3];
            LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_x264_encode: x %d y %d "
                      "cx %d cy %d", x, y, cx, cy);
            src8 += twidth * y + x;
            dst8 += xe->x264_params.i_width * (y - top) + (x - left);
            for (; cy > 0; cy -= 1)
            {
                g_memcpy(dst8, src8, cx);
                src8 += twidth;
                dst8 += xe->x264_params.i_width;
            }
        }
        for (index = 0; index < num_crects; index++)
        {
            src8 = data;
            src8 += twidth * theight;
            dst8 = xe->yuvdata;
            dst8 += x264_width_height;
            x = crects[index * 4 + 0];
            y = crects[index * 4 + 1];
            cx = crects[index * 4 + 2];
            cy = crects[index * 4 + 3];
            src8 += twidth * (y / 2) + x;
            dst8 += xe->x264_params.i_width * ((y - top) / 2) + (x - left);
            for (; cy > 0; cy -= 2)
            {
                g_memcpy(dst8, src8, cx);
                src8 += twidth;
                dst8 += xe->x264_params.i_width;
            }
        }
        g_memset(&pic_in, 0, sizeof(pic_in));
        pic_in.img.i_csp = X264_CSP_NV12;
        pic_in.img.i_plane = 2;
        pic_in.img.plane[0] = (unsigned char *) (xe->yuvdata);
        pic_in.img.plane[1] = (unsigned char *)
                              (xe->yuvdata + x264_width_height);
        pic_in.img.i_stride[0] = xe->x264_params.i_width;
        pic_in.img.i_stride[1] = xe->x264_params.i_width;
        num_nals = 0;
        frame_size = x264_encoder_encode(xe->x264_enc_han, &nals, &num_nals,
                                         &pic_in, &pic_out);
        LOG(LOG_LEVEL_TRACE, "i_type %d", pic_out.i_type);
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
    if (flags_ptr != NULL)
    {
        *flags_ptr = flags;
    }
    return 0;
}
