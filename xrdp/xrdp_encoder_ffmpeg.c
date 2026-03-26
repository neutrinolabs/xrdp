/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026
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
 * FFmpeg H.264 encoder backend
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp_encoder_ffmpeg.h"

#include <drm_fourcc.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/buffer.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xrdp_encoder.h"
#include "xrdp_tconfig.h"

#define FFMPEG_DEFAULT_FPS_NUM 60
#define FFMPEG_DEFAULT_FPS_DEN 1
#define FFMPEG_DEFAULT_GOP_SIZE 60
#define FFMPEG_DEFAULT_THREADS 1
#define FFMPEG_MAX_FILTER_DESC 1024

#define FFMPEG_ALIGN(_value, _align) (((_value) + ((_align) - 1)) & ~((_align) - 1))

enum ffmpeg_encoder_input_type
{
    FFMPEG_INPUT_NONE = 0,
    FFMPEG_INPUT_SHM,
    FFMPEG_INPUT_DMABUF
};

struct ffmpeg_encoder
{
    struct xrdp_tconfig_gfx_ffmpeg cfg;
    enum xrdp_tconfig_ffmpeg_paths active_path;
    enum ffmpeg_encoder_input_type input_type;
    enum AVHWDeviceType graph_device_type;
    int input_width;
    int input_height;
    int visible_width;
    int visible_height;
    int coded_width;
    int coded_height;
    int color_format;
    int connection_type;
    unsigned int dmabuf_fourcc;
    int64_t pts;
    AVBufferRef *drm_device_ctx;
    AVBufferRef *drm_frames_ctx;
    AVBufferRef *vaapi_device_ctx;
    AVBufferRef *vulkan_device_ctx;
    AVCodecContext *codec_ctx;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffer_src_ctx;
    AVFilterContext *buffer_sink_ctx;
    AVFrame *dmabuf_sw_frame;
    AVFrame *sw_frame;
};

static int
ffmpeg_log_error(const char *what, int error)
{
    char text[AV_ERROR_MAX_STRING_SIZE];

    av_strerror(error, text, sizeof(text));
    LOG(LOG_LEVEL_ERROR, "%s: %s", what, text);
    return 1;
}

static const char *
ffmpeg_path_name(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_SOFTWARE:
            return "software";
        case XTC_FFMPEG_PATH_VAAPI:
            return "vaapi";
        case XTC_FFMPEG_PATH_VULKAN:
            return "vulkan";
        default:
            return "unknown";
    }
}

static int
ffmpeg_path_is_hardware(enum xrdp_tconfig_ffmpeg_paths path)
{
    return path == XTC_FFMPEG_PATH_VAAPI ||
           path == XTC_FFMPEG_PATH_VULKAN;
}

static const char *
ffmpeg_input_name(enum ffmpeg_encoder_input_type input_type)
{
    switch (input_type)
    {
        case FFMPEG_INPUT_SHM:
            return "shm";
        case FFMPEG_INPUT_DMABUF:
            return "dmabuf";
        default:
            return "none";
    }
}

static const char *
ffmpeg_path_encoder_name(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_SOFTWARE:
            return "libx264";
        case XTC_FFMPEG_PATH_VAAPI:
            return "h264_vaapi";
        case XTC_FFMPEG_PATH_VULKAN:
            return "h264_vulkan";
        default:
            return NULL;
    }
}

static enum AVHWDeviceType
ffmpeg_path_device_type(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_VAAPI:
            return AV_HWDEVICE_TYPE_VAAPI;
        case XTC_FFMPEG_PATH_VULKAN:
            return AV_HWDEVICE_TYPE_VULKAN;
        default:
            return AV_HWDEVICE_TYPE_NONE;
    }
}

static enum AVPixelFormat
ffmpeg_path_sink_format(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_VAAPI:
            return AV_PIX_FMT_VAAPI;
        case XTC_FFMPEG_PATH_VULKAN:
            return AV_PIX_FMT_VULKAN;
        default:
            return AV_PIX_FMT_NONE;
    }
}

static const char *
ffmpeg_path_scale_filter(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_VAAPI:
            return "scale_vaapi";
        case XTC_FFMPEG_PATH_VULKAN:
            return "scale_vulkan";
        default:
            return NULL;
    }
}

static const char *
ffmpeg_path_hwmap_device_name(enum xrdp_tconfig_ffmpeg_paths path)
{
    switch (path)
    {
        case XTC_FFMPEG_PATH_VAAPI:
            return "vaapi";
        case XTC_FFMPEG_PATH_VULKAN:
            return "vulkan";
        default:
            return NULL;
    }
}

static int
ffmpeg_connection_type(int connection_type)
{
    if (connection_type < CONNECTION_TYPE_MODEM ||
            connection_type > CONNECTION_TYPE_LAN)
    {
        return CONNECTION_TYPE_LAN;
    }

    return connection_type;
}

static int
ffmpeg_get_target_bitrate(int connection_type)
{
    switch (connection_type)
    {
        case CONNECTION_TYPE_MODEM:
            return 600000;
        case CONNECTION_TYPE_BROADBAND_LOW:
            return 1600000;
        case CONNECTION_TYPE_SATELLITE:
            return 4000000;
        case CONNECTION_TYPE_BROADBAND_HIGH:
            return 8000000;
        case CONNECTION_TYPE_WAN:
            return 10000000;
        case CONNECTION_TYPE_LAN:
        case CONNECTION_TYPE_AUTODETECT:
        default:
            return 20000000;
    }
}

static const struct xrdp_tconfig_gfx_ffmpeg_param *
ffmpeg_get_param(const struct ffmpeg_encoder *fe, int connection_type)
{
    return &(fe->cfg.param[ffmpeg_connection_type(connection_type)]);
}

static int
ffmpeg_use_baseline_profile(const struct xrdp_tconfig_gfx_ffmpeg_param *param)
{
    return param == NULL ||
           g_strcasecmp(param->profile, "baseline") == 0 ||
           g_strcasecmp(param->preset, "ultrafast") == 0;
}

static const char *
ffmpeg_get_profile_name(const struct xrdp_tconfig_gfx_ffmpeg_param *param,
                        int hardware_profile)
{
    if (ffmpeg_use_baseline_profile(param))
    {
        return hardware_profile ? "constrained_baseline" : "baseline";
    }

    if (g_strcasecmp(param->profile, "main") == 0)
    {
        return "main";
    }
    if (g_strcasecmp(param->profile, "high") == 0)
    {
        return "high";
    }
    if (!hardware_profile && g_strcasecmp(param->profile, "high10") == 0)
    {
        return "high10";
    }
    if (!hardware_profile && g_strcasecmp(param->profile, "high422") == 0)
    {
        return "high422";
    }
    if (!hardware_profile && g_strcasecmp(param->profile, "high444") == 0)
    {
        return "high444";
    }

    return hardware_profile ? "constrained_baseline" : "baseline";
}

static const char *
ffmpeg_get_entropy_coder(const struct xrdp_tconfig_gfx_ffmpeg_param *param)
{
    return ffmpeg_use_baseline_profile(param) ? "cavlc" : "cabac";
}

static void
ffmpeg_get_framerate(const struct ffmpeg_encoder *fe,
                     int connection_type,
                     int *fps_num,
                     int *fps_den)
{
    const struct xrdp_tconfig_gfx_ffmpeg_param *param;

    param = ffmpeg_get_param(fe, connection_type);
    *fps_num = (param->fps_num > 0) ? param->fps_num : FFMPEG_DEFAULT_FPS_NUM;
    *fps_den = (param->fps_den > 0) ? param->fps_den : FFMPEG_DEFAULT_FPS_DEN;
}

