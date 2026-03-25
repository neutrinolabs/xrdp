/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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

/**
 * @file    common/xup_client_info.h
 * @brief   Data shared with xorgxrdp
 */

#if !defined(XUP_CLIENT_INFO_H)
#define XUP_CLIENT_INFO_H

#include "xrdp_client_info.h"

/**
 * Top-level message types used on the xrdp/xorgxrdp wire.
 */
enum xup_msg_type
{
    XUP_MSG_ORDER_LIST_LEGACY = 1,
    XUP_MSG_CAPS = 2,
    XUP_MSG_ORDER_LIST = 3,
    XUP_MSG_METADATA = 100,
    XUP_MSG_CLIENT_DATA = 103,
    XUP_MSG_CLIENT_INFO = 104,
    XUP_MSG_CLIENT_REGION = 105,
    XUP_MSG_CLIENT_REGION_EX = 106,
    XUP_MSG_CLIENT_SUPPRESS_OUTPUT = 108
};

/**
 * Sub-message types carried in @ref XUP_MSG_CLIENT_DATA.
 */
enum xup_client_data_type
{
    XUP_CLIENT_DATA_INVALIDATE = 200,
    XUP_CLIENT_DATA_DESKTOP_RESIZE = 300,
    XUP_CLIENT_DATA_VERSION = 301,
    XUP_CLIENT_DATA_MONITOR_UPDATE = 302
};

/**
 * Capability IDs carried in @ref XUP_MSG_CAPS.
 */
enum xup_capability_type
{
    XUP_CAPS_VERSION = 100
};

/**
 * Metadata item IDs carried in @ref XUP_MSG_METADATA.
 */
enum xup_metadata_type
{
    XUP_METADATA_CLEAR_MONITORS = 1,
    XUP_METADATA_ADD_MONITOR = 2,
    XUP_METADATA_MEMORY_ALLOCATION_COMPLETE = 3
};

/**
 * Order IDs carried in @ref XUP_MSG_ORDER_LIST and
 * @ref XUP_MSG_ORDER_LIST_LEGACY.
 */
enum xup_order_type
{
    XUP_ORDER_BEGIN_UPDATE = 1,
    XUP_ORDER_END_UPDATE = 2,
    XUP_ORDER_FILL_RECT = 3,
    XUP_ORDER_SCREEN_BLT = 4,
    XUP_ORDER_PAINT_RECT = 5,
    XUP_ORDER_SET_CLIP = 10,
    XUP_ORDER_RESET_CLIP = 11,
    XUP_ORDER_SET_FGCOLOR = 12,
    XUP_ORDER_SET_BGCOLOR = 13,
    XUP_ORDER_SET_OPCODE = 14,
    XUP_ORDER_SET_PEN = 17,
    XUP_ORDER_DRAW_LINE = 18,
    XUP_ORDER_SET_CURSOR = 19,
    XUP_ORDER_CREATE_OS_SURFACE = 20,
    XUP_ORDER_SWITCH_OS_SURFACE = 21,
    XUP_ORDER_DELETE_OS_SURFACE = 22,
    XUP_ORDER_PAINT_RECT_OS = 23,
    XUP_ORDER_SET_HINTS = 24,
    XUP_ORDER_WINDOW_NEW_UPDATE = 25,
    XUP_ORDER_WINDOW_DELETE = 26,
    XUP_ORDER_WINDOW_SHOW = 27,
    XUP_ORDER_ADD_CHAR = 28,
    XUP_ORDER_ADD_CHAR_ALPHA = 29,
    XUP_ORDER_DRAW_TEXT = 30,
    XUP_ORDER_CREATE_OS_SURFACE_BPP = 31,
    XUP_ORDER_PAINT_RECT_BPP = 32,
    XUP_ORDER_COMPOSITE = 33,
    XUP_ORDER_SET_CURSOR_EX = 51,
    XUP_ORDER_PAINT_RECT_SHMEM = 60,
    XUP_ORDER_PAINT_RECT_SHMEM_EX = 61,
    XUP_ORDER_EGFX_SHMFD = 62,
    XUP_ORDER_SET_POINTER_SHMFD = 63,
    XUP_ORDER_PAINT_RECT_SHMFD = 64,
    XUP_ORDER_SET_POINTER_SYSTEM = 65,
    XUP_ORDER_SET_POINTER_POSITION = 66
};

/**
 * Current xup wire protocol version.
 */
#define XUP_PROTOCOL_VERSION 1

/**
 * Information about the xrdp client which is passed to xorgxrdp
 *
 * This is a subset of 'struct xrdp_client_info'
 *
 * @note If you change this structure, you MUST bump the
 *       XUP_CLIENT_INFO_CURRENT_VERSION number so that the mismatch
 *       can be detected.
 */
struct xup_client_info
{
    int size; /* bytes for this structure */
    int version; /* Should be XUP_CLIENT_INFO_CURRENT_VERSION */
    int bpp;
    int jpeg; /* non standard bitmap cache v2 cap */
    int offscreen_support_level;
    int offscreen_cache_size;
    int offscreen_cache_entries;

    char orders[XR_PRIMARY_ORDER_COUNT];
    int order_flags_ex;
    int pointer_flags; /* 0 color, 1 new, 2 no new */
    int large_pointer_support_flags;

    struct display_size_description display_sizes;

    enum xrdp_capture_code capture_code;
    int capture_format;

    /* X11 keyboard layout - inferred from keyboard type/subtype */
    char model[CI_KBD_MODEL_SIZE];
    char layout[CI_KBD_LAYOUT_SIZE];
    char variant[CI_KBD_VARIANT_SIZE];
    char options[CI_KBD_OPTIONS_SIZE];
    char xkb_rules[CI_KBD_XKB_RULES_SIZE];
    // A few x11 keycodes are needed by the xup module
    int x11_keycode_caps_lock;
    int x11_keycode_num_lock;
    int x11_keycode_scroll_lock;

    /* xorgxrdp: frame capture interval (milliseconds) */
    int rfx_frame_interval;
    int h264_frame_interval;
    int normal_frame_interval;
};

/* yyyymmdd of last incompatible change to xup_client_info */
#define XUP_CLIENT_INFO_CURRENT_VERSION 20250528

#endif // XUP_CLIENT_INFO_H
