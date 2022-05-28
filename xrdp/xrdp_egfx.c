/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2019
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
 * MS-RDPEGFX
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arch.h"
#include "os_calls.h"
#include "parse.h"
#include "xrdp.h"
#include "xrdp_egfx.h"
#include "libxrdp.h"
#include "xrdp_channel.h"
#include <limits.h>

/******************************************************************************/
static int
xrdp_egfx_send_data(struct xrdp_egfx *egfx, const char *data, int bytes)
{
    int error;
    int to_send;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_data:");

    if (bytes <= 1500)
    {
        error = libxrdp_drdynvc_data(egfx->session, egfx->channel_id,
                                     data, bytes);
    }
    else
    {
        error = libxrdp_drdynvc_data_first(egfx->session, egfx->channel_id,
                                           data, 1500, bytes);
        data += 1500;
        bytes -= 1500;
        while ((bytes > 0) && (error == 0))
        {
            to_send = bytes;
            if (to_send > 1500)
            {
                to_send = 1500;
            }
            error = libxrdp_drdynvc_data(egfx->session, egfx->channel_id,
                                         data, to_send);
            data += to_send;
            bytes -= to_send;
        }
    }
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_create_surface(struct xrdp_egfx *egfx, int surface_id,
                              int width, int height, int pixel_format)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_create_surface:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_CREATESURFACE); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint16_le(s, surface_id);
    out_uint16_le(s, width);
    out_uint16_le(s, height);
    out_uint8(s, pixel_format);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_create_surface: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_delete_surface(struct xrdp_egfx *egfx, int surface_id)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_delete_surface:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_DELETESURFACE); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint16_le(s, surface_id);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_delete_surface: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_map_surface(struct xrdp_egfx *egfx, int surface_id,
                           int x, int y)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_map_surface:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_MAPSURFACETOOUTPUT); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint16_le(s, surface_id);
    out_uint16_le(s, 0);
    out_uint32_le(s, x);
    out_uint32_le(s, y);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_map_surface: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_fill_surface(struct xrdp_egfx *egfx, int surface_id,
                            int fill_color, int num_rects,
                            const struct xrdp_egfx_rect *rects)
{
    int error;
    int bytes;
    int index;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_fill_surface:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_SOLIDFILL); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint16_le(s, surface_id);
    out_uint32_le(s, fill_color);
    out_uint16_le(s, num_rects);
    for (index = 0; index < num_rects; index++)
    {
        out_uint16_le(s, rects[index].x1);
        out_uint16_le(s, rects[index].y1);
        out_uint16_le(s, rects[index].x2);
        out_uint16_le(s, rects[index].y2);
    }
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_fill_surface: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_surface_to_surface(struct xrdp_egfx *egfx, int src_surface_id,
                                  int dst_surface_id,
                                  const struct xrdp_egfx_rect *src_rect,
                                  int num_dst_points,
                                  const struct xrdp_egfx_point *dst_points)
{
    int error;
    int bytes;
    int index;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_surface_to_surface:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_SURFACETOSURFACE); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint16_le(s, src_surface_id);
    out_uint16_le(s, dst_surface_id);
    out_uint16_le(s, src_rect->x1);
    out_uint16_le(s, src_rect->y1);
    out_uint16_le(s, src_rect->x2);
    out_uint16_le(s, src_rect->y2);
    out_uint16_le(s, num_dst_points);
    for (index = 0; index < num_dst_points; index++)
    {
        out_uint16_le(s, dst_points[index].x);
        out_uint16_le(s, dst_points[index].y);
    }
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_surface_to_surface: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_frame_start(struct xrdp_egfx *egfx, int frame_id, int timestamp)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_frame_start:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_STARTFRAME); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint32_le(s, timestamp);
    out_uint32_le(s, frame_id);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_frame_start: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_frame_end(struct xrdp_egfx *egfx, int frame_id)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_frame_end:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_ENDFRAME); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint32_le(s, frame_id);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_frame_end: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_capsconfirm(struct xrdp_egfx *egfx, int version, int flags)
{
    int error;
    int bytes;
    struct stream *s;
    char *holdp;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_capsconfirm:");
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_CAPSCONFIRM); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    holdp = s->p;
    out_uint8s(s, 4); /* pduLength, set later */
    out_uint32_le(s, version); /* version */
    out_uint32_le(s, 4); /* capsDataLength */
    out_uint32_le(s, flags);
    s_mark_end(s);
    bytes = (int) ((s->end - holdp) + 4);
    s->p = holdp;
    out_uint32_le(s, bytes);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_capsconfirm: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_wire_to_surface1(struct xrdp_egfx *egfx, int surface_id,
                                int codec_id, int pixel_format,
                                struct xrdp_egfx_rect *dest_rect,
                                void *bitmap_data, int bitmap_data_length)
{
    int error;
    int bytes;
    int index;
    int segment_size;
    int segment_count;
    struct stream *s;
    char *hold_segment_count;
    char *bitmap_data8;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_wire_to_surface1:");
    make_stream(s);
    bytes = bitmap_data_length + 8192;
    bytes += 5 * (bitmap_data_length / 0xFFFF);
    init_stream(s, bytes);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE1); /* descriptor = MULTIPART */
    hold_segment_count = s->p;
    out_uint8s(s, 2); /* segmentCount set later */
    out_uint32_le(s, 25 + bitmap_data_length); /* uncompressedSize */
    /* RDP_DATA_SEGMENT */
    out_uint32_le(s, 1 + 25); /* segmentArray size */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_WIRETOSURFACE_1); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    out_uint32_le(s, 25 + bitmap_data_length); /* pduLength */
    out_uint16_le(s, surface_id);
    out_uint16_le(s, codec_id);
    out_uint8(s, pixel_format);
    out_uint16_le(s, dest_rect->x1);
    out_uint16_le(s, dest_rect->y1);
    out_uint16_le(s, dest_rect->x2);
    out_uint16_le(s, dest_rect->y2);
    out_uint32_le(s, bitmap_data_length);
    segment_count = 1;
    index = 0;
    bitmap_data8 = (char *) bitmap_data;
    while (index < bitmap_data_length)
    {
        segment_size = bitmap_data_length - index;
        if (segment_size > USHRT_MAX)
        {
            segment_size = USHRT_MAX;
        }
        /* RDP_DATA_SEGMENT */
        out_uint32_le(s, 1 + segment_size); /* segmentArray size */
        /* RDP8_BULK_ENCODED_DATA */
        out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
        out_uint8a(s, bitmap_data8 + index, segment_size);
        LOG(LOG_LEVEL_DEBUG, "  segment index %d segment_size %d", segment_count, segment_size);
        index += segment_size;
        segment_count++;
    }
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    /* segment_count */
    s->p = hold_segment_count;
    out_uint16_le(s, segment_count);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_wire_to_surface1: xrdp_egfx_send_data error %d segment_count %d", error, segment_count);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_wire_to_surface2(struct xrdp_egfx *egfx, int surface_id,
                                int codec_id, int codec_context_id,
                                int pixel_format,
                                void *bitmap_data, int bitmap_data_length)
{
    int error;
    int bytes;
    int index;
    int segment_size;
    int segment_count;
    struct stream *s;
    char *hold_segment_count;
    char *bitmap_data8;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_send_wire_to_surface2:");
    make_stream(s);
    bytes = bitmap_data_length + 8192;
    bytes += 5 * (bitmap_data_length / 0xFFFF);
    init_stream(s, bytes);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE1); /* descriptor = MULTIPART */
    hold_segment_count = s->p;
    out_uint8s(s, 2); /* segmentCount set later */
    out_uint32_le(s, 21 + bitmap_data_length); /* uncompressedSize */
    /* RDP_DATA_SEGMENT */
    out_uint32_le(s, 1 + 21); /* segmentArray size */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_WIRETOSURFACE_2); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    out_uint32_le(s, 21 + bitmap_data_length); /* pduLength */
    out_uint16_le(s, surface_id);
    out_uint16_le(s, codec_id);
    out_uint32_le(s, codec_context_id);
    out_uint8(s, pixel_format);
    out_uint32_le(s, bitmap_data_length);
    segment_count = 1;
    index = 0;
    bitmap_data8 = (char *) bitmap_data;
    while (index < bitmap_data_length)
    {
        segment_size = bitmap_data_length - index;
        if (segment_size > USHRT_MAX)
        {
            segment_size = USHRT_MAX;
        }
        /* RDP_DATA_SEGMENT */
        out_uint32_le(s, 1 + segment_size); /* segmentArray size */
        /* RDP8_BULK_ENCODED_DATA */
        out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
        out_uint8a(s, bitmap_data8 + index, segment_size);
        LOG(LOG_LEVEL_DEBUG, "  segment index %d segment_size %d", segment_count, segment_size);
        index += segment_size;
        segment_count++;
    }
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    /* segment_count */
    s->p = hold_segment_count;
    out_uint16_le(s, segment_count);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_send_wire_to_surface2: xrdp_egfx_send_data error %d segment_count %d", error, segment_count);
    free_stream(s);
    return error;
}