static void
ffmpeg_get_rate_control(const struct ffmpeg_encoder *fe,
                        int connection_type,
                        int *bit_rate,
                        int *max_rate,
                        int *buffer_size)
{
    const struct xrdp_tconfig_gfx_ffmpeg_param *param;
    int default_bitrate;

    param = ffmpeg_get_param(fe, connection_type);
    default_bitrate = ffmpeg_get_target_bitrate(
                          ffmpeg_connection_type(connection_type));

    if (param->vbv_max_bitrate > 0)
    {
        *bit_rate = param->vbv_max_bitrate * 1000;
        *max_rate = *bit_rate;
    }
    else
    {
        *bit_rate = default_bitrate;
        *max_rate = default_bitrate;
    }

    if (param->vbv_buffer_size > 0)
    {
        *buffer_size = param->vbv_buffer_size * 1000;
    }
    else
    {
        *buffer_size = *max_rate / 2;
    }
}

static void
ffmpeg_get_color_props(int color_format,
                       enum AVColorRange *color_range,
                       enum AVColorSpace *color_space)
{
    if (color_format == XRDP_nv12_709fr)
    {
        *color_range = AVCOL_RANGE_JPEG;
        *color_space = AVCOL_SPC_BT709;
    }
    else
    {
        *color_range = AVCOL_RANGE_MPEG;
        *color_space = AVCOL_SPC_BT470BG;
    }
}

static int
ffmpeg_get_color_names(int color_format,
                       const char **range_name,
                       const char **matrix_name,
                       const char **primaries_name,
                       const char **transfer_name)
{
    if (range_name == NULL || matrix_name == NULL ||
            primaries_name == NULL || transfer_name == NULL)
    {
        return 1;
    }

    if (color_format == XRDP_nv12_709fr)
    {
        *range_name = "full";
        *matrix_name = "bt709";
        *primaries_name = "bt709";
        *transfer_name = "bt709";
    }
    else
    {
        *range_name = "limited";
        *matrix_name = "bt470bg";
        *primaries_name = "bt470bg";
        *transfer_name = "bt470bg";
    }

    return 0;
}

static int
ffmpeg_black_luma(int color_format)
{
    return (color_format == XRDP_nv12_709fr) ? 0 : 16;
}

static const char *
ffmpeg_get_drm_device_name(const struct ffmpeg_encoder *fe)
{
    if (fe != NULL && fe->cfg.drm_device[0] != '\0')
    {
        return fe->cfg.drm_device;
    }

    return "/dev/dri/renderD128";
}

static enum AVPixelFormat
ffmpeg_dmabuf_fourcc_to_sw_format(unsigned int fourcc)
{
    switch (fourcc)
    {
        case DRM_FORMAT_XRGB8888:
            return AV_PIX_FMT_BGR0;
        case DRM_FORMAT_ARGB8888:
            return AV_PIX_FMT_BGRA;
        default:
            return AV_PIX_FMT_NONE;
    }
}

static int
ffmpeg_get_coded_dimension(enum xrdp_tconfig_ffmpeg_paths path, int value)
{
    if (path == XTC_FFMPEG_PATH_VULKAN)
    {
        return FFMPEG_ALIGN(value, 2);
    }

    return FFMPEG_ALIGN(value, 16);
}

static void
ffmpeg_encoder_get_coded_size(struct ffmpeg_encoder *fe,
                              enum ffmpeg_encoder_input_type input_type,
                              int visible_width, int visible_height,
                              int *coded_width, int *coded_height)
{
    if (fe->active_path == XTC_FFMPEG_PATH_SOFTWARE &&
            input_type == FFMPEG_INPUT_DMABUF)
    {
        *coded_width = FFMPEG_ALIGN(visible_width, 16);
        *coded_height = FFMPEG_ALIGN(visible_height, 16);
    }
    else
    {
        *coded_width = ffmpeg_get_coded_dimension(fe->active_path,
                                                  visible_width);
        *coded_height = ffmpeg_get_coded_dimension(fe->active_path,
                                                   visible_height);
    }
}

static enum AVPixelFormat
ffmpeg_encoder_get_graph_src_format(const struct ffmpeg_encoder *fe)
{
    return (fe->input_type == FFMPEG_INPUT_DMABUF) ?
           AV_PIX_FMT_DRM_PRIME : AV_PIX_FMT_NV12;
}

static void
ffmpeg_encoder_set_frame_color(AVFrame *frame, int color_format)
{
    enum AVColorRange color_range;
    enum AVColorSpace color_space;

    if (frame == NULL)
    {
        return;
    }

    ffmpeg_get_color_props(color_format, &color_range, &color_space);
    frame->color_range = color_range;
    frame->colorspace = color_space;
}

static void
ffmpeg_encoder_set_frame_crop(struct ffmpeg_encoder *fe, AVFrame *frame)
{
    if (fe == NULL || frame == NULL)
    {
        return;
    }

    frame->crop_left = 0;
    frame->crop_top = 0;
    frame->crop_right = MAX(0, fe->coded_width - fe->visible_width);
    frame->crop_bottom = MAX(0, fe->coded_height - fe->visible_height);
}

static void
ffmpeg_encoder_reset_runtime(struct ffmpeg_encoder *fe)
{
    if (fe == NULL)
    {
        return;
    }

    if (fe->codec_ctx != NULL)
    {
        avcodec_free_context(&fe->codec_ctx);
    }
    if (fe->filter_graph != NULL)
    {
        avfilter_graph_free(&fe->filter_graph);
    }
    if (fe->sw_frame != NULL)
    {
        av_frame_free(&fe->sw_frame);
    }
    if (fe->dmabuf_sw_frame != NULL)
    {
        av_frame_free(&fe->dmabuf_sw_frame);
    }
    if (fe->drm_frames_ctx != NULL)
    {
        av_buffer_unref(&fe->drm_frames_ctx);
    }

    fe->buffer_src_ctx = NULL;
    fe->buffer_sink_ctx = NULL;
    fe->input_type = FFMPEG_INPUT_NONE;
    fe->graph_device_type = AV_HWDEVICE_TYPE_NONE;
    fe->input_width = 0;
    fe->input_height = 0;
    fe->visible_width = 0;
    fe->visible_height = 0;
    fe->coded_width = 0;
    fe->coded_height = 0;
    fe->color_format = 0;
    fe->connection_type = 0;
    fe->dmabuf_fourcc = 0;
    fe->pts = 0;
}

static void
ffmpeg_encoder_release(struct ffmpeg_encoder *fe)
{
    if (fe == NULL)
    {
        return;
    }

    ffmpeg_encoder_reset_runtime(fe);
    av_buffer_unref(&fe->drm_device_ctx);
    av_buffer_unref(&fe->vaapi_device_ctx);
    av_buffer_unref(&fe->vulkan_device_ctx);
}

static int
ffmpeg_encoder_get_device(struct ffmpeg_encoder *fe,
                          enum AVHWDeviceType type,
                          AVBufferRef **device_ctx)
{
    AVBufferRef **cache;
    const char *device_name;
    int error;

    if (fe == NULL || device_ctx == NULL)
    {
        return 1;
    }

    switch (type)
    {
        case AV_HWDEVICE_TYPE_DRM:
            cache = &fe->drm_device_ctx;
            device_name = ffmpeg_get_drm_device_name(fe);
            break;
        case AV_HWDEVICE_TYPE_VAAPI:
            cache = &fe->vaapi_device_ctx;
            device_name = (fe->cfg.drm_device[0] != '\0') ?
                          fe->cfg.drm_device : NULL;
            break;
        case AV_HWDEVICE_TYPE_VULKAN:
            cache = &fe->vulkan_device_ctx;
            device_name = (fe->cfg.vulkan_device[0] != '\0') ?
                          fe->cfg.vulkan_device : NULL;
            break;
        default:
            LOG(LOG_LEVEL_ERROR, "Unsupported FFmpeg hardware device type %d",
                (int) type);
            return 1;
    }

    if (*cache == NULL)
    {
        error = av_hwdevice_ctx_create(cache, type, device_name, NULL, 0);
        if (error < 0)
        {
            av_buffer_unref(cache);
            return ffmpeg_log_error("av_hwdevice_ctx_create", error);
        }
    }

    *device_ctx = *cache;
    return 0;
}

