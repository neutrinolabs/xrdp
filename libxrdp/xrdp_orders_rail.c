/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp_orders_rail.h"
#include "libxrdp.h"
#include "ms-rdpegdi.h"
#include "xrdp_rail.h"
#include "string_calls.h"

/* [MS-RDPERP]: Remote Desktop Protocol:
   Remote Programs Virtual Channel Extension
   http://msdn.microsoft.com/en-us/library/cc242568(v=prot.10) */

/*****************************************************************************/
/* RAIL */
/* returns error */
int
xrdp_orders_send_window_delete(struct xrdp_orders *self, int window_id)
{
    int order_size;
    int order_flags;
    int field_present_flags;

    order_size = 11;
    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_window_delete: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    field_present_flags = WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_STATE_DELETED;
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_WINDOW_ORDER_HEADER "
              "OrderSize %d, "
              "FieldsPresentFlags 0x%8.8x (WINDOW_ORDER_TYPE_WINDOW | WINDOW_ORDER_STATE_DELETED), "
              "WindowId 0x%8.8x",
              order_size, field_present_flags, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] Deleted Window");
    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
/* flags can contain WINDOW_ORDER_STATE_NEW and/or
   WINDOW_ORDER_FIELD_ICON_BIG */
int
xrdp_orders_send_window_cached_icon(struct xrdp_orders *self,
                                    int window_id, int cache_entry,
                                    int cache_id, int flags)
{
    int order_size;
    int order_flags;
    int field_present_flags;

    order_size = 14;
    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_window_cached_icon: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    field_present_flags = flags | WINDOW_ORDER_TYPE_WINDOW |
                          WINDOW_ORDER_CACHED_ICON;
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_WINDOW_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x, WindowId 0x%8.8x",
              order_size, field_present_flags, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] Cached Icon");

    /* CacheEntry (2 bytes) */
    out_uint16_le(self->out_s, cache_entry);
    /* CacheId (1 byte) */
    out_uint8(self->out_s, cache_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order field [MS-RDPERP] TS_CACHED_ICON_INFO "
              "CacheEntry %d, CacheId %d", cache_entry, cache_id);
    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
static int
xrdp_orders_send_ts_icon(struct stream *s, int cache_entry, int cache_id,
                         struct rail_icon_info *icon_info)
{
    int use_cmap;

    use_cmap = 0;

    if ((icon_info->bpp == 1) || (icon_info->bpp == 2) || (icon_info->bpp == 4))
    {
        use_cmap = 1;
    }

    /* TS_ICON_INFO */
    out_uint16_le(s, cache_entry);
    out_uint8(s, cache_id);
    out_uint8(s, icon_info->bpp);
    out_uint16_le(s, icon_info->width);
    out_uint16_le(s, icon_info->height);

    if (use_cmap)
    {
        out_uint16_le(s, icon_info->cmap_bytes);
    }

    out_uint16_le(s, icon_info->mask_bytes);
    out_uint16_le(s, icon_info->data_bytes);
    out_uint8p(s, icon_info->mask, icon_info->mask_bytes);

    if (use_cmap)
    {
        out_uint8p(s, icon_info->cmap, icon_info->cmap_bytes);
    }

    out_uint8p(s, icon_info->data, icon_info->data_bytes);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order fields [MS-RDPERP] TS_ICON_INFO "
              "CacheEntry %d, CacheId %d, Bpp %d, Width %d, Height %d, "
              "CbColorTable <%s>, "
              "CbBitsMask %d, CbBitsColor %d, BitsMask <omitted from log>, "
              "ColorTable <%s>, "
              "BitsColor <omitted from log>",
              cache_entry, cache_id, icon_info->bpp, icon_info->width,
              icon_info->height,
              (use_cmap ? "present, omitted from log" : "not present"),
              icon_info->mask_bytes, icon_info->data_bytes,
              (use_cmap ? "present, omitted from log" : "not present")
             );
    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
/* flags can contain WINDOW_ORDER_STATE_NEW and/or
   WINDOW_ORDER_FIELD_ICON_BIG */
