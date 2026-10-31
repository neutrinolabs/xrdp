/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Tim Pletcher 2026
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

/* VA-API H264 encoder
 * libva allocates the NV12 input surface, its Y and UV planes are
 * imported into GL as R8 and GR88 textures and the shaders render into
 * them, no copy
 * SPS, PPS and slice headers are written here as packed headers */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_enc_h264.h>

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xrdp_accel_assist.h"
#include "xrdp_accel_assist_x11.h"
#include "xrdp_accel_assist_vaapi.h"
#include "log.h"

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

/* drm_fourcc.h values */
#define XH_FOURCC(a, b, c, d) \
    ((unsigned int) (a) | ((unsigned int) (b) << 8) | \
     ((unsigned int) (c) << 16) | ((unsigned int) (d) << 24))
#define XH_DRM_FORMAT_R8 XH_FOURCC('R', '8', ' ', ' ')
#define XH_DRM_FORMAT_GR88 XH_FOURCC('G', 'R', '8', '8')
#define XH_DRM_FORMAT_MOD_LINEAR 0ULL
/* no explicit modifier, the layout is in the buffer, drivers without
   modifier support, radeonsi before GFX9 */
#define XH_DRM_FORMAT_MOD_INVALID 0x00ffffffffffffffULL

#define XH_MAX_MODIFIERS 64

#define XH_VAAPI_DEFAULT_QP 28
/* weaker deblocking, the default darkens the white pixel next to a thin
   black line at macroblock edges */
#define XH_VAAPI_DEBLOCK_OFFSET -6

extern EGLDisplay g_egl_display; /* in xrdp_accel_assist_egl.c */

static int g_drm_fd = -1;
static VADisplay g_va_dpy = NULL;

static VAProfile g_va_profile = VAProfileH264High;
static VAEntrypoint g_va_entrypoint = VAEntrypointEncSliceLP;
static int g_profile_idc = 100;
static int g_transform_8x8 = 1;

/* kept across encoders, two IDRs in a row must not share an id */
static int g_idr_pic_id = 0;

struct va_candidate
{
    VAProfile profile;
    VAEntrypoint entrypoint;
    int profile_idc;
    int transform_8x8;
    const char *name;
};

/* first one the driver has wins, no baseline, we use CABAC */
static const struct va_candidate g_va_candidates[] =
{
    { VAProfileH264High, VAEntrypointEncSliceLP, 100, 1, "High/EncSliceLP" },
    { VAProfileH264High, VAEntrypointEncSlice,   100, 1, "High/EncSlice"   },
    { VAProfileH264Main, VAEntrypointEncSliceLP,  77, 0, "Main/EncSliceLP" },
    { VAProfileH264Main, VAEntrypointEncSlice,    77, 0, "Main/EncSlice"   }
};
#define XH_VA_NUM_CANDIDATES \
    ((int) (sizeof(g_va_candidates) / sizeof(g_va_candidates[0])))

struct enc_info
{
    int width;
    int height;
    int frameCount;
    int enc_width; /* coded size, the SPS crops to width x height */
    int enc_height;
    int qp;
    int rc_mode;
    int bitrate_kbps;
    int frame_num;
    int poc;
    int have_ref;
    int cur_recon;
    VAPictureH264 ref_pic;
    EGLImageKHR egl_image[2];
    GLuint plane_tex[2];
    VASurfaceID input_surface;
    VASurfaceID recon_surfaces[2];
    VAConfigID config;
    VAContextID context;
    VABufferID coded_buf;
};

struct bitstream
{
    unsigned char *p;
    int bitpos;
};

/*****************************************************************************/
static void
bs_put(struct bitstream *b, int nbits, unsigned int value)
{
    int index;

    for (index = nbits - 1; index >= 0; index--)
    {
        if ((value >> index) & 1)
        {
            b->p[b->bitpos >> 3] |= 1 << (7 - (b->bitpos & 7));
        }
        b->bitpos++;
    }
}

/*****************************************************************************/
/* unsigned exp golomb */
static void
bs_ue(struct bitstream *b, unsigned int value)
{
    int nbits;
    unsigned int t;

    nbits = 0;
    t = value + 1;
    while (t > 1)
    {
        t >>= 1;
        nbits++;
    }
    bs_put(b, nbits, 0);
    bs_put(b, nbits + 1, value + 1);
}

/*****************************************************************************/
/* signed exp golomb */
static void
bs_se(struct bitstream *b, int value)
{
    if (value <= 0)
    {
        bs_ue(b, (unsigned int) (-2 * value));
    }
    else
    {
        bs_ue(b, (unsigned int) (2 * value - 1));
    }
}

/*****************************************************************************/
static void
bs_trailing(struct bitstream *b)
{
    bs_put(b, 1, 1);
    while (b->bitpos & 7)
    {
        bs_put(b, 1, 0);
    }
}

/*****************************************************************************/
/* start code, nal header and rbsp with emulation prevention */
static int
build_nal(unsigned char *out, int nal_ref_idc, int nal_unit_type,
          const unsigned char *rbsp, int rbsp_bytes)
{
    int index;
    int out_bytes;
    int zeros;

    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 1;
    out[4] = (nal_ref_idc << 5) | nal_unit_type;
    out_bytes = 5;
    zeros = 0;
    for (index = 0; index < rbsp_bytes; index++)
    {
        if ((zeros >= 2) && (rbsp[index] <= 3))
        {
            out[out_bytes++] = 3;
            zeros = 0;
        }
        out[out_bytes++] = rbsp[index];
        zeros = (rbsp[index] == 0) ? zeros + 1 : 0;
    }
    return out_bytes;
}

/*****************************************************************************/
/* H264 table A-1, from the macroblock count at 60 fps */
static int
get_level_idc(int width, int height)
{
    static const int levels[][3] =
    {
        /* level_idc, max frame size, max macroblocks per second */
        { 40,   8192,   245760 },
        { 42,   8704,   522240 },
        { 50,  22080,   589824 },
        { 51,  36864,   983040 },
        { 52,  36864,  2073600 },
        { 60, 139264,  4177920 },
        { 61, 139264,  8355840 },
        { 62, 139264, 16711680 }
    };
    int mbs;
    int index;

    mbs = ((width + 15) / 16) * ((height + 15) / 16);
    for (index = 0; index < (int) (sizeof(levels) / sizeof(levels[0]));
            index++)
    {
        if ((mbs <= levels[index][1]) && (mbs * 60 <= levels[index][2]))
        {
            return levels[index][0];
        }
    }
    return 62;
}