/******************************************************************************/
int
xrdp_egfx_send_reset_graphics(struct xrdp_egfx *egfx, int width, int height,
                              int monitor_count, struct monitor_info *mi)
{
    int error;
    int bytes;
    int index;
    struct stream *s;

    LOG(LOG_LEVEL_INFO, "xrdp_egfx_send_reset_graphics:");
    if (monitor_count > 16)
    {
        return 1;
    }
    make_stream(s);
    init_stream(s, 8192);
    /* RDP_SEGMENTED_DATA */
    out_uint8(s, 0xE0); /* descriptor = SINGLE */
    /* RDP8_BULK_ENCODED_DATA */
    out_uint8(s, 0x04); /* header = PACKET_COMPR_TYPE_RDP8 */
    /* RDPGFX_HEADER */
    out_uint16_le(s, XR_RDPGFX_CMDID_RESETGRAPHICS); /* cmdId */
    out_uint16_le(s, 0); /* flags = 0 */
    out_uint32_le(s, 340);
    out_uint32_le(s, width);
    out_uint32_le(s, height);
    out_uint32_le(s, monitor_count == 0 ? 1 : monitor_count);
    if (monitor_count == 0)
    {
        out_uint32_le(s, 0);
        out_uint32_le(s, 0);
        out_uint32_le(s, width - 1);
        out_uint32_le(s, height - 1);
        out_uint32_le(s, 1);
        monitor_count = 1;
    }
    else
    {
        for (index = 0; index < monitor_count; ++index)
        {
            out_uint32_le(s, mi[index].left);
            out_uint32_le(s, mi[index].top);
            out_uint32_le(s, mi[index].right);
            out_uint32_le(s, mi[index].bottom);
            out_uint32_le(s, mi[index].is_primary);
            LOG(LOG_LEVEL_INFO, "xrdp_egfx_send_reset_graphics: (index %d) monitor left %d top %d right %d bottom %d is_primary %d",
                index, mi[index].left, mi[index].top, mi[index].right, mi[index].bottom, mi[index].is_primary);
        }
    }
    LOG(LOG_LEVEL_INFO, "xrdp_egfx_send_reset_graphics: width %d height %d monitorcount %d",
        width, height, monitor_count);
    if (monitor_count < 16)
    {
        bytes = 340 - (20 + (monitor_count * 20));
        out_uint8s(s, bytes);
    }
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    error = xrdp_egfx_send_data(egfx, s->data, bytes);
    LOG(LOG_LEVEL_INFO, "xrdp_egfx_send_reset_graphics: xrdp_egfx_send_data error %d", error);
    free_stream(s);
    return error;
}