int
xrdp_orders_send_window_icon(struct xrdp_orders *self,
                             int window_id, int cache_entry, int cache_id,
                             struct rail_icon_info *icon_info,
                             int flags)
{
    int order_size;
    int order_flags;
    int field_present_flags;
    int use_cmap;

    use_cmap = 0;

    if ((icon_info->bpp == 1) || (icon_info->bpp == 2) || (icon_info->bpp == 4))
    {
        use_cmap = 1;
    }

    order_size = 23 + icon_info->mask_bytes + icon_info->data_bytes;

    if (use_cmap)
    {
        order_size += icon_info->cmap_bytes + 2;
    }

    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_window_icon: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    field_present_flags = flags | WINDOW_ORDER_TYPE_WINDOW |
                          WINDOW_ORDER_ICON;
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_WINDOW_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x, WindowId 0x%8.8x",
              order_size, field_present_flags, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] Window Icon");

    xrdp_orders_send_ts_icon(self->out_s, cache_entry, cache_id, icon_info);

    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_orders_send_as_unicode(struct stream *s, const char *text)
{
    unsigned int text_len = strlen(text);
    int i32 = utf8_as_utf16_word_count(text, text_len) * 2;

    out_uint16_le(s, i32);
    out_utf8_as_utf16_le(s, text, text_len);
    return 0;
}