/*****************************************************************************/
static int
build_sps(unsigned char *out, struct enc_info *ei)
{
    unsigned char rbsp[256];
    struct bitstream b;
    int crop_right;
    int crop_bottom;

    /* in 2 pixel units for 4:2:0 */
    crop_right = (ei->enc_width - ei->width) / 2;
    crop_bottom = (ei->enc_height - ei->height) / 2;
    g_memset(rbsp, 0, sizeof(rbsp));
    b.p = rbsp;
    b.bitpos = 0;
    bs_put(&b, 8, g_profile_idc);
    bs_put(&b, 8, 0);           /* constraint flags */
    bs_put(&b, 8, get_level_idc(ei->enc_width, ei->enc_height));
    bs_ue(&b, 0);               /* seq_parameter_set_id */
    if (g_profile_idc >= 100)
    {
        bs_ue(&b, 1);           /* chroma_format_idc 4:2:0 */
        bs_ue(&b, 0);           /* bit_depth_luma_minus8 */
        bs_ue(&b, 0);           /* bit_depth_chroma_minus8 */
        bs_put(&b, 1, 0);       /* qpprime_y_zero_transform_bypass_flag */
        bs_put(&b, 1, 0);       /* seq_scaling_matrix_present_flag */
    }
    bs_ue(&b, 0);               /* log2_max_frame_num_minus4 */
    bs_ue(&b, 2);               /* pic_order_cnt_type */
    bs_ue(&b, 1);               /* max_num_ref_frames */
    bs_put(&b, 1, 0);           /* gaps_in_frame_num_value_allowed_flag */
    bs_ue(&b, ei->enc_width / 16 - 1);
    bs_ue(&b, ei->enc_height / 16 - 1);
    bs_put(&b, 1, 1);           /* frame_mbs_only_flag */
    bs_put(&b, 1, 1);           /* direct_8x8_inference_flag */
    if ((crop_right != 0) || (crop_bottom != 0))
    {
        bs_put(&b, 1, 1);       /* frame_cropping_flag */
        bs_ue(&b, 0);
        bs_ue(&b, crop_right);
        bs_ue(&b, 0);
        bs_ue(&b, crop_bottom);
    }
    else
    {
        bs_put(&b, 1, 0);       /* frame_cropping_flag */
    }
    bs_put(&b, 1, 1);           /* vui_parameters_present_flag */
    bs_put(&b, 1, 0);           /* aspect_ratio_info_present_flag */
    bs_put(&b, 1, 0);           /* overscan_info_present_flag */
    /* bt709 full range, what the shader writes, a decoder that assumes
       limited range crushes black and white */
    bs_put(&b, 1, 1);           /* video_signal_type_present_flag */
    bs_put(&b, 3, 5);           /* video_format unspecified */
    bs_put(&b, 1, 1);           /* video_full_range_flag */
    bs_put(&b, 1, 1);           /* colour_description_present_flag */
    bs_put(&b, 8, 1);           /* colour_primaries */
    bs_put(&b, 8, 1);           /* transfer_characteristics */
    bs_put(&b, 8, 1);           /* matrix_coefficients */
    bs_put(&b, 1, 0);           /* chroma_loc_info_present_flag */
    bs_put(&b, 1, 0);           /* timing_info_present_flag */
    bs_put(&b, 1, 0);           /* nal_hrd_parameters_present_flag */
    bs_put(&b, 1, 0);           /* vcl_hrd_parameters_present_flag */
    bs_put(&b, 1, 0);           /* pic_struct_present_flag */
    /* no reordering, so a decoder outputs each frame right away */
    bs_put(&b, 1, 1);           /* bitstream_restriction_flag */
    bs_put(&b, 1, 1);           /* motion_vectors_over_pic_boundaries_flag */
    bs_ue(&b, 0);               /* max_bytes_per_pic_denom */
    bs_ue(&b, 0);               /* max_bits_per_mb_denom */
    bs_ue(&b, 16);              /* log2_max_mv_length_horizontal */
    bs_ue(&b, 16);              /* log2_max_mv_length_vertical */
    bs_ue(&b, 0);               /* max_num_reorder_frames */
    bs_ue(&b, 1);               /* max_dec_frame_buffering */
    bs_trailing(&b);
    return build_nal(out, 3, 7, rbsp, b.bitpos / 8);
}

/*****************************************************************************/
static int
build_pps(unsigned char *out, struct enc_info *ei)
{
    unsigned char rbsp[64];
    struct bitstream b;

    g_memset(rbsp, 0, sizeof(rbsp));
    b.p = rbsp;
    b.bitpos = 0;
    bs_ue(&b, 0);               /* pic_parameter_set_id */
    bs_ue(&b, 0);               /* seq_parameter_set_id */
    bs_put(&b, 1, 1);           /* entropy_coding_mode_flag, CABAC */
    bs_put(&b, 1, 0);           /* bottom_field_pic_order_present_flag */
    bs_ue(&b, 0);               /* num_slice_groups_minus1 */
    bs_ue(&b, 0);               /* num_ref_idx_l0_default_active_minus1 */
    bs_ue(&b, 0);               /* num_ref_idx_l1_default_active_minus1 */
    bs_put(&b, 1, 0);           /* weighted_pred_flag */
    bs_put(&b, 2, 0);           /* weighted_bipred_idc */
    bs_se(&b, ei->qp - 26);     /* pic_init_qp_minus26 */
    bs_se(&b, 0);               /* pic_init_qs_minus26 */
    bs_se(&b, 0);               /* chroma_qp_index_offset */
    bs_put(&b, 1, 1);           /* deblocking_filter_control_present_flag */
    bs_put(&b, 1, 0);           /* constrained_intra_pred_flag */
    bs_put(&b, 1, 0);           /* redundant_pic_cnt_present_flag */
    if (g_transform_8x8)
    {
        bs_put(&b, 1, 1);       /* transform_8x8_mode_flag */
        bs_put(&b, 1, 0);       /* pic_scaling_matrix_present_flag */
        bs_se(&b, 0);           /* second_chroma_qp_index_offset */
    }
    bs_trailing(&b);
    return build_nal(out, 3, 8, rbsp, b.bitpos / 8);
}

