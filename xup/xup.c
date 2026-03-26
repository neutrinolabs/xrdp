/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * libxup main file
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xup.h"
#include "xup_client_info.h"
#include "log.h"
#include "trans.h"
#include "string_calls.h"
#include "scancode.h"
#include "../xrdp/xrdp_encoder.h"

static int
send_server_monitor_update(struct mod *v, struct stream *s,
                           int width, int height,
                           int num_monitors,
                           const struct monitor_info *monitors);

static int
send_server_monitor_full_invalidate(
    struct mod *mod, struct stream *s, int width, int height);

static int
send_server_version_message(struct mod *v, struct stream *s);

static int
lib_mod_process_message(struct mod *mod, struct stream *s);

static void
xup_dmabuf_surface_delete(struct xrdp_encoder_dmabuf_surface **surface);

/******************************************************************************/
static int
lib_send_copy(struct mod *mod, struct stream *s)
{
    return trans_write_copy_s(mod->trans, s);
}

/******************************************************************************/
static void
xup_dmabuf_surface_delete(struct xrdp_encoder_dmabuf_surface **surface)
{
    if (surface == NULL || *surface == NULL)
    {
        return;
    }
    if ((*surface)->fd >= 0)
    {
        g_file_close((*surface)->fd);
    }
    g_free(*surface);
    *surface = NULL;
}

/******************************************************************************/
/* return error */
static int
lib_mod_start(struct mod *mod, int w, int h, int bpp)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_start");
    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_start");
    return 0;
}

/******************************************************************************/
static int
lib_mod_log_peer(struct mod *mod)
{
    int my_pid;
    int pid;
    int uid;
    int gid;

    my_pid = g_getpid();
    if (g_sck_get_peer_cred(mod->trans->sck, &pid, &uid, &gid) == 0)
    {
        LOG(LOG_LEVEL_INFO, "lib_mod_log_peer: xrdp_pid=%d connected "
            "to Xorg_pid=%d Xorg_uid=%d Xorg_gid=%d "
            "client=%s",
            my_pid, pid, uid, gid,
            mod->client_info.client_description);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "lib_mod_log_peer: g_sck_get_peer_cred "
            "failed");
    }
    return 0;
}