/******************************************************************************/
/* RDPGFX_CMDID_FRAMEACKNOWLEDGE */
static int
xrdp_egfx_process_frame_ack(struct xrdp_egfx *egfx, struct stream *s)
{
    uint32_t queueDepth;
    uint32_t intframeId;
    uint32_t totalFramesDecoded;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_process_frame_ack:");
    if (!s_check_rem(s, 12))
    {
        return 1;
    }
    in_uint32_le(s, queueDepth);
    in_uint32_le(s, intframeId);
    in_uint32_le(s, totalFramesDecoded);
    LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_process_frame_ack: queueDepth %d intframeId %d totalFramesDecoded %d",
        queueDepth, intframeId, totalFramesDecoded);
    if (egfx->frame_ack != NULL)
    {
        egfx->frame_ack(egfx->user, queueDepth, intframeId, totalFramesDecoded);
    }
    return 0;
}

/******************************************************************************/
/* RDPGFX_CMDID_CAPSADVERTISE */
static int
xrdp_egfx_process_capsadvertise(struct xrdp_egfx *egfx, struct stream *s)
{
    int index;
    int capsSetCount;
    int caps_count;
    int version;
    int capsDataLength;
    int flags;
    char *holdp;
    int *versions;
    int *flagss;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_process_capsadvertise:");
    if (egfx->caps_advertise == NULL)
    {
        return 0;
    }
    in_uint16_le(s, capsSetCount);
    if ((capsSetCount < 1) || (capsSetCount > 1024))
    {
        return 1;
    }
    caps_count = 0;
    versions = g_new(int, capsSetCount);
    if (versions == NULL)
    {
        return 1;
    }
    flagss = g_new(int, capsSetCount);
    if (flagss == NULL)
    {
        g_free(versions);
        return 1;
    }
    for (index = 0; index < capsSetCount; index++)
    {
        if (!s_check_rem(s, 8))
        {
            return 1;
        }
        in_uint32_le(s, version);
        in_uint32_le(s, capsDataLength);
        if (!s_check_rem(s, capsDataLength))
        {
            return 1;
        }
        holdp = s->p;
        if (capsDataLength == 4)
        {
            in_uint32_le(s, flags);
            versions[caps_count] = version;
            flagss[caps_count] = flags;
            caps_count++;
        }
        s->p = holdp + capsDataLength;
    }
    if (caps_count > 0)
    {
        egfx->caps_advertise(egfx->user, caps_count, versions, flagss);
    }
    g_free(versions);
    g_free(flagss);
    return 0;
}

