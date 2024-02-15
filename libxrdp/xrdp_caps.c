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

#include <limits.h>

#include "guid.h"
#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "ms-rdperp.h"

/**
 * The largest supported size for a fastpath update
 * (TS_MULTIFRAGMENTUPDATE_CAPABILITYSET) we advertise to the client. This
 * size is big enough for the tiles required for two 3840x2160 monitors
 * without using multiple update PDUS.
 *
 * Consult calculate_multifragmentupdate_len() below before changing this
 * value.
 */
#define MAX_MULTIFRAGMENTUPDATE_SIZE (2U * (3840 * 2160) * 16384 + 16384)

/*****************************************************************************/
static int
xrdp_caps_send_monitorlayout(struct xrdp_rdp *self)
{
    struct stream *s;
    uint32_t i;
    struct display_size_description *description;
    int rv = 0;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    description = &self->client_info.display_sizes;

    out_uint32_le(s, description->monitorCount); /* monitorCount (4 bytes) */

    /* TODO: validate for allowed monitors in terminal server (maybe by config?) */
    for (i = 0; i < description->monitorCount; i++)
    {
        out_uint32_le(s, description->minfo[i].left);
        out_uint32_le(s, description->minfo[i].top);
        out_uint32_le(s, description->minfo[i].right);
        out_uint32_le(s, description->minfo[i].bottom);
        out_uint32_le(s, description->minfo[i].is_primary);
    }

    s_mark_end(s);

    // [MS-RDPBCGR]
    // - 2.2.12.1 - ...the pduSource field MUST be set to zero.
    // - 3.3.5.12.1 - The contents of this PDU SHOULD NOT be compressed
    rv = xrdp_rdp_send_data_from_channel(self, s,
                                         PDUTYPE2_MONITOR_LAYOUT_PDU, 0, 0);
    free_stream(s);
    return rv;
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
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 12, remaining %d", len);
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
xrdp_caps_process_bitmap(struct xrdp_rdp *self, struct stream *s,
                         int len)
{
    /* [MS-RDPBCGR] 2.2.7.1.2 */
    int desktopResizeFlag;
    if (len < 14 + 2)
    {
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 16, remaining %d", len);
        return 1;
    }

    in_uint8s(s, 14);
    in_uint16_le(s, desktopResizeFlag);

    /* Work out what kind of client resizing we can do from the server */
    int early_cap_flags = self->client_info.mcs_early_capability_flags;
    if (desktopResizeFlag == 0)
    {
        self->client_info.client_resize_mode = CRMODE_NONE;
        LOG(LOG_LEVEL_INFO, "Client cannot be resized by xrdp");
    }
    else if ((early_cap_flags & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU) == 0)
    {
        self->client_info.client_resize_mode = CRMODE_SINGLE_SCREEN;
        LOG(LOG_LEVEL_INFO, "Client supports single-screen resizes by xrdp");
    }
    else
    {
        self->client_info.client_resize_mode = CRMODE_MULTI_SCREEN;
        LOG(LOG_LEVEL_INFO, "Client supports multi-screen resizes by xrdp");
    }

    return 0;
}

/*****************************************************************************/
/*
 * Process [MS-RDPBCGR] TS_ORDER_CAPABILITYSET (2.2.7.1.3) message.
 */
static int
xrdp_caps_process_order(struct xrdp_rdp *self, struct stream *s,
                        int len)
{
    int i;
    char order_caps[32];
    int ex_flags;
    int cap_flags;

    if (len < 20 + 2 + 2 + 2 + 2 + 2 + 2 + 32 + 2 + 2 + 4 + 4 + 4 + 4)
    {
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 84, remaining %d", len);
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

    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: terminalDescriptor (ignored as per protocol spec)");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: desktopSaveXGranularity (ignored as per protocol spec)");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: desktopSaveYGranularity (ignored as per protocol spec)");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: maximumOrderLevel (ignored as per protocol spec)");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: numberFonts (ignored as per protocol spec)");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderFlags 0x%4.4x", cap_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 0: DstBlt %d", order_caps[0]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 1: PatBlt %d", order_caps[1]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 2: ScrBlt %d", order_caps[2]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 3,13: MemBlt %d %d", order_caps[3], order_caps[13]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 4,14: Mem3Blt %d %d", order_caps[4], order_caps[14]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 5-6: unused index");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 7: DrawNineGrid %d", order_caps[7]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 8: LineTo %d", order_caps[8]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 9: MultiDrawNineGrid %d", order_caps[9]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 10: unused index");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 11: SaveBitmap %d", order_caps[11]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 12-14: unused index");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 15: MultiDstBlt %d", order_caps[15]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 16: MultiPatBlt %d", order_caps[16]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 17: MultiScrBlt %d", order_caps[17]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 18: MultiOpaqueRect %d", order_caps[18]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 19: FastIndex %d", order_caps[19]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 20: PolygonSC %d", order_caps[20]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 21: PolygonCB %d", order_caps[21]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 22: Polyline %d", order_caps[22]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 23: unused index");
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 24: FastGlyph %d", order_caps[24]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 25: EllipseSC %d", order_caps[25]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 26: EllipseCB %d", order_caps[26]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 27: GlyphIndex %d", order_caps[27]);
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupport index 28-31: unused index");
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: order_caps", order_caps, 32);

    in_uint8s(s, 2); /* Text capability flags */
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: textFlags (ignored as per protocol spec)");
    /* read extended order support flags */
    in_uint16_le(s, ex_flags); /* Ex flags */
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: orderSupportExFlags 0x%4.4x", ex_flags);

    if (cap_flags & 0x80) /* ORDER_FLAGS_EXTRA_SUPPORT */
    {
        self->client_info.order_flags_ex = ex_flags;
        if (ex_flags & XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT)
        {
            LOG_DEVEL(LOG_LEVEL_INFO, "Client Capability: bitmap cache v3 supported");
            self->client_info.bitmap_cache_version |= 4;
        }
    }
    in_uint8s(s, 4); /* Pad */

    in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
    self->client_info.desktop_cache = i;
    LOG_DEVEL(LOG_LEVEL_TRACE, "TS_ORDER_CAPABILITYSET: desktopSaveSize %d", i);
    in_uint8s(s, 4); /* Pad */
    in_uint8s(s, 4); /* Pad */

    /* check if libpainter should be used for drawing, instead of orders */
    if (!(order_caps[TS_NEG_DSTBLT_INDEX] && order_caps[TS_NEG_PATBLT_INDEX] &&
            order_caps[TS_NEG_SCRBLT_INDEX] && order_caps[TS_NEG_MEMBLT_INDEX]))
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "Client Capability: not enough orders supported by client, using painter.");
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
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 36, remaining %d", len);
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
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache1 entries %d size %d", self->client_info.cache1_entries,
              self->client_info.cache1_size);
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache2 entries %d size %d", self->client_info.cache2_entries,
              self->client_info.cache2_size);
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache3 entries %d size %d", self->client_info.cache3_entries,
              self->client_info.cache3_size);
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
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 16, remaining %d", len);
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
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache1 entries %d size %d", self->client_info.cache1_entries,
              self->client_info.cache1_size);
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache2 entries %d size %d", self->client_info.cache2_entries,
              self->client_info.cache2_size);
    LOG_DEVEL(LOG_LEVEL_TRACE, "cache3 entries %d size %d", self->client_info.cache3_entries,
              self->client_info.cache3_size);
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
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 1, remaining %d", len);
        return 1;
    }
    in_uint8(s, codec_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_process_cache_v3_codec_id: cache_v3_codec_id %d",
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
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_pointer: error");
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
        LOG(LOG_LEVEL_INFO, "xrdp_caps_process_pointer: client supports "
            "new(color) cursor");
        in_uint16_le(s, i);
        i = MIN(i, 32);
        self->client_info.pointer_cache_entries = i;
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "xrdp_caps_process_pointer: client does not support "
            "new(color) cursor");
    }
    if (no_new_cursor)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_caps_process_pointer: new(color) cursor is "
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
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_brushcache: error");
        return 1;
    }
    in_uint32_le(s, code);
    self->client_info.brush_cache_code = code;
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_glyphcache(struct xrdp_rdp *self, struct stream *s,
                             int len)
{
    int glyph_support_level;

    if (len < 40 + 4 + 2 + 2) /* MS-RDPBCGR 2.2.7.1.8 */
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_glyphcache: error");
        return 1;
    }

    in_uint8s(s, 40);  /* glyph cache */
    in_uint8s(s, 4);   /* frag cache */
    in_uint16_le(s, glyph_support_level);
    in_uint8s(s, 2);   /* pad */

    if (glyph_support_level == GLYPH_SUPPORT_ENCODE)
    {
        self->client_info.use_cache_glyph_v2 = 1;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_process_glyphcache: support level %d ",
              glyph_support_level);
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
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_offscreen_bmpcache: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.offscreen_support_level = i32;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_size = i32 * 1024;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_entries = i32;
    LOG(LOG_LEVEL_INFO, "xrdp_process_offscreen_bmpcache: support level %d "
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
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream: "
            "len 4, remaining %d", len);
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.rail_support_level = i32;
    LOG(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - CAPSTYPE_RAIL "
        "RailSupportLevel 0x%8.8x (%s%s%s%s%s%s%s%s)",
        self->client_info.rail_support_level,
        (self->client_info.rail_support_level & 0x01) ? "TS_RAIL_LEVEL_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x02) ? "TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x04) ? "TS_RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x08) ? "TS_RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x10) ? "TS_RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x20) ? "TS_RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x40) ? "TS_RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED " : "",
        (self->client_info.rail_support_level & 0x80) ? "TS_RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED " : ""
       );
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_window(struct xrdp_rdp *self, struct stream *s, int len)
{
    int i32;

    if (len < 4 + 1 + 2)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_window: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.wnd_support_level = i32;
    in_uint8(s, i32);
    self->client_info.wnd_num_icon_caches = i32;
    in_uint16_le(s, i32);
    self->client_info.wnd_num_icon_cache_entries = i32;
    LOG(LOG_LEVEL_INFO, "xrdp_process_capset_window wnd_support_level %d, "
        "wnd_num_icon_caches %d, wnd_num_icon_cache_entries %d",
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
    struct guid guid;
    char codec_guid_str[GUID_STR_SIZE];

    guid_clear(&guid);

    if (len < 1)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_codecs: error");
        return 1;
    }
    in_uint8(s, codec_count);
    len--;

    for (index = 0; index < codec_count; index++)
    {
        codec_guid = s->p;

        g_memcpy(guid.g, s->p, GUID_SIZE);
        guid_to_str(&guid, codec_guid_str);

        if (len < 16 + 1 + 2)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_codecs: error");
            return 1;
        }
        in_uint8s(s, 16);
        in_uint8(s, codec_id);
        in_uint16_le(s, codec_properties_length);
        len -= 16 + 1 + 2;
        if (len < codec_properties_length)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_caps_process_codecs: error");
            return 1;
        }
        len -= codec_properties_length;
        next_guid = s->p + codec_properties_length;

        if (g_memcmp(codec_guid, XR_CODEC_GUID_NSCODEC, 16) == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_caps_process_codecs: NSCodec(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
            self->client_info.ns_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.ns_prop, s->p, i1);
            self->client_info.ns_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_REMOTEFX, 16) == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_caps_process_codecs: RemoteFX(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
            self->client_info.rfx_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.rfx_prop, s->p, i1);
            self->client_info.rfx_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_JPEG, 16) == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_caps_process_codecs: JPEG(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
            self->client_info.jpeg_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.jpeg_prop, s->p, i1);
            self->client_info.jpeg_prop_len = i1;
            /* make sure that requested quality is  between 0 to 100 */
            if (self->client_info.jpeg_prop[0] < 0 || self->client_info.jpeg_prop[0] > 100)
            {
                LOG(LOG_LEVEL_WARNING, "  Warning: the requested jpeg quality (%d) is invalid, "
                    "falling back to default", self->client_info.jpeg_prop[0]);
                self->client_info.jpeg_prop[0] = 75; /* use default */
            }
            LOG(LOG_LEVEL_INFO, "  jpeg quality set to %d", self->client_info.jpeg_prop[0]);
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_H264, 16) == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_caps_process_codecs: H264(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
            self->client_info.h264_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.h264_prop, s->p, i1);
            self->client_info.h264_prop_len = i1;
        }
        /* other known codec but not supported yet */
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_IMAGE_REMOTEFX, 16) == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_caps_process_codecs: Image RemoteFX(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "xrdp_caps_process_codecs: unknown(%s), codec id [%d], properties len [%d]",
                codec_guid_str, codec_id, codec_properties_length);
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
    if (self->client_info.use_fast_path & 1)
    {
        self->client_info.max_fastpath_frag_bytes = MaxRequestSize;
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_largepointer(struct xrdp_rdp *self, struct stream *s,
                               int len)
{
    int largePointerSupportFlags;

    in_uint16_le(s, largePointerSupportFlags);
    self->client_info.large_pointer_support_flags = largePointerSupportFlags;
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_frame_ack(struct xrdp_rdp *self, struct stream *s, int len)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_process_frame_ack:");
    self->client_info.use_frame_acks = 1;
    in_uint32_le(s, self->client_info.max_unacknowledged_frame_count);
    if (self->client_info.max_unacknowledged_frame_count < 0)
    {
        LOG(LOG_LEVEL_WARNING, "  invalid max_unacknowledged_frame_count value (%d), setting to 0",
            self->client_info.max_unacknowledged_frame_count);
        self->client_info.max_unacknowledged_frame_count = 0;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "  max_unacknowledged_frame_count %d", self->client_info.max_unacknowledged_frame_count);
    return 0;
}

/*****************************************************************************/
static int
xrdp_caps_process_surface_cmds(struct xrdp_rdp *self, struct stream *s, int len)
{
    int cmdFlags;
#ifndef USE_DEVEL_LOGGING
    /* TODO: remove UNUSED_VAR once the `cmdFlags` variable is used for more than
    logging in debug mode */
    UNUSED_VAR(cmdFlags);
#endif

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_process_surface_cmds:");
    in_uint32_le(s, cmdFlags);
    in_uint8s(s, 4); /* reserved */
    LOG_DEVEL(LOG_LEVEL_TRACE, "  cmdFlags 0x%08x", cmdFlags);
    return 0;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU (2.2.1.13.2.1) message.
 */
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

    if (!s_check_rem_and_log(s, 10,
                             "Parsing [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU"
                             " - header"))
    {
        return 1;
    }
    in_uint8s(s, 4); /* rdp_shareid */
    in_uint8s(s, 2); /* userid */
    in_uint16_le(s, source_len); /* sizeof RDP_SOURCE */
    in_uint16_le(s, cap_len);

    if (!s_check_rem_and_log(s, source_len + 2 + 2,
                             "Parsing [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU"
                             " - header2"))
    {
        return 1;
    }
    in_uint8s(s, source_len);
    in_uint16_le(s, num_caps);
    in_uint8s(s, 2); /* pad */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU "
              "shareID (ignored), originatorID (ignored), lengthSourceDescriptor %d, "
              "lengthCombinedCapabilities  %d, sourceDescriptor (ignored), "
              "numberCapabilities %d", source_len, cap_len, num_caps);

    if ((cap_len < 0) || (cap_len > 1024 * 1024))
    {
        LOG(LOG_LEVEL_ERROR, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU "
            "lengthCombinedCapabilities %d is too long (> %d)",
            cap_len, 1024 * 1024);
        return 1;
    }

    for (index = 0; index < num_caps; index++)
    {
        p = s->p;
        if (!s_check_rem_and_log(s, 4,
                                 "Parsing [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET"))
        {
            return 1;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, len);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                  "capabilitySetType %d, lengthCapability %d", type, len);
        if (len < 4)
        {
            LOG(LOG_LEVEL_ERROR,
                "Protocol error [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                "lengthCapability must be greater than 3, received %d", len);
            return 1;
        }
        if (!s_check_rem_and_log(s, len - 4,
                                 "Parsing [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "))
        {
            return 1;
        }
        len -= 4;
        switch (type)
        {
            case CAPSTYPE_GENERAL:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_GENERAL");
                xrdp_caps_process_general(self, s, len);
                break;
            case CAPSTYPE_BITMAP:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_BITMAP");
                xrdp_caps_process_bitmap(self, s, len);
                break;
            case CAPSTYPE_ORDER:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_ORDER");
                xrdp_caps_process_order(self, s, len);
                break;
            case CAPSTYPE_BITMAPCACHE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_BITMAPCACHE");
                xrdp_caps_process_bmpcache(self, s, len);
                break;
            case CAPSTYPE_CONTROL:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_CONTROL - Ignored");
                break;
            case 6:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = 6");
                xrdp_caps_process_cache_v3_codec_id(self, s, len);
                break;
            case CAPSTYPE_ACTIVATION:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_ACTIVATION - Ignored");
                break;
            case CAPSTYPE_POINTER:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_POINTER");
                xrdp_caps_process_pointer(self, s, len);
                break;
            case CAPSTYPE_SHARE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_SHARE - Ignored");
                break;
            case CAPSTYPE_COLORCACHE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_COLORCACHE - Ignored");
                break;
            case CAPSTYPE_SOUND:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_SOUND - Ignored");
                break;
            case CAPSTYPE_INPUT:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_INPUT");
                xrdp_caps_process_input(self, s, len);
                break;
            case CAPSTYPE_FONT:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_FONT - Ignored");
                break;
            case CAPSTYPE_BRUSH:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_BRUSH");
                xrdp_caps_process_brushcache(self, s, len);
                break;
            case CAPSTYPE_GLYPHCACHE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_GLYPHCACHE");
                xrdp_caps_process_glyphcache(self, s, len);
                break;
            case CAPSTYPE_OFFSCREENCACHE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_OFFSCREENCACHE");
                xrdp_caps_process_offscreen_bmpcache(self, s, len);
                break;
            case CAPSTYPE_BITMAPCACHE_REV2:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_BITMAPCACHE_REV2");
                xrdp_caps_process_bmpcache2(self, s, len);
                break;
            case CAPSTYPE_VIRTUALCHANNEL:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_VIRTUALCHANNEL - Ignored");
                break;
            case CAPSTYPE_DRAWNINGRIDCACHE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_DRAWNINGRIDCACHE - Ignored");
                break;
            case CAPSTYPE_DRAWGDIPLUS:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_DRAWGDIPLUS - Ignored");
                break;
            case CAPSTYPE_RAIL:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_RAIL");
                xrdp_caps_process_rail(self, s, len);
                break;
            case CAPSTYPE_WINDOW:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_WINDOW");
                xrdp_caps_process_window(self, s, len);
                break;
            case CAPSSETTYPE_MULTIFRAGMENTUPDATE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSSETTYPE_MULTIFRAGMENTUPDATE");
                xrdp_caps_process_multifragmentupdate(self, s, len);
                break;
            case CAPSETTYPE_LARGE_POINTER:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSETTYPE_LARGE_POINTER");
                xrdp_caps_process_largepointer(self, s, len);
                break;
            case CAPSETTYPE_SURFACE_COMMANDS:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSETTYPE_SURFACE_COMMANDS");
                xrdp_caps_process_surface_cmds(self, s, len);
                break;
            case CAPSSETTYPE_BITMAP_CODECS:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSSETTYPE_BITMAP_CODECS");
                xrdp_caps_process_codecs(self, s, len);
                break;
            case CAPSTYPE_FRAME_ACKNOWLEDGE:
                LOG_DEVEL(LOG_LEVEL_INFO, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                          "capabilitySetType = CAPSTYPE_FRAME_ACKNOWLEDGE");
                xrdp_caps_process_frame_ack(self, s, len);
                break;
            default:
                LOG(LOG_LEVEL_WARNING, "Received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU - TS_CAPS_SET "
                    "capabilitySetType = %d is unknown - Ignored", type);
                break;
        }

        s->p = p + len + 4;
    }

    if (self->client_info.no_orders_supported &&
            (self->client_info.offscreen_support_level != 0))
    {
        LOG(LOG_LEVEL_WARNING, "Client Capability: not enough orders "
            "supported by client, client wants off screen bitmap but "
            "offscreen bitmaps disabled");
        self->client_info.offscreen_support_level = 0;
        self->client_info.offscreen_cache_size = 0;
        self->client_info.offscreen_cache_entries = 0;
    }

    if (self->client_info.use_fast_path)
    {
        if ((self->client_info.large_pointer_support_flags & LARGE_POINTER_FLAG_96x96) &&
                (self->client_info.max_fastpath_frag_bytes < 38055))
        {
            LOG(LOG_LEVEL_WARNING, "Client Capability: client requested "
                "LARGE_POINTER_FLAG_96x96 but max_fastpath_frag_bytes(%d) is less then "
                "the required size of 38055, 96x96 large cursor support disabled",
                self->client_info.max_fastpath_frag_bytes);
            self->client_info.large_pointer_support_flags &= ~LARGE_POINTER_FLAG_96x96;
        }
        if ((self->client_info.large_pointer_support_flags & LARGE_POINTER_FLAG_384x384) &&
                (self->client_info.max_fastpath_frag_bytes < 608299))
        {
            LOG(LOG_LEVEL_WARNING, "Client Capability: client requested "
                "LARGE_POINTER_FLAG_384x384 but max_fastpath_frag_bytes(%d) is less then "
                "the required size of 608299, 384x384 large cursor support disabled",
                self->client_info.max_fastpath_frag_bytes);
            self->client_info.large_pointer_support_flags &= ~LARGE_POINTER_FLAG_384x384;
        }
    }
    else
    {
        if (self->client_info.large_pointer_support_flags != 0)
        {
            LOG(LOG_LEVEL_WARNING, "Client Capability: client requested "
                "large pointers but use_fast_path is false, "
                "all large cursor support disabled");
            self->client_info.large_pointer_support_flags = 0;
        }
    }
    if (self->client_info.large_pointer_support_flags & LARGE_POINTER_FLAG_96x96)
    {
        LOG(LOG_LEVEL_INFO, "Client Capability: LARGE_POINTER_FLAG_96x96 supported");
    }
    if (self->client_info.large_pointer_support_flags & LARGE_POINTER_FLAG_384x384)
    {
        LOG(LOG_LEVEL_INFO, "Client Capability: LARGE_POINTER_FLAG_384x384 supported");
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Completed processing received [MS-RDPBCGR] TS_CONFIRM_ACTIVE_PDU");
    return 0;
}

/**************************************************************************//**
 * Calculate the multifragmentupdate len we advertised to the client
 * for fastpath updates
 *
 * See [MS-RDPBCGR] 2.2.7.2.6
 *
 * The basic logic is taken from freerdp 2.4. We try to use the highest
 * useful request size that will allow us to pack a complete screen
 * update into a single fast path PDU using any of the supported codecs.
 * For RemoteFX, the client MUST use at least this value
 *
 * A backstop on the maximum advertised size is implemented to prevent
 * extreme memory usage for large screen configurations. RDP supports a
 * maximum desktop size of 32768x32768, which would cause overflow for
 * 32-bit integers using a simple calculation.
 *
 * The codecs have to deal with the value returned by the client after
 * we advertise our own value, and must not assume a complete update
 * will fit in a single PDU
 */
static
unsigned int calculate_multifragmentupdate_len(const struct xrdp_rdp *self)
{
    unsigned int result = MAX_MULTIFRAGMENTUPDATE_SIZE;

    unsigned int x_tiles = (self->client_info.display_sizes.session_width + 63) / 64;
    unsigned int y_tiles = (self->client_info.display_sizes.session_height + 63) / 64;

    /* Check for overflow on calculation if bad parameters are supplied */
    if ((x_tiles * y_tiles  + 1) < (UINT_MAX / 16384))
    {
        result = x_tiles * y_tiles * 16384;
        /* and add room for headers, regions, frame markers, etc. */
        result += 16384;
    }

    return result;
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


    if (xrdp_rdp_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_send_demand_active: xrdp_rdp_init failed");
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
    out_uint16_le(s, CAPSTYPE_SHARE);
    out_uint16_le(s, CAPSTYPE_SHARE_LEN);
    out_uint16_le(s, self->mcs_channel);
    out_uint16_be(s, 0xb5e2); /* 0x73e1 */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_SHARE "
              "channel ID = 0x%x", self->mcs_channel);

    /* Output general capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_GENERAL);
    out_uint16_le(s, CAPSTYPE_GENERAL_LEN);
    out_uint16_le(s, OSMAJORTYPE_WINDOWS);
    out_uint16_le(s, OSMINORTYPE_WINDOWS_NT);
    out_uint16_le(s, 0x200); /* Protocol version */
    out_uint16_le(s, 0); /* pad */
    out_uint16_le(s, 0); /* Compression types */
    if (self->client_info.use_fast_path & 1)
    {
        out_uint16_le(s, NO_BITMAP_COMPRESSION_HDR | FASTPATH_OUTPUT_SUPPORTED);
    }
    else
    {
        out_uint16_le(s, NO_BITMAP_COMPRESSION_HDR);
    }
    out_uint16_le(s, 0); /* Update capability */
    out_uint16_le(s, 0); /* Remote unshare capability */
    out_uint16_le(s, 0); /* Compression level */
    out_uint8(s, 1); /* refreshRectSupport */
    out_uint8(s, 1); /* suppressOutputSupport */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_GENERAL TODO");

    /* Output bitmap capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_BITMAP);
    out_uint16_le(s, CAPSTYPE_BITMAP_LEN);
    out_uint16_le(s, self->client_info.bpp); /* Preferred BPP */
    out_uint16_le(s, 1); /* Receive 1 BPP */
    out_uint16_le(s, 1); /* Receive 4 BPP */
    out_uint16_le(s, 1); /* Receive 8 BPP */
    out_uint16_le(s, self->client_info.display_sizes.session_width); /* width */
    out_uint16_le(s, self->client_info.display_sizes.session_height); /* height */
    out_uint16_le(s, 0); /* Pad */
    out_uint16_le(s, 1); /* Allow resize */
    out_uint16_le(s, 1); /* bitmap compression */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* pad */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_BITMAP TODO");

    /* Output font capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_FONT);
    out_uint16_le(s, CAPSTYPE_FONT_LEN);
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_FONT");

    /* Output order capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_ORDER);
    out_uint16_le(s, CAPSTYPE_ORDER_LEN);
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
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_ORDER "
              "TODO");

    /* Output bmpcodecs capability set */
    caps_count++;
    out_uint16_le(s, CAPSSETTYPE_BITMAP_CODECS);
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
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "NSCODEC "
              "fAllowDynamicFidelity = 0x01,"
              "fAllowSubsampling = 0x01,"
              "colorLossLevel = 0x03");
#if defined(XRDP_RFXCODEC) || defined(XRDP_NEUTRINORDP)
    /* remotefx */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_REMOTEFX, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 4); /* codecPropertiesLength */
    out_uint32_le(s, 0); /* reserved */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "REMOTEFX");
    /* image remotefx */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_IMAGE_REMOTEFX, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 4); /* codecPropertiesLength */
    out_uint32_le(s, 0); /* reserved */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "IMAGE_REMOTEFX");