static int
ffmpeg_encoder_prepare_dmabuf_frames_ctx(struct ffmpeg_encoder *fe,
                                         unsigned int fourcc)
{
    AVBufferRef *device_ctx;
    AVBufferRef *frames_ctx;
    AVHWFramesContext *hw_frames;
    enum AVPixelFormat sw_format;
    int error;

    if (fe == NULL)
    {
        return 1;
    }

    sw_format = ffmpeg_dmabuf_fourcc_to_sw_format(fourcc);
    if (sw_format == AV_PIX_FMT_NONE)
    {
        LOG(LOG_LEVEL_ERROR, "Unsupported DMA-BUF fourcc 0x%8.8x", fourcc);
        return 1;
    }

    if (fe->drm_frames_ctx != NULL && fe->dmabuf_fourcc == fourcc)
    {
        return 0;
    }

    if (ffmpeg_encoder_get_device(fe, AV_HWDEVICE_TYPE_DRM,
                                  &device_ctx) != 0)
    {
        return 1;
    }

    frames_ctx = av_hwframe_ctx_alloc(device_ctx);
    if (frames_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "av_hwframe_ctx_alloc(drm) failed");
        return 1;
    }

    hw_frames = (AVHWFramesContext *) frames_ctx->data;
    hw_frames->format = AV_PIX_FMT_DRM_PRIME;
    hw_frames->sw_format = sw_format;
    hw_frames->width = fe->input_width;
    hw_frames->height = fe->input_height;
    error = av_hwframe_ctx_init(frames_ctx);
    if (error < 0)
    {
        av_buffer_unref(&frames_ctx);
        return ffmpeg_log_error("av_hwframe_ctx_init(drm)", error);
    }

    av_buffer_unref(&fe->drm_frames_ctx);
    fe->drm_frames_ctx = frames_ctx;
    fe->dmabuf_fourcc = fourcc;
    return 0;
}

static int
ffmpeg_encoder_alloc_sw_frame(struct ffmpeg_encoder *fe,
                              int width, int height,
                              int color_format)
{
    int error;

    if (fe->sw_frame != NULL)
    {
        if (fe->sw_frame->width == width && fe->sw_frame->height == height)
        {
            return 0;
        }
        av_frame_free(&fe->sw_frame);
    }

    fe->sw_frame = av_frame_alloc();
    if (fe->sw_frame == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "av_frame_alloc failed");
        return 1;
    }

    fe->sw_frame->format = AV_PIX_FMT_NV12;
    fe->sw_frame->width = width;
    fe->sw_frame->height = height;
    ffmpeg_encoder_set_frame_color(fe->sw_frame, color_format);

    error = av_frame_get_buffer(fe->sw_frame, 32);
    if (error < 0)
    {
        return ffmpeg_log_error("av_frame_get_buffer", error);
    }

    return 0;
}

static int
ffmpeg_encoder_alloc_dmabuf_sw_frame(struct ffmpeg_encoder *fe,
                                     enum AVPixelFormat format,
                                     int width, int height)
{
    int error;

    if (fe->dmabuf_sw_frame != NULL)
    {
        if (fe->dmabuf_sw_frame->format == format &&
                fe->dmabuf_sw_frame->width == width &&
                fe->dmabuf_sw_frame->height == height)
        {
            return 0;
        }
        av_frame_free(&fe->dmabuf_sw_frame);
    }

    fe->dmabuf_sw_frame = av_frame_alloc();
    if (fe->dmabuf_sw_frame == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "av_frame_alloc failed");
        return 1;
    }

    fe->dmabuf_sw_frame->format = format;
    fe->dmabuf_sw_frame->width = width;
    fe->dmabuf_sw_frame->height = height;

    error = av_frame_get_buffer(fe->dmabuf_sw_frame, 32);
    if (error < 0)
    {
        return ffmpeg_log_error("av_frame_get_buffer", error);
    }

    return 0;
}

static void
ffmpeg_encoder_fill_black_nv12(AVFrame *frame, int color_format)
{
    int black_y;
    int y;

    if (frame == NULL)
    {
        return;
    }

    black_y = ffmpeg_black_luma(color_format);
    for (y = 0; y < frame->height; ++y)
    {
        g_memset(frame->data[0] + y * frame->linesize[0], black_y,
                 frame->width);
    }
    for (y = 0; y < frame->height / 2; ++y)
    {
        g_memset(frame->data[1] + y * frame->linesize[1], 128,
                 frame->width);
    }
}

static int
ffmpeg_encoder_copy_nv12(AVFrame *frame,
                         const char *data,
                         int width, int height,
                         int src_width, int src_height,
                         int color_format,
                         int clear_to_black)
{
    const char *src_uv;
    const char *src_y;
    int error;
    int y;

    if (frame == NULL || data == NULL)
    {
        return 1;
    }

    error = av_frame_make_writable(frame);
    if (error < 0)
    {
        return ffmpeg_log_error("av_frame_make_writable", error);
    }

    if (clear_to_black)
    {
        ffmpeg_encoder_fill_black_nv12(frame, color_format);
    }

    src_y = data;
    src_uv = data + src_width * src_height;
    for (y = 0; y < height; ++y)
    {
        g_memcpy(frame->data[0] + y * frame->linesize[0],
                 src_y + y * src_width,
                 width);
    }
    for (y = 0; y < height / 2; ++y)
    {
        g_memcpy(frame->data[1] + y * frame->linesize[1],
                 src_uv + y * src_width,
                 width);
    }

    ffmpeg_encoder_set_frame_color(frame, color_format);
    return 0;
}

static void
ffmpeg_dmabuf_descriptor_free(void *opaque, uint8_t *data)
{
    (void) opaque;
    av_free(data);
}

static AVFrame *
ffmpeg_encoder_create_dmabuf_frame(
    const struct xrdp_encoder_dmabuf_surface *surface,
    AVBufferRef *hw_frames_ctx)
{
    AVDRMFrameDescriptor *desc;
    AVBufferRef *desc_buf;
    AVFrame *frame;

    if (surface == NULL || surface->fd < 0 || surface->width <= 0 ||
            surface->height <= 0 || surface->stride <= 0 ||
            surface->size <= 0)
    {
        return NULL;
    }

    frame = av_frame_alloc();
    if (frame == NULL)
    {
        return NULL;
    }

    desc = av_mallocz(sizeof(*desc));
    if (desc == NULL)
    {
        av_frame_free(&frame);
        return NULL;
    }

    desc->nb_objects = 1;
    desc->objects[0].fd = surface->fd;
    desc->objects[0].size = surface->size;
    desc->objects[0].format_modifier = DRM_FORMAT_MOD_INVALID;
    desc->nb_layers = 1;
    desc->layers[0].format = surface->fourcc;
    desc->layers[0].nb_planes = 1;
    desc->layers[0].planes[0].object_index = 0;
    desc->layers[0].planes[0].offset = 0;
    desc->layers[0].planes[0].pitch = surface->stride;

    desc_buf = av_buffer_create((uint8_t *) desc, sizeof(*desc),
                                ffmpeg_dmabuf_descriptor_free, NULL, 0);
    if (desc_buf == NULL)
    {
        av_free(desc);
        av_frame_free(&frame);
        return NULL;
    }

    frame->format = AV_PIX_FMT_DRM_PRIME;
    frame->width = surface->width;
    frame->height = surface->height;
    if (hw_frames_ctx != NULL)
    {
        frame->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
        if (frame->hw_frames_ctx == NULL)
        {
            av_buffer_unref(&desc_buf);
            av_frame_free(&frame);
            return NULL;
        }
    }
    frame->buf[0] = desc_buf;
    frame->data[0] = (uint8_t *) desc;
    frame->linesize[0] = surface->stride;
    return frame;
}

static int
ffmpeg_encoder_set_sink_pix_fmt(AVFilterContext *sink_ctx,
                                enum AVPixelFormat pix_fmt)
{
    int error;

    error = av_opt_set_bin(sink_ctx, "pix_fmts",
                           (const uint8_t *) &pix_fmt,
                           sizeof(pix_fmt),
                           AV_OPT_SEARCH_CHILDREN);
    if (error >= 0 || error != AVERROR_OPTION_NOT_FOUND)
    {
        return error;
    }