/*****************************************************************************/
/* returns length in bits, the driver adds the emulation prevention bytes
   since the slice data follows */
static int
build_slice_header(unsigned char *out, struct enc_info *ei, int is_idr,
                   int slice_type, int idr_pic_id)
{
    unsigned char rbsp[64];
    struct bitstream b;
    int index;

    g_memset(rbsp, 0, sizeof(rbsp));
    b.p = rbsp;
    b.bitpos = 0;
    bs_ue(&b, 0);               /* first_mb_in_slice */
    bs_ue(&b, slice_type);      /* 0 P, 2 I */
    bs_ue(&b, 0);               /* pic_parameter_set_id */
    bs_put(&b, 4, ei->frame_num & 15);
    if (is_idr)
    {
        bs_ue(&b, idr_pic_id);
    }
    if (slice_type == 0)
    {
        bs_put(&b, 1, 1);       /* num_ref_idx_active_override_flag */
        bs_ue(&b, 0);           /* num_ref_idx_l0_active_minus1 */
        bs_put(&b, 1, 0);       /* ref_pic_list_modification_flag_l0 */
    }
    if (is_idr)
    {
        bs_put(&b, 1, 0);       /* no_output_of_prior_pics_flag */
        bs_put(&b, 1, 0);       /* long_term_reference_flag */
    }
    else
    {
        bs_put(&b, 1, 0);       /* adaptive_ref_pic_marking_mode_flag */
    }
    if (slice_type != 2)
    {
        bs_ue(&b, 0);           /* cabac_init_idc */
    }
    bs_se(&b, 0);               /* slice_qp_delta */
    bs_ue(&b, 0);               /* disable_deblocking_filter_idc */
    bs_se(&b, XH_VAAPI_DEBLOCK_OFFSET); /* slice_alpha_c0_offset_div2 */
    bs_se(&b, XH_VAAPI_DEBLOCK_OFFSET); /* slice_beta_offset_div2 */
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 1;
    out[4] = (3 << 5) | (is_idr ? 5 : 1);
    for (index = 0; index < (b.bitpos + 7) / 8; index++)
    {
        out[5 + index] = rbsp[index];
    }
    return 5 * 8 + b.bitpos;
}

/*****************************************************************************/
/* returns 1 if the driver has the profile / entrypoint with constant QP
   and packed headers */
static int
xrdp_accel_assist_vaapi_probe(VAProfile profile, VAEntrypoint entrypoint)
{
    VAEntrypoint *entrypoints;
    VAConfigAttrib attrib[2];
    int num_entrypoints;
    int index;
    int found;

    entrypoints = g_new(VAEntrypoint, vaMaxNumEntrypoints(g_va_dpy));
    if (entrypoints == NULL)
    {
        return 0;
    }
    found = 0;
    if (vaQueryConfigEntrypoints(g_va_dpy, profile, entrypoints,
                                 &num_entrypoints) == VA_STATUS_SUCCESS)
    {
        for (index = 0; index < num_entrypoints; index++)
        {
            if (entrypoints[index] == entrypoint)
            {
                found = 1;
                break;
            }
        }
    }
    g_free(entrypoints);
    if (!found)
    {
        return 0;
    }
    g_memset(attrib, 0, sizeof(attrib));
    attrib[0].type = VAConfigAttribRateControl;
    attrib[1].type = VAConfigAttribEncPackedHeaders;
    if (vaGetConfigAttributes(g_va_dpy, profile, entrypoint,
                              attrib, 2) != VA_STATUS_SUCCESS)
    {
        return 0;
    }
    if ((attrib[0].value == VA_ATTRIB_NOT_SUPPORTED) ||
            ((attrib[0].value & VA_RC_CQP) == 0) ||
            (attrib[1].value == VA_ATTRIB_NOT_SUPPORTED) ||
            ((attrib[1].value & VA_ENC_PACKED_HEADER_SEQUENCE) == 0) ||
            ((attrib[1].value & VA_ENC_PACKED_HEADER_PICTURE) == 0) ||
            ((attrib[1].value & VA_ENC_PACKED_HEADER_SLICE) == 0))
    {
        return 0;
    }
    return 1;
}

/*****************************************************************************/
/* the render node EGL is on, the encoder must use the same GPU */
static const char *
xrdp_accel_assist_vaapi_get_egl_node(void)
{
    EGLAttrib attr;

    if (!epoxy_has_egl_extension(EGL_NO_DISPLAY, "EGL_EXT_device_query"))
    {
        return NULL;
    }
    attr = 0;
    if (!eglQueryDisplayAttribEXT(g_egl_display, EGL_DEVICE_EXT, &attr) ||
            (attr == 0))
    {
        return NULL;
    }
    return eglQueryDeviceStringEXT((EGLDeviceEXT) attr,
                                   EGL_DRM_RENDER_NODE_FILE_EXT);
}