/*****************************************************************************/
static int
xrdp_orders_get_unicode_bytes(const char *text)
{
    unsigned int text_len = strlen(text);
    /* Add 1 to word size to include length ([MS-RDPERP] 2.2.1.2.1) */
    return (utf8_as_utf16_word_count(text, text_len) + 1) * 2;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
/* flags can contain WINDOW_ORDER_STATE_NEW */
int
xrdp_orders_send_window_new_update(struct xrdp_orders *self, int window_id,
                                   struct rail_window_state_order *window_state,
                                   int flags)
{
    int order_size;
    int order_flags;
    int field_present_flags;
    int index;

    order_size = 11;
    field_present_flags = flags | WINDOW_ORDER_TYPE_WINDOW;

    if (field_present_flags & WINDOW_ORDER_FIELD_OWNER)
    {
        /* ownerWindowId (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_STYLE)
    {
        /* style (4 bytes) */
        order_size += 4;
        /* extendedStyle (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_SHOW)
    {
        /* showState (1 byte) */
        order_size += 1;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_TITLE)
    {
        /* titleInfo */
        order_size += xrdp_orders_get_unicode_bytes(window_state->title_info);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
    {
        /* clientOffsetX (4 bytes) */
        order_size += 4;
        /* clientOffsetY (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
    {
        /* clientAreaWidth (4 bytes) */
        order_size += 4;
        /* clientAreaHeight (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_RP_CONTENT)
    {
        /* RPContent (1 byte) */
        order_size += 1;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_ROOT_PARENT)
    {
        /* rootParentHandle (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_OFFSET)
    {
        /* windowOffsetX (4 bytes) */
        order_size += 4;
        /* windowOffsetY (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
    {
        /* windowClientDeltaX (4 bytes) */
        order_size += 4;
        /* windowClientDeltaY (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_SIZE)
    {
        /* windowWidth (4 bytes) */
        order_size += 4;
        /* windowHeight (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_RECTS)
    {
        /* numWindowRects (2 bytes) */
        order_size += 2;
        order_size += 8 * window_state->num_window_rects;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_VIS_OFFSET)
    {
        /* visibleOffsetX (4 bytes) */
        order_size += 4;
        /* visibleOffsetY (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_VISIBILITY)
    {
        /* numVisibilityRects (2 bytes) */
        order_size += 2;
        order_size += 8 * window_state->num_visibility_rects;
    }

    if (order_size < 12)
    {
        /* no flags set */
        return 0;
    }

    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_window_new_update: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_WINDOW_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x, WindowId 0x%8.8x",
              order_size, field_present_flags, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] New or Existing Window (TS_WINDOW_INFO)");

    if (field_present_flags & WINDOW_ORDER_FIELD_OWNER)
    {
        /* ownerWindowId (4 bytes) */
        out_uint32_le(self->out_s, window_state->owner_window_id);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "OwnerWindowId 0x%8.8x", window_state->owner_window_id);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_STYLE)
    {
        /* style (4 bytes) */
        out_uint32_le(self->out_s, window_state->style);
        /* extendedStyle (4 bytes) */
        out_uint32_le(self->out_s, window_state->extended_style);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "Style 0x%8.8x, ExtendedStyle 0x%8.8x",
                  window_state->style, window_state->extended_style);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_SHOW)
    {
        /* showState (1 byte) */
        out_uint8(self->out_s, window_state->show_state);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "ShowState 0x%2.2x", window_state->show_state);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_TITLE)
    {
        /* titleInfo */
        xrdp_orders_send_as_unicode(self->out_s, window_state->title_info);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "TitleInfo %s", window_state->title_info);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
    {
        /* clientOffsetX (4 bytes) */
        out_uint32_le(self->out_s, window_state->client_offset_x);
        /* clientOffsetY (4 bytes) */
        out_uint32_le(self->out_s, window_state->client_offset_y);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "ClientOffsetX  %d, ClientOffsetY %d",
                  window_state->client_offset_x, window_state->client_offset_y);
    }

    /* TODO: The [MS-RDPERP] spec says that:
     * The ClientAreaWidth and ClientAreaHeight field only appears if the WndSupportLevel field of the
     * Window List Capability Set message is set to TS_WINDOW_LEVEL_SUPPORTED_EX
     * (as specified in [MS-RDPERP] section 2.2.1.1.2)
     */
    if (field_present_flags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
    {
        /* clientAreaWidth (4 bytes) */
        out_uint32_le(self->out_s, window_state->client_area_width);
        /* clientAreaHeight (4 bytes) */
        out_uint32_le(self->out_s, window_state->client_area_height);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "ClientAreaWidth %d, ClientAreaHeight %d",
                  window_state->client_area_width, window_state->client_area_height);
    }
    /* TODO: The [MS-RDPERP] spec section 2.2.1.3.1.2.1 has the following additional fields:
     * WindowLeftResizeMargin (optional)
     * WindowRightResizeMargin (optional)
     * WindowTopResizeMargin (optional)
     * WindowBottomResizeMargin (optional)
     */

    /* TODO: The [MS-RDPERP] spec says that:
     * The RPContent field only appears if the WndSupportLevel field of the
     * Window List Capability Set message is set to TS_WINDOW_LEVEL_SUPPORTED_EX
     * (as specified in [MS-RDPERP] section 2.2.1.1.2)
     */
    if (field_present_flags & WINDOW_ORDER_FIELD_RP_CONTENT)
    {
        /* RPContent (1 byte) */
        out_uint8(self->out_s, window_state->rp_content);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "RPContent 0x%2.2x", window_state->rp_content);
    }

    /* TODO: The [MS-RDPERP] spec says that:
     * The RootParentHandle field only appears if the WndSupportLevel field of the
     * Window List Capability Set message is set to TS_WINDOW_LEVEL_SUPPORTED_EX
     * (as specified in [MS-RDPERP] section 2.2.1.1.2)
     */
    if (field_present_flags & WINDOW_ORDER_FIELD_ROOT_PARENT)
    {
        /* rootParentHandle (4 bytes) */
        out_uint32_le(self->out_s, window_state->root_parent_handle);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "RootParentHandle 0x%8.8x", window_state->root_parent_handle);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_OFFSET)
    {
        /* windowOffsetX (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_offset_x);
        /* windowOffsetY (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_offset_y);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "WindowOffsetX %d, WindowOffsetY %d",
                  window_state->window_offset_x, window_state->window_offset_y);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
    {
        /* windowClientDeltaX (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_client_delta_x);
        /* windowClientDeltaY (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_client_delta_y);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "WindowClientDeltaX %d, WindowClientDeltaY %d",
                  window_state->window_client_delta_x, window_state->window_client_delta_y);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_SIZE)
    {
        /* windowWidth (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_width);
        /* windowHeight (4 bytes) */
        out_uint32_le(self->out_s, window_state->window_height);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "WindowWidth %d, WindowHeight %d",
                  window_state->window_width, window_state->window_height);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_WND_RECTS)
    {
        /* numWindowRects (2 bytes) */
        out_uint16_le(self->out_s, window_state->num_window_rects);

        for (index = 0; index < window_state->num_window_rects; index++)
        {
            out_uint16_le(self->out_s, window_state->window_rects[index].left);
            out_uint16_le(self->out_s, window_state->window_rects[index].top);
            out_uint16_le(self->out_s, window_state->window_rects[index].right);
            out_uint16_le(self->out_s, window_state->window_rects[index].bottom);
        }
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "NumWindowRects %d, WindowRects <omitted from log>",
                  window_state->num_window_rects);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_VIS_OFFSET)
    {
        /* visibleOffsetX (4 bytes) */
        out_uint32_le(self->out_s, window_state->visible_offset_x);
        /* visibleOffsetY (4 bytes) */
        out_uint32_le(self->out_s, window_state->visible_offset_y);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "VisibleOffsetX %d, VisibleOffsetY %d",
                  window_state->visible_offset_x, window_state->visible_offset_y);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_VISIBILITY)
    {
        /* numVisibilityRects (2 bytes) */
        out_uint16_le(self->out_s, window_state->num_visibility_rects);

        for (index = 0; index < window_state->num_visibility_rects; index++)
        {
            out_uint16_le(self->out_s, window_state->visibility_rects[index].left);
            out_uint16_le(self->out_s, window_state->visibility_rects[index].top);
            out_uint16_le(self->out_s, window_state->visibility_rects[index].right);
            out_uint16_le(self->out_s, window_state->visibility_rects[index].bottom);
        }
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding optional field [MS-RDPERP] TS_WINDOW_INFO "
                  "NumVisibilityRects %d, VisibilityRects <omitted from log>",
                  window_state->num_visibility_rects);
    }
    /* TODO: The [MS-RDPERP] spec section 2.2.1.3.1.2.1 has the following additional fields:
     * OverlayDescription (optional, variable)
     * TaskbarButton (optional)
     * EnforceServerZOrder (optional)
     * AppBarState (optional)
     * AppBarEdge (optional)
     */

    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
