/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 * Copyright (C) Idan Freiberg 2004-2014
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
 * RDP Capability Sets
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"

/*****************************************************************************/
static int
xrdp_caps_send_monitorlayout(struct xrdp_rdp *self)
{
    struct stream *s;
    int i;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, self->client_info.monitorCount); /* monitorCount (4 bytes) */

    /* TODO: validate for allowed monitors in terminal server (maybe by config?) */
    for (i = 0; i < self->client_info.monitorCount; i++)
    {
        out_uint32_le(s, self->client_info.minfo[i].left);
        out_uint32_le(s, self->client_info.minfo[i].top);
        out_uint32_le(s, self->client_info.minfo[i].right);
        out_uint32_le(s, self->client_info.minfo[i].bottom);
        out_uint32_le(s, self->client_info.minfo[i].is_primary);
    }

    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, 0x37) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_general(struct xrdp_rdp *self, struct stream *s,
                          int len)
{
    int extraFlags;
    int client_does_fastpath_output;

    if (len < 10 + 2)
    {
        g_writeln("xrdp_caps_process_general: error");
        return 1;
    }

    in_uint16_le(s, self->client_info.client_os_major); /* osMajorType (2 bytes) */
    in_uint16_le(s, self->client_info.client_os_minor); /* osMinorType (2 bytes) */
    in_uint8s(s, 6);
    in_uint16_le(s, extraFlags); /* extraFlags (2 bytes) */

    self->client_info.op1 = extraFlags & NO_BITMAP_COMPRESSION_HDR;
    /* use_compact_packets is pretty much 'use rdp5' */
    self->client_info.use_compact_packets = (extraFlags != 0);
    /* op2 is a boolean to use compact bitmap headers in bitmap cache */
    /* set it to same as 'use rdp5' boolean */
    self->client_info.op2 = self->client_info.use_compact_packets;
    /* FASTPATH_OUTPUT_SUPPORTED 0x0001 */
    client_does_fastpath_output = extraFlags & FASTPATH_OUTPUT_SUPPORTED;
    if ((self->client_info.use_fast_path & 1) && !client_does_fastpath_output)
    {
        /* server supports fast path output and client does not, turn it off */
        self->client_info.use_fast_path &= ~1;
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_order(struct xrdp_rdp *self, struct stream *s,
                        int len)
{
    int i;
    char order_caps[32];
    int ex_flags;
    int cap_flags;

    DEBUG(("order capabilities"));
    if (len < 20 + 2 + 2 + 2 + 2 + 2 + 2 + 32 + 2 + 2 + 4 + 4 + 4 + 4)
    {
        g_writeln("xrdp_caps_process_order: error");
        return 1;
    }
    in_uint8s(s, 20); /* Terminal desc, pad */
    in_uint8s(s, 2); /* Cache X granularity */
    in_uint8s(s, 2); /* Cache Y granularity */
    in_uint8s(s, 2); /* Pad */
    in_uint8s(s, 2); /* Max order level */
    in_uint8s(s, 2); /* Number of fonts */
    in_uint16_le(s, cap_flags); /* Capability flags */
    in_uint8a(s, order_caps, 32); /* Orders supported */
    g_memcpy(self->client_info.orders, order_caps, 32);
    DEBUG(("dest blt-0 %d", order_caps[0]));
    DEBUG(("pat blt-1 %d", order_caps[1]));
    DEBUG(("screen blt-2 %d", order_caps[2]));
    DEBUG(("memblt-3-13 %d %d", order_caps[3], order_caps[13]));
    DEBUG(("triblt-4-14 %d %d", order_caps[4], order_caps[14]));
    DEBUG(("line-8 %d", order_caps[8]));
    DEBUG(("line-9 %d", order_caps[9]));
    DEBUG(("rect-10 %d", order_caps[10]));
    DEBUG(("desksave-11 %d", order_caps[11]));
    DEBUG(("polygon-20 %d", order_caps[20]));
    DEBUG(("polygon2-21 %d", order_caps[21]));
    DEBUG(("polyline-22 %d", order_caps[22]));
    DEBUG(("ellipse-25 %d", order_caps[25]));
    DEBUG(("ellipse2-26 %d", order_caps[26]));
    DEBUG(("text2-27 %d", order_caps[27]));
    DEBUG(("order_caps dump"));
#if defined(XRDP_DEBUG)
    g_hexdump(order_caps, 32);
#endif
    in_uint8s(s, 2); /* Text capability flags */
    /* read extended order support flags */
    in_uint16_le(s, ex_flags); /* Ex flags */

    if (cap_flags & 0x80) /* ORDER_FLAGS_EXTRA_SUPPORT */
    {
        self->client_info.order_flags_ex = ex_flags;
        if (ex_flags & XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT)
        {
            g_writeln("xrdp_caps_process_order: bitmap cache v3 supported");
            self->client_info.bitmap_cache_version |= 4;
        }
    }
    in_uint8s(s, 4); /* Pad */

    in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
    self->client_info.desktop_cache = i;
    DEBUG(("desktop cache size %d", i));
    in_uint8s(s, 4); /* Unknown */
    in_uint8s(s, 4); /* Unknown */

    /* check if libpainter should be used for drawing, instead of orders */
    if (!(order_caps[TS_NEG_DSTBLT_INDEX] && order_caps[TS_NEG_PATBLT_INDEX] &&
          order_caps[TS_NEG_SCRBLT_INDEX] && order_caps[TS_NEG_MEMBLT_INDEX]))
    {
        g_writeln("xrdp_caps_process_order: not enough orders supported by client, using painter.");
        self->client_info.no_orders_supported = 1;
    }

    return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int
xrdp_caps_process_bmpcache(struct xrdp_rdp *self, struct stream *s,
                           int len)
{
    int i;

    if (len < 24 + 2 + 2 + 2 + 2 + 2 + 2)
    {
        g_writeln("xrdp_caps_process_bmpcache: error");
        return 1;
    }
    self->client_info.bitmap_cache_version |= 1;
    in_uint8s(s, 24);
    /* cache 1 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache1_entries = i;
    in_uint16_le(s, self->client_info.cache1_size);
    /* cache 2 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache2_entries = i;
    in_uint16_le(s, self->client_info.cache2_size);
    /* cache 3 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache3_entries = i;
    in_uint16_le(s, self->client_info.cache3_size);
    DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
           self->client_info.cache1_size));
    DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
           self->client_info.cache2_size));
    DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
           self->client_info.cache3_size));
    return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int
xrdp_caps_process_bmpcache2(struct xrdp_rdp *self, struct stream *s,
                            int len)
{
    int Bpp = 0;
    int i = 0;

    if (len < 2 + 2 + 4 + 4 + 4)
    {
        g_writeln("xrdp_caps_process_bmpcache2: error");
        return 1;
    }
    self->client_info.bitmap_cache_version |= 2;
    Bpp = (self->client_info.bpp + 7) / 8;
    in_uint16_le(s, i); /* cache flags */
    self->client_info.bitmap_cache_persist_enable = i;
    in_uint8s(s, 2); /* number of caches in set, 3 */
    in_uint32_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache1_entries = i;
    self->client_info.cache1_size = 256 * Bpp;
    in_uint32_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache2_entries = i;
    self->client_info.cache2_size = 1024 * Bpp;
    in_uint32_le(s, i);
    i = i & 0x7fffffff;
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache3_entries = i;
    self->client_info.cache3_size = 4096 * Bpp;
    DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
           self->client_info.cache1_size));
    DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
           self->client_info.cache2_size));
    DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
           self->client_info.cache3_size));
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_cache_v3_codec_id(struct xrdp_rdp *self, struct stream *s,
                                    int len)
{
    int codec_id;

    if (len < 1)
    {
        g_writeln("xrdp_caps_process_cache_v3_codec_id: error");
        return 1;
    }
    in_uint8(s, codec_id);
    g_writeln("xrdp_caps_process_cache_v3_codec_id: cache_v3_codec_id %d",
              codec_id);
    self->client_info.v3_codec_id = codec_id;
    return 0;
}

