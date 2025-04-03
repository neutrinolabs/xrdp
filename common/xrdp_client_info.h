/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * xrdp / xserver info / caps
 */

#include "ms-rdpbcgr.h"

#if !defined(XRDP_CLIENT_INFO_H)
#define XRDP_CLIENT_INFO_H

/*
 * 2.2.1.3.6.1 Monitor Definition (TS_MONITOR_DEF)
 * 2.2.1.3.9.1 Monitor Attributes (TS_MONITOR_ATTRIBUTES)
 * 2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT
 */
struct monitor_info
{
    /* From 2.2.1.3.6.1 Monitor Definition (TS_MONITOR_DEF) */
    int left;
    int top;
    int right;
    int bottom;
    int flags;

    /* From [MS-RDPEDISP] 2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT, or
     * [MS-RDPBCGR] 2.2.1.3.9.1 (TS_MONITOR_ATTRIBUTES) */
    unsigned int physical_width;
    unsigned int physical_height;
    unsigned int orientation;
    unsigned int desktop_scale_factor;
    unsigned int device_scale_factor;

    /* Derived setting */
    unsigned int is_primary;
};

struct display_size_description
{
    unsigned int monitorCount; /* 2.2.2.2 DISPLAYCONTROL_MONITOR_LAYOUT_PDU: number of monitors detected (max = 16) */
    struct monitor_info minfo[CLIENT_MONITOR_DATA_MAXIMUM_MONITORS]; /* client monitor data */
    struct monitor_info minfo_wm[CLIENT_MONITOR_DATA_MAXIMUM_MONITORS]; /* client monitor data, non-negative values */
    unsigned int session_width;
    unsigned int session_height;
};

enum xrdp_capture_code
{
    CC_SIMPLE       = 0,
    CC_SUF_A16      = 1,
    CC_SUF_RFX      = 2,
    CC_SUF_A2       = 3,
    CC_GFX_PRO      = 4,
    CC_GFX_A2       = 5
};

/**
 * Information about the xrdp client
 *
 * @note This structure is shared with xorgxrdp. If you change anything
 *       in this structure, you MUST bump the CLIENT_INFO_CURRENT_VERSION
 *       number so that the mismatch can be detected.
 */
struct xrdp_client_info
{
    int size; /* bytes for this structure */
    int version; /* Should be CLIENT_INFO_CURRENT_VERSION */
    int bpp;
    int jpeg; /* non standard bitmap cache v2 cap */
    int offscreen_support_level;
    int offscreen_cache_size;
    int offscreen_cache_entries;

    char orders[32];
    int order_flags_ex;
    int pointer_flags; /* 0 color, 1 new, 2 no new */
    struct display_size_description display_sizes;

    enum xrdp_capture_code capture_code;
    int capture_format;

    /* X11 keyboard layout - inferred from keyboard type/subtype */
    char model[16];
    char layout[16];
    char variant[16];
    char options[256];
    char xkb_rules[32];
    // A few x11 keycodes are needed by the xup module
    int x11_keycode_caps_lock;
    int x11_keycode_num_lock;
    int x11_keycode_scroll_lock;

    /* xorgxrdp: frame capture interval (milliseconds) */
    int rfx_frame_interval;
    int h264_frame_interval;
    int normal_frame_interval;

    int large_pointer_support_flags;
};

enum xrdp_encoder_flags
{
    NONE                                   = 0,
    ENCODE_COMPLETE                        = 1 << 0,
    GFX_PROGRESSIVE_RFX                    = 1 << 1,
    GFX_H264                               = 1 << 2,
    KEY_FRAME_REQUESTED                    = 1 << 3
};

/* yyyymmdd of last incompatible change to xrdp_client_info */
/* also used for changes to all the xrdp installed headers */
#define CLIENT_INFO_CURRENT_VERSION 20250403

#endif