int
xrdp_orders_send_notify_delete(struct xrdp_orders *self, int window_id,
                               int notify_id)
{
    int order_size;
    int order_flags;
    int field_present_flags;

    order_size = 15;
    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_notify_delete: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    field_present_flags = WINDOW_ORDER_TYPE_NOTIFY | WINDOW_ORDER_STATE_DELETED;
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    /* notifyIconId (4 bytes) */
    out_uint32_le(self->out_s, notify_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_NOTIFYICON_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x, WindowId 0x%8.8x, NotifyIconId 0x%8.8x",
              order_size, field_present_flags, window_id, notify_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] Deleted Notification Icons");

    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
/* flags can contain WINDOW_ORDER_STATE_NEW */
int
xrdp_orders_send_notify_new_update(struct xrdp_orders *self,
                                   int window_id, int notify_id,
                                   struct rail_notify_state_order *notify_state,
                                   int flags)
{
    int order_size;
    int order_flags;
    int field_present_flags;
    int use_cmap;

    order_size = 15;
    field_present_flags = flags | WINDOW_ORDER_TYPE_NOTIFY;

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
    {
        /* Version (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
    {
        /* ToolTip (variable) UNICODE_STRING */
        order_size += xrdp_orders_get_unicode_bytes(notify_state->tool_tip);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
    {
        /* InfoTip (variable) TS_NOTIFY_ICON_INFOTIP */
        /* UNICODE_STRING */
        order_size += xrdp_orders_get_unicode_bytes(notify_state->infotip.title);
        /* UNICODE_STRING */
        order_size += xrdp_orders_get_unicode_bytes(notify_state->infotip.text);
        /* Timeout (4 bytes) */
        /* InfoFlags (4 bytes) */
        order_size += 8;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
    {
        /* State (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_ICON)
    {
        /* Icon (variable) */
        use_cmap = 0;

        if ((notify_state->icon_info.bpp == 1) || (notify_state->icon_info.bpp == 2) ||
                (notify_state->icon_info.bpp == 4))
        {
            use_cmap = 1;
        }

        order_size += 12 + notify_state->icon_info.mask_bytes +
                      notify_state->icon_info.data_bytes;

        if (use_cmap)
        {
            order_size += notify_state->icon_info.cmap_bytes + 2;
        }
    }

    if (field_present_flags & WINDOW_ORDER_CACHED_ICON)
    {
        /* CachedIcon (3 bytes) */
        order_size += 3;
    }

    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_notify_new_update: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    out_uint8(self->out_s, order_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    out_uint32_le(self->out_s, field_present_flags);
    /* windowId (4 bytes) */
    out_uint32_le(self->out_s, window_id);
    /* notifyIconId (4 bytes) */
    out_uint32_le(self->out_s, notify_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_NOTIFYICON_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x, WindowId 0x%8.8x, NotifyIconId 0x%8.8x",
              order_size, field_present_flags, window_id, notify_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] New or Existing Notification Icons");

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
    {
        /* Version (4 bytes) */
        out_uint32_le(self->out_s, notify_state->version);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] "
                  "Version %d", notify_state->version);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
    {
        /* ToolTip (variable) UNICODE_STRING */
        xrdp_orders_send_as_unicode(self->out_s, notify_state->tool_tip);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] "
                  "ToolTip %s", notify_state->tool_tip);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
    {
        /* InfoTip (variable) TS_NOTIFY_ICON_INFOTIP */
        out_uint32_le(self->out_s, notify_state->infotip.timeout);
        out_uint32_le(self->out_s, notify_state->infotip.flags);
        xrdp_orders_send_as_unicode(self->out_s, notify_state->infotip.text);
        xrdp_orders_send_as_unicode(self->out_s, notify_state->infotip.title);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] TS_NOTIFY_ICON_INFOTIP "
                  "Timeout %d, InfoFlags 0x%8.8x, InfoTipText %s, Title %s",
                  notify_state->infotip.timeout, notify_state->infotip.flags,
                  notify_state->infotip.text, notify_state->infotip.title);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
    {
        /* State (4 bytes) */
        out_uint32_le(self->out_s, notify_state->state);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] "
                  "State %d", notify_state->state);
    }

    if (field_present_flags & WINDOW_ORDER_ICON)
    {
        /* Icon (variable) */
        xrdp_orders_send_ts_icon(self->out_s, notify_state->icon_cache_entry,
                                 notify_state->icon_cache_id,
                                 &notify_state->icon_info);
    }

    if (field_present_flags & WINDOW_ORDER_CACHED_ICON)
    {
        /* CacheEntry (2 bytes) */
        out_uint16_le(self->out_s, notify_state->icon_cache_entry);
        /* CacheId (1 byte) */
        out_uint8(self->out_s, notify_state->icon_cache_id);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order field [MS-RDPERP] TS_CACHED_ICON_INFO "
                  "CacheEntry %d, CacheId %d",
                  notify_state->icon_cache_entry, notify_state->icon_cache_id);
    }

    return 0;
}