/*****************************************************************************/
int
xrdp_accel_assist_vaapi_init(void)
{
    const char *node;
    const char *egl_node;
    VAStatus va_status;
    int major;
    int minor;
    int index;

    /* this env var can be added / changed in sesman.ini SessionVariables
       example
       XRDP_VAAPI_DEVICE=/dev/dri/renderD129
       default is the render node EGL is on */
    egl_node = xrdp_accel_assist_vaapi_get_egl_node();
    node = g_getenv("XRDP_VAAPI_DEVICE");
    if (node == NULL)
    {
        node = (egl_node != NULL) ? egl_node : "/dev/dri/renderD128";
    }
    else if ((egl_node != NULL) && (g_strcmp(node, egl_node) != 0))
    {
        LOG(LOG_LEVEL_WARNING, "xrdp_accel_assist_vaapi_init: "
            "XRDP_VAAPI_DEVICE %s is not the EGL render node %s",
            node, egl_node);
    }
    LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_init: using %s", node);
    g_drm_fd = g_file_open_ex(node, 1, 1, 0, 0);
    if (g_drm_fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_init: "
            "open %s failed", node);
        return 1;
    }
    g_va_dpy = vaGetDisplayDRM(g_drm_fd);
    if (g_va_dpy == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_init: "
            "vaGetDisplayDRM failed");
        g_file_close(g_drm_fd);
        g_drm_fd = -1;
        return 1;
    }
    va_status = vaInitialize(g_va_dpy, &major, &minor);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_init: "
            "vaInitialize failed %d", va_status);
        vaTerminate(g_va_dpy);
        g_va_dpy = NULL;
        g_file_close(g_drm_fd);
        g_drm_fd = -1;
        return 1;
    }
    LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_init: VA-API %d.%d "
        "driver '%s'", major, minor, vaQueryVendorString(g_va_dpy));
    for (index = 0; index < XH_VA_NUM_CANDIDATES; index++)
    {
        if (xrdp_accel_assist_vaapi_probe(g_va_candidates[index].profile,
                                          g_va_candidates[index].entrypoint))
        {
            g_va_profile = g_va_candidates[index].profile;
            g_va_entrypoint = g_va_candidates[index].entrypoint;
            g_profile_idc = g_va_candidates[index].profile_idc;
            g_transform_8x8 = g_va_candidates[index].transform_8x8;
            LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_init: using %s",
                g_va_candidates[index].name);
            return 0;
        }
    }
    LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_init: no H264 encode "
        "profile with constant QP and packed headers");
    vaTerminate(g_va_dpy);
    g_va_dpy = NULL;
    g_file_close(g_drm_fd);
    g_drm_fd = -1;
    return 1;
}

/*****************************************************************************/
/* can GL render into a dma-buf of this format and modifier */
static int
xrdp_accel_assist_vaapi_egl_modifier_ok(unsigned int fourcc,
                                        unsigned long long modifier)
{
    EGLuint64KHR mods[XH_MAX_MODIFIERS];
    EGLBoolean external_only[XH_MAX_MODIFIERS];
    EGLint num_mods;
    int index;

    if (modifier == XH_DRM_FORMAT_MOD_INVALID)
    {
        /* imported without a modifier, checked when bound to a fbo */
        return 1;
    }
    num_mods = 0;
    if (epoxy_has_egl_extension(g_egl_display,
                                "EGL_EXT_image_dma_buf_import_modifiers"))
    {
        if (!eglQueryDmaBufModifiersEXT(g_egl_display, fourcc,
                                        XH_MAX_MODIFIERS, mods,
                                        external_only, &num_mods))
        {
            num_mods = 0;
        }
    }
    if (num_mods == 0)
    {
        return modifier == XH_DRM_FORMAT_MOD_LINEAR;
    }
    for (index = 0; index < num_mods; index++)
    {
        if (mods[index] == modifier)
        {
            return !external_only[index];
        }
    }
    return 0;
}

/*****************************************************************************/
/* modifiers GL can render both R8 and GR88 into */
static int
xrdp_accel_assist_vaapi_egl_modifiers(uint64_t *out, int max_out)
{
    EGLuint64KHR mods[XH_MAX_MODIFIERS];
    EGLBoolean external_only[XH_MAX_MODIFIERS];
    EGLint num_mods;
    int index;
    int count;

    num_mods = 0;
    if (epoxy_has_egl_extension(g_egl_display,
                                "EGL_EXT_image_dma_buf_import_modifiers"))
    {
        if (!eglQueryDmaBufModifiersEXT(g_egl_display, XH_DRM_FORMAT_R8,
                                        XH_MAX_MODIFIERS, mods,
                                        external_only, &num_mods))
        {
            num_mods = 0;
        }
    }
    count = 0;
    for (index = 0; (index < num_mods) && (count < max_out); index++)
    {
        if (!external_only[index] &&
                xrdp_accel_assist_vaapi_egl_modifier_ok(XH_DRM_FORMAT_GR88,
                    mods[index]))
        {
            out[count++] = mods[index];
        }
    }
    if ((count == 0) && (max_out > 0))
    {
        out[count++] = XH_DRM_FORMAT_MOD_LINEAR;
    }
    return count;
}

/*****************************************************************************/
static void
xrdp_accel_assist_vaapi_close_desc(VADRMPRIMESurfaceDescriptor *desc)
{
    unsigned int index;

    for (index = 0; index < desc->num_objects; index++)
    {
        if (desc->objects[index].fd >= 0)
        {
            g_file_close(desc->objects[index].fd);
            desc->objects[index].fd = -1;
        }
    }
}

/*****************************************************************************/
/* NV12 surface exported as one dma-buf layer per plane, if use_list, only
   in modifiers GL can render into */
static int
xrdp_accel_assist_vaapi_alloc_surface(int width, int height, int use_list,
                                      VASurfaceID *surface,
                                      VADRMPRIMESurfaceDescriptor *desc)
{
    VASurfaceAttrib attribs[2];
    VADRMFormatModifierList mod_list;
    uint64_t mods[XH_MAX_MODIFIERS];
    int num_attribs;
    VAStatus va_status;

    g_memset(attribs, 0, sizeof(attribs));
    attribs[0].type = VASurfaceAttribPixelFormat;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_FOURCC_NV12;
    num_attribs = 1;
    if (use_list)
    {
        g_memset(&mod_list, 0, sizeof(mod_list));
        mod_list.num_modifiers =
            xrdp_accel_assist_vaapi_egl_modifiers(mods, XH_MAX_MODIFIERS);
        mod_list.modifiers = mods;
        attribs[1].type = VASurfaceAttribDRMFormatModifiers;
        attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
        attribs[1].value.type = VAGenericValueTypePointer;
        attribs[1].value.value.p = &mod_list;
        num_attribs = 2;
    }
    va_status = vaCreateSurfaces(g_va_dpy, VA_RT_FORMAT_YUV420,
                                 width, height, surface, 1,
                                 attribs, num_attribs);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_alloc_surface: "
            "vaCreateSurfaces %dx%d use_list %d failed %d (%s)",
            width, height, use_list, va_status, vaErrorStr(va_status));
        *surface = VA_INVALID_ID;
        return 1;
    }
    g_memset(desc, 0, sizeof(*desc));
    va_status = vaExportSurfaceHandle(g_va_dpy, *surface,
                                      VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_WRITE |
                                      VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                      desc);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_alloc_surface: "
            "vaExportSurfaceHandle failed %d (%s)",
            va_status, vaErrorStr(va_status));
        vaDestroySurfaces(g_va_dpy, surface, 1);
        *surface = VA_INVALID_ID;
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* R8 and GR88 layers, both renderable */
static int
xrdp_accel_assist_vaapi_layers_ok(const VADRMPRIMESurfaceDescriptor *desc)
{
    static const unsigned int formats[2] =
    {
        XH_DRM_FORMAT_R8, XH_DRM_FORMAT_GR88
    };
    unsigned int obj;
    int plane;

    if (desc->num_layers != 2)
    {
        return 0;
    }
    for (plane = 0; plane < 2; plane++)
    {
        obj = desc->layers[plane].object_index[0];
        if ((desc->layers[plane].drm_format != formats[plane]) ||
                (desc->layers[plane].num_planes != 1) ||
                (obj >= desc->num_objects) ||
                !xrdp_accel_assist_vaapi_egl_modifier_ok(formats[plane],
                        desc->objects[obj].drm_format_modifier))
        {
            return 0;
        }
    }
    return 1;
}

