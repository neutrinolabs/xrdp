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

/**
 *
 * @file xrdp_egfx.h
 * @brief Stream function headers for the EGFX extension to the MSRDP protocol.
 * @author Jay Sorg, Christopher Pitstick
 *
 */

#ifndef _XRDP_EGFX_H
#define _XRDP_EGFX_H

#include "xrdp.h"

#define XR_RDPGFX_CAPVERSION_8      0x00080004
#define XR_RDPGFX_CAPVERSION_81     0x00080105
#define XR_RDPGFX_CAPVERSION_10     0x000A0002
#define XR_RDPGFX_CAPVERSION_101    0x000A0100
#define XR_RDPGFX_CAPVERSION_102    0x000A0200
#define XR_RDPGFX_CAPVERSION_103    0x000A0301
#define XR_RDPGFX_CAPVERSION_104    0x000A0400
#define XR_RDPGFX_CAPVERSION_105    0x000A0502
#define XR_RDPGFX_CAPVERSION_106    0x000A0600
#define XR_RDPGFX_CAPVERSION_107    0x000A0701

#define XR_PIXEL_FORMAT_XRGB_8888   0x20
#define XR_PIXEL_FORMAT_ARGB_8888   0x21

#define XR_RDPGFX_CAPS_FLAG_THINCLIENT      0x00000001
#define XR_RDPGFX_CAPS_FLAG_SMALL_CACHE     0x00000002
#define XR_RDPGFX_CAPS_FLAG_AVC420_ENABLED  0x00000010
#define XR_RDPGFX_CAPS_FLAG_AVC_DISABLED    0x00000020
#define XR_RDPGFX_CAPS_FLAG_AVC_THINCLIENT  0x00000040

#define XR_RDPGFX_CMDID_WIRETOSURFACE_1             0x0001
#define XR_RDPGFX_CMDID_WIRETOSURFACE_2             0x0002
#define XR_RDPGFX_CMDID_DELETEENCODINGCONTEXT       0x0003
#define XR_RDPGFX_CMDID_SOLIDFILL                   0x0004
#define XR_RDPGFX_CMDID_SURFACETOSURFACE            0x0005
#define XR_RDPGFX_CMDID_SURFACETOCACHE              0x0006
#define XR_RDPGFX_CMDID_CACHETOSURFACE              0x0007
#define XR_RDPGFX_CMDID_EVICTCACHEENTRY             0x0008
#define XR_RDPGFX_CMDID_CREATESURFACE               0x0009
#define XR_RDPGFX_CMDID_DELETESURFACE               0x000A
#define XR_RDPGFX_CMDID_STARTFRAME                  0x000B
#define XR_RDPGFX_CMDID_ENDFRAME                    0x000C
#define XR_RDPGFX_CMDID_FRAMEACKNOWLEDGE            0x000D
#define XR_RDPGFX_CMDID_RESETGRAPHICS               0x000E
#define XR_RDPGFX_CMDID_MAPSURFACETOOUTPUT          0x000F
#define XR_RDPGFX_CMDID_CACHEIMPORTOFFER            0x0010
#define XR_RDPGFX_CMDID_CACHEIMPORTREPLY            0x0011
#define XR_RDPGFX_CMDID_CAPSADVERTISE               0x0012
#define XR_RDPGFX_CMDID_CAPSCONFIRM                 0x0013
#define XR_RDPGFX_CMDID_MAPSURFACETOWINDOW          0x0015
#define XR_RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE         0x0016
#define XR_RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT    0x0017
#define XR_RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW    0x0018

#define XR_QUEUE_DEPTH_UNAVAILABLE          0x00000000
#define XR_SUSPEND_FRAME_ACKNOWLEDGEMENT    0xFFFFFFFF

/* The bitmap data encapsulated in the bitmapData field is uncompressed.
   Pixels in the uncompressed data are ordered from left to right and then
   top to bottom. */
#define XR_RDPGFX_CODECID_UNCOMPRESSED      0x0000
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the RemoteFX Codec ([MS-RDPRFX] sections 2.2.1 and 3.1.8). Note that the
   TS_RFX_RECT ([MS-RDPRFX] section 2.2.2.1.6) structures encapsulated in
   the bitmapData field MUST all be relative to the top-left corner of the
   rectangle defined by the destRect field. */
#define XR_RDPGFX_CODECID_CAVIDEO           0x0003
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the ClearCodec Codec (sections 2.2.4.1 and 3.3.8.1). */
#define XR_RDPGFX_CODECID_CLEARCODEC        0x0008
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the Planar Codec ([MS-RDPEGDI] sections 2.2.2.5.1 and 3.1.9). */
#define XR_RDPGFX_CODECID_PLANAR            0x000A
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the MPEG-4 AVC/H.264 Codec in YUV420p mode (section 2.2.4.4). */
#define XR_RDPGFX_CODECID_AVC420            0x000B
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the Alpha Codec (section 2.2.4.3). */
#define XR_RDPGFX_CODECID_ALPHA             0x000C
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the MPEG-4 AVC/H.264 Codec in YUV444 mode (section 2.2.4.5). */
#define XR_RDPGFX_CODECID_AVC444            0x000E
/* The bitmap data encapsulated in the bitmapData field is compressed using
   the MPEG-4 AVC/H.264 Codec in YUV444v2 mode (section 2.2.4.6). */