    return av_opt_set_array(sink_ctx, "pixel_formats",
                            AV_OPT_SEARCH_CHILDREN | AV_OPT_ARRAY_REPLACE,
                            0, 1, AV_OPT_TYPE_PIXEL_FMT, &pix_fmt);
}

static int
ffmpeg_encoder_create_buffer_sink(struct ffmpeg_encoder *fe,
                                  const AVFilter *buffersink,
                                  enum AVPixelFormat sink_fmt)
{
    int error;

    fe->buffer_sink_ctx = avfilter_graph_alloc_filter(fe->filter_graph,
                                                      buffersink, "sink");
    if (fe->buffer_sink_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "avfilter_graph_alloc_filter(buffersink) failed");
        return 1;
    }

    error = ffmpeg_encoder_set_sink_pix_fmt(fe->buffer_sink_ctx, sink_fmt);
    if (error < 0)
    {
        return ffmpeg_log_error("configure buffersink pixel format", error);
    }

    error = avfilter_init_str(fe->buffer_sink_ctx, NULL);
    if (error < 0)
    {
        return ffmpeg_log_error("avfilter_init_str(buffersink)", error);
    }

    return 0;
}

static int
ffmpeg_encoder_create_buffer_src(struct ffmpeg_encoder *fe,
                                 const AVFilter *buffersrc,
                                 enum AVPixelFormat src_fmt,
                                 AVBufferSrcParameters *src_params,
                                 const char *src_args,
                                 int fps_num, int fps_den)
{
    char framerate[32];
    char time_base[32];
    int error;
    const char *src_fmt_name;

    src_fmt_name = av_get_pix_fmt_name(src_fmt);
    LOG(LOG_LEVEL_INFO,
        "FFmpeg buffer source setup: src_fmt=%s(%d) drm_prime=%d input=%s path=%s",
        (src_fmt_name != NULL) ? src_fmt_name : "unknown",
        (int) src_fmt, (int) AV_PIX_FMT_DRM_PRIME,
        ffmpeg_input_name(fe->input_type),
        ffmpeg_path_name(fe->active_path));

    if (src_params == NULL || src_params->hw_frames_ctx == NULL)
    {
        error = avfilter_graph_create_filter(&fe->buffer_src_ctx, buffersrc,
                                             "src", src_args, NULL,
                                             fe->filter_graph);
        if (error < 0)
        {
            return ffmpeg_log_error("avfilter_graph_create_filter(buffer)",
                                    error);
        }

        error = av_buffersrc_parameters_set(fe->buffer_src_ctx, src_params);
        if (error < 0)
        {
            return ffmpeg_log_error("av_buffersrc_parameters_set", error);
        }

        return 0;
    }

    fe->buffer_src_ctx = avfilter_graph_alloc_filter(fe->filter_graph,
                                                     buffersrc, "src");
    if (fe->buffer_src_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "avfilter_graph_alloc_filter(buffer) failed");
        return 1;
    }

    g_snprintf(time_base, sizeof(time_base), "%d/%d", fps_den, fps_num);
    g_snprintf(framerate, sizeof(framerate), "%d/%d", fps_num, fps_den);
    av_opt_set_int(fe->buffer_src_ctx, "width", fe->input_width,
                   AV_OPT_SEARCH_CHILDREN);
    av_opt_set_int(fe->buffer_src_ctx, "height", fe->input_height,
                   AV_OPT_SEARCH_CHILDREN);
    av_opt_set(fe->buffer_src_ctx, "pix_fmt", src_fmt_name,
               AV_OPT_SEARCH_CHILDREN);
    av_opt_set(fe->buffer_src_ctx, "time_base", time_base,
               AV_OPT_SEARCH_CHILDREN);
    av_opt_set(fe->buffer_src_ctx, "pixel_aspect", "1/1",
               AV_OPT_SEARCH_CHILDREN);
    av_opt_set(fe->buffer_src_ctx, "frame_rate", framerate,
               AV_OPT_SEARCH_CHILDREN);

    error = av_buffersrc_parameters_set(fe->buffer_src_ctx, src_params);
    if (error < 0)
    {
        return ffmpeg_log_error("av_buffersrc_parameters_set", error);
    }

    error = avfilter_init_str(fe->buffer_src_ctx, NULL);
    if (error < 0)
    {
        return ffmpeg_log_error("avfilter_init_str(buffer)", error);
    }

    return 0;
}

static int
ffmpeg_filter_needs_hw_device(const AVFilterContext *filter_ctx)
{
    const char *name;

    if (filter_ctx == NULL || filter_ctx->filter == NULL)
    {
        return 0;
    }

    if ((filter_ctx->filter->flags & AVFILTER_FLAG_HWDEVICE) != 0)
    {
        return 1;
    }

    name = filter_ctx->filter->name;
    if (name == NULL)
    {
        return 0;
    }

    return (g_strcmp(name, "hwmap") == 0 ||
            g_strcmp(name, "hwupload") == 0 ||
            g_strcmp(name, "scale_vaapi") == 0 ||
            g_strcmp(name, "scale_vulkan") == 0);
}

static int
ffmpeg_encoder_set_graph_hw_device(struct ffmpeg_encoder *fe)
{
    AVFilterContext *filter_ctx;
    unsigned int index;

    if (fe == NULL || fe->filter_graph == NULL ||
            fe->graph_device_type == AV_HWDEVICE_TYPE_NONE)
    {
        return 0;
    }

    for (index = 0; index < fe->filter_graph->nb_filters; ++index)
    {
        filter_ctx = fe->filter_graph->filters[index];
        if (ffmpeg_filter_needs_hw_device(filter_ctx))
        {
            AVBufferRef *device_ctx;

            if (ffmpeg_encoder_get_device(fe, fe->graph_device_type,
                                          &device_ctx) != 0)
            {
                return 1;
            }

            av_buffer_unref(&filter_ctx->hw_device_ctx);
            filter_ctx->hw_device_ctx = av_buffer_ref(device_ctx);
            if (filter_ctx->hw_device_ctx == NULL)
            {
                LOG(LOG_LEVEL_ERROR,
                    "av_buffer_ref(hw_device_ctx) failed for filter %s",
                    filter_ctx->filter->name);
                return 1;
            }
        }
    }

    return 0;
}