/*****************************************************************************/
/* GL texture over one layer of the input surface */
static int
xrdp_accel_assist_vaapi_import_plane(struct enc_info *lei, int plane,
                                     const VADRMPRIMESurfaceDescriptor *desc,
                                     int width, int height)
{
    EGLint attrs[32];
    int num_attrs;
    unsigned int obj;
    unsigned long long modifier;
    GLuint fb;
    GLenum status;
    GLenum gl_error;

    obj = desc->layers[plane].object_index[0];
    modifier = desc->objects[obj].drm_format_modifier;
    num_attrs = 0;
    attrs[num_attrs++] = EGL_WIDTH;
    attrs[num_attrs++] = width;
    attrs[num_attrs++] = EGL_HEIGHT;
    attrs[num_attrs++] = height;
    attrs[num_attrs++] = EGL_LINUX_DRM_FOURCC_EXT;
    attrs[num_attrs++] = (EGLint) desc->layers[plane].drm_format;
    attrs[num_attrs++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attrs[num_attrs++] = desc->objects[obj].fd;
    attrs[num_attrs++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attrs[num_attrs++] = (EGLint) desc->layers[plane].offset[0];
    attrs[num_attrs++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attrs[num_attrs++] = (EGLint) desc->layers[plane].pitch[0];
    if ((modifier != XH_DRM_FORMAT_MOD_INVALID) &&
            epoxy_has_egl_extension(g_egl_display,
                                    "EGL_EXT_image_dma_buf_import_modifiers"))
    {
        attrs[num_attrs++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attrs[num_attrs++] = (EGLint) (modifier & 0xffffffff);
        attrs[num_attrs++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attrs[num_attrs++] = (EGLint) (modifier >> 32);
    }
    attrs[num_attrs++] = EGL_NONE;
    lei->egl_image[plane] = eglCreateImageKHR(g_egl_display, EGL_NO_CONTEXT,
                            EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
    if (lei->egl_image[plane] == EGL_NO_IMAGE_KHR)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_import_plane: "
            "eglCreateImageKHR plane %d modifier 0x%llx failed 0x%x",
            plane, modifier, eglGetError());
        return 1;
    }
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glGenTextures(1, &(lei->plane_tex[plane]));
    glBindTexture(GL_TEXTURE_2D, lei->plane_tex[plane]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                                 (GLeglImageOES) lei->egl_image[plane]);
    gl_error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    /* imported does not mean renderable */
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, lei->plane_tex[plane], 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
    {
        /* the padding past width x height is never drawn, black Y and
           neutral UV */
        if (plane == 0)
        {
            glClearColor(0.0, 0.0, 0.0, 0.0);
        }
        else
        {
            glClearColor(0.5, 0.5, 0.0, 0.0);
        }
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    if ((gl_error != GL_NO_ERROR) || (status != GL_FRAMEBUFFER_COMPLETE))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_import_plane: "
            "plane %d modifier 0x%llx not renderable, gl error 0x%x "
            "fbo status 0x%x", plane, modifier, gl_error, status);
        return 1;
    }
    LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_import_plane: plane %d "
        "%dx%d modifier 0x%llx offset %u pitch %u", plane, width, height,
        modifier, desc->layers[plane].offset[0],
        desc->layers[plane].pitch[0]);
    return 0;
}

/*****************************************************************************/
/* the input surface is libva's, at the coded size, iHD fails vaEndPicture
   on an imported surface smaller than that */
static int
xrdp_accel_assist_vaapi_create_input(struct enc_info *lei)
{
    VADRMPRIMESurfaceDescriptor desc;
    int rv;

    if (xrdp_accel_assist_vaapi_alloc_surface(lei->enc_width,
            lei->enc_height, 0, &(lei->input_surface), &desc) != 0)
    {
        return 1;
    }
    if (!xrdp_accel_assist_vaapi_layers_ok(&desc))
    {
        LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_create_input: "
            "default surface modifier 0x%llx not renderable, retrying",
            (unsigned long long) desc.objects[0].drm_format_modifier);
        xrdp_accel_assist_vaapi_close_desc(&desc);
        vaDestroySurfaces(g_va_dpy, &(lei->input_surface), 1);
        lei->input_surface = VA_INVALID_ID;
        if (xrdp_accel_assist_vaapi_alloc_surface(lei->enc_width,
                lei->enc_height, 1, &(lei->input_surface), &desc) != 0)
        {
            return 1;
        }
        if (!xrdp_accel_assist_vaapi_layers_ok(&desc))
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_create_input: "
                "no NV12 layout both libva and GL support");
            xrdp_accel_assist_vaapi_close_desc(&desc);
            return 1;
        }
    }
    rv = xrdp_accel_assist_vaapi_import_plane(lei, 0, &desc,
         lei->enc_width, lei->enc_height);
    if (rv == 0)
    {
        rv = xrdp_accel_assist_vaapi_import_plane(lei, 1, &desc,
             lei->enc_width / 2, lei->enc_height / 2);
    }
    /* EGL has its own references */
    xrdp_accel_assist_vaapi_close_desc(&desc);
    return rv;
}