/******************************************************************************/
static int
xrdp_egfx_process(struct xrdp_egfx *egfx, struct stream *s)
{
    int error;
    int cmdId;
    int flags;
    int pduLength;
    char *holdp;
    char *holdend;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_process:");
    error = 0;
    while (s_check_rem(s, 8))
    {
        holdp = s->p;
        holdend = s->end;
        in_uint16_le(s, cmdId);
        in_uint16_le(s, flags);
        in_uint32_le(s, pduLength);
        s->end = holdp + pduLength;
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_process: cmdId 0x%x flags %d pduLength %d", cmdId, flags, pduLength);
        if (pduLength < 8)
        {
            return 1;
        }
        if (!s_check_rem(s, pduLength - 8))
        {
            return 1;
        }
        switch (cmdId)
        {
            case XR_RDPGFX_CMDID_FRAMEACKNOWLEDGE:
                error = xrdp_egfx_process_frame_ack(egfx, s);
                break;
            case XR_RDPGFX_CMDID_CAPSADVERTISE:
                error = xrdp_egfx_process_capsadvertise(egfx, s);
                break;
            case XR_RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE:
                break;
            default:
                LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_process: unknown cmdId 0x%x", cmdId);
                //g_hexdump(s->p, MIN(pduLength - 8, 64));
                break;
        }
        if (error != 0)
        {
            LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_process: error %d", error);
            return error;
        }
        s->p = holdp + pduLength;
        s->end = holdend;
    }
    return error;
}

/******************************************************************************/
/* from client */
static int
xrdp_egfx_open_response(intptr_t id, int chan_id, int creation_status)
{
    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_open_response:");
    return 0;
}

int
advance_resize_state_machine(tbus resize_state_machine,
                             struct dynamic_monitor_description *description,
                             enum display_resize_state new_state);

/******************************************************************************/
/* from client */
static int
xrdp_egfx_close_response(intptr_t id, int chan_id)
{
    struct xrdp_process *process;
    struct xrdp_mm *mm;
    struct dynamic_monitor_description *description;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_close_response:");

    process = (struct xrdp_process *) id;
    mm = process->wm->mm;

    if (mm->resize_queue == 0 || mm->resize_queue->count <= 0)
    {
        return 0;
    }
    description = (struct dynamic_monitor_description *)list_get_item(mm->resize_queue, 0);
    if (description->state == WMRZ_EGFX_CONN_CLOSING)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_close_response: egfx deleted.");
        advance_resize_state_machine(mm->resize_state_machine, description, WMRZ_EGFX_CONN_CLOSED);
    }
    return 0;
}

/******************************************************************************/
/* from client */
static int
xrdp_egfx_data_first(intptr_t id, int chan_id, char *data, int bytes,
                     int total_bytes)
{
    struct xrdp_process *process;
    struct xrdp_egfx *egfx;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_data_first: bytes %d total_bytes %d", bytes, total_bytes);
    process = (struct xrdp_process *) id;
    egfx = process->wm->mm->egfx;
    if (egfx->s != NULL)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_data_first: Error! Stream is not working on initial data received!");
    }
    make_stream(egfx->s);
    init_stream(egfx->s, total_bytes);
    out_uint8a(egfx->s, data, bytes);
    return 0;
}