static int
ffmpeg_encoder_create_filter_graph(struct ffmpeg_encoder *fe,
                                   enum ffmpeg_encoder_input_type input_type,
                                   enum AVPixelFormat src_fmt,
                                   enum AVPixelFormat sink_fmt,
                                   const char *filter_desc,
                                   int fps_num, int fps_den)
{
    enum AVColorRange color_range;
    enum AVColorSpace color_space;
    AVBufferSrcParameters *src_params;
    AVFilterInOut *inputs;
    AVFilterInOut *outputs;
    const AVFilter *buffersink;
    const AVFilter *buffersrc;
    const char *src_fmt_name;
    char src_args[256];
    int error;

    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");
    if (buffersrc == NULL || buffersink == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "FFmpeg buffer filters are unavailable");
        return 1;
    }

    fe->filter_graph = avfilter_graph_alloc();
    if (fe->filter_graph == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "avfilter_graph_alloc failed");
        return 1;
    }

    if (src_fmt == AV_PIX_FMT_NONE)
    {
        LOG(LOG_LEVEL_ERROR, "FFmpeg buffer source has unspecified pixel "
            "format");
        return 1;
    }

    src_fmt_name = av_get_pix_fmt_name(src_fmt);
    if (src_fmt_name == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "FFmpeg buffer source has unsupported pixel "
            "format %d", (int) src_fmt);
        return 1;
    }

    g_snprintf(src_args, sizeof(src_args),
               "video_size=%dx%d:pix_fmt=%s:time_base=%d/%d:"
               "pixel_aspect=1/1",
               fe->input_width, fe->input_height, src_fmt_name,
               fps_den, fps_num);
    if (ffmpeg_encoder_create_buffer_sink(fe, buffersink, sink_fmt) != 0)
    {
        return 1;
    }

    src_params = av_buffersrc_parameters_alloc();
    if (src_params == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "av_buffersrc_parameters_alloc failed");
        return 1;
    }

    src_params->format = src_fmt;
    src_params->width = fe->input_width;
    src_params->height = fe->input_height;
    src_params->time_base.num = fps_den;
    src_params->time_base.den = fps_num;
    src_params->frame_rate.num = fps_num;
    src_params->frame_rate.den = fps_den;
    src_params->sample_aspect_ratio.num = 1;
    src_params->sample_aspect_ratio.den = 1;
    if (src_fmt == AV_PIX_FMT_DRM_PRIME)
    {
        src_params->hw_frames_ctx = fe->drm_frames_ctx;
    }
    if (src_fmt == AV_PIX_FMT_NV12)
    {
        ffmpeg_get_color_props(fe->color_format, &color_range, &color_space);
        src_params->color_range = color_range;
        src_params->color_space = color_space;
    }
    error = ffmpeg_encoder_create_buffer_src(fe, buffersrc, src_fmt,
                                             src_params, src_args,
                                             fps_num, fps_den);
    av_free(src_params);
    if (error != 0)
    {
        return 1;
    }

    inputs = avfilter_inout_alloc();
    outputs = avfilter_inout_alloc();
    if (inputs == NULL || outputs == NULL)
    {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        LOG(LOG_LEVEL_ERROR, "avfilter_inout_alloc failed");
        return 1;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = fe->buffer_src_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = fe->buffer_sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    error = avfilter_graph_parse_ptr(fe->filter_graph, filter_desc,
                                     &inputs, &outputs, NULL);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (error < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to parse filter graph: %s",
            filter_desc);
        return ffmpeg_log_error("avfilter_graph_parse_ptr", error);
    }

    if (fe->graph_device_type != AV_HWDEVICE_TYPE_NONE &&
            ffmpeg_encoder_set_graph_hw_device(fe) != 0)
    {
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "FFmpeg configuring filter graph (%s/%s): %s",
        ffmpeg_path_name(fe->active_path),
        ffmpeg_input_name(input_type),
        filter_desc);
    error = avfilter_graph_config(fe->filter_graph, NULL);
    if (error < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to configure filter graph: %s",
            filter_desc);
        return ffmpeg_log_error("avfilter_graph_config", error);
    }

    LOG(LOG_LEVEL_INFO, "FFmpeg H.264 filter graph (%s/%s): %s",
        ffmpeg_path_name(fe->active_path),
        ffmpeg_input_name(input_type),
        filter_desc);
    return 0;
}

static int
ffmpeg_encoder_build_filter_desc(struct ffmpeg_encoder *fe,
                                 char *filter_desc, unsigned int filter_desc_bytes)
{
    const char *transfer_name;
    const char *primaries_name;
    const char *hwmap_device_name;
    const char *matrix_name;
    const char *range_name;
    const char *scale_filter;

    if (fe == NULL || filter_desc == NULL || filter_desc_bytes == 0)
    {
        return 1;
    }

    if (fe->active_path == XTC_FFMPEG_PATH_SOFTWARE)
    {
        if (fe->input_type != FFMPEG_INPUT_DMABUF)
        {
            LOG(LOG_LEVEL_ERROR, "No FFmpeg filter graph available for %s/%s",
                ffmpeg_path_name(fe->active_path),
                ffmpeg_input_name(fe->input_type));
            return 1;
        }
        if (ffmpeg_get_color_names(fe->color_format, &range_name,
                                   &matrix_name, &primaries_name,
                                   &transfer_name) != 0)
        {
            return 1;
        }

        g_snprintf(filter_desc, filter_desc_bytes,
                   "scale=w=%d:h=%d:flags=bilinear+accurate_rnd:"
                   "in_range=full:out_range=%s:out_color_matrix=%s:"
                   "out_primaries=%s:out_transfer=%s,"
                   "format=nv12,"
                   "pad=w=%d:h=%d:x=0:y=0:color=black",
                   fe->visible_width, fe->visible_height,
                   range_name, matrix_name, primaries_name, transfer_name,
                   fe->coded_width, fe->coded_height);
        return 0;
    }

    scale_filter = ffmpeg_path_scale_filter(fe->active_path);
    if (scale_filter == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No FFmpeg filter graph available for %s/%s",
            ffmpeg_path_name(fe->active_path),
            ffmpeg_input_name(fe->input_type));
        return 1;
    }

    if (fe->input_type == FFMPEG_INPUT_SHM)
    {
        g_snprintf(filter_desc, filter_desc_bytes,
                   "hwupload,%s=w=%d:h=%d:format=nv12",
                   scale_filter, fe->coded_width, fe->coded_height);
        return 0;
    }

    if (fe->input_type == FFMPEG_INPUT_DMABUF)
    {
        hwmap_device_name = ffmpeg_path_hwmap_device_name(fe->active_path);
        if (hwmap_device_name == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "No FFmpeg hwmap device available for %s",
                ffmpeg_path_name(fe->active_path));
            return 1;
        }

        g_snprintf(filter_desc, filter_desc_bytes,
                   "hwmap=derive_device=%s:mode=read+write,"
                   "%s=w=%d:h=%d:format=nv12",
                   hwmap_device_name, scale_filter,
                   fe->coded_width, fe->coded_height);
        return 0;
    }

    LOG(LOG_LEVEL_ERROR, "No FFmpeg filter graph available for %s/%s",
        ffmpeg_path_name(fe->active_path),
        ffmpeg_input_name(fe->input_type));
    return 1;
}

static int
ffmpeg_encoder_attach_hw_frames_ctx(struct ffmpeg_encoder *fe)
{
    AVBufferRef *hw_frames_ctx;

    hw_frames_ctx = av_buffersink_get_hw_frames_ctx(fe->buffer_sink_ctx);
    if (hw_frames_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "%s filter graph returned no hw_frames_ctx",
            ffmpeg_path_name(fe->active_path));
        return 1;
    }

    fe->codec_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
    if (fe->codec_ctx->hw_frames_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "av_buffer_ref(hw_frames_ctx) failed");
        return 1;
    }

    return 0;
}

static int
ffmpeg_encoder_configure_hardware_codec(
    struct ffmpeg_encoder *fe,
    const struct xrdp_tconfig_gfx_ffmpeg_param *param,
    const char *profile_name,
    const char *entropy_coder)
{
    const char *vulkan_tune;

    fe->codec_ctx->pix_fmt = ffmpeg_path_sink_format(fe->active_path);
    av_opt_set(fe->codec_ctx->priv_data, "profile", profile_name, 0);
    av_opt_set(fe->codec_ctx->priv_data, "coder", entropy_coder, 0);
    av_opt_set_int(fe->codec_ctx->priv_data, "async_depth", 1, 0);

    switch (fe->active_path)
    {
        case XTC_FFMPEG_PATH_VAAPI:
            av_opt_set_int(fe->codec_ctx->priv_data, "aud", 1, 0);
            av_opt_set(fe->codec_ctx->priv_data, "sei",
                       "identifier+recovery_point", 0);
            av_opt_set(fe->codec_ctx->priv_data, "rc_mode",
                       (param->vbv_max_bitrate > 0) ? "CBR" : "auto", 0);
            break;

        case XTC_FFMPEG_PATH_VULKAN:
            vulkan_tune = (g_strcasecmp(param->tune, "zerolatency") == 0 ||
                           g_strcasecmp(param->preset, "ultrafast") == 0) ?
                          "ull" : "ll";
            av_opt_set(fe->codec_ctx->priv_data, "rc_mode",
                       (param->vbv_max_bitrate > 0) ? "cbr" : "auto", 0);
            av_opt_set(fe->codec_ctx->priv_data, "tune", vulkan_tune, 0);
            av_opt_set(fe->codec_ctx->priv_data, "usage", "conference", 0);
            av_opt_set(fe->codec_ctx->priv_data, "content", "desktop", 0);
            av_opt_set(fe->codec_ctx->priv_data, "units",
                       "aud+identifier+recovery", 0);
            break;

        default:
            LOG(LOG_LEVEL_ERROR, "Unsupported FFmpeg hardware path %d",
                (int) fe->active_path);
            return 1;
    }

    return ffmpeg_encoder_attach_hw_frames_ctx(fe);
}