/*****************************************************************************/
int
xrdp_accel_assist_vaapi_create_encoder(int width, int height, int tex,
                                       int tex_format, struct enc_info **ei)
{
    struct enc_info *lei;
    VAStatus va_status;
    VAConfigAttrib config_attrib[2];
    VASurfaceID render_targets[3];
    char *qp_str;
    char *bitrate_str;
    int qp_int;

    (void) tex;
    (void) tex_format;
    lei = g_new0(struct enc_info, 1);
    if (lei == NULL)
    {
        return 1;
    }
    lei->width = width;
    lei->height = height;
    /* mstsc's software decoder rejects an odd number of macroblocks across,
       so code those 32 aligned */
    lei->enc_width = (width + 15) & ~15;
    if ((lei->enc_width / 16) & 1)
    {
        lei->enc_width = (width + 31) & ~31;
    }
    lei->enc_height = (height + 15) & ~15;
    lei->egl_image[0] = EGL_NO_IMAGE_KHR;
    lei->egl_image[1] = EGL_NO_IMAGE_KHR;
    lei->input_surface = VA_INVALID_ID;
    lei->recon_surfaces[0] = VA_INVALID_ID;
    lei->recon_surfaces[1] = VA_INVALID_ID;
    lei->config = VA_INVALID_ID;
    lei->context = VA_INVALID_ID;
    lei->coded_buf = VA_INVALID_ID;

    /* these env vars can be added / changed in sesman.ini SessionVariables
       example
       XRDP_VAAPI_QP=26
       or
       XRDP_VAAPI_BITRATE=5000
       bitrate in kbit/s, uses VBR with the QP as the starting point */
    lei->qp = XH_VAAPI_DEFAULT_QP;
    qp_str = g_getenv("XRDP_VAAPI_QP");
    if (qp_str != NULL)
    {
        qp_int = g_atoi(qp_str);
        if ((qp_int >= 1) && (qp_int <= 51))
        {
            lei->qp = qp_int;
        }
    }
    lei->rc_mode = VA_RC_CQP;
    bitrate_str = g_getenv("XRDP_VAAPI_BITRATE");
    if (bitrate_str != NULL)
    {
        lei->bitrate_kbps = g_atoi(bitrate_str);
        if (lei->bitrate_kbps > 0)
        {
            lei->rc_mode = VA_RC_VBR;
        }
        else
        {
            lei->bitrate_kbps = 0;
        }
    }
    LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_vaapi_create_encoder: "
        "width %d height %d coded %dx%d qp %d bitrate %d kbit/s",
        width, height, lei->enc_width, lei->enc_height, lei->qp,
        lei->bitrate_kbps);

    if (xrdp_accel_assist_vaapi_create_input(lei) != 0)
    {
        xrdp_accel_assist_vaapi_delete_encoder(lei);
        return 1;
    }
    va_status = vaCreateSurfaces(g_va_dpy, VA_RT_FORMAT_YUV420,
                                 lei->enc_width, lei->enc_height,
                                 lei->recon_surfaces, 2, NULL, 0);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_create_encoder: "
            "vaCreateSurfaces failed %d", va_status);
        lei->recon_surfaces[0] = VA_INVALID_ID;
        xrdp_accel_assist_vaapi_delete_encoder(lei);
        return 1;
    }
    g_memset(config_attrib, 0, sizeof(config_attrib));
    config_attrib[0].type = VAConfigAttribRateControl;
    config_attrib[0].value = lei->rc_mode;
    config_attrib[1].type = VAConfigAttribEncPackedHeaders;
    config_attrib[1].value = VA_ENC_PACKED_HEADER_SEQUENCE |
                             VA_ENC_PACKED_HEADER_PICTURE |
                             VA_ENC_PACKED_HEADER_SLICE;
    va_status = vaCreateConfig(g_va_dpy, g_va_profile, g_va_entrypoint,
                               config_attrib, 2, &(lei->config));
    if ((va_status != VA_STATUS_SUCCESS) && (lei->rc_mode != VA_RC_CQP))
    {
        LOG(LOG_LEVEL_WARNING, "xrdp_accel_assist_vaapi_create_encoder: "
            "vaCreateConfig VBR failed %d, using constant QP", va_status);
        lei->rc_mode = VA_RC_CQP;
        lei->bitrate_kbps = 0;
        config_attrib[0].value = lei->rc_mode;
        va_status = vaCreateConfig(g_va_dpy, g_va_profile, g_va_entrypoint,
                                   config_attrib, 2, &(lei->config));
    }
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_create_encoder: "
            "vaCreateConfig failed %d", va_status);
        lei->config = VA_INVALID_ID;
        xrdp_accel_assist_vaapi_delete_encoder(lei);
        return 1;
    }
    render_targets[0] = lei->input_surface;
    render_targets[1] = lei->recon_surfaces[0];
    render_targets[2] = lei->recon_surfaces[1];
    va_status = vaCreateContext(g_va_dpy, lei->config,
                                lei->enc_width, lei->enc_height,
                                VA_PROGRESSIVE, render_targets, 3,
                                &(lei->context));
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_create_encoder: "
            "vaCreateContext failed %d", va_status);
        lei->context = VA_INVALID_ID;
        xrdp_accel_assist_vaapi_delete_encoder(lei);
        return 1;
    }
    va_status = vaCreateBuffer(g_va_dpy, lei->context, VAEncCodedBufferType,
                               lei->enc_width * lei->enc_height * 3 / 2, 1,
                               NULL, &(lei->coded_buf));
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_create_encoder: "
            "vaCreateBuffer failed %d", va_status);
        lei->coded_buf = VA_INVALID_ID;
        xrdp_accel_assist_vaapi_delete_encoder(lei);
        return 1;
    }
    *ei = lei;
    return 0;
}

/*****************************************************************************/
int
xrdp_accel_assist_vaapi_delete_encoder(struct enc_info *ei)
{
    int plane;

    if (ei == NULL)
    {
        return 0;
    }
    if (ei->coded_buf != VA_INVALID_ID)
    {
        vaDestroyBuffer(g_va_dpy, ei->coded_buf);
    }
    if (ei->context != VA_INVALID_ID)
    {
        vaDestroyContext(g_va_dpy, ei->context);
    }
    if (ei->config != VA_INVALID_ID)
    {
        vaDestroyConfig(g_va_dpy, ei->config);
    }
    if (ei->recon_surfaces[0] != VA_INVALID_ID)
    {
        vaDestroySurfaces(g_va_dpy, ei->recon_surfaces, 2);
    }
    for (plane = 0; plane < 2; plane++)
    {
        if (ei->plane_tex[plane] != 0)
        {
            glDeleteTextures(1, &(ei->plane_tex[plane]));
        }
        if (ei->egl_image[plane] != EGL_NO_IMAGE_KHR)
        {
            eglDestroyImageKHR(g_egl_display, ei->egl_image[plane]);
        }
    }
    if (ei->input_surface != VA_INVALID_ID)
    {
        vaDestroySurfaces(g_va_dpy, &(ei->input_surface), 1);
    }
    g_free(ei);
    return 0;
}