#endif
    /* jpeg */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_JPEG, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 1); /* codecPropertiesLength */
    out_uint8(s, 75); /* jpeg compression ratio */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "JPEG: "
              "jpeg compression ratio = 75");
    /* calculate and set size and count */
    codec_caps_size = (int)(s->p - codec_caps_size_ptr);
    codec_caps_size += 2; /* 2 bytes for CAPSTYPE_BMPCODECS above */
    codec_caps_size_ptr[0] = codec_caps_size;
    codec_caps_size_ptr[1] = codec_caps_size >> 8;
    codec_caps_count_ptr[0] = codec_caps_count;

    /* Output color cache capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_COLORCACHE);
    out_uint16_le(s, CAPSTYPE_COLORCACHE_LEN);
    out_uint16_le(s, 6); /* cache size */
    out_uint16_le(s, 0); /* pad */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_COLORCACHE: "
              "colorTableCacheSize = 6");

    /* Output pointer capability set */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_POINTER);
    out_uint16_le(s, CAPSTYPE_POINTER_LEN);
    out_uint16_le(s, 1); /* Colour pointer */
    out_uint16_le(s, 0x19); /* Cache size */
    out_uint16_le(s, 0x19); /* Cache size */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_POINTER: "
              "colorPointerFlag = true"
              "colorPointerCacheSize = 0x19"
              "pointerCacheSize = 0x19");

    /* Output input capability set */
    /* https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpbcgr/b3bc76ae-9ee5-454f-b197-ede845ca69cc */
    caps_count++;
    out_uint16_le(s, CAPSTYPE_INPUT);
    out_uint16_le(s, CAPSTYPE_INPUT_LEN);

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
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "CAPSTYPE_INPUT: "
              "inputFlags = 0x%x", flags);

    if (self->client_info.rail_enable) /* MS-RDPERP 3.3.5.1.4 */
    {
        /* Remote Programs Capability Set */
        caps_count++;
        out_uint16_le(s, CAPSTYPE_RAIL);
        out_uint16_le(s, 8); /* LengthCapability: MS-RDPERP 2.2.1.1.1 */
        out_uint32_le(s, 3); /* See: https://msdn.microsoft.com/en-us/library/cc242518.aspx
                                TS_RAIL_LEVEL_SUPPORTED
                                TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED */
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
                  "CAPSTYPE_RAIL: "
                  "RailSupportLevel = "
                  "TS_RAIL_LEVEL_SUPPORTED | TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED");

        /* Window List Capability Set */
        caps_count++;
        out_uint16_le(s, CAPSTYPE_WINDOW);
        out_uint16_le(s, 11); /* LengthCapability: MS-RDPERP 2.2.1.1.2 */
        out_uint32_le(s, TS_WINDOW_LEVEL_SUPPORTED_EX);
        out_uint8(s, 3); /* NumIconCaches */
        out_uint16_le(s, 12); /* NumIconCacheEntries */
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
                  "CAPSTYPE_WINDOW: "
                  "WndSupportLevel = TS_WINDOW_LEVEL_SUPPORTED_EX, "
                  "NumIconCaches = 3,"
                  "NumIconCacheEntries = 12");
    }

    /* 6 - bitmap cache v3 codecid */
    caps_count++;
    out_uint16_le(s, 0x0006);
    out_uint16_le(s, 5);
    out_uint8(s, 0); /* client sets */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
              "0x0006 = 0");

    if (self->client_info.use_fast_path & FASTPATH_OUTPUT_SUPPORTED) /* fastpath output on */
    {
        unsigned int max_request_size = calculate_multifragmentupdate_len(self);
        caps_count++;
        out_uint16_le(s, CAPSSETTYPE_MULTIFRAGMENTUPDATE);
        out_uint16_le(s, CAPSSETTYPE_MULTIFRAGMENTUPDATE_LEN);
        out_uint32_le(s, max_request_size);
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
                  "CAPSSETTYPE_MULTIFRAGMENTUPDATE = %d", max_request_size);

        /* large pointer 96x96 */
        caps_count++;
        out_uint16_le(s, CAPSETTYPE_LARGE_POINTER);
        out_uint16_le(s, CAPSETTYPE_LARGE_POINTER_LEN);
        out_uint16_le(s, LARGE_POINTER_FLAG_96x96);

        /* frame acks */
        caps_count++;
        out_uint16_le(s, CAPSTYPE_FRAME_ACKNOWLEDGE);
        out_uint16_le(s, CAPSTYPE_FRAME_ACKNOWLEDGE_LEN);
        out_uint32_le(s, 2); /* 2 frames in flight */
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
                  "CAPSTYPE_FRAME_ACKNOWLEDGE = 2 frames");

        /* surface commands */
        caps_count++;
        out_uint16_le(s, CAPSETTYPE_SURFACE_COMMANDS);
        out_uint16_le(s, CAPSETTYPE_SURFACE_COMMANDS_LEN);
        out_uint32_le(s, (SURFCMDS_SETSURFACEBITS |
                          SURFCMDS_FRAMEMARKER |
                          SURFCMDS_STREAMSUFRACEBITS)); /* cmdFlags */
        out_uint32_le(s, 0); /* reserved */
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: Server Capability "
                  "CAPSETTYPE_SURFACE_COMMANDS = "
                  "SURFCMDS_SETSURFACEBITS | SURFCMDS_FRAMEMARKER | SURFCMDS_STREAMSUFRACEBITS");
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

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: sending PDUTYPE_DEMANDACTIVEPDU "
              "message with the server's capabilities");
    if (xrdp_rdp_send(self, s, PDUTYPE_DEMANDACTIVEPDU) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_caps_send_demand_active: xrdp_rdp_send failed");
        free_stream(s);
        return 1;
    }

    /* send Monitor Layout PDU for multi-monitor */
    int early_cap_flags = self->client_info.mcs_early_capability_flags;

    if ((early_cap_flags & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU) != 0 &&
            self->client_info.display_sizes.monitorCount > 0 &&
            self->client_info.multimon == 1)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_caps_send_demand_active: sending monitor layout pdu");
        if (xrdp_caps_send_monitorlayout(self) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_caps_send_demand_active: error sending monitor layout pdu");
        }
    }

    free_stream(s);
    return 0;
}