/******************************************************************************/
static int
lib_data_in(struct trans *trans)
{
    struct mod *self;
    struct stream *s;
    int len;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lib_data_in:");
    if (trans == 0)
    {
        return 1;
    }

    self = (struct mod *)(trans->callback_data);
    s = trans_get_in_s(trans);

    if (s == 0)
    {
        return 1;
    }

    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint8s(s, 4); /* processed later in lib_mod_process_message */
            in_uint32_le(s, len);
            if (len < 0 || len > 128 * 1024)
            {
                LOG(LOG_LEVEL_ERROR, "lib_data_in: bad size");
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len + 8;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (lib_mod_process_message(self, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "lib_data_in: lib_mod_process_message failed");
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 1;
            break;
    }

    return 0;
}

/******************************************************************************/
/*
 * Wait for module caps message from Xorg module
 *
 * This routine waits for the Xorg module to send a caps message.
 *
 * We use this to check the caps are compatible with us before we
 * go for a fuull-on connect
 */
static int
wait_for_module_caps_message(struct mod *mod)
{
    int robjs_count;
    intptr_t robjs[10];

    mod->caps_processing_status = E_CAPS_NOT_PROCESSED;

    while (mod->caps_processing_status == E_CAPS_NOT_PROCESSED)
    {
        robjs_count = 0;
        if (trans_get_wait_objs(mod->trans, robjs, &robjs_count) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Xorg module has dropped connection");
            return 1;
        }

        // We don't need a big timeout here, as all the module has to do is
        // turn around the version message.
        int status = g_obj_wait(robjs, robjs_count, 0, 0, 3 * 1000);

        if (status < 0)
        {
            LOG(LOG_LEVEL_ERROR, "No response from Xorg module before timeout");
            return 1;
        }

        (void)trans_check_wait_objs(mod->trans);
    }

    return (mod->caps_processing_status == E_CAPS_OK) ? 0 : 1;
}

/******************************************************************************/
/* Convert the internal xrdp_client_info structure to an
 * external xup_client_info structure */
static void
convert_xrdp_client_info_to_xup_client_info(
    const struct xrdp_client_info *src,
    struct xup_client_info *dst)
{
    dst->size = sizeof(*dst);
    dst->version = XUP_CLIENT_INFO_CURRENT_VERSION;
    dst->bpp = src->bpp;
    dst->jpeg = src->jpeg;
    dst->offscreen_support_level = src->offscreen_support_level;
    dst->offscreen_cache_size = src->offscreen_cache_size;
    dst->offscreen_cache_entries = src->offscreen_cache_entries;

    memcpy(dst->orders, src->orders, XR_PRIMARY_ORDER_COUNT);
    dst->order_flags_ex = src->order_flags_ex;
    dst->pointer_flags = src->pointer_flags;
    dst->large_pointer_support_flags = src->large_pointer_support_flags;

    dst->display_sizes = src->display_sizes;

    dst->capture_code = src->capture_code;
    dst->capture_format = src->capture_format;
    dst->capture_transport_flags = src->capture_transport_flags;

    memcpy(dst->model, src->model, CI_KBD_MODEL_SIZE);
    memcpy(dst->layout, src->layout, CI_KBD_LAYOUT_SIZE);
    memcpy(dst->variant, src->variant, CI_KBD_VARIANT_SIZE);
    memcpy(dst->options, src->options, CI_KBD_OPTIONS_SIZE);
    memcpy(dst->xkb_rules, src->xkb_rules, CI_KBD_XKB_RULES_SIZE);

    dst->x11_keycode_caps_lock = src->x11_keycode_caps_lock;
    dst->x11_keycode_num_lock = src->x11_keycode_num_lock;
    dst->x11_keycode_scroll_lock = src->x11_keycode_scroll_lock;

    dst->rfx_frame_interval = src->rfx_frame_interval;
    dst->h264_frame_interval = src->h264_frame_interval;
    dst->normal_frame_interval = src->normal_frame_interval;
}

/******************************************************************************/
/* return error */
static int
lib_send_client_info(struct mod *mod)
{
    struct stream *s;
    int len;
    struct xup_client_info xup_client_info;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lib_send_client_info:");

    convert_xrdp_client_info_to_xup_client_info(&mod->client_info,
            &xup_client_info);
    make_stream(s);
    init_stream(s, (int)sizeof(xup_client_info) + 64);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_INFO);
    g_memcpy(s->p, &xup_client_info, sizeof(xup_client_info));
    s->p += sizeof(xup_client_info);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_connect(struct mod *mod, int fd)
{
    int error;
    int socket_mode;
    struct stream *s;
    char con_port[256];

    mod->server_msg(mod, "started connecting", 0);

    /* only support 8, 15, 16, 24, and 32 bpp connections from rdp client */
    if (mod->bpp != 8
            && mod->bpp != 15
            && mod->bpp != 16
            && mod->bpp != 24
            && mod->bpp != 32)
    {
        mod->server_msg(mod,
                        "error - only supporting 8, 15, 16, 24, and 32"
                        " bpp rdp connections", 0);
        return 1;
    }

    // This is a good place to finalise any parameters that need to
    // be set.
    //
    // Load the XKB layout
    if (mod->keycode_set[0] != '\0')
    {
        if (scancode_set_keycode_set(mod->keycode_set) == 0)
        {
            LOG(LOG_LEVEL_INFO, "Loaded '%s' keycode set", mod->keycode_set);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "Unable to load '%s' keycode set",
                mod->keycode_set);
        }
    }
    mod->server_init_xkb_layout(mod, &(mod->client_info));
    LOG(LOG_LEVEL_INFO, "XKB rules '%s' will be used by the module",
        mod->client_info.xkb_rules);

    if (mod->client_info.h264_frame_interval <= 0)
    {
        mod->client_info.h264_frame_interval = DEFAULT_H264_FRAME_INTERVAL;
    }
    if (mod->client_info.rfx_frame_interval <= 0)
    {
        mod->client_info.rfx_frame_interval = DEFAULT_RFX_FRAME_INTERVAL;
    }
    if (mod->client_info.normal_frame_interval <= 0)
    {
        mod->client_info.normal_frame_interval = DEFAULT_NORMAL_FRAME_INTERVAL;
    }

    make_stream(s);
    g_sprintf(con_port, "%s", mod->port);

    error = 0;
    mod->sck_closed = 0;

    if (con_port[0] == '/')
    {
        socket_mode = TRANS_MODE_UNIX;
        LOG(LOG_LEVEL_INFO, "lib_mod_connect: connecting via UNIX socket");
    }
    else
    {
        socket_mode = TRANS_MODE_TCP;
        LOG(LOG_LEVEL_INFO, "lib_mod_connect: connecting via TCP socket");
        if (g_strcmp(mod->ip, "") == 0)
        {
            mod->server_msg(mod, "error - no ip set", 0);
            free_stream(s);
            return 1;
        }
    }

    mod->trans = trans_create(socket_mode, 8 * 8192, 8192);
    if (mod->trans == 0)
    {
        free_stream(s);
        return 1;
    }

    // Set the transport up
    mod->trans->si = mod->si;
    mod->trans->my_source = XRDP_SOURCE_MOD;
    mod->trans->is_term = mod->server_is_term;
    mod->trans->trans_data_in = lib_data_in;
    mod->trans->header_size = 8;
    mod->trans->callback_data = mod;
    mod->trans->no_stream_init_on_data_in = 1;
    mod->trans->extra_flags = 1;

    if (fd >= 0)
    {
        mod->trans->sck = fd;
        mod->trans->status = TRANS_STATUS_UP; /* ok */
        mod->trans->type1 = TRANS_TYPE_CLIENT; /* client */
        error = 0;
    }
    else
    {
        /* Give the X server a bit of time to start */
        error = trans_connect(mod->trans, mod->ip, con_port, 30 * 1000);
    }

    if (error == 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "lib_mod_connect: connected to Xserver "
                  "(Xorg) sck %lld",
                  (long long) (mod->trans->sck));
        if (socket_mode == TRANS_MODE_UNIX)
        {
            lib_mod_log_peer(mod);
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "Error connecting to X server [%s]",
            g_get_strerror());
    }

    if (error == 0)
    {
        error = send_server_version_message(mod, s);
    }

    if (error == 0)
    {
        error = wait_for_module_caps_message(mod);
    }

    if (error == 0)
    {
        error = lib_send_client_info(mod);
    }

    if (error == 0)
    {
        error = send_server_monitor_full_invalidate(
                    mod, s, mod->width, mod->height);
    }

    free_stream(s);

    if (error != 0)
    {
        trans_delete(mod->trans);
        mod->trans = 0;
        mod->server_msg(mod, "Error connecting to Xorg - check log", 0);
    }
    else
    {
        mod->server_msg(mod, "connected ok", 0);
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_connect");
    return error;
}