/*****************************************************************************/
int
xrdp_accel_assist_vaapi_get_planes(struct enc_info *ei, int *y_tex,
                                   int *uv_tex)
{
    *y_tex = ei->plane_tex[0];
    *uv_tex = ei->plane_tex[1];
    return 0;
}

/*****************************************************************************/
static int
render_buffer(VAContextID context, VABufferType type, unsigned int size,
              void *data, VABufferID *bufs, int *num_bufs)
{
    VABufferID buf;
    VAStatus va_status;

    va_status = vaCreateBuffer(g_va_dpy, context, type, size, 1, data, &buf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "render_buffer: vaCreateBuffer type %d "
            "failed %d", type, va_status);
        return 1;
    }
    bufs[(*num_bufs)++] = buf;
    va_status = vaRenderPicture(g_va_dpy, context, &buf, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "render_buffer: vaRenderPicture type %d "
            "failed %d", type, va_status);
        return 1;
    }
    return 0;
}

/*****************************************************************************/
static int
render_misc(VAContextID context, VAEncMiscParameterType type,
            const void *data, unsigned int size,
            VABufferID *bufs, int *num_bufs)
{
    unsigned char buf[sizeof(VAEncMiscParameterBuffer) + 128];
    VAEncMiscParameterBuffer *misc;

    if (size > 128)
    {
        return 1;
    }
    g_memset(buf, 0, sizeof(buf));
    misc = (VAEncMiscParameterBuffer *) buf;
    misc->type = type;
    g_memcpy(misc->data, data, size);
    return render_buffer(context, VAEncMiscParameterBufferType,
                         sizeof(VAEncMiscParameterBuffer) + size,
                         buf, bufs, num_bufs);
}

/*****************************************************************************/
static int
render_packed(VAContextID context, int type, unsigned char *data,
              int bits, int has_emulation_bytes,
              VABufferID *bufs, int *num_bufs)
{
    VAEncPackedHeaderParameterBuffer param;

    g_memset(&param, 0, sizeof(param));
    param.type = type;
    param.bit_length = bits;
    param.has_emulation_bytes = has_emulation_bytes;
    if (render_buffer(context, VAEncPackedHeaderParameterBufferType,
                      sizeof(param), &param, bufs, num_bufs) != 0)
    {
        return 1;
    }
    return render_buffer(context, VAEncPackedHeaderDataBufferType,
                         (bits + 7) / 8, data, bufs, num_bufs);
}

/*****************************************************************************/
static int
xrdp_accel_assist_vaapi_render(struct enc_info *ei, int is_idr,
                               VAPictureH264 *curr,
                               VABufferID *bufs, int *num_bufs)
{
    VAEncSequenceParameterBufferH264 seq;
    VAEncPictureParameterBufferH264 pic;
    VAEncSliceParameterBufferH264 slice;
    VAEncMiscParameterRateControl rc;
    VAEncMiscParameterHRD hrd;
    unsigned char header[256];
    int header_bytes;
    int header_bits;
    int index;

    if (is_idr)
    {
        g_memset(&seq, 0, sizeof(seq));
        seq.level_idc = get_level_idc(ei->enc_width, ei->enc_height);
        seq.ip_period = 1;
        seq.bits_per_second = ei->bitrate_kbps * 1000;
        seq.max_num_ref_frames = 1;
        seq.picture_width_in_mbs = ei->enc_width / 16;
        seq.picture_height_in_mbs = ei->enc_height / 16;
        seq.seq_fields.bits.chroma_format_idc = 1;
        seq.seq_fields.bits.frame_mbs_only_flag = 1;
        seq.seq_fields.bits.direct_8x8_inference_flag = 1;
        seq.seq_fields.bits.pic_order_cnt_type = 2;
        if ((ei->enc_width != ei->width) || (ei->enc_height != ei->height))
        {
            seq.frame_cropping_flag = 1;
            seq.frame_crop_right_offset = (ei->enc_width - ei->width) / 2;
            seq.frame_crop_bottom_offset = (ei->enc_height - ei->height) / 2;
        }
        if (render_buffer(ei->context, VAEncSequenceParameterBufferType,
                          sizeof(seq), &seq, bufs, num_bufs) != 0)
        {
            return 1;
        }
        if (ei->rc_mode == VA_RC_VBR)
        {
            g_memset(&rc, 0, sizeof(rc));
            rc.bits_per_second = ei->bitrate_kbps * 1000;
            rc.target_percentage = 80;
            rc.window_size = 1000;
            rc.initial_qp = ei->qp;
            rc.min_qp = 1;
            rc.max_qp = 51;
            rc.rc_flags.bits.disable_frame_skip = 1;
            if (render_misc(ei->context, VAEncMiscParameterTypeRateControl,
                            &rc, sizeof(rc), bufs, num_bufs) != 0)
            {
                return 1;
            }
            /* one second of buffer */
            g_memset(&hrd, 0, sizeof(hrd));
            hrd.buffer_size = ei->bitrate_kbps * 1000;
            hrd.initial_buffer_fullness = hrd.buffer_size / 2;
            if (render_misc(ei->context, VAEncMiscParameterTypeHRD,
                            &hrd, sizeof(hrd), bufs, num_bufs) != 0)
            {
                return 1;
            }
        }
        header_bytes = build_sps(header, ei);
        if (render_packed(ei->context, VAEncPackedHeaderSequence,
                          header, header_bytes * 8, 1,
                          bufs, num_bufs) != 0)
        {
            return 1;
        }
    }

    g_memset(&pic, 0, sizeof(pic));
    pic.CurrPic = *curr;
    for (index = 0; index < 16; index++)
    {
        pic.ReferenceFrames[index].picture_id = VA_INVALID_SURFACE;
        pic.ReferenceFrames[index].flags = VA_PICTURE_H264_INVALID;
    }
    if (!is_idr && ei->have_ref)
    {
        pic.ReferenceFrames[0] = ei->ref_pic;
    }
    pic.coded_buf = ei->coded_buf;
    pic.frame_num = ei->frame_num;
    pic.pic_init_qp = ei->qp;
    pic.pic_fields.bits.idr_pic_flag = is_idr;
    pic.pic_fields.bits.reference_pic_flag = 1;
    pic.pic_fields.bits.entropy_coding_mode_flag = 1;
    pic.pic_fields.bits.deblocking_filter_control_present_flag = 1;
    pic.pic_fields.bits.transform_8x8_mode_flag = g_transform_8x8;
    if (render_buffer(ei->context, VAEncPictureParameterBufferType,
                      sizeof(pic), &pic, bufs, num_bufs) != 0)
    {
        return 1;
    }
    if (is_idr)
    {
        header_bytes = build_pps(header, ei);
        if (render_packed(ei->context, VAEncPackedHeaderPicture,
                          header, header_bytes * 8, 1,
                          bufs, num_bufs) != 0)
        {
            return 1;
        }
    }

    g_memset(&slice, 0, sizeof(slice));
    slice.num_macroblocks = (ei->enc_width / 16) * (ei->enc_height / 16);
    slice.macroblock_info = VA_INVALID_ID;
    slice.slice_type = (is_idr || !ei->have_ref) ? 2 : 0;
    slice.idr_pic_id = g_idr_pic_id;
    for (index = 0; index < 32; index++)
    {
        slice.RefPicList0[index].picture_id = VA_INVALID_SURFACE;
        slice.RefPicList0[index].flags = VA_PICTURE_H264_INVALID;
        slice.RefPicList1[index].picture_id = VA_INVALID_SURFACE;
        slice.RefPicList1[index].flags = VA_PICTURE_H264_INVALID;
    }
    if (slice.slice_type == 0)
    {
        slice.RefPicList0[0] = ei->ref_pic;
    }
    slice.num_ref_idx_active_override_flag = 1;
    slice.slice_alpha_c0_offset_div2 = XH_VAAPI_DEBLOCK_OFFSET;
    slice.slice_beta_offset_div2 = XH_VAAPI_DEBLOCK_OFFSET;
    /* before the slice parameters */
    header_bits = build_slice_header(header, ei, is_idr, slice.slice_type,
                                     g_idr_pic_id);
    if (render_packed(ei->context, VAEncPackedHeaderSlice,
                      header, header_bits, 0, bufs, num_bufs) != 0)
    {
        return 1;
    }
    return render_buffer(ei->context, VAEncSliceParameterBufferType,
                         sizeof(slice), &slice, bufs, num_bufs);
}