/******************************************************************************/
/* from client */
static int
xrdp_egfx_data(intptr_t id, int chan_id, char *data, int bytes)
{
    int error;
    struct stream ls;
    struct xrdp_process *process;
    struct xrdp_wm *wm;
    struct xrdp_mm *mm;
    struct xrdp_egfx *egfx;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_data:");

    process = (struct xrdp_process *) id;
    if (process == NULL)
    {
        return 0;
    }

    wm = process->wm;
    if (wm == NULL)
    {
        return 0;
    }

    mm = wm->mm;
    if (mm == NULL)
    {
        return 0;
    }

    egfx = mm->egfx;
    if (egfx == NULL)
    {
        return 0;
    }

    if (egfx->s == NULL)
    {
        g_memset(&ls, 0, sizeof(ls));
        ls.data = data;
        ls.size = bytes;
        ls.p = data;
        ls.end = data + bytes;
        return xrdp_egfx_process(egfx, &ls);
    }
    if (!s_check_rem_out(egfx->s, bytes))
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_data: error");
        return 1;
    }
    out_uint8a(egfx->s, data, bytes);
    if (!s_check_rem_out(egfx->s, 1))
    {
        s_mark_end(egfx->s);
        egfx->s->p = egfx->s->data;
        error = xrdp_egfx_process(egfx, egfx->s);
        free_stream(egfx->s);
        egfx->s = NULL;
        return error;
    }
    return 0;
}

/******************************************************************************/
int
xrdp_egfx_create(struct xrdp_mm *mm, struct xrdp_egfx **egfx)
{
    int error;
    struct xrdp_drdynvc_procs procs;
    struct xrdp_egfx *self;
    struct xrdp_process *process;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_create:");

    self = g_new0(struct xrdp_egfx, 1);
    if (self == NULL)
    {
        return 1;
    }
    procs.open_response = xrdp_egfx_open_response;
    procs.close_response = xrdp_egfx_close_response;
    procs.data_first = xrdp_egfx_data_first;
    procs.data = xrdp_egfx_data;
    process = mm->wm->pro_layer;
    error = libxrdp_drdynvc_open(process->session,
                                 "Microsoft::Windows::RDS::Graphics",
                                 1, /* WTS_CHANNEL_OPTION_DYNAMIC */
                                 &procs, &(self->channel_id));
    LOG(LOG_LEVEL_INFO, "xrdp_egfx_create: error %d channel_id %d", error, self->channel_id);
    self->session = process->session;
    self->surface_id = 0;
    *egfx = self;
    return 0;
}

/******************************************************************************/
int
xrdp_egfx_shutdown_delete_surface(struct xrdp_egfx *egfx)
{
    int error;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_shutdown_delete_surface:");

    if (egfx == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_delete_surface: EGFX is already null!");
        return 0;
    }

    error = xrdp_egfx_send_delete_surface(egfx, egfx->surface_id);
    if (error != 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_delete_surface: xrdp_egfx_send_delete_surface failed %d", error);
    }
    return error;
}

/******************************************************************************/
int
xrdp_egfx_shutdown_close_connection(struct xrdp_egfx *egfx)
{
    int error;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_shutdown_close_connection:");

    if (egfx == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_close_connection: EGFX is already null!");
        return 0;
    }

    error = libxrdp_drdynvc_close(egfx->session, egfx->channel_id);
    if (error != 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_close_connection: libxrdp_drdynvc_close failed %d", error);
        return error;
    }

    return error;
}

/******************************************************************************/
int
xrdp_egfx_shutdown_delete(struct xrdp_egfx *egfx)
{
    int error = 0;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_delete:");

    if (egfx == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_delete: EGFX is already null!");
        return 0;
    }

    g_free(egfx);

    return error;
}

/******************************************************************************/
int
xrdp_egfx_shutdown_full(struct xrdp_egfx *egfx)
{
    int error;

    LOG(LOG_LEVEL_TRACE, "xrdp_egfx_shutdown_full:");

    if (egfx == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_full: EGFX is already null!");
        return 0;
    }

    error = xrdp_egfx_shutdown_delete_surface(egfx);
    if (error != 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_full: xrdp_egfx_shutdown_delete_surface failed %d", error);
        return error;
    }

    error = xrdp_egfx_shutdown_close_connection(egfx);
    if (error != 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_full: xrdp_egfx_shutdown_close_connection failed %d", error);
        return error;
    }

    error = xrdp_egfx_shutdown_delete(egfx);
    if (error != 0)
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_egfx_shutdown_full: xrdp_egfx_shutdown_delete failed %d", error);
        return error;
    }

    return error;
}