static int
ffmpeg_encoder_open_codec(struct ffmpeg_encoder *fe)
{
    const struct xrdp_tconfig_gfx_ffmpeg_param *param;
    const char *encoder_name;
    const char *entropy_coder;
    const char *profile_name;
    const AVCodec *codec;
    int bit_rate;
    int buffer_size;
    int error;
    int fps_den;
    int fps_num;
    int gop_size;
    int max_rate;

    param = ffmpeg_get_param(fe, fe->connection_type);
    entropy_coder = ffmpeg_get_entropy_coder(param);
    ffmpeg_get_framerate(fe, fe->connection_type, &fps_num, &fps_den);
    ffmpeg_get_rate_control(fe, fe->connection_type, &bit_rate, &max_rate,
                            &buffer_size);
    gop_size = (fps_den > 0) ? (fps_num / fps_den) : FFMPEG_DEFAULT_GOP_SIZE;
    if (gop_size < 1)
    {
        gop_size = FFMPEG_DEFAULT_GOP_SIZE;
    }

    encoder_name = ffmpeg_path_encoder_name(fe->active_path);
    profile_name = ffmpeg_get_profile_name(param,
                                           ffmpeg_path_is_hardware(fe->active_path));
    if (encoder_name == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Unsupported FFmpeg path %d",
            (int) fe->active_path);
        return 1;
    }

    codec = avcodec_find_encoder_by_name(encoder_name);
    if (codec == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "FFmpeg encoder %s not found", encoder_name);
        return 1;
    }

    fe->codec_ctx = avcodec_alloc_context3(codec);
    if (fe->codec_ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "avcodec_alloc_context3(%s) failed", encoder_name);
        return 1;
    }

    fe->codec_ctx->width = fe->coded_width;
    fe->codec_ctx->height = fe->coded_height;
    fe->codec_ctx->time_base.num = fps_den;
    fe->codec_ctx->time_base.den = fps_num;
    fe->codec_ctx->framerate.num = fps_num;
    fe->codec_ctx->framerate.den = fps_den;
    fe->codec_ctx->gop_size = gop_size;
    fe->codec_ctx->max_b_frames = 0;
    fe->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    fe->codec_ctx->bit_rate = bit_rate;
    fe->codec_ctx->rc_max_rate = max_rate;
    fe->codec_ctx->rc_buffer_size = buffer_size;

    switch (fe->active_path)
    {
        case XTC_FFMPEG_PATH_SOFTWARE:
            fe->codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
            fe->codec_ctx->thread_count =
                (param->threads > 0) ? param->threads : FFMPEG_DEFAULT_THREADS;
            av_opt_set(fe->codec_ctx->priv_data, "preset", param->preset, 0);
            av_opt_set(fe->codec_ctx->priv_data, "tune", param->tune, 0);
            av_opt_set(fe->codec_ctx->priv_data, "profile", profile_name, 0);
            av_opt_set(fe->codec_ctx->priv_data, "coder", entropy_coder, 0);
            av_opt_set_int(fe->codec_ctx->priv_data, "aud", 1, 0);
            av_opt_set(fe->codec_ctx->priv_data, "x264-params",
                       "bframes=0:repeat-headers=1:annexb=1:aud=1:"
                       "scenecut=0:rc-lookahead=0:sync-lookahead=0:force-cfr=1",
                       0);
            av_opt_set_int(fe->codec_ctx->priv_data, "forced-idr", 1, 0);
            break;

        case XTC_FFMPEG_PATH_VAAPI:
        case XTC_FFMPEG_PATH_VULKAN:
            if (ffmpeg_encoder_configure_hardware_codec(fe, param,
                                                        profile_name,
                                                        entropy_coder) != 0)
            {
                return 1;
            }
            break;
    }

    error = avcodec_open2(fe->codec_ctx, codec, NULL);
    if (error < 0)
    {
        return ffmpeg_log_error("avcodec_open2", error);
    }

    LOG(LOG_LEVEL_INFO,
        "FFmpeg H.264 configured: path=%s input=%s visible=%dx%d "
        "coded=%dx%d bitrate=%d buffer=%d fps=%d/%d profile=%s",
        ffmpeg_path_name(fe->active_path),
        ffmpeg_input_name(fe->input_type),
        fe->visible_width, fe->visible_height,
        fe->coded_width, fe->coded_height,
        bit_rate, buffer_size,
        fps_num, fps_den, profile_name);
    return 0;
}

static int
ffmpeg_encoder_prepare_graph(struct ffmpeg_encoder *fe,
                             enum AVPixelFormat src_fmt,
                             enum AVPixelFormat sink_fmt);

static int
ffmpeg_encoder_prepare_software(struct ffmpeg_encoder *fe,
                                unsigned int dmabuf_fourcc)
{
    enum AVPixelFormat src_fmt;

    fe->graph_device_type = AV_HWDEVICE_TYPE_NONE;

    if (fe->input_type == FFMPEG_INPUT_SHM)
    {
        if (ffmpeg_encoder_alloc_sw_frame(fe, fe->coded_width, fe->coded_height,
                                          fe->color_format) != 0)
        {
            return 1;
        }

        return ffmpeg_encoder_open_codec(fe);
    }

    if (fe->input_type != FFMPEG_INPUT_DMABUF ||
            ffmpeg_encoder_prepare_dmabuf_frames_ctx(fe, dmabuf_fourcc) != 0)
    {
        return 1;
    }

    src_fmt = ffmpeg_dmabuf_fourcc_to_sw_format(dmabuf_fourcc);
    if (src_fmt == AV_PIX_FMT_NONE)
    {
        LOG(LOG_LEVEL_ERROR, "Unsupported DMA-BUF fourcc 0x%8.8x",
            dmabuf_fourcc);
        return 1;
    }

    return ffmpeg_encoder_prepare_graph(fe, src_fmt, AV_PIX_FMT_NV12);
}

static int
ffmpeg_encoder_prepare_graph(struct ffmpeg_encoder *fe,
                             enum AVPixelFormat src_fmt,
                             enum AVPixelFormat sink_fmt)
{
    char filter_desc[FFMPEG_MAX_FILTER_DESC];
    int fps_den;
    int fps_num;

    if (ffmpeg_encoder_build_filter_desc(fe, filter_desc,
                                         sizeof(filter_desc)) != 0)
    {
        return 1;
    }

    ffmpeg_get_framerate(fe, fe->connection_type, &fps_num, &fps_den);
    if (ffmpeg_encoder_create_filter_graph(fe, fe->input_type,
                                           src_fmt, sink_fmt,
                                           filter_desc,
                                           fps_num, fps_den) != 0)
    {
        return 1;
    }

    return ffmpeg_encoder_open_codec(fe);
}

static int
ffmpeg_encoder_prepare_hardware(struct ffmpeg_encoder *fe,
                                unsigned int dmabuf_fourcc)
{
    AVBufferRef *device_ctx;
    enum AVPixelFormat sink_fmt;

    fe->graph_device_type = ffmpeg_path_device_type(fe->active_path);
    sink_fmt = ffmpeg_path_sink_format(fe->active_path);
    if (fe->graph_device_type == AV_HWDEVICE_TYPE_NONE ||
            sink_fmt == AV_PIX_FMT_NONE)
    {
        LOG(LOG_LEVEL_ERROR, "Unsupported FFmpeg hardware path %d",
            (int) fe->active_path);
        return 1;
    }

    if (ffmpeg_encoder_get_device(fe, fe->graph_device_type,
                                  &device_ctx) != 0)
    {
        return 1;
    }

    if (fe->input_type == FFMPEG_INPUT_DMABUF &&
            ffmpeg_encoder_prepare_dmabuf_frames_ctx(fe, dmabuf_fourcc) != 0)
    {
        return 1;
    }

    return ffmpeg_encoder_prepare_graph(fe,
                                        ffmpeg_encoder_get_graph_src_format(fe),
                                        sink_fmt);
}