/*****************************************************************************/
/* RAIL */
/* returns error */
/* used for both Non-Monitored Desktop and Actively Monitored Desktop */
int
xrdp_orders_send_monitored_desktop(struct xrdp_orders *self,
                                   struct rail_monitored_desktop_order *mdo,
                                   int flags)
{
    int order_size;
    int order_flags;
    int field_present_flags;
    int index;

    order_size = 7;
    field_present_flags = flags | WINDOW_ORDER_TYPE_DESKTOP;

    if (field_present_flags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
    {
        /* ActiveWindowId (4 bytes) */
        order_size += 4;
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
    {
        /* NumWindowIds (1 byte) */
        order_size += 1;
        /* WindowIds (variable) */
        order_size += mdo->num_window_ids *  4;
    }

    if (xrdp_orders_check(self, order_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_orders_send_monitored_desktop: xrdp_orders_check failed");
        return 1;
    }
    self->order_count++;
    order_flags = TS_SECONDARY;
    order_flags |= 0xb << 2; /* type TS_ALTSEC_WINDOW */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPEGDI] ALTSEC_DRAWING_ORDER_HEADER "
              "controlFlags.class 0x%1.1x (TS_SECONDARY), "
              "controlFlags.orderType 0x%2.2x (TS_ALTSEC_WINDOW)",
              (order_flags & 0x3), (order_flags >> 2));

    out_uint8(self->out_s, order_flags);
    /* orderSize (2 bytes) */
    out_uint16_le(self->out_s, order_size);
    /* FieldsPresentFlags (4 bytes) */
    out_uint32_le(self->out_s, field_present_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPERP] TS_DESKTOP_ORDER_HEADER "
              "OrderSize %d, FieldsPresentFlags 0x%8.8x",
              order_size, field_present_flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order [MS-RDPERP] %s",
              ((field_present_flags & WINDOW_ORDER_FIELD_DESKTOP_NONE) ?
               "Non-Monitored Desktop" : "Actively Monitored Desktop"));

    if (field_present_flags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
    {
        /* ActiveWindowId (4 bytes) */
        out_uint32_le(self->out_s, mdo->active_window_id);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] "
                  "ActiveWindowId 0x%8.8x", mdo->active_window_id);
    }

    if (field_present_flags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
    {
        /* NumWindowIds (1 byte) */
        out_uint8(self->out_s, mdo->num_window_ids);

        /* WindowIds (variable) */
        for (index = 0; index < mdo->num_window_ids; index++)
        {
            out_uint32_le(self->out_s, mdo->window_ids[index]);
        }

        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding order optional field [MS-RDPERP] "
                  "NumWindowIds %d, WindowIds <omitted from log>",
                  mdo->num_window_ids);
    }

    return 0;
}