/******************************************************************************/
/* return error */
static int
lib_mod_event(struct mod *mod, int msg, tbus param1, tbus param2,
              tbus param3, tbus param4)
{
    struct stream *s;
    int len;
    int key;
    int rv;
    int scancode;

    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_event");
    make_stream(s);

    if ((msg >= 15) && (msg <= 16)) /* key events */
    {
        key = param2;

        if (key > 0)
        {
            if (key == 65027) /* altgr */
            {
                if (mod->shift_state)
                {
                    LOG_DEVEL(LOG_LEVEL_TRACE, "special");
                    /* fix for mstsc sending left control down with altgr */
                    /* control down / up
                    msg param1 param2 param3 param4
                    15  0      65507  29     0
                    16  0      65507  29     49152 */
                    init_stream(s, 8192);
                    s_push_layer(s, iso_hdr, 4);
                    out_uint16_le(s, XUP_MSG_CLIENT_DATA);
                    out_uint32_le(s, 16); /* key up */
                    out_uint32_le(s, 0);
                    out_uint32_le(s, 65507); /* left control */
                    out_uint32_le(s, 29); /* RDP scan code */
                    out_uint32_le(s, 0xc000); /* flags */
                    s_mark_end(s);
                    len = (int)(s->end - s->data);
                    s_pop_layer(s, iso_hdr);
                    out_uint32_le(s, len);
                    lib_send_copy(mod, s);
                }
            }

            if (key == 65507) /* left control */
            {
                mod->shift_state = msg == 15;
            }
        }

        /* xup doesn't need the Unicode character mapping in param1. Send
         * the X11 scancode instead, so xorgxrdp doesn't have to do this
         * work again */
        scancode = SCANCODE_FROM_KBD_EVENT(param3, param4);
        param1 = scancode_to_x11_keycode(scancode);
    }

    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_DATA);
    out_uint32_le(s, msg);
    out_uint32_le(s, param1);
    out_uint32_le(s, param2);
    out_uint32_le(s, param3);
    out_uint32_le(s, param4);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    rv = lib_send_copy(mod, s);
    free_stream(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_event");
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_fill_rect(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    rv = mod->server_fill_rect(mod, x, y, cx, cy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_screen_blt(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    rv = mod->server_screen_blt(mod, x, y, cx, cy, srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;
    int width;
    int height;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    rv = mod->server_paint_rect(mod, x, y, cx, cy,
                                bmpdata, width, height,
                                srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_clip(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    rv = mod->server_set_clip(mod, x, y, cx, cy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_reset_clip(struct mod *mod, struct stream *s)
{
    int rv;

    rv = mod->server_reset_clip(mod);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_fgcolor(struct mod *mod, struct stream *s)
{
    int rv;
    int fgcolor;

    in_uint32_le(s, fgcolor);
    rv = mod->server_set_fgcolor(mod, fgcolor);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_bgcolor(struct mod *mod, struct stream *s)
{
    int rv;
    int bgcolor;

    in_uint32_le(s, bgcolor);
    rv = mod->server_set_bgcolor(mod, bgcolor);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_opcode(struct mod *mod, struct stream *s)
{
    int rv;
    int opcode;

    in_uint16_le(s, opcode);
    rv = mod->server_set_opcode(mod, opcode);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pen(struct mod *mod, struct stream *s)
{
    int rv;
    int style;
    int width;

    in_uint16_le(s, style);
    in_uint16_le(s, width);
    rv = mod->server_set_pen(mod, style, width);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_draw_line(struct mod *mod, struct stream *s)
{
    int rv;
    int x1;
    int y1;
    int x2;
    int y2;

    in_sint16_le(s, x1);
    in_sint16_le(s, y1);
    in_sint16_le(s, x2);
    in_sint16_le(s, y2);
    rv = mod->server_draw_line(mod, x1, y1, x2, y2);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_cursor(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    char cur_data[32 * (32 * 3)];
    char cur_mask[32 * (32 / 8)];

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint8a(s, cur_data, 32 * (32 * 3));
    in_uint8a(s, cur_mask, 32 * (32 / 8));
    rv = mod->server_set_cursor(mod, x, y, cur_data, cur_mask);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_create_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;
    int width;
    int height;

    in_uint32_le(s, rdpid);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    rv = mod->server_create_os_surface(mod, rdpid, width, height);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_switch_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;

    in_uint32_le(s, rdpid);
    rv = mod->server_switch_os_surface(mod, rdpid);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_delete_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;

    in_uint32_le(s, rdpid);
    rv = mod->server_delete_os_surface(mod, rdpid);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_os(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int rdpid;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, rdpid);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    rv = mod->server_paint_rect_os(mod, x, y, cx, cy,
                                   rdpid, srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_hints(struct mod *mod, struct stream *s)
{
    int rv;
    int hints;
    int mask;

    in_uint32_le(s, hints);
    in_uint32_le(s, mask);
    rv = mod->server_set_hints(mod, hints, mask);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_new_update(struct mod *mod, struct stream *s)
{
    int flags;
    int window_id;
    int title_bytes;
    int index;
    int bytes;
    int rv;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, rwso.owner_window_id);
    in_uint32_le(s, rwso.style);
    in_uint32_le(s, rwso.extended_style);
    in_uint32_le(s, rwso.show_state);
    in_uint16_le(s, title_bytes);

    if (title_bytes > 0)
    {
        rwso.title_info = g_new(char, title_bytes + 1);
        in_uint8a(s, rwso.title_info, title_bytes);
        rwso.title_info[title_bytes] = 0;
    }

    in_uint32_le(s, rwso.client_offset_x);
    in_uint32_le(s, rwso.client_offset_y);
    in_uint32_le(s, rwso.client_area_width);
    in_uint32_le(s, rwso.client_area_height);
    in_uint32_le(s, rwso.rp_content);
    in_uint32_le(s, rwso.root_parent_handle);
    in_uint32_le(s, rwso.window_offset_x);
    in_uint32_le(s, rwso.window_offset_y);
    in_uint32_le(s, rwso.window_client_delta_x);
    in_uint32_le(s, rwso.window_client_delta_y);
    in_uint32_le(s, rwso.window_width);
    in_uint32_le(s, rwso.window_height);
    in_uint16_le(s, rwso.num_window_rects);

    if (rwso.num_window_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_window_rects;
        rwso.window_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_window_rects; index++)
        {
            in_uint16_le(s, rwso.window_rects[index].left);
            in_uint16_le(s, rwso.window_rects[index].top);
            in_uint16_le(s, rwso.window_rects[index].right);
            in_uint16_le(s, rwso.window_rects[index].bottom);
        }
    }

    in_uint32_le(s, rwso.visible_offset_x);
    in_uint32_le(s, rwso.visible_offset_y);
    in_uint16_le(s, rwso.num_visibility_rects);

    if (rwso.num_visibility_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_visibility_rects;
        rwso.visibility_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_visibility_rects; index++)
        {
            in_uint16_le(s, rwso.visibility_rects[index].left);
            in_uint16_le(s, rwso.visibility_rects[index].top);
            in_uint16_le(s, rwso.visibility_rects[index].right);
            in_uint16_le(s, rwso.visibility_rects[index].bottom);
        }
    }

    in_uint32_le(s, flags);
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    g_free(rwso.title_info);
    g_free(rwso.window_rects);
    g_free(rwso.visibility_rects);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_delete(struct mod *mod, struct stream *s)
{
    int window_id;
    int rv;

    in_uint32_le(s, window_id);
    mod->server_window_delete(mod, window_id);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_show(struct mod *mod, struct stream *s)
{
    int window_id;
    int rv;
    int flags;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, flags);
    in_uint32_le(s, rwso.show_state);
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_add_char(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int character;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, character);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    rv = mod->server_add_char(mod, font, character, x, y, cx, cy, bmpdata);
    return rv;
}


/******************************************************************************/
/* return error */
static int
process_server_add_char_alpha(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int character;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, character);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    rv = mod->server_add_char_alpha(mod, font, character, x, y, cx, cy,
                                    bmpdata);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_draw_text(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int flags;
    int mixmode;
    int clip_left;
    int clip_top;
    int clip_right;
    int clip_bottom;
    int box_left;
    int box_top;
    int box_right;
    int box_bottom;
    int x;
    int y;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, flags);
    in_uint16_le(s, mixmode);
    in_sint16_le(s, clip_left);
    in_sint16_le(s, clip_top);
    in_sint16_le(s, clip_right);
    in_sint16_le(s, clip_bottom);
    in_sint16_le(s, box_left);
    in_sint16_le(s, box_top);
    in_sint16_le(s, box_right);
    in_sint16_le(s, box_bottom);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    rv = mod->server_draw_text(mod, font, flags, mixmode, clip_left, clip_top,
                               clip_right, clip_bottom, box_left, box_top,
                               box_right, box_bottom, x, y, bmpdata, len_bmpdata);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_create_os_surface_bpp(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;
    int width;
    int height;
    int bpp;

    in_uint32_le(s, rdpid);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint8(s, bpp);
    rv = mod->server_create_os_surface_bpp(mod, rdpid, width, height, bpp);
    return rv;
}


/******************************************************************************/
/* return error */
static int
process_server_paint_rect_bpp(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;
    int width;
    int height;
    int srcx;
    int srcy;
    int bpp;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    in_uint8(s, bpp);
    rv = mod->server_paint_rect_bpp(mod, x, y, cx, cy,
                                    bmpdata, width, height,
                                    srcx, srcy, bpp);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_composite(struct mod *mod, struct stream *s)
{
    int rv;
    int srcidx;
    int srcformat;
    int srcwidth;
    int srcrepeat;
    int transform[10];
    int mskflags;
    int mskidx;
    int mskformat;
    int mskwidth;
    int mskrepeat;
    int op;
    int srcx;
    int srcy;
    int mskx;
    int msky;
    int dstx;
    int dsty;
    int width;
    int height;
    int dstformat;

    in_uint16_le(s, srcidx);
    in_uint32_le(s, srcformat);
    in_uint16_le(s, srcwidth);
    in_uint8(s, srcrepeat);
    g_memcpy(transform, s->p, 40);
    in_uint8s(s, 40);
    in_uint8(s, mskflags);
    in_uint16_le(s, mskidx);
    in_uint32_le(s, mskformat);
    in_uint16_le(s, mskwidth);
    in_uint8(s, mskrepeat);
    in_uint8(s, op);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    in_sint16_le(s, mskx);
    in_sint16_le(s, msky);
    in_sint16_le(s, dstx);
    in_sint16_le(s, dsty);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint32_le(s, dstformat);
    rv = mod->server_composite(mod, srcidx, srcformat, srcwidth, srcrepeat,
                               transform, mskflags, mskidx, mskformat,
                               mskwidth, mskrepeat, op, srcx, srcy, mskx, msky,
                               dstx, dsty, width, height, dstformat);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pointer_ex(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int bpp;
    int Bpp;
    char cur_data[32 * (32 * 4)];
    char cur_mask[32 * (32 / 8)];

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, bpp);
    Bpp = (bpp == 0) ? 3 : (bpp + 7) / 8;
    in_uint8a(s, cur_data, 32 * (32 * Bpp));
    in_uint8a(s, cur_mask, 32 * (32 / 8));
    rv = mod->server_set_cursor_ex(mod, x, y, cur_data, cur_mask, bpp);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_paint_rect_ack(struct mod *mod, int flags, int x, int y, int cx, int cy,
                    int frame_id)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_REGION);
    out_uint32_le(s, flags);
    out_uint32_le(s, frame_id);
    out_uint32_le(s, x);
    out_uint32_le(s, y);
    out_uint32_le(s, cx);
    out_uint32_le(s, cy);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_shmem(struct mod *amod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int flags;
    int frame_id;
    int shmem_id;
    int shmem_offset;
    int width;
    int height;
    int srcx;
    int srcy;
    char *bmpdata;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_id);
    in_uint32_le(s, shmem_offset);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);

    bmpdata = 0;
    rv = 0;
    if (amod->screen_shmem_id_mapped == 0)
    {
        amod->screen_shmem_id = shmem_id;
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
        else
        {
            amod->screen_shmem_id_mapped = 1;
        }
    }
    else if (amod->screen_shmem_id != shmem_id)
    {
        amod->screen_shmem_id = shmem_id;
        g_shmdt(amod->screen_shmem_pixels);
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
    }
    if (amod->screen_shmem_pixels != 0)
    {
        bmpdata = amod->screen_shmem_pixels + shmem_offset;
    }
    if (bmpdata != 0)
    {
        rv = amod->server_paint_rect(amod, x, y, cx, cy,
                                     bmpdata, width, height,
                                     srcx, srcy);
    }
    send_paint_rect_ack(amod, flags, x, y, cx, cy, frame_id);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_paint_rect_ex_ack(struct mod *mod, int flags, int frame_id)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_REGION_EX);
    out_uint32_le(s, flags);
    out_uint32_le(s, frame_id);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
send_suppress_output(struct mod *mod, int suppress,
                     int left, int top, int right, int bottom)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_SUPPRESS_OUTPUT);
    out_uint32_le(s, suppress);
    out_uint32_le(s, left);
    out_uint32_le(s, top);
    out_uint32_le(s, right);
    out_uint32_le(s, bottom);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_shmem_ex(struct mod *amod, struct stream *s)
{
    LOG(LOG_LEVEL_TRACE, "process_server_paint_rect_shmem_ex:");

    int num_drects;
    int num_crects;
    int flags;
    int frame_id;
    int shmem_id;
    int shmem_offset;
    int width;
    int height;
    int index;
    int rv;
    tsi16 *ldrects;
    tsi16 *ldrects1;
    tsi16 *lcrects;
    tsi16 *lcrects1;
    char *bmpdata;

    /* dirty pixels */
    in_uint16_le(s, num_drects);
    ldrects = (tsi16 *) g_malloc(2 * 4 * num_drects, 0);
    ldrects1 = ldrects;
    for (index = 0; index < num_drects; index++)
    {
        in_sint16_le(s, ldrects1[0]);
        in_sint16_le(s, ldrects1[1]);
        in_sint16_le(s, ldrects1[2]);
        in_sint16_le(s, ldrects1[3]);
        ldrects1 += 4;
    }

    /* copied pixels */
    in_uint16_le(s, num_crects);
    lcrects = (tsi16 *) g_malloc(2 * 4 * num_crects, 0);
    lcrects1 = lcrects;
    for (index = 0; index < num_crects; index++)
    {
        in_sint16_le(s, lcrects1[0]);
        in_sint16_le(s, lcrects1[1]);
        in_sint16_le(s, lcrects1[2]);
        in_sint16_le(s, lcrects1[3]);
        lcrects1 += 4;
    }

    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_id);
    in_uint32_le(s, shmem_offset);

    in_uint16_le(s, width);
    in_uint16_le(s, height);

    bmpdata = 0;
    if (amod->screen_shmem_id_mapped == 0)
    {
        amod->screen_shmem_id = shmem_id;
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
        else
        {
            amod->screen_shmem_id_mapped = 1;
        }
    }
    else if (amod->screen_shmem_id != shmem_id)
    {
        amod->screen_shmem_id = shmem_id;
        g_shmdt(amod->screen_shmem_pixels);
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "process_server_paint_rect_shmem_ex:"
                  " flags=%d frame_id=%d, shmem_id=%d, shmem_offset=%d,"
                  " width=%d, height=%d",
                  flags, frame_id, shmem_id, shmem_offset,
                  width, height);
    }
    if (amod->screen_shmem_pixels != 0)
    {
        bmpdata = amod->screen_shmem_pixels + shmem_offset;
    }
    if (bmpdata != 0)
    {
        rv = amod->server_paint_rects(amod, num_drects, ldrects,
                                      num_crects, lcrects,
                                      bmpdata, width, height,
                                      flags, frame_id);
    }
    else
    {
        rv = 1;
    }

    g_free(lcrects);
    g_free(ldrects);

    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_egfx_shmfd(struct mod *amod, struct stream *s)
{
    char *data;
    char *cmd;
    int rv;
    int cmd_bytes;
    int shmem_bytes;
    int fd;
    int recv_bytes;
    unsigned int num_fds;
    void *shmem_ptr;
    char msg[4];

    rv = 0;
    in_uint32_le(s, cmd_bytes);
    in_uint8p(s, cmd, cmd_bytes);
    in_uint32_le(s, shmem_bytes);
    if (shmem_bytes == 0)
    {
        return amod->server_egfx_cmd(amod, cmd, cmd_bytes, NULL, 0, NULL);
    }
    fd = -1;
    num_fds = -1;
    if (g_tcp_can_recv(amod->trans->sck, 5000) == 0)
    {
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(amod->trans->sck, msg, 4, &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_server_egfx_shmfd: "
              "g_sck_recv_fd_set rv %d fd %d", recv_bytes, fd);
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            if (g_file_map(fd, 1, 0, shmem_bytes, &shmem_ptr) == 0)
            {
                /* we give up ownership of shmem_ptr
                   will get cleaned up in server_egfx_cmd or
                   xrdp_mm_process_enc_done(gfx) */
                data = (char *) shmem_ptr;
                rv = amod->server_egfx_cmd(amod, cmd, cmd_bytes,
                                           data, shmem_bytes, NULL);
            }
            g_file_close(fd);
        }
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_egfx_dmabuf(struct mod *amod, struct stream *s)
{
    char *cmd;
    int cmd_bytes;
    int width;
    int height;
    int stride;
    int size;
    unsigned int fourcc;
    int fd;
    int recv_bytes;
    unsigned int num_fds;
    char msg[4];
    int rv;
    struct xrdp_encoder_dmabuf_surface *surface;

    in_uint32_le(s, cmd_bytes);
    in_uint8p(s, cmd, cmd_bytes);
    in_uint32_le(s, width);
    in_uint32_le(s, height);
    in_uint32_le(s, stride);
    in_uint32_le(s, fourcc);
    in_uint32_le(s, size);

    fd = -1;
    num_fds = 0;
    if (g_tcp_can_recv(amod->trans->sck, 5000) == 0)
    {
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(amod->trans->sck, msg, 4, &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_server_egfx_dmabuf: "
              "g_sck_recv_fd_set rv %d fd %d", recv_bytes, fd);
    if (recv_bytes != 4 || num_fds != 1)
    {
        if (fd >= 0)
        {
            g_file_close(fd);
        }
        return 1;
    }

    surface = g_new0(struct xrdp_encoder_dmabuf_surface, 1);
    if (surface == NULL)
    {
        g_file_close(fd);
        return 1;
    }
    surface->surface_id = -1;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
    surface->fourcc = fourcc;
    surface->size = size;
    surface->fd = fd;

    rv = amod->server_egfx_cmd(amod, cmd, cmd_bytes, NULL, 0, surface);
    if (rv != 0)
    {
        xup_dmabuf_surface_delete(&surface);
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pointer_shmfd(struct mod *amod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int bpp;
    int Bpp;
    int width;
    int height;
    int fd;
    int recv_bytes;
    int shmembytes;
    unsigned int num_fds;
    void *shmemptr;
    char *cur_data;
    char *cur_mask;
    char msg[4];

    rv = 0;
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, bpp);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    fd = -1;
    num_fds = -1;
    if (g_tcp_can_recv(amod->trans->sck, 5000) == 0)
    {
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(amod->trans->sck, msg, 4, &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_server_set_pointer_shmfd: "
              "g_sck_recv_fd_set rv %d fd %d", recv_bytes, fd);
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            Bpp = (bpp == 0) ? 3 : (bpp + 7) / 8;
            shmembytes = width * height * Bpp + width * height / 8;
            if (g_file_map(fd, 1, 0, shmembytes, &shmemptr) == 0)
            {
                cur_data = (char *)shmemptr;
                cur_mask = cur_data + width * height * Bpp;
                rv = amod->server_set_pointer_large(amod, x, y,
                                                    cur_data, cur_mask,
                                                    bpp, width, height);
                g_munmap(shmemptr, shmembytes);
            }
            g_file_close(fd);
        }
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_shmfd(struct mod *amod, struct stream *s)
{
    int num_drects;
    int num_crects;
    int flags;
    int frame_id;
    int shmem_bytes;
    int shmem_offset;
    int left;
    int top;
    int width;
    int height;
    int index;
    int rv;
    int16_t *ldrects;
    int16_t *ldrects1;
    int16_t *lcrects;
    int16_t *lcrects1;
    char *bmpdata;
    int fd;
    int recv_bytes;
    unsigned int num_fds;
    void *shmem_ptr;
    char msg[4];

    /* dirty pixels */
    in_uint16_le(s, num_drects);
    ldrects = g_new(int16_t, 2 * 4 * num_drects);
    ldrects1 = ldrects;
    for (index = 0; index < num_drects; index++)
    {
        in_sint16_le(s, ldrects1[0]);
        in_sint16_le(s, ldrects1[1]);
        in_sint16_le(s, ldrects1[2]);
        in_sint16_le(s, ldrects1[3]);
        ldrects1 += 4;
    }

    /* copied pixels */
    in_uint16_le(s, num_crects);
    lcrects = g_new(int16_t, 2 * 4 * num_crects);
    lcrects1 = lcrects;
    for (index = 0; index < num_crects; index++)
    {
        in_sint16_le(s, lcrects1[0]);
        in_sint16_le(s, lcrects1[1]);
        in_sint16_le(s, lcrects1[2]);
        in_sint16_le(s, lcrects1[3]);
        lcrects1 += 4;
    }

    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_bytes);
    in_uint32_le(s, shmem_offset);

    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, width);
    in_uint16_le(s, height);

    if (shmem_bytes == 0)
    {
        g_free(ldrects);
        g_free(lcrects);
        return 1;
    }

    if (g_tcp_can_recv(amod->trans->sck, 5000) == 0)
    {
        g_free(ldrects);
        g_free(lcrects);
        return 1;
    }
    rv = 1;
    recv_bytes = g_sck_recv_fd_set(amod->trans->sck, msg, 4, &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_server_paint_rect_shmfd: "
              "g_sck_recv_fd_set rv %d fd %d", recv_bytes, fd);
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            if (g_file_map(fd, 1, 0, shmem_bytes, &shmem_ptr) == 0)
            {
                bmpdata = (char *)shmem_ptr;
                bmpdata += shmem_offset;
                /* we give up ownership of shmem_ptr
                   will get cleaned up in server_paint_rects_ex or
                   xrdp_mm_process_enc_done(rfx, gfx) */
                rv = amod->server_paint_rects_ex(amod, num_drects, ldrects,
                                                 num_crects, lcrects, bmpdata,
                                                 left, top, width, height,
                                                 flags, frame_id,
                                                 shmem_ptr, shmem_bytes, NULL);
            }
            g_file_close(fd);
        }
    }
    g_free(ldrects);
    g_free(lcrects);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_dmabuf(struct mod *amod, struct stream *s)
{
    int num_drects;
    int num_crects;
    int flags;
    int frame_id;
    int left;
    int top;
    int width;
    int height;
    int surface_width;
    int surface_height;
    int stride;
    int size;
    int index;
    int rv;
    int fd;
    int recv_bytes;
    int16_t *ldrects;
    int16_t *ldrects1;
    int16_t *lcrects;
    int16_t *lcrects1;
    unsigned int fourcc;
    unsigned int num_fds;
    char msg[4];
    struct xrdp_encoder_dmabuf_surface *surface;

    in_uint16_le(s, num_drects);
    ldrects = g_new(int16_t, 2 * 4 * num_drects);
    ldrects1 = ldrects;
    for (index = 0; index < num_drects; index++)
    {
        in_sint16_le(s, ldrects1[0]);
        in_sint16_le(s, ldrects1[1]);
        in_sint16_le(s, ldrects1[2]);
        in_sint16_le(s, ldrects1[3]);
        ldrects1 += 4;
    }

    in_uint16_le(s, num_crects);
    lcrects = g_new(int16_t, 2 * 4 * num_crects);
    lcrects1 = lcrects;
    for (index = 0; index < num_crects; index++)
    {
        in_sint16_le(s, lcrects1[0]);
        in_sint16_le(s, lcrects1[1]);
        in_sint16_le(s, lcrects1[2]);
        in_sint16_le(s, lcrects1[3]);
        lcrects1 += 4;
    }

    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint32_le(s, surface_width);
    in_uint32_le(s, surface_height);
    in_uint32_le(s, stride);
    in_uint32_le(s, fourcc);
    in_uint32_le(s, size);

    fd = -1;
    num_fds = 0;
    if (g_tcp_can_recv(amod->trans->sck, 5000) == 0)
    {
        g_free(ldrects);
        g_free(lcrects);
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(amod->trans->sck, msg, 4, &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_server_paint_rect_dmabuf: "
              "g_sck_recv_fd_set rv %d fd %d", recv_bytes, fd);
    if (recv_bytes != 4 || num_fds != 1)
    {
        if (fd >= 0)
        {
            g_file_close(fd);
        }
        g_free(ldrects);
        g_free(lcrects);
        return 1;
    }

    surface = g_new0(struct xrdp_encoder_dmabuf_surface, 1);
    if (surface == NULL)
    {
        g_file_close(fd);
        g_free(ldrects);
        g_free(lcrects);
        return 1;
    }
    surface->surface_id = (flags >> 28) & 0xF;
    surface->width = surface_width;
    surface->height = surface_height;
    surface->stride = stride;
    surface->fourcc = fourcc;
    surface->size = size;
    surface->fd = fd;

    rv = amod->server_paint_rects_ex(amod, num_drects, ldrects,
                                     num_crects, lcrects, NULL,
                                     left, top, width, height,
                                     flags, frame_id,
                                     NULL, 0, surface);
    if (rv != 0)
    {
        xup_dmabuf_surface_delete(&surface);
    }
    g_free(ldrects);
    g_free(lcrects);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pointer_system(struct mod *amod, struct stream *s)
{
    int rv;
    int pointer_type;

    in_uint32_le(s, pointer_type);
    rv = amod->server_set_pointer_system(amod, pointer_type);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pointer_position(struct mod *amod, struct stream *s)
{
    int rv;
    int x;
    int y;

    in_uint16_le(s, x);
    in_uint16_le(s, y);
    rv = amod->server_set_pointer_position(amod, x, y);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_server_version_message(struct mod *mod, struct stream *s)
{
    /* send version message */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_DATA);
    out_uint32_le(s, XUP_CLIENT_DATA_VERSION);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    out_uint32_le(s, XUP_PROTOCOL_VERSION);
    s_mark_end(s);
    int len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    int rv = lib_send_copy(mod, s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_server_monitor_update(struct mod *mod, struct stream *s,
                           int width, int height,
                           int num_monitors,
                           const struct monitor_info *monitors)
{
    /* send monitor update message */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_DATA);
    out_uint32_le(s, XUP_CLIENT_DATA_MONITOR_UPDATE);
    out_uint32_le(s, width);
    out_uint32_le(s, height);
    out_uint32_le(s, num_monitors);
    out_uint32_le(s, 0);
    out_uint8a(s, monitors, sizeof(monitors[0]) * num_monitors);
    s_mark_end(s);
    int len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    int rv = lib_send_copy(mod, s);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "send_server_monitor_update:"
              " sent monitor updsate message with following properties to"
              " xorgxrdp backend width=%d, height=%d, num=%d, return value=%d",
              width, height, num_monitors, rv);
    return rv;
}

static int
send_server_monitor_full_invalidate(
    struct mod *mod, struct stream *s, int width, int height)
{
    /* send invalidate message */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XUP_MSG_CLIENT_DATA);
    out_uint32_le(s, XUP_CLIENT_DATA_INVALIDATE);
    /* x and y */
    int i = 0;
    out_uint32_le(s, i);
    /* width and height */
    i = ((width & 0xffff) << 16) | height;
    out_uint32_le(s, i);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    s_mark_end(s);
    int len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    int rv = lib_send_copy(mod, s);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "send_server_monitor_full_invalidate:"
              " sent invalidate message with following"
              " properties to xorgxrdp backend"
              " width=%d, height=%d, return value=%d",
              width, height, rv);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_server_version_message(struct mod *mod)
{
    /* send server version message */
    struct stream *s;
    make_stream(s);
    int rv = send_server_version_message(mod, s);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_server_monitor_resize(struct mod *mod, int width, int height,
                               int num_monitors,
                               const struct monitor_info *monitors,
                               int *in_progress)
{
    struct stream *s;
    make_stream(s);
    int rv = send_server_monitor_update(mod, s, width, height,
                                        num_monitors, monitors);
    *in_progress = (rv == 0);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_server_monitor_full_invalidate(struct mod *mod, int width, int height)
{
    /* send invalidate message */
    struct stream *s;
    make_stream(s);
    int rv = send_server_monitor_full_invalidate(mod, s, width, height);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_process_orders(struct mod *mod, int type, struct stream *s)
{
    int rv;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lib_mod_process_orders: type %d", type);
    rv = 0;
    switch (type)
    {
        case XUP_ORDER_BEGIN_UPDATE:
            rv = mod->server_begin_update(mod);
            break;
        case XUP_ORDER_END_UPDATE:
            rv = mod->server_end_update(mod);
            break;
        case XUP_ORDER_FILL_RECT:
            rv = process_server_fill_rect(mod, s);
            break;
        case XUP_ORDER_SCREEN_BLT:
            rv = process_server_screen_blt(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT:
            rv = process_server_paint_rect(mod, s);
            break;
        case XUP_ORDER_SET_CLIP:
            rv = process_server_set_clip(mod, s);
            break;
        case XUP_ORDER_RESET_CLIP:
            rv = process_server_reset_clip(mod, s);
            break;
        case XUP_ORDER_SET_FGCOLOR:
            rv = process_server_set_fgcolor(mod, s);
            break;
        case XUP_ORDER_SET_BGCOLOR:
            rv = process_server_set_bgcolor(mod, s);
            break;
        case XUP_ORDER_SET_OPCODE:
            rv =  process_server_set_opcode(mod, s);
            break;
        case XUP_ORDER_SET_PEN:
            rv = process_server_set_pen(mod, s);
            break;
        case XUP_ORDER_DRAW_LINE:
            rv = process_server_draw_line(mod, s);
            break;
        case XUP_ORDER_SET_CURSOR:
            rv = process_server_set_cursor(mod, s);
            break;
        case XUP_ORDER_CREATE_OS_SURFACE:
            rv = process_server_create_os_surface(mod, s);
            break;
        case XUP_ORDER_SWITCH_OS_SURFACE:
            rv = process_server_switch_os_surface(mod, s);
            break;
        case XUP_ORDER_DELETE_OS_SURFACE:
            rv = process_server_delete_os_surface(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_OS:
            rv = process_server_paint_rect_os(mod, s);
            break;
        case XUP_ORDER_SET_HINTS:
            rv = process_server_set_hints(mod, s);
            break;
        case XUP_ORDER_WINDOW_NEW_UPDATE:
            rv = process_server_window_new_update(mod, s);
            break;
        case XUP_ORDER_WINDOW_DELETE:
            rv = process_server_window_delete(mod, s);
            break;
        case XUP_ORDER_WINDOW_SHOW:
            rv = process_server_window_show(mod, s);
            break;
        case XUP_ORDER_ADD_CHAR:
            rv = process_server_add_char(mod, s);
            break;
        case XUP_ORDER_ADD_CHAR_ALPHA:
            rv = process_server_add_char_alpha(mod, s);
            break;
        case XUP_ORDER_DRAW_TEXT:
            rv = process_server_draw_text(mod, s);
            break;
        case XUP_ORDER_CREATE_OS_SURFACE_BPP:
            rv = process_server_create_os_surface_bpp(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_BPP:
            rv = process_server_paint_rect_bpp(mod, s);
            break;
        case XUP_ORDER_COMPOSITE:
            rv = process_server_composite(mod, s);
            break;
        case XUP_ORDER_SET_CURSOR_EX:
            rv = process_server_set_pointer_ex(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_SHMEM:
            rv = process_server_paint_rect_shmem(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_SHMEM_EX:
            rv = process_server_paint_rect_shmem_ex(mod, s);
            break;
        case XUP_ORDER_EGFX_SHMFD:
            rv = process_server_egfx_shmfd(mod, s);
            break;
        case XUP_ORDER_SET_POINTER_SHMFD:
            rv = process_server_set_pointer_shmfd(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_SHMFD:
            rv = process_server_paint_rect_shmfd(mod, s);
            break;
        case XUP_ORDER_PAINT_RECT_DMABUF:
            rv = process_server_paint_rect_dmabuf(mod, s);
            break;
        case XUP_ORDER_SET_POINTER_SYSTEM:
            rv = process_server_set_pointer_system(mod, s);
            break;
        case XUP_ORDER_SET_POINTER_POSITION:
            rv = process_server_set_pointer_position(mod, s);
            break;
        case XUP_ORDER_EGFX_DMABUF:
            rv = process_server_egfx_dmabuf(mod, s);
            break;
        default:
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "lib_mod_process_orders: unknown order type %d", type);
            rv = 0;
            break;
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_process_message(struct mod *mod, struct stream *s)
{
    int num_orders;
    int index;
    int rv;
    int len;
    int type;
    char *phold;
    int version;

    int width;
    int height;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lib_mod_process_message:");
    in_uint16_le(s, type);
    in_uint16_le(s, num_orders);
    in_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "lib_mod_process_message: type %d", type);

    rv = 0;
    if (type == XUP_MSG_ORDER_LIST_LEGACY)
    {
        for (index = 0; index < num_orders; ++index)
        {
            in_uint16_le(s, type);
            rv = lib_mod_process_orders(mod, type, s);

            if (rv != 0)
            {
                break;
            }
        }
    }
    else if (type == XUP_MSG_CAPS)
    {
        mod->caps_processing_status = E_CAPS_OK;   /* Assume all OK */
        LOG_DEVEL(LOG_LEVEL_TRACE,
                  "lib_mod_process_message: caps len %d", len);
        for (index = 0; index < num_orders; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, len);

            switch (type)
            {
                case XUP_CAPS_VERSION:
                    in_uint32_le(s, version);
                    if (version != XUP_CLIENT_INFO_CURRENT_VERSION)
                    {
                        char msg[128];
                        g_snprintf(msg, sizeof(msg),
                                   "Xorg module has version %d, expected %d",
                                   version, XUP_CLIENT_INFO_CURRENT_VERSION);
                        LOG(LOG_LEVEL_ERROR, "%s", msg);
                        mod->server_msg(mod, msg, 0);
                        mod->caps_processing_status = E_CAPS_NOT_OK;
                    }
                    break;

                default:
                    LOG_DEVEL(LOG_LEVEL_TRACE,
                              "lib_mod_process_message: unknown"
                              " cap type %d len %d",
                              type, len);
                    break;
            }
            s->p = phold + len;
        }
    }
    else if (type == XUP_MSG_ORDER_LIST)
    {
        LOG_DEVEL(LOG_LEVEL_INFO,
                  "lib_mod_process_message: order list len %d", len);
        for (index = 0; index < num_orders; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, len);
            rv = lib_mod_process_orders(mod, type, s);

            if (rv != 0)
            {
                break;
            }

            s->p = phold + len;
        }
    }
    else if (type == XUP_MSG_METADATA)
    {
        LOG_DEVEL(LOG_LEVEL_INFO,
                  "lib_mod_process_message: metadata len %d", len);
        for (index = 0; index < num_orders; ++index)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, len);
            switch (type)
            {
                case XUP_METADATA_MEMORY_ALLOCATION_COMPLETE:
                    in_uint16_le(s, width);
                    in_uint16_le(s, height);
                    LOG(LOG_LEVEL_INFO, "Received memory_allocation_complete"
                        " command. width: %d, height: %d",
                        width, height);
                    rv = mod->server_monitor_resize_done(mod);
                    break;
            }
            s->p = phold + len;
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "unknown type %d", type);
    }

    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_signal(struct mod *mod)
{
    // no-op
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_end(struct mod *mod)
{
    if (mod->screen_shmem_pixels != 0)
    {
        g_shmdt(mod->screen_shmem_pixels);
        mod->screen_shmem_pixels = 0;
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_set_param(struct mod *mod, const char *name, const char *value)
{
    if (g_strcasecmp(name, "username") == 0)
    {
        g_strncpy(mod->username, value, INFO_CLIENT_MAX_CB_LEN - 1);
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
        g_strncpy(mod->password, value, INFO_CLIENT_MAX_CB_LEN - 1);
    }
    else if (g_strcasecmp(name, "ip") == 0)
    {
        g_strncpy(mod->ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
        g_strncpy(mod->port, value, 255);
    }
    else if (g_strcasecmp(name, "keycode_set") == 0)
    {
        g_snprintf(mod->keycode_set, sizeof(mod->keycode_set), "%s", value);
    }
    else if (g_strcasecmp(name, "h264_frame_interval") == 0)
    {
        mod->client_info.h264_frame_interval = g_atoi(value);
    }
    else if (g_strcasecmp(name, "rfx_frame_interval") == 0)
    {
        mod->client_info.rfx_frame_interval = g_atoi(value);
    }
    else if (g_strcasecmp(name, "normal_frame_interval") == 0)
    {
        mod->client_info.normal_frame_interval = g_atoi(value);
    }
    else if (g_strcasecmp(name, "client_info") == 0)
    {
        g_memcpy(&(mod->client_info), value, sizeof(mod->client_info));
    }

    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_get_wait_objs(struct mod *mod, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    if (mod != 0)
    {
        if (mod->trans != 0)
        {
            trans_get_wait_objs_rw(mod->trans, read_objs, rcount,
                                   write_objs, wcount, timeout);
        }
    }
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_check_wait_objs(struct mod *mod)
{
    int rv;

    rv = 0;
    if (mod != 0)
    {
        if (mod->trans != 0)
        {
            rv = trans_check_wait_objs(mod->trans);
            if (rv != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Xorg server closed connection");
            }
        }
    }

    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_frame_ack(struct mod *amod, int flags, int frame_id)
{
    LOG_DEVEL(LOG_LEVEL_TRACE,
              "lib_mod_frame_ack: flags 0x%8.8x frame_id %d", flags, frame_id);
    send_paint_rect_ex_ack(amod, flags, frame_id);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_suppress_output(struct mod *amod, int suppress,
                        int left, int top, int right, int bottom)
{
    LOG_DEVEL(LOG_LEVEL_TRACE,
              "lib_mod_suppress_output: suppress 0x%8.8x left %d top %d "
              "right %d bottom %d", suppress, left, top, right, bottom);
    send_suppress_output(amod, suppress, left, top, right, bottom);
    return 0;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct mod *mod;

    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (tintptr) mod;
    mod->mod_connect = lib_mod_connect;
    mod->mod_start = lib_mod_start;
    mod->mod_event = lib_mod_event;
    mod->mod_signal = lib_mod_signal;
    mod->mod_end = lib_mod_end;
    mod->mod_set_param = lib_mod_set_param;
    mod->mod_get_wait_objs = lib_mod_get_wait_objs;
    mod->mod_check_wait_objs = lib_mod_check_wait_objs;
    mod->mod_frame_ack = lib_mod_frame_ack;
    mod->mod_suppress_output = lib_mod_suppress_output;
    mod->mod_server_monitor_resize = lib_send_server_monitor_resize;
    mod->mod_server_monitor_full_invalidate
        = lib_send_server_monitor_full_invalidate;
    mod->mod_server_version_message = lib_send_server_version_message;
    return (tintptr) mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct mod *mod = (struct mod *) handle;

    if (mod == 0)
    {
        return 0;
    }
    trans_delete(mod->trans);
    g_free(mod);
    return 0;
}