static int
ffmpeg_encoder_reconfigure(struct ffmpeg_encoder *fe,
                           enum ffmpeg_encoder_input_type input_type,
                           int input_width, int input_height,
                           int visible_width, int visible_height,
                           int color_format,
                           unsigned int dmabuf_fourcc,
                           int connection_type)
{
    int wanted_coded_width;
    int wanted_coded_height;

    ffmpeg_encoder_get_coded_size(fe, input_type,
                                  visible_width, visible_height,
                                  &wanted_coded_width, &wanted_coded_height);

    if (fe->codec_ctx != NULL &&
            fe->input_type == input_type &&
            fe->input_width == input_width &&
            fe->input_height == input_height &&
            fe->visible_width == visible_width &&
            fe->visible_height == visible_height &&
            fe->coded_width == wanted_coded_width &&
            fe->coded_height == wanted_coded_height &&
            fe->color_format == color_format &&
            fe->dmabuf_fourcc == dmabuf_fourcc &&
            fe->connection_type == ffmpeg_connection_type(connection_type))
    {
        return 0;
    }

    ffmpeg_encoder_reset_runtime(fe);
    fe->input_type = input_type;
    fe->input_width = input_width;
    fe->input_height = input_height;
    fe->visible_width = visible_width;
    fe->visible_height = visible_height;
    fe->coded_width = wanted_coded_width;
    fe->coded_height = wanted_coded_height;
    fe->color_format = color_format;
    fe->dmabuf_fourcc = dmabuf_fourcc;
    fe->connection_type = ffmpeg_connection_type(connection_type);

    switch (fe->active_path)
    {
        case XTC_FFMPEG_PATH_SOFTWARE:
            return ffmpeg_encoder_prepare_software(fe, dmabuf_fourcc);

        case XTC_FFMPEG_PATH_VAAPI:
        case XTC_FFMPEG_PATH_VULKAN:
            return ffmpeg_encoder_prepare_hardware(fe, dmabuf_fourcc);
        default:
            break;
    }

    return 1;
}

static AVFrame *
ffmpeg_encoder_clone_frame(const AVFrame *frame)
{
    AVFrame *clone;
    int error;

    clone = av_frame_alloc();
    if (clone == NULL)
    {
        return NULL;
    }

    error = av_frame_ref(clone, frame);
    if (error < 0)
    {
        av_frame_free(&clone);
        return NULL;
    }

    return clone;
}

