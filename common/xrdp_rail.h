/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2014
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

#if !defined(_RAIL_H)
#define _RAIL_H

/*
  ORDER_TYPE_WINDOW
    WINDOW_ORDER_TYPE_WINDOW
      WINDOW_ORDER_ICON
      WINDOW_ORDER_CACHED_ICON
      WINDOW_ORDER_STATE_DELETED
      WINDOW_ORDER_STATE_NEW on
      WINDOW_ORDER_STATE_NEW off
    WINDOW_ORDER_TYPE_NOTIFY
      WINDOW_ORDER_STATE_DELETED
      WINDOW_ORDER_STATE_NEW on
      WINDOW_ORDER_STATE_NEW off
    WINDOW_ORDER_TYPE_DESKTOP
      WINDOW_ORDER_FIELD_DESKTOP_NONE on
      WINDOW_ORDER_FIELD_DESKTOP_NONE off
*/

/* Window Order Header Flags */
#define WINDOW_ORDER_TYPE_WINDOW                        0x01000000
#define WINDOW_ORDER_TYPE_NOTIFY                        0x02000000
#define WINDOW_ORDER_TYPE_DESKTOP                       0x04000000
#define WINDOW_ORDER_STATE_NEW                          0x10000000
#define WINDOW_ORDER_STATE_DELETED                      0x20000000
#define WINDOW_ORDER_FIELD_OWNER                        0x00000002
#define WINDOW_ORDER_FIELD_STYLE                        0x00000008
#define WINDOW_ORDER_FIELD_SHOW                         0x00000010
#define WINDOW_ORDER_FIELD_TITLE                        0x00000004
#define WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET           0x00004000
#define WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE             0x00010000
#define WINDOW_ORDER_FIELD_RP_CONTENT                   0x00020000
#define WINDOW_ORDER_FIELD_ROOT_PARENT                  0x00040000
#define WINDOW_ORDER_FIELD_WND_OFFSET                   0x00000800
#define WINDOW_ORDER_FIELD_WND_CLIENT_DELTA             0x00008000
#define WINDOW_ORDER_FIELD_WND_SIZE                     0x00000400
#define WINDOW_ORDER_FIELD_WND_RECTS                    0x00000100
#define WINDOW_ORDER_FIELD_VIS_OFFSET                   0x00001000
#define WINDOW_ORDER_FIELD_VISIBILITY                   0x00000200
#define WINDOW_ORDER_FIELD_ICON_BIG                     0x00002000
#define WINDOW_ORDER_ICON                               0x40000000
#define WINDOW_ORDER_CACHED_ICON                        0x80000000
#define WINDOW_ORDER_FIELD_NOTIFY_VERSION               0x00000008
#define WINDOW_ORDER_FIELD_NOTIFY_TIP                   0x00000001
#define WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP              0x00000002
#define WINDOW_ORDER_FIELD_NOTIFY_STATE                 0x00000004
#define WINDOW_ORDER_FIELD_DESKTOP_NONE                 0x00000001
#define WINDOW_ORDER_FIELD_DESKTOP_HOOKED               0x00000002
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED        0x00000004
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_BEGAN            0x00000008
#define WINDOW_ORDER_FIELD_DESKTOP_ZORDER               0x00000010
#define WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND           0x00000020

struct rail_icon_info
{
    int bpp;
    int width;
    int height;
    int cmap_bytes;
    int mask_bytes;
    int data_bytes;
    char *mask;
    char *cmap;
    char *data;
};

struct rail_window_rect
{
    short left;
    short top;
    short right;
    short bottom;
};

struct rail_notify_icon_infotip
{
    int timeout;
    int flags;
    char *text;
    char *title;
};

struct rail_window_state_order
{
    int owner_window_id;
    int style;
    int extended_style;
    int show_state;
    char *title_info;
    int client_offset_x;
    int client_offset_y;
    int client_area_width;
    int client_area_height;
    int rp_content;
    int root_parent_handle;
    int window_offset_x;
    int window_offset_y;
    int window_client_delta_x;
    int window_client_delta_y;
    int window_width;
    int window_height;
    int num_window_rects;
    struct rail_window_rect *window_rects;
    int visible_offset_x;
    int visible_offset_y;
    int num_visibility_rects;
    struct rail_window_rect *visibility_rects;
};

struct rail_notify_state_order
{
    int version;
    char *tool_tip;
    struct rail_notify_icon_infotip infotip;
    int state;
    int icon_cache_entry;
    int icon_cache_id;
    struct rail_icon_info icon_info;
};

struct rail_monitored_desktop_order
{
    int active_window_id;
    int num_window_ids;
    int *window_ids;
};

#endif