#define XR_RDPGFX_CODECID_AVC444V2          0x000F

struct xrdp_egfx_rect
{
    short x1;
    short y1;
    short x2;
    short y2;
};

struct xrdp_egfx_point
{
    short x;
    short y;
};

struct xrdp_egfx
{
    struct xrdp_session *session;
    int channel_id;
    int surface_id;
    int frame_id;
    struct stream *s;
    void *user;
    struct xrdp_egfx_bulk *bulk;
    int (*caps_advertise)(void *user, int num_caps, int *version, int *flags);
    int (*frame_ack)(void *user, uint32_t queue_depth,
                     int frame_id, int frames_decoded);
};

struct xrdp_egfx_bulk
{
    int id;
};

int
xrdp_egfx_send_data(struct xrdp_egfx *egfx, const char *data, int bytes);
int
xrdp_egfx_send_s(struct xrdp_egfx *egfx, struct stream *s);
int
xrdp_egfx_create(struct xrdp_mm *mm, struct xrdp_egfx **egfx);
int
xrdp_egfx_shutdown_delete_surface(struct xrdp_egfx *egfx);
int
xrdp_egfx_shutdown_close_connection(struct xrdp_egfx *egfx);
int
xrdp_egfx_shutdown_delete(struct xrdp_egfx *egfx);
int
xrdp_egfx_shutdown_full(struct xrdp_egfx *egfx);
struct stream *
xrdp_egfx_create_surface(struct xrdp_egfx_bulk *bulk, int surface_id,
                         int width, int height, int pixel_format);
int
xrdp_egfx_send_create_surface(struct xrdp_egfx *egfx, int surface_id,
                              int width, int height, int pixel_format);
struct stream *
xrdp_egfx_delete_surface(struct xrdp_egfx_bulk *bulk, int surface_id);
int
xrdp_egfx_send_delete_surface(struct xrdp_egfx *egfx, int surface_id);
struct stream *
xrdp_egfx_map_surface(struct xrdp_egfx_bulk *bulk, int surface_id,
                      int x, int y);
int
xrdp_egfx_send_map_surface(struct xrdp_egfx *egfx, int surface_id,
                           int x, int y);
struct stream *
xrdp_egfx_fill_surface(struct xrdp_egfx_bulk *bulk, int surface_id,
                       int fill_color, int num_rects,
                       const struct xrdp_egfx_rect *rects);
int
xrdp_egfx_send_fill_surface(struct xrdp_egfx *egfx, int surface_id,
                            int fill_color, int num_rects,
                            const struct xrdp_egfx_rect *rects);
struct stream *
xrdp_egfx_surface_to_surface(struct xrdp_egfx_bulk *bulk, int src_surface_id,
                             int dst_surface_id,
                             const struct xrdp_egfx_rect *src_rect,
                             int num_dst_points,
                             const struct xrdp_egfx_point *dst_points);
int
xrdp_egfx_send_surface_to_surface(struct xrdp_egfx *egfx, int src_surface_id,
                                  int dst_surface_id,
                                  const struct xrdp_egfx_rect *src_rect,
                                  int num_dst_points,
                                  const struct xrdp_egfx_point *dst_points);
struct stream *
xrdp_egfx_frame_start(struct xrdp_egfx_bulk *bulk, int frame_id, int timestamp);
int
xrdp_egfx_send_frame_start(struct xrdp_egfx *egfx, int frame_id, int timestamp);
struct stream *
xrdp_egfx_frame_end(struct xrdp_egfx_bulk *bulk, int frame_id);
int
xrdp_egfx_send_frame_end(struct xrdp_egfx *egfx, int frame_id);
struct stream *
xrdp_egfx_capsconfirm(struct xrdp_egfx_bulk *bulk, int version, int flags);
int
xrdp_egfx_send_capsconfirm(struct xrdp_egfx *egfx, int version, int flags);
struct stream *
xrdp_egfx_wire_to_surface1(struct xrdp_egfx_bulk *bulk, int surface_id,
                           int codec_id, int pixel_format,
                           struct xrdp_egfx_rect *dest_rect,
                           void *bitmap_data, int bitmap_data_length);
int
xrdp_egfx_send_wire_to_surface1(struct xrdp_egfx *egfx, int surface_id,
                                int codec_id, int pixel_format,
                                struct xrdp_egfx_rect *dest_rect,
                                void *bitmap_data, int bitmap_data_length);
struct stream *
xrdp_egfx_wire_to_surface2(struct xrdp_egfx_bulk *bulk, int surface_id,
                           int codec_id, int codec_context_id,
                           int pixel_format,
                           void *bitmap_data, int bitmap_data_length);
int
xrdp_egfx_send_wire_to_surface2(struct xrdp_egfx *egfx, int surface_id,
                                int codec_id, int codec_context_id,
                                int pixel_format,
                                void *bitmap_data, int bitmap_data_length);
struct stream *
xrdp_egfx_reset_graphics(struct xrdp_egfx_bulk *bulk, int width, int height,
                         int monitor_count, struct monitor_info *mi);
int
xrdp_egfx_send_reset_graphics(struct xrdp_egfx *egfx, int width, int height,
                              int monitor_count, struct monitor_info *mi);

#endif