static int
ffmpeg_encoder_send_frame(struct ffmpeg_encoder *fe, AVFrame *frame,
                          int keyframe,
                          char *cdata, int *cdata_bytes,
                          int *flags_ptr)
{
    AVPacket *pkt;
    int error;
    int got_packet;
    int local_flags;
    int total_bytes;

    pkt = av_packet_alloc();
    if (pkt == NULL)
    {
        av_frame_free(&frame);
        return 1;
    }

    if (frame->pts == AV_NOPTS_VALUE)
    {
        frame->pts = fe->pts++;
    }
    else
    {
        fe->pts = frame->pts + 1;
    }

    ffmpeg_encoder_set_frame_color(frame, fe->color_format);
    ffmpeg_encoder_set_frame_crop(fe, frame);
    if (keyframe)
    {
        frame->pict_type = AV_PICTURE_TYPE_I;
        frame->flags |= AV_FRAME_FLAG_KEY;
    }

    error = avcodec_send_frame(fe->codec_ctx, frame);
    av_frame_free(&frame);
    if (error < 0)
    {
        av_packet_free(&pkt);
        return ffmpeg_log_error("avcodec_send_frame", error);
    }

    local_flags = 0;
    total_bytes = 0;
    got_packet = 0;
    for (;;)
    {
        error = avcodec_receive_packet(fe->codec_ctx, pkt);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
        {
            break;
        }
        if (error < 0)
        {
            av_packet_free(&pkt);
            return ffmpeg_log_error("avcodec_receive_packet", error);
        }
        if (total_bytes + pkt->size > *cdata_bytes)
        {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            LOG(LOG_LEVEL_ERROR, "FFmpeg packet overflow (%d > %d)",
                total_bytes + pkt->size, *cdata_bytes);
            return 1;
        }
        g_memcpy(cdata + total_bytes, pkt->data, pkt->size);
        total_bytes += pkt->size;
        if ((pkt->flags & AV_PKT_FLAG_KEY) != 0)
        {
            local_flags |= ENCODE_COMPLETE;
        }
        got_packet = 1;
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    if (!got_packet)
    {
        LOG(LOG_LEVEL_WARNING, "FFmpeg %s encode produced no output packet",
            ffmpeg_path_name(fe->active_path));
        return 1;
    }

    *cdata_bytes = total_bytes;
    if (flags_ptr != NULL)
    {
        *flags_ptr = local_flags;
    }
    return 0;
}

static int
ffmpeg_encoder_push_filter_frame(struct ffmpeg_encoder *fe,
                                 AVFrame *src_frame,
                                 int keyframe,
                                 char *cdata, int *cdata_bytes,
                                 int *flags_ptr)
{
    AVFrame *dst_frame;
    int error;

    error = av_buffersrc_write_frame(fe->buffer_src_ctx, src_frame);
    if (error < 0)
    {
        return ffmpeg_log_error("av_buffersrc_write_frame", error);
    }

    dst_frame = av_frame_alloc();
    if (dst_frame == NULL)
    {
        return 1;
    }

    error = av_buffersink_get_frame(fe->buffer_sink_ctx, dst_frame);
    if (error < 0)
    {
        av_frame_free(&dst_frame);
        return ffmpeg_log_error("av_buffersink_get_frame", error);
    }

    return ffmpeg_encoder_send_frame(fe, dst_frame, keyframe,
                                     cdata, cdata_bytes, flags_ptr);
}

static int
ffmpeg_encoder_encode_sw_dmabuf_frame(struct ffmpeg_encoder *fe,
                                      AVFrame *src_frame,
                                      int keyframe,
                                      char *cdata, int *cdata_bytes,
                                      int *flags_ptr)
{
    AVFrame *enc_frame;
    enum AVPixelFormat src_format;
    int error;

    if (fe == NULL || src_frame == NULL)
    {
        return 1;
    }

    src_format = ffmpeg_dmabuf_fourcc_to_sw_format(fe->dmabuf_fourcc);
    if (src_format == AV_PIX_FMT_NONE)
    {
        LOG(LOG_LEVEL_ERROR, "Unsupported DMA-BUF fourcc 0x%8.8x",
            fe->dmabuf_fourcc);
        return 1;
    }

    if (ffmpeg_encoder_alloc_dmabuf_sw_frame(fe, src_format,
                                             src_frame->width,
                                             src_frame->height) != 0)
    {
        return 1;
    }

    error = av_frame_make_writable(fe->dmabuf_sw_frame);
    if (error < 0)
    {
        return ffmpeg_log_error("av_frame_make_writable", error);
    }

    error = av_hwframe_transfer_data(fe->dmabuf_sw_frame, src_frame, 0);
    if (error < 0)
    {
        return ffmpeg_log_error("av_hwframe_transfer_data", error);
    }
    fe->dmabuf_sw_frame->pts = src_frame->pts;
    enc_frame = ffmpeg_encoder_clone_frame(fe->dmabuf_sw_frame);
    if (enc_frame == NULL)
    {
        return 1;
    }

    return ffmpeg_encoder_push_filter_frame(fe, enc_frame, keyframe,
                                            cdata, cdata_bytes, flags_ptr);
}

static int
ffmpeg_encoder_try_software_fallback(struct ffmpeg_encoder *fe,
                                     enum ffmpeg_encoder_input_type input_type,
                                     int input_width, int input_height,
                                     int visible_width, int visible_height,
                                     int color_format,
                                     unsigned int dmabuf_fourcc,
                                     int connection_type,
                                     const char *reason)
{
    if (fe->active_path == XTC_FFMPEG_PATH_SOFTWARE)
    {
        return 1;
    }

    LOG(LOG_LEVEL_WARNING, "FFmpeg %s path failed (%s); falling back to "
        "software", ffmpeg_path_name(fe->active_path), reason);
    fe->active_path = XTC_FFMPEG_PATH_SOFTWARE;
    ffmpeg_encoder_reset_runtime(fe);
    return ffmpeg_encoder_reconfigure(fe, input_type,
                                      input_width, input_height,
                                      visible_width, visible_height,
                                      color_format, dmabuf_fourcc,
                                      connection_type);
}

static int
ffmpeg_encoder_ensure_ready(struct ffmpeg_encoder *fe,
                            enum ffmpeg_encoder_input_type input_type,
                            int input_width, int input_height,
                            int visible_width, int visible_height,
                            int color_format,
                            unsigned int dmabuf_fourcc,
                            int connection_type)
{
    int error;

    error = ffmpeg_encoder_reconfigure(fe, input_type,
                                       input_width, input_height,
                                       visible_width, visible_height,
                                       color_format, dmabuf_fourcc,
                                       connection_type);
    if (error == 0)
    {
        return 0;
    }

    return ffmpeg_encoder_try_software_fallback(fe, input_type,
                                                input_width, input_height,
                                                visible_width, visible_height,
                                                color_format, dmabuf_fourcc,
                                                connection_type,
                                                "reconfigure");
}

static int
ffmpeg_encoder_encode_sw_frame(struct ffmpeg_encoder *fe,
                               const char *data,
                               int width, int height,
                               int src_width, int src_height,
                               int color_format,
                               int keyframe,
                               char *cdata, int *cdata_bytes,
                               int *flags_ptr)
{
    AVFrame *frame;

    if (ffmpeg_encoder_copy_nv12(fe->sw_frame, data, width, height,
                                 src_width, src_height,
                                 color_format, 1) != 0)
    {
        return 1;
    }

    frame = ffmpeg_encoder_clone_frame(fe->sw_frame);
    if (frame == NULL)
    {
        return 1;
    }

    return ffmpeg_encoder_send_frame(fe, frame, keyframe,
                                     cdata, cdata_bytes, flags_ptr);
}

static int
ffmpeg_encoder_encode_shm_internal(struct ffmpeg_encoder *fe,
                                   int width, int height,
                                   int src_width, int src_height,
                                   int color_format,
                                   const char *data,
                                   int connection_type,
                                   char *cdata, int *cdata_bytes,
                                   int *flags_ptr)
{
    if (ffmpeg_encoder_ensure_ready(fe, FFMPEG_INPUT_SHM,
                                    width, height,
                                    width, height,
                                    color_format, 0,
                                    connection_type) != 0)
    {
        return 1;
    }

    if (fe->active_path == XTC_FFMPEG_PATH_SOFTWARE)
    {
        return ffmpeg_encoder_encode_sw_frame(fe, data, width, height,
                                              src_width, src_height,
                                              color_format, 0,
                                              cdata, cdata_bytes, flags_ptr);
    }

    if (ffmpeg_encoder_alloc_sw_frame(fe, width, height, color_format) != 0)
    {
        return 1;
    }
    if (ffmpeg_encoder_copy_nv12(fe->sw_frame, data, width, height,
                                 src_width, src_height,
                                 color_format, 0) != 0)
    {
        return 1;
    }
    fe->sw_frame->pts = fe->pts++;
    return ffmpeg_encoder_push_filter_frame(fe, fe->sw_frame, 0,
                                            cdata, cdata_bytes, flags_ptr);
}

static int
ffmpeg_encoder_encode_dmabuf_internal(
    struct ffmpeg_encoder *fe,
    const struct xrdp_encoder_dmabuf_surface *surface,
    int width, int height, int format,
    int connection_type, int keyframe,
    char *cdata, int *cdata_bytes,
    int *flags_ptr)
{
    AVFrame *src_frame;
    int error;

    if (surface == NULL)
    {
        return 1;
    }

    error = ffmpeg_encoder_ensure_ready(fe, FFMPEG_INPUT_DMABUF,
                                        surface->width, surface->height,
                                        width, height, format,
                                        surface->fourcc,
                                        connection_type);
    if (error != 0)
    {
        return 1;
    }

    src_frame = ffmpeg_encoder_create_dmabuf_frame(surface,
                                                   fe->drm_frames_ctx);
    if (src_frame == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Failed to create DRM_PRIME AVFrame");
        return 1;
    }
    src_frame->pts = fe->pts++;
    if (fe->active_path == XTC_FFMPEG_PATH_SOFTWARE)
    {
        error = ffmpeg_encoder_encode_sw_dmabuf_frame(fe, src_frame,
                                                      keyframe,
                                                      cdata, cdata_bytes,
                                                      flags_ptr);
    }
    else
    {
        error = ffmpeg_encoder_push_filter_frame(fe, src_frame, keyframe,
                                                 cdata, cdata_bytes,
                                                 flags_ptr);
    }
    av_frame_free(&src_frame);
    return error;
}

int
xrdp_encoder_ffmpeg_install_ok(void)
{
    return avcodec_find_encoder_by_name("libx264") != NULL &&
           avfilter_get_by_name("buffer") != NULL &&
           avfilter_get_by_name("buffersink") != NULL &&
           avfilter_get_by_name("scale") != NULL &&
           avfilter_get_by_name("format") != NULL &&
           avfilter_get_by_name("pad") != NULL;
}

void *
xrdp_encoder_ffmpeg_create(void)
{
    struct ffmpeg_encoder *fe;
    struct xrdp_tconfig_gfx gfxconfig;

    fe = g_new0(struct ffmpeg_encoder, 1);
    if (fe == NULL)
    {
        return NULL;
    }

    tconfig_load_gfx(GFX_CONF, &gfxconfig);
    g_memcpy(&fe->cfg, &gfxconfig.ffmpeg, sizeof(fe->cfg));
    fe->active_path = fe->cfg.path;
    fe->graph_device_type = AV_HWDEVICE_TYPE_NONE;
    fe->pts = 0;
    return fe;
}

int
xrdp_encoder_ffmpeg_delete(void *handle)
{
    struct ffmpeg_encoder *fe;

    fe = (struct ffmpeg_encoder *) handle;
    if (fe == NULL)
    {
        return 0;
    }

    ffmpeg_encoder_release(fe);
    g_free(fe);
    return 0;
}

int
xrdp_encoder_ffmpeg_encode(void *handle, int session, int left, int top,
                           int width, int height, int twidth, int theight,
                           int format, const char *data,
                           short *crects, int num_crects,
                           char *cdata, int *cdata_bytes,
                           int connection_type, int *flags_ptr)
{
    struct ffmpeg_encoder *fe;

    (void) session;
    (void) left;
    (void) top;
    (void) crects;
    (void) num_crects;

    fe = (struct ffmpeg_encoder *) handle;
    if (fe == NULL || data == NULL || width <= 0 || height <= 0 ||
            twidth <= 0 || theight <= 0 || cdata == NULL ||
            cdata_bytes == NULL)
    {
        return 1;
    }

    return ffmpeg_encoder_encode_shm_internal(fe, width, height,
                                              twidth, theight,
                                              format, data,
                                              connection_type,
                                              cdata, cdata_bytes,
                                              flags_ptr);
}

int
xrdp_encoder_ffmpeg_encode_dmabuf(
    void *handle,
    const struct xrdp_encoder_dmabuf_surface *surface,
    int width, int height, int format,
    short *crects, int num_crects,
    int connection_type, int keyframe,
    char *cdata, int *cdata_bytes,
    int *flags_ptr)
{
    struct ffmpeg_encoder *fe;

    (void) crects;
    (void) num_crects;

    fe = (struct ffmpeg_encoder *) handle;
    if (fe == NULL || surface == NULL || width <= 0 || height <= 0 ||
            cdata == NULL || cdata_bytes == NULL)
    {
        return 1;
    }

    return ffmpeg_encoder_encode_dmabuf_internal(fe, surface,
                                                 width, height, format,
                                                 connection_type, keyframe,
                                                 cdata, cdata_bytes,
                                                 flags_ptr);
}