/*****************************************************************************/
/* get the number of client cursor cache */
static int
xrdp_caps_process_pointer(struct xrdp_rdp *self, struct stream *s,
                          int len)
{
    int i;
    int colorPointerFlag;
    int no_new_cursor;

    if (len < 2 + 2 + 2)
    {
        g_writeln("xrdp_caps_process_pointer: error");
        return 1;
    }
    no_new_cursor = self->client_info.pointer_flags & 2;
    in_uint16_le(s, colorPointerFlag);
    self->client_info.pointer_flags = colorPointerFlag;
    in_uint16_le(s, i);
    i = MIN(i, 32);
    self->client_info.pointer_cache_entries = i;
    if (colorPointerFlag & 1)
    {
        g_writeln("xrdp_caps_process_pointer: client supports "
                  "new(color) cursor");
        in_uint16_le(s, i);
        i = MIN(i, 32);
        self->client_info.pointer_cache_entries = i;
    }
    else
    {
        g_writeln("xrdp_caps_process_pointer: client does not support "
                  "new(color) cursor");
    }
    if (no_new_cursor)
    {
        g_writeln("xrdp_caps_process_pointer: new(color) cursor is "
                  "disabled by config");
        self->client_info.pointer_flags = 0;
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_input(struct xrdp_rdp *self, struct stream *s,
                        int len)
{
    int inputFlags;
    int client_does_fastpath_input;

    in_uint16_le(s, inputFlags);
    client_does_fastpath_input = (inputFlags & INPUT_FLAG_FASTPATH_INPUT) ||
                                 (inputFlags & INPUT_FLAG_FASTPATH_INPUT2);
    if ((self->client_info.use_fast_path & 2) && !client_does_fastpath_input)
    {
        self->client_info.use_fast_path &= ~2;
    }
    return 0;
}

/*****************************************************************************/
/* get the type of client brush cache */
int
xrdp_caps_process_brushcache(struct xrdp_rdp *self, struct stream *s,
                             int len)
{
    int code;

    if (len < 4)
    {
        g_writeln("xrdp_caps_process_brushcache: error");
        return 1;
    }
    in_uint32_le(s, code);
    self->client_info.brush_cache_code = code;
    return 0;
}

/*****************************************************************************/
int
xrdp_caps_process_offscreen_bmpcache(struct xrdp_rdp *self, struct stream *s,
                                     int len)
{
    int i32;

    if (len < 4 + 2 + 2)
    {
        g_writeln("xrdp_caps_process_offscreen_bmpcache: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.offscreen_support_level = i32;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_size = i32 * 1024;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_entries = i32;
    g_writeln("xrdp_process_offscreen_bmpcache: support level %d "
              "cache size %d MB cache entries %d",
              self->client_info.offscreen_support_level,
              self->client_info.offscreen_cache_size,
              self->client_info.offscreen_cache_entries);
    return 0;
}

/*****************************************************************************/
int
xrdp_caps_process_rail(struct xrdp_rdp *self, struct stream *s, int len)
{
    int i32;

    if (len < 4)
    {
        g_writeln("xrdp_caps_process_rail: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.rail_support_level = i32;
    g_writeln("xrdp_process_capset_rail: rail_support_level %d",
              self->client_info.rail_support_level);
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_window(struct xrdp_rdp *self, struct stream *s, int len)
{
    int i32;

    if (len < 4 + 1 + 2)
    {
        g_writeln("xrdp_caps_process_window: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.wnd_support_level = i32;
    in_uint8(s, i32);
    self->client_info.wnd_num_icon_caches = i32;
    in_uint16_le(s, i32);
    self->client_info.wnd_num_icon_cache_entries = i32;
    g_writeln("xrdp_process_capset_window wnd_support_level %d "
              "wnd_num_icon_caches %d wnd_num_icon_cache_entries %d",
              self->client_info.wnd_support_level,
              self->client_info.wnd_num_icon_caches,
              self->client_info.wnd_num_icon_cache_entries);
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_codecs(struct xrdp_rdp *self, struct stream *s, int len)
{
    int codec_id;
    int codec_count;
    int index;
    int codec_properties_length;
    int i1;
    char *codec_guid;
    char *next_guid;

    if (len < 1)
    {
        g_writeln("xrdp_caps_process_codecs: error");
        return 1;
    }
    in_uint8(s, codec_count);
    len--;

    for (index = 0; index < codec_count; index++)
    {
        codec_guid = s->p;
        if (len < 16 + 1 + 2)
        {
            g_writeln("xrdp_caps_process_codecs: error");
            return 1;
        }
        in_uint8s(s, 16);
        in_uint8(s, codec_id);
        in_uint16_le(s, codec_properties_length);
        len -= 16 + 1 + 2;
        if (len < codec_properties_length)
        {
            g_writeln("xrdp_caps_process_codecs: error");
            return 1;
        }
        len -= codec_properties_length;
        next_guid = s->p + codec_properties_length;

        if (g_memcmp(codec_guid, XR_CODEC_GUID_NSCODEC, 16) == 0)
        {
            g_writeln("xrdp_caps_process_codecs: nscodec, codec id %d, properties len %d",
                      codec_id, codec_properties_length);
            self->client_info.ns_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.ns_prop, s->p, i1);
            self->client_info.ns_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_REMOTEFX, 16) == 0)
        {
            g_writeln("xrdp_caps_process_codecs: RemoteFX, codec id %d, properties len %d",
                      codec_id, codec_properties_length);
            self->client_info.rfx_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.rfx_prop, s->p, i1);
            self->client_info.rfx_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_JPEG, 16) == 0)
        {
            g_writeln("xrdp_caps_process_codecs: jpeg, codec id %d, properties len %d",
                      codec_id, codec_properties_length);
            self->client_info.jpeg_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.jpeg_prop, s->p, i1);
            self->client_info.jpeg_prop_len = i1;
            /* make sure that requested quality is  between 0 to 100 */
            if (self->client_info.jpeg_prop[0] < 0 || self->client_info.jpeg_prop[0] > 100)
            {
                g_writeln("  Warning: the requested jpeg quality (%d) is invalid,"
                          " falling back to default", self->client_info.jpeg_prop[0]);
                self->client_info.jpeg_prop[0] = 75; /* use default */
            }
            g_writeln("  jpeg quality set to %d", self->client_info.jpeg_prop[0]);
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_H264, 16) == 0)
        {
            g_writeln("xrdp_caps_process_codecs: h264, codec id %d, properties len %d",
                      codec_id, codec_properties_length);
            self->client_info.h264_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.h264_prop, s->p, i1);
            self->client_info.h264_prop_len = i1;
        }
        else
        {
            g_writeln("xrdp_caps_process_codecs: unknown codec id %d", codec_id);
        }

        s->p = next_guid;
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_multifragmentupdate(struct xrdp_rdp *self, struct stream *s,
                                      int len)
{
    int MaxRequestSize;

    in_uint32_le(s, MaxRequestSize);
    self->client_info.max_fastpath_frag_bytes = MaxRequestSize;
    return 0;
}

 /*****************************************************************************/
static int
xrdp_caps_process_frame_ack(struct xrdp_rdp *self, struct stream *s, int len)
{
    g_writeln("xrdp_caps_process_frame_ack:");
    self->client_info.use_frame_acks = 1;
    in_uint32_le(s, self->client_info.max_unacknowledged_frame_count);
    if (self->client_info.max_unacknowledged_frame_count < 0)
    {
        g_writeln("  invalid max_unacknowledged_frame_count value (%d), setting to 0",
                  self->client_info.max_unacknowledged_frame_count);
        self->client_info.max_unacknowledged_frame_count = 0;
    }
    g_writeln("  max_unacknowledged_frame_count %d", self->client_info.max_unacknowledged_frame_count);
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_surface_cmds(struct xrdp_rdp *self, struct stream *s, int len)
{
    int cmdFlags;
    g_writeln("xrdp_caps_process_surface_cmds:");
    in_uint32_le(s, cmdFlags);
    in_uint8s(s, 4); /* reserved */
    g_writeln("  cmdFlags 0x%08x", cmdFlags);
    return 0;
}

/*****************************************************************************/
int
xrdp_caps_process_confirm_active(struct xrdp_rdp *self, struct stream *s)
{
    int cap_len;
    int source_len;
    int num_caps;
    int index;
    int type;
    int len;
    char *p;

    DEBUG(("in xrdp_caps_process_confirm_active"));
    in_uint8s(s, 4); /* rdp_shareid */
    in_uint8s(s, 2); /* userid */
    in_uint16_le(s, source_len); /* sizeof RDP_SOURCE */
    in_uint16_le(s, cap_len);
    in_uint8s(s, source_len);
    in_uint16_le(s, num_caps);
    in_uint8s(s, 2); /* pad */

    if ((cap_len < 0) || (cap_len > 1024 * 1024))
    {
        return 1;
    }

    for (index = 0; index < num_caps; index++)
    {
        p = s->p;
        if (!s_check_rem(s, 4))
        {
            g_writeln("xrdp_caps_process_confirm_active: error 1");
            return 1;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, len);
        if ((len < 4) || !s_check_rem(s, len - 4))
        {
            g_writeln("xrdp_caps_process_confirm_active: error: len %d, "
                      "remaining %d", len, (int) (s->end - s->p));
            return 1;
        }
        len -= 4;
        switch (type)
        {
            case RDP_CAPSET_GENERAL:
                DEBUG(("RDP_CAPSET_GENERAL"));
                xrdp_caps_process_general(self, s, len);
                break;
            case RDP_CAPSET_BITMAP:
                DEBUG(("RDP_CAPSET_BITMAP"));
                break;
            case RDP_CAPSET_ORDER:
                DEBUG(("RDP_CAPSET_ORDER"));
                xrdp_caps_process_order(self, s, len);
                break;
            case RDP_CAPSET_BMPCACHE:
                DEBUG(("RDP_CAPSET_BMPCACHE"));
                xrdp_caps_process_bmpcache(self, s, len);
                break;
            case RDP_CAPSET_CONTROL:
                DEBUG(("RDP_CAPSET_CONTROL"));
                break;
            case 6:
                xrdp_caps_process_cache_v3_codec_id(self, s, len);
                break;
            case RDP_CAPSET_ACTIVATE:
                DEBUG(("RDP_CAPSET_ACTIVATE"));
                break;
            case RDP_CAPSET_POINTER:
                DEBUG(("RDP_CAPSET_POINTER"));
                xrdp_caps_process_pointer(self, s, len);
                break;
            case RDP_CAPSET_SHARE:
                DEBUG(("RDP_CAPSET_SHARE"));
                break;
            case RDP_CAPSET_COLCACHE:
                DEBUG(("RDP_CAPSET_COLCACHE"));
                break;
            case RDP_CAPSET_SOUND:
                DEBUG(("--0x0C"));
                break;
            case RDP_CAPSET_INPUT:
                xrdp_caps_process_input(self, s, len);
                break;
            case RDP_CAPSET_FONT:
                DEBUG(("--0x0D"));
                break;
            case RDP_CAPSET_BRUSHCACHE:
                xrdp_caps_process_brushcache(self, s, len);
                break;
            case RDP_CAPSET_GLYPHCACHE:
                DEBUG(("--0x11"));
                break;
            case RDP_CAPSET_OFFSCREENCACHE:
                DEBUG(("CAPSET_TYPE_OFFSCREEN_CACHE"));
                xrdp_caps_process_offscreen_bmpcache(self, s, len);
                break;
            case RDP_CAPSET_BMPCACHE2:
                DEBUG(("RDP_CAPSET_BMPCACHE2"));
                xrdp_caps_process_bmpcache2(self, s, len);
                break;
            case RDP_CAPSET_VIRCHAN:
                DEBUG(("--0x14"));
                break;
            case RDP_CAPSET_DRAWNINEGRIDCACHE:
                DEBUG(("--0x15"));
                break;
            case RDP_CAPSET_DRAWGDIPLUS:
                DEBUG(("--0x16"));
                break;
            case RDP_CAPSET_RAIL:
                xrdp_caps_process_rail(self, s, len);
                break;
            case RDP_CAPSET_WINDOW:
                xrdp_caps_process_window(self, s, len);
                break;
            case RDP_CAPSET_MULTIFRAGMENT:
                xrdp_caps_process_multifragmentupdate(self, s, len);
                break;
            case RDP_CAPSET_SURFCMDS:
                xrdp_caps_process_surface_cmds(self, s, len);
                break;
            case RDP_CAPSET_BMPCODECS:
                xrdp_caps_process_codecs(self, s, len);
                break;
            case RDP_CAPSET_FRAME_ACKNOWLEDGE:
                xrdp_caps_process_frame_ack(self, s, len);
                break;
            default:
                g_writeln("unknown in xrdp_caps_process_confirm_active %d", type);
                break;
        }

        s->p = p + len + 4;
    }

    if (self->client_info.no_orders_supported &&
        (self->client_info.offscreen_support_level != 0))
    {
        g_writeln("xrdp_caps_process_confirm_active: not enough orders "
                  "supported by client, client wants off screen bitmap but "
                  "offscreen bitmaps disabled");
        self->client_info.offscreen_support_level = 0;
        self->client_info.offscreen_cache_size = 0;
        self->client_info.offscreen_cache_entries = 0;
    }

    DEBUG(("out xrdp_caps_process_confirm_active"));
    return 0;
}
/*****************************************************************************/
int
xrdp_caps_send_demand_active(struct xrdp_rdp *self)
{
    struct stream *s;
    int caps_count;
    int caps_size;
    int codec_caps_count;
    int codec_caps_size;
    int flags;
    char *caps_count_ptr;
    char *caps_size_ptr;
    char *caps_ptr;
    char *codec_caps_count_ptr;
    char *codec_caps_size_ptr;

    make_stream(s);
    init_stream(s, 8192);

    DEBUG(("in xrdp_caps_send_demand_active"));

    if (xrdp_rdp_init(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    caps_count = 0;
    out_uint32_le(s, self->share_id);
    out_uint16_le(s, 4); /* 4 chars for RDP\0 */
    /* 2 bytes size after num caps, set later */
    caps_size_ptr = s->p;
    out_uint8s(s, 2);
    out_uint8a(s, "RDP", 4);
    /* 4 byte num caps, set later */
    caps_count_ptr = s->p;
    out_uint8s(s, 4);
    caps_ptr = s->p;

    /* Output share capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_SHARE);
    out_uint16_le(s, RDP_CAPLEN_SHARE);
    out_uint16_le(s, self->mcs_channel);
    out_uint16_be(s, 0xb5e2); /* 0x73e1 */

    /* Output general capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_GENERAL); /* 1 */
    out_uint16_le(s, RDP_CAPLEN_GENERAL); /* 24(0x18) */
    out_uint16_le(s, 1); /* OS major type */
    out_uint16_le(s, 3); /* OS minor type */
    out_uint16_le(s, 0x200); /* Protocol version */
    out_uint16_le(s, 0); /* pad */
    out_uint16_le(s, 0); /* Compression types */
    /* NO_BITMAP_COMPRESSION_HDR 0x0400
       FASTPATH_OUTPUT_SUPPORTED 0x0001 */
    if (self->client_info.use_fast_path & 1)
    {
        out_uint16_le(s, 0x401);
    }
    else
    {
        out_uint16_le(s, 0x400);
    }
    out_uint16_le(s, 0); /* Update capability */
    out_uint16_le(s, 0); /* Remote unshare capability */
    out_uint16_le(s, 0); /* Compression level */
    out_uint16_le(s, 0); /* Pad */

    /* Output bitmap capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_BITMAP); /* 2 */
    out_uint16_le(s, RDP_CAPLEN_BITMAP); /* 28(0x1c) */
    out_uint16_le(s, self->client_info.bpp); /* Preferred BPP */
    out_uint16_le(s, 1); /* Receive 1 BPP */
    out_uint16_le(s, 1); /* Receive 4 BPP */
    out_uint16_le(s, 1); /* Receive 8 BPP */
    out_uint16_le(s, self->client_info.width); /* width */
    out_uint16_le(s, self->client_info.height); /* height */
    out_uint16_le(s, 0); /* Pad */
    out_uint16_le(s, 1); /* Allow resize */
    out_uint16_le(s, 1); /* bitmap compression */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* pad */

    /* Output font capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_FONT); /* 14 */
    out_uint16_le(s, RDP_CAPLEN_FONT); /* 4 */

    /* Output order capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_ORDER); /* 3 */
    out_uint16_le(s, RDP_CAPLEN_ORDER); /* 88(0x58) */
    out_uint8s(s, 16);
    out_uint32_be(s, 0x40420f00);
    out_uint16_le(s, 1); /* Cache X granularity */
    out_uint16_le(s, 20); /* Cache Y granularity */
    out_uint16_le(s, 0); /* Pad */
    out_uint16_le(s, 1); /* Max order level */
    out_uint16_le(s, 0x2f); /* Number of fonts */
    out_uint16_le(s, 0x22); /* Capability flags */
    /* caps */
    out_uint8(s, 1); /* NEG_DSTBLT_INDEX                0x00 0 */
    out_uint8(s, 1); /* NEG_PATBLT_INDEX                0x01 1 */
    out_uint8(s, 1); /* NEG_SCRBLT_INDEX                0x02 2 */
    out_uint8(s, 1); /* NEG_MEMBLT_INDEX                0x03 3 */
    out_uint8(s, 0); /* NEG_MEM3BLT_INDEX               0x04 4 */
    out_uint8(s, 0); /* NEG_ATEXTOUT_INDEX              0x05 5 */
    out_uint8(s, 0); /* NEG_AEXTTEXTOUT_INDEX           0x06 6 */
    out_uint8(s, 0); /* NEG_DRAWNINEGRID_INDEX          0x07 7 */
    out_uint8(s, 1); /* NEG_LINETO_INDEX                0x08 8 */
    out_uint8(s, 0); /* NEG_MULTI_DRAWNINEGRID_INDEX    0x09 9 */
    out_uint8(s, 1); /* NEG_OPAQUE_RECT_INDEX           0x0A 10 */
    out_uint8(s, 0); /* NEG_SAVEBITMAP_INDEX            0x0B 11 */
    out_uint8(s, 0); /* NEG_WTEXTOUT_INDEX              0x0C 12 */
    out_uint8(s, 0); /* NEG_MEMBLT_V2_INDEX             0x0D 13 */
    out_uint8(s, 0); /* NEG_MEM3BLT_V2_INDEX            0x0E 14 */
    out_uint8(s, 0); /* NEG_MULTIDSTBLT_INDEX           0x0F 15 */
    out_uint8(s, 0); /* NEG_MULTIPATBLT_INDEX           0x10 16 */
    out_uint8(s, 0); /* NEG_MULTISCRBLT_INDEX           0x11 17 */
    out_uint8(s, 1); /* NEG_MULTIOPAQUERECT_INDEX       0x12 18 */
    out_uint8(s, 0); /* NEG_FAST_INDEX_INDEX            0x13 19 */
    out_uint8(s, 0); /* NEG_POLYGON_SC_INDEX            0x14 20 */
    out_uint8(s, 0); /* NEG_POLYGON_CB_INDEX            0x15 21 */
    out_uint8(s, 0); /* NEG_POLYLINE_INDEX              0x16 22 */
    out_uint8(s, 0); /* unused                          0x17 23 */
    out_uint8(s, 0); /* NEG_FAST_GLYPH_INDEX            0x18 24 */
    out_uint8(s, 0); /* NEG_ELLIPSE_SC_INDEX            0x19 25 */
    out_uint8(s, 0); /* NEG_ELLIPSE_CB_INDEX            0x1A 26 */
    out_uint8(s, 1); /* NEG_GLYPH_INDEX_INDEX           0x1B 27 */
    out_uint8(s, 0); /* NEG_GLYPH_WEXTTEXTOUT_INDEX     0x1C 28 */
    out_uint8(s, 0); /* NEG_GLYPH_WLONGTEXTOUT_INDEX    0x1D 29 */
    out_uint8(s, 0); /* NEG_GLYPH_WLONGEXTTEXTOUT_INDEX 0x1E 30 */
    out_uint8(s, 0); /* unused                          0x1F 31 */
    out_uint16_le(s, 0x6a1);
    /* declare support of bitmap cache rev3 */
    out_uint16_le(s, XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT);
    out_uint32_le(s, 0x0f4240); /* desk save */
    out_uint32_le(s, 0x0f4240); /* desk save */
    out_uint32_le(s, 1); /* ? */
    out_uint32_le(s, 0); /* ? */

    /* Output bmpcodecs capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_BMPCODECS);
    codec_caps_size_ptr = s->p;
    out_uint8s(s, 2); /* cap len set later */
    codec_caps_count = 0;
    codec_caps_count_ptr = s->p;
    out_uint8s(s, 1); /* bitmapCodecCount set later */
    /* nscodec */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_NSCODEC, 16);
    out_uint8(s, 1); /* codec id, must be 1 */
    out_uint16_le(s, 3); /* codecPropertiesLength */
    out_uint8(s, 0x01); /* fAllowDynamicFidelity */
    out_uint8(s, 0x01); /* fAllowSubsampling */
    out_uint8(s, 0x03); /* colorLossLevel */
#if defined(XRDP_RFXCODEC) || defined(XRDP_NEUTRINORDP)
    /* remotefx */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_REMOTEFX, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 4); /* codecPropertiesLength */
    out_uint32_le(s, 0); /* reserved */
    /* image remotefx */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_IMAGE_REMOTEFX, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 4); /* codecPropertiesLength */
    out_uint32_le(s, 0); /* reserved */
#endif
    /* jpeg */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_JPEG, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 1); /* codecPropertiesLength */
    out_uint8(s, 75); /* jpeg compression ratio */
    /* calculate and set size and count */
    codec_caps_size = (int)(s->p - codec_caps_size_ptr);
    codec_caps_size += 2; /* 2 bytes for RDP_CAPSET_BMPCODECS above */
    codec_caps_size_ptr[0] = codec_caps_size;
    codec_caps_size_ptr[1] = codec_caps_size >> 8;
    codec_caps_count_ptr[0] = codec_caps_count;

    /* Output color cache capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_COLCACHE);
    out_uint16_le(s, RDP_CAPLEN_COLCACHE);
    out_uint16_le(s, 6); /* cache size */
    out_uint16_le(s, 0); /* pad */

    /* Output pointer capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_POINTER);
    out_uint16_le(s, RDP_CAPLEN_POINTER);
    out_uint16_le(s, 1); /* Colour pointer */
    out_uint16_le(s, 0x19); /* Cache size */
    out_uint16_le(s, 0x19); /* Cache size */

    /* Output input capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_INPUT); /* 13(0xd) */
    out_uint16_le(s, RDP_CAPLEN_INPUT); /* 88(0x58) */

    flags = INPUT_FLAG_SCANCODES |
            INPUT_FLAG_MOUSEX    |
            INPUT_FLAG_UNICODE   |
            TS_INPUT_FLAG_MOUSE_HWHEEL;

    if (self->client_info.use_fast_path & 2)
    {
        flags |= INPUT_FLAG_FASTPATH_INPUT | INPUT_FLAG_FASTPATH_INPUT2;
    }
    out_uint16_le(s, flags);
    out_uint8s(s, 82);

    /* Remote Programs Capability Set */
    caps_count++;
    out_uint16_le(s, 0x0017); /* CAPSETTYPE_RAIL */
    out_uint16_le(s, 8);
    out_uint32_le(s, 3); /* TS_RAIL_LEVEL_SUPPORTED
                          TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED */

    /* Window List Capability Set */
    caps_count++;
    out_uint16_le(s, 0x0018); /* CAPSETTYPE_WINDOW */
    out_uint16_le(s, 11);
    out_uint32_le(s, 2); /* TS_WINDOW_LEVEL_SUPPORTED_EX */
    out_uint8(s, 3); /* NumIconCaches */
    out_uint16_le(s, 12); /* NumIconCacheEntries */

    /* 6 - bitmap cache v3 codecid */
    caps_count++;
    out_uint16_le(s, 0x0006);
    out_uint16_le(s, 5);
    out_uint8(s, 0); /* client sets */

    if (self->client_info.use_fast_path & FASTPATH_OUTPUT_SUPPORTED) /* fastpath output on */
    {
        /* multifragment update */
        caps_count++;
        out_uint16_le(s, RDP_CAPSET_MULTIFRAGMENT); /* 26 CAPSETTYPE_MULTIFRAGMENTUPDATE */
        out_uint16_le(s, RDP_CAPLEN_MULTIFRAGMENT);
        out_uint32_le(s, 3 * 1024 * 1024); /* 3MB */

        /* frame acks */
        caps_count++;
        out_uint16_le(s, RDP_CAPSET_FRAME_ACKNOWLEDGE); /* CAPSETTYPE_FRAME_ACKNOWLEDGE */
        out_uint16_le(s, RDP_CAPLEN_FRAME_ACKNOWLEDGE);
        out_uint32_le(s, 2); /* 2 frames in flight */

        /* surface commands */
        caps_count++;
        out_uint16_le(s, RDP_CAPSET_SURFCMDS); /* CAPSETTYPE_SURFACE_COMMANDS */
        out_uint16_le(s, RDP_CAPLEN_SURFCMDS); /* lengthCapability */
        out_uint32_le(s, (SURFCMDS_SETSURFACEBITS |
                          SURFCMDS_FRAMEMARKER |
                          SURFCMDS_STREAMSUFRACEBITS)); /* cmdFlags */
        out_uint32_le(s, 0); /* reserved */
    }

    out_uint8s(s, 4); /* pad */

    s_mark_end(s);

    caps_size = (int)(s->end - caps_ptr);
    caps_size_ptr[0] = caps_size;
    caps_size_ptr[1] = caps_size >> 8;

    caps_count_ptr[0] = caps_count;
    caps_count_ptr[1] = caps_count >> 8;
    caps_count_ptr[2] = caps_count >> 16;
    caps_count_ptr[3] = caps_count >> 24;

    if (xrdp_rdp_send(self, s, RDP_PDU_DEMAND_ACTIVE) != 0)
    {
        free_stream(s);
        return 1;
    }
    DEBUG(("out (1) xrdp_caps_send_demand_active"));

    /* send Monitor Layout PDU for dual monitor */
    if (self->client_info.monitorCount > 0 &&
        self->client_info.multimon == 1)
    {
        DEBUG(("xrdp_caps_send_demand_active: sending monitor layout pdu"));
        if (xrdp_caps_send_monitorlayout(self) != 0)
        {
          g_writeln("xrdp_caps_send_demand_active: error sending monitor layout pdu");
        }
    }

    free_stream(s);
    return 0;
}