/*****************************************************************************/
enum encoder_result
xrdp_accel_assist_vaapi_encode(struct enc_info *ei, int tex,
                               void *cdata, int *cdata_bytes,
                               int flags)
{
    VAPictureH264 curr;
    VABufferID bufs[16];
    VACodedBufferSegment *seg;
    VAStatus va_status;
    int num_bufs;
    int is_idr;
    int error;
    int index;
    int total;

    (void) tex;
    /* flush is enough, the implicit sync on the dma-buf orders the GL
       writes before the encode */
    glFlush();

    is_idr = (flags & XH_ENC_FLAGS_FORCEIDR) || (ei->frameCount < 1);
    if (is_idr)
    {
        ei->frame_num = 0;
        ei->poc = 0;
        ei->have_ref = 0;
    }
    g_memset(&curr, 0, sizeof(curr));
    curr.picture_id = ei->recon_surfaces[ei->cur_recon];
    curr.frame_idx = ei->frame_num;
    curr.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    curr.TopFieldOrderCnt = ei->poc;
    curr.BottomFieldOrderCnt = ei->poc;

    va_status = vaBeginPicture(g_va_dpy, ei->context, ei->input_surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_encode: "
            "vaBeginPicture failed %d (%s)", va_status,
            vaErrorStr(va_status));
        return ENCODER_ERROR;
    }
    num_bufs = 0;
    error = xrdp_accel_assist_vaapi_render(ei, is_idr, &curr,
                                           bufs, &num_bufs);
    va_status = vaEndPicture(g_va_dpy, ei->context);
    for (index = 0; index < num_bufs; index++)
    {
        vaDestroyBuffer(g_va_dpy, bufs[index]);
    }
    if (error || (va_status != VA_STATUS_SUCCESS))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_encode: "
            "vaEndPicture %d (%s) error %d", va_status,
            vaErrorStr(va_status), error);
        return ENCODER_ERROR;
    }

    /* one previous picture, ping pong the recon surfaces */
    ei->ref_pic = curr;
    ei->have_ref = 1;
    ei->cur_recon ^= 1;
    ei->frame_num = (ei->frame_num + 1) & 15;
    ei->poc += 2;
    if (is_idr)
    {
        g_idr_pic_id = (g_idr_pic_id + 1) & 0xffff;
    }
    ei->frameCount++;

    va_status = vaSyncSurface(g_va_dpy, ei->input_surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_encode: "
            "vaSyncSurface failed %d", va_status);
        return ENCODER_ERROR;
    }
    seg = NULL;
    va_status = vaMapBuffer(g_va_dpy, ei->coded_buf, (void **) &seg);
    if (va_status != VA_STATUS_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_encode: "
            "vaMapBuffer failed %d", va_status);
        return ENCODER_ERROR;
    }
    total = 0;
    error = 0;
    while (seg != NULL)
    {
        if (total + (int) (seg->size) > *cdata_bytes)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_vaapi_encode: "
                "error not enough room %d %d", *cdata_bytes,
                total + (int) (seg->size));
            error = 1;
            break;
        }
        g_memcpy(((char *) cdata) + total, seg->buf, seg->size);
        total += seg->size;
        seg = (VACodedBufferSegment *) (seg->next);
    }
    vaUnmapBuffer(g_va_dpy, ei->coded_buf);
    if (error)
    {
        return ENCODER_ERROR;
    }
    *cdata_bytes = total;
    return is_idr ? KEY_FRAME_ENCODED : INCREMENTAL_FRAME_ENCODED;
}
