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
 * module manager
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif
#include "xrdp.h"
#include "log.h"
#include "string_calls.h"
#include "guid.h"
#include "ms-rdpedisp.h"
#include "ms-rdpbcgr.h"
#include "scp.h"
#include <ctype.h>
#include "xrdp_encoder.h"
#include "xrdp_sockets.h"
#include "xrdp_egfx.h"
#include "libxrdp.h"
#include "xrdp_channel.h"
#include <limits.h>

/* Forward declarations */
static int
xrdp_mm_chansrv_connect(struct xrdp_mm *self, const char *port);
static void
xrdp_mm_connect_sm(struct xrdp_mm *self);

/*****************************************************************************/
struct xrdp_mm *
xrdp_mm_create(struct xrdp_wm *owner)
{
    struct xrdp_mm *self;

    self = (struct xrdp_mm *)g_malloc(sizeof(struct xrdp_mm), 1);
    self->wm = owner;
    self->login_names = list_create();
    self->login_names->auto_free = 1;
    self->login_values = list_create();
    self->login_values->auto_free = 1;

    self->uid = -1; /* Never good to default UIDs to 0 */

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_mm_create: bpp %d mcs_connection_type %d "
              "jpeg_codec_id %d v3_codec_id %d rfx_codec_id %d "
              "h264_codec_id %d",
              self->wm->client_info->bpp,
              self->wm->client_info->mcs_connection_type,
              self->wm->client_info->jpeg_codec_id,
              self->wm->client_info->v3_codec_id,
              self->wm->client_info->rfx_codec_id,
              self->wm->client_info->h264_codec_id);

    if ((self->wm->client_info->gfx == 0) &&
            ((self->wm->client_info->h264_codec_id != 0) ||
             (self->wm->client_info->jpeg_codec_id != 0) ||
             (self->wm->client_info->rfx_codec_id != 0)))
    {
        self->encoder = xrdp_encoder_create(self);
    }

    return self;
}

/*****************************************************************************/
/* called from main thread */
static long
xrdp_mm_sync_unload(long param1, long param2)
{
    return g_free_library(param1);
}

/*****************************************************************************/
/* called from main thread */
static long
xrdp_mm_sync_load(long param1, long param2)
{
    long rv;
    char *libname;

    libname = (char *)param1;
    rv = g_load_library(libname);
    return rv;
}

/*****************************************************************************/
static void
xrdp_mm_module_cleanup(struct xrdp_mm *self)
{
    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_module_cleanup");

    if (self->mod != 0)
    {
        if (self->mod_exit != 0)
        {
            /* let the module cleanup */
            self->mod_exit(self->mod);
        }
    }

    if (self->mod_handle != 0)
    {
        /* Let the main thread unload the module.*/
        g_xrdp_sync(xrdp_mm_sync_unload, self->mod_handle, 0);
    }

    trans_delete(self->chan_trans);
    self->chan_trans = 0;
    self->mod_init = 0;
    self->mod_exit = 0;
    self->mod = 0;
    self->mod_handle = 0;

    if (self->wm->hide_log_window)
    {
        /* make sure autologin is off */
        self->wm->session->client_info->rdp_autologin = 0;
        xrdp_wm_set_login_state(self->wm, WMLS_RESET); /* reset session */
    }

}

/*****************************************************************************/
void
xrdp_mm_delete(struct xrdp_mm *self)
{
    if (self == 0)
    {
        return;
    }

    /* free any module stuff */
    xrdp_mm_module_cleanup(self);

    /* shutdown thread */
    xrdp_encoder_delete(self->encoder);

    trans_delete(self->sesman_trans);
    self->sesman_trans = 0;
    list_delete(self->login_names);
    list_delete(self->login_values);
    list_delete(self->resize_queue);
    g_free(self->resize_data);
    g_delete_wait_obj(self->resize_ready);
    xrdp_egfx_shutdown_full(self->egfx);
    g_free(self);
}

/**************************************************************************//**
 * Looks for a string value in the login_names/login_values array
 *
 * In the event of multiple matches, the LAST value matched is returned.
 * This currently allows for values to be replaced by writing a new value
 * to the end of the list
 *
 * Returned strings are valid until the module is destroyed.
 *
 * @param self This module
 * @param aname Name to lookup (case-insensitive)
 *
 * @return pointer to value, or NULL if not found.
 */
static const char *
xrdp_mm_get_value(struct xrdp_mm *self, const char *aname)
{
    const char *name;
    const char *value = NULL;
    unsigned int index = self->login_names->count;

    while (index > 0 && value == NULL)
    {
        --index;
        name = (const char *)list_get_item(self->login_names, index);

        if (name != NULL && g_strcasecmp(name, aname) == 0)
        {
            value = (const char *)list_get_item(self->login_values, index);
        }
    }

    return value;
}
/**************************************************************************//**
 * Looks for a numeric value in the login_names/login_values array
 *
 * Returned strings are valid until the module is destroyed.
 *
 * @param self This module
 * @param aname Name to lookup (case-insensitive)
 * @param def Default to return if value not found.
 *
 * @return value from name, or the specified default.
 */
static int
xrdp_mm_get_value_int(struct xrdp_mm *self, const char *aname, int def)
{
    const char *value = xrdp_mm_get_value(self, aname);

    return (value == NULL) ? def : g_atoi(value);
}

/*****************************************************************************/
/* Send gateway login information to sesman */
static int
xrdp_mm_send_sys_login_request(struct xrdp_mm *self, const char *username,
                               const char *password)
{
    xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                    "sending login info to session manager, please wait...");

    return scp_send_sys_login_request(
               self->sesman_trans, username, password,
               self->wm->client_info->client_ip);
}

/*****************************************************************************/
/* Send a create session request to sesman */
static int
xrdp_mm_create_session(struct xrdp_mm *self)
{
    int rv = 0;
    int xserverbpp;
    enum scp_session_type type;

    /* Map the session code to an SCP session type */
    switch (self->code)
    {
        case XVNC_SESSION_CODE:
            type = SCP_SESSION_TYPE_XVNC;
            break;

        case  XORG_SESSION_CODE:
            type = SCP_SESSION_TYPE_XORG;
            break;

        default:
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                            "Unrecognised session code %d", self->code);
            rv = 1;
    }

    if (rv == 0)
    {
        xserverbpp = xrdp_mm_get_value_int(self, "xserverbpp",
                                           self->wm->screen->bpp);

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                        "sending create session request to session"
                        " manager. Please wait...");
        rv = scp_send_create_session_request(
                 self->sesman_trans,
                 type,
                 self->wm->screen->width,
                 self->wm->screen->height,
                 xserverbpp,
                 self->wm->client_info->program,
                 self->wm->client_info->directory);
    }

    return rv;
}

/*****************************************************************************/
static int
xrdp_mm_setup_mod1(struct xrdp_mm *self)
{
    void *func;
    const char *lib;
    char text[256];

    if (self == 0)
    {
        return 1;
    }

    if ((lib = xrdp_mm_get_value(self, "lib")) == NULL)
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "no library name specified in xrdp.ini, please add "
                        "lib=libxrdp-vnc.so or similar");

        return 1;
    }

    if (lib[0] == 0)
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "empty library name specified in xrdp.ini, please "
                        "add lib=libxrdp-vnc.so or similar");

        return 1;
    }

    if (self->mod_handle == 0)
    {
        g_snprintf(text, sizeof(text), "%s/%s", XRDP_MODULE_PATH, lib);
        /* Let the main thread load the lib,*/
        self->mod_handle = g_xrdp_sync(xrdp_mm_sync_load, (tintptr)text, 0);

        if (self->mod_handle != 0)
        {
            func = g_get_proc_address(self->mod_handle, "mod_init");

            if (func == 0)
            {
                func = g_get_proc_address(self->mod_handle, "_mod_init");
            }

            if (func == 0)
            {
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                "error finding proc mod_init in %s, "
                                "not a valid xrdp backend", lib);
            }

            self->mod_init = (struct xrdp_mod * ( *)(void))func;
            func = g_get_proc_address(self->mod_handle, "mod_exit");

            if (func == 0)
            {
                func = g_get_proc_address(self->mod_handle, "_mod_exit");
            }

            if (func == 0)
            {
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                "error finding proc mod_exit in %s, "
                                "not a valid xrdp backend", lib);
            }

            self->mod_exit = (int ( *)(struct xrdp_mod *))func;

            if ((self->mod_init != 0) && (self->mod_exit != 0))
            {
                self->mod = self->mod_init();

                if (self->mod != 0)
                {
                    LOG(LOG_LEVEL_INFO, "loaded module '%s' ok, interface size %d, version %d", lib,
                        self->mod->size, self->mod->version);
                }
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "no mod_init or mod_exit address found");
            }
        }
        else
        {
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                            "error loading %s specified in xrdp.ini, please "
                            "add a valid entry like lib=libxrdp-vnc.so or "
                            "similar", lib);
            return 1;
        }

        if (self->mod != 0)
        {
            self->mod->wm = (long)(self->wm);
            self->mod->server_begin_update = server_begin_update;
            self->mod->server_end_update = server_end_update;
            self->mod->server_bell_trigger = server_bell_trigger;
            self->mod->server_chansrv_in_use = server_chansrv_in_use;
            self->mod->server_fill_rect = server_fill_rect;
            self->mod->server_screen_blt = server_screen_blt;
            self->mod->server_paint_rect = server_paint_rect;
            self->mod->server_set_pointer = server_set_pointer;
            self->mod->server_set_pointer_ex = server_set_pointer_ex;
            self->mod->server_palette = server_palette;
            self->mod->server_msg = server_msg;
            self->mod->server_is_term = g_is_term;
            self->mod->server_set_clip = server_set_clip;
            self->mod->server_reset_clip = server_reset_clip;
            self->mod->server_set_fgcolor = server_set_fgcolor;
            self->mod->server_set_bgcolor = server_set_bgcolor;
            self->mod->server_set_opcode = server_set_opcode;
            self->mod->server_set_mixmode = server_set_mixmode;
            self->mod->server_set_brush = server_set_brush;
            self->mod->server_set_pen = server_set_pen;
            self->mod->server_draw_line = server_draw_line;
            self->mod->server_add_char = server_add_char;
            self->mod->server_draw_text = server_draw_text;
            self->mod->client_monitor_resize = client_monitor_resize;
            self->mod->server_monitor_resize_done = server_monitor_resize_done;
            self->mod->server_get_channel_count = server_get_channel_count;
            self->mod->server_query_channel = server_query_channel;
            self->mod->server_get_channel_id = server_get_channel_id;
            self->mod->server_send_to_channel = server_send_to_channel;
            self->mod->server_create_os_surface = server_create_os_surface;
            self->mod->server_switch_os_surface = server_switch_os_surface;
            self->mod->server_delete_os_surface = server_delete_os_surface;
            self->mod->server_paint_rect_os = server_paint_rect_os;
            self->mod->server_set_hints = server_set_hints;
            self->mod->server_window_new_update = server_window_new_update;
            self->mod->server_window_delete = server_window_delete;
            self->mod->server_window_icon = server_window_icon;
            self->mod->server_window_cached_icon = server_window_cached_icon;
            self->mod->server_notify_new_update = server_notify_new_update;
            self->mod->server_notify_delete = server_notify_delete;
            self->mod->server_monitored_desktop = server_monitored_desktop;
            self->mod->server_add_char_alpha = server_add_char_alpha;
            self->mod->server_create_os_surface_bpp = server_create_os_surface_bpp;
            self->mod->server_paint_rect_bpp = server_paint_rect_bpp;
            self->mod->server_composite = server_composite;
            self->mod->server_paint_rects = server_paint_rects;
            self->mod->server_session_info = server_session_info;
            self->mod->server_egfx_cmd = server_egfx_cmd;
            self->mod->server_set_pointer_large = server_set_pointer_large;
            self->mod->server_paint_rects_ex = server_paint_rects_ex;
            self->mod->si = &(self->wm->session->si);
        }
    }

    /* id self->mod is null, there must be a problem */
    if (self->mod == 0)
    {
        LOG(LOG_LEVEL_ERROR, "problem loading lib in xrdp_mm_setup_mod1");
        return 1;
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_setup_mod2(struct xrdp_mm *self)
{
    char text[256];
    const char *name;
    const char *value;
    int i;
    int rv;
    int key_flags;
    int device_flags;

    rv = 1; /* failure */
    g_memset(text, 0, sizeof(text));

    if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
    {
        if (self->mod->mod_start(self->mod, self->wm->screen->width,
                                 self->wm->screen->height,
                                 self->wm->screen->bpp) != 0)
        {
            g_set_wait_obj(self->wm->pro_layer->self_term_event); /* kill session */
        }
    }

    if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
    {
        if (self->display > 0)
        {
            if (self->code == XVNC_SESSION_CODE)
            {
                g_snprintf(text, sizeof(text), "%d", 5900 + self->display);
            }
            else if (self->code == XORG_SESSION_CODE)
            {
                g_snprintf(text, sizeof(text), XRDP_X11RDP_STR,
                           self->uid, self->display);
            }
            else
            {
                g_set_wait_obj(self->wm->pro_layer->self_term_event); /* kill session */
            }
        }
    }

    if (!g_is_wait_obj_set(self->wm->pro_layer->self_term_event))
    {
        /* this adds the port to the end of the list, it will already be in
           the list as -1
           the module should use the last one */
        if (g_strlen(text) > 0)
        {
            list_add_strdup(self->login_names, "port");
            list_add_strdup(self->login_values, text);
        }

        /* always set these */

        self->mod->mod_set_param(self->mod, "client_info",
                                 (const char *) (self->wm->session->client_info));

        name = self->wm->session->client_info->hostname;
        self->mod->mod_set_param(self->mod, "hostname", name);
        g_snprintf(text, 255, "%d", self->wm->session->client_info->keylayout);
        self->mod->mod_set_param(self->mod, "keylayout", text);
        if (guid_is_set(&self->guid))
        {
            self->mod->mod_set_param(self->mod, "guid", (char *) &self->guid);
        }

        for (i = 0; i < self->login_names->count; i++)
        {
            name = (const char *) list_get_item(self->login_names, i);
            value = (const char *) list_get_item(self->login_values, i);
            self->mod->mod_set_param(self->mod, name, value);
        }

        /* connect */
        if (self->mod->mod_connect(self->mod) == 0)
        {
            rv = 0; /* connect success */
        }
        else
        {
            xrdp_wm_show_log(self->wm);
            if (self->wm->hide_log_window)
            {
                rv = 1;
            }
        }
    }

    if (rv == 0)
    {
        /* sync modifiers */
        key_flags = 0;
        device_flags = 0;

        if (self->wm->scroll_lock)
        {
            key_flags |= 1;
        }

        if (self->wm->num_lock)
        {
            key_flags |= 2;
        }

        if (self->wm->caps_lock)
        {
            key_flags |= 4;
        }

        if (self->mod != 0)
        {
            if (self->mod->mod_event != 0)
            {
                self->mod->mod_event(self->mod, WM_KEYBRD_SYNC, key_flags,
                                     device_flags, key_flags, device_flags);
            }
        }
    }

    return rv;
}

/*****************************************************************************/
/* returns error
   send a list of channels to the channel handler */
static int
xrdp_mm_trans_send_channel_setup(struct xrdp_mm *self, struct trans *trans)
{
    int chan_count;
    /* This value should be the same as chan_count, but we need to
     * cater for a possible failure of libxrdp_query_channel() */
    int output_chan_count;
    int chan_id;
    int chan_flags;
    int size;
    struct stream *s;
    char chan_name[256];

    g_memset(chan_name, 0, sizeof(char) * 256);

    s = trans_get_out_s(trans, 8192);

    if (s == 0)
    {
        return 1;
    }

    s_push_layer(s, iso_hdr, 8);
    s_push_layer(s, mcs_hdr, 8);
    s_push_layer(s, sec_hdr, 2);

    chan_count = libxrdp_get_channel_count(self->wm->session);
    output_chan_count = 0;
    for (chan_id = 0 ; chan_id < chan_count; ++chan_id)
    {
        if (libxrdp_query_channel(self->wm->session, chan_id, chan_name,
                                  &chan_flags) == 0)
        {
            out_uint8a(s, chan_name, 8);
            out_uint16_le(s, chan_id);
            out_uint16_le(s, chan_flags);
            ++output_chan_count;
        }
    }

    s_mark_end(s);
    s_pop_layer(s, sec_hdr);
    out_uint16_le(s, output_chan_count);
    s_pop_layer(s, mcs_hdr);
    size = (int)(s->end - s->p);
    out_uint32_le(s, 3); /* msg id */
    out_uint32_le(s, size); /* msg size */
    s_pop_layer(s, iso_hdr);
    size = (int)(s->end - s->p);
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, size); /* block size */
    return trans_force_write(trans);
}

/*****************************************************************************/
/* returns error
   data coming in from the channel handler, send it to the client */
static int
xrdp_mm_trans_process_channel_data(struct xrdp_mm *self, struct stream *s)
{
    unsigned int size;
    unsigned int total_size;
    int chan_id;
    int chan_flags;
    int rv = 0;

    if (!s_check_rem_and_log(s, 10, "Reading channel data header"))
    {
        rv = 1;
    }
    else
    {
        in_uint16_le(s, chan_id);
        in_uint16_le(s, chan_flags);
        in_uint16_le(s, size);
        in_uint32_le(s, total_size);
        if (!s_check_rem_and_log(s, size, "Reading channel data data"))
        {
            rv = 1;
        }
        else
        {
            rv = libxrdp_send_to_channel(self->wm->session, chan_id,
                                         s->p, size, total_size, chan_flags);
        }
    }

    return rv;
}

/*****************************************************************************/
/* returns error
   process rail create window order */
static int
xrdp_mm_process_rail_create_window(struct xrdp_mm *self, struct stream *s)
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

    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_process_rail_create_window: 0x%8.8x", window_id);

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
    rv = libxrdp_orders_init(self->wm->session);
    if (rv == 0)
    {
        rv = libxrdp_window_new_update(self->wm->session, window_id, &rwso, flags);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(self->wm->session);
    }
    g_free(rwso.title_info);
    g_free(rwso.window_rects);
    g_free(rwso.visibility_rects);
    return rv;
}

#if 0
/*****************************************************************************/
/* returns error
   process rail configure window order */
static int
xrdp_mm_process_rail_configure_window(struct xrdp_mm *self, struct stream *s)
{
    int flags;
    int window_id;
    int index;
    int bytes;
    int rv;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);

    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_process_rail_configure_window: 0x%8.8x", window_id);

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
    rv = libxrdp_orders_init(self->wm->session);
    if (rv == 0)
    {
        rv = libxrdp_window_new_update(self->wm->session, window_id, &rwso, flags);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(self->wm->session);
    }
    g_free(rwso.window_rects);
    g_free(rwso.visibility_rects);
    return rv;
}
#endif

/*****************************************************************************/
/* returns error
   process rail destroy window order */
static int
xrdp_mm_process_rail_destroy_window(struct xrdp_mm *self, struct stream *s)
{
    int window_id;
    int rv;

    in_uint32_le(s, window_id);
    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_process_rail_destroy_window 0x%8.8x", window_id);
    rv = libxrdp_orders_init(self->wm->session);
    if (rv == 0)
    {
        rv = libxrdp_window_delete(self->wm->session, window_id);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(self->wm->session);
    }
    return rv;
}

/*****************************************************************************/
/* returns error
   process rail update window (show state) order */
static int
xrdp_mm_process_rail_show_window(struct xrdp_mm *self, struct stream *s)
{
    int window_id;
    int rv;
    int flags;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, flags);
    in_uint32_le(s, rwso.show_state);
    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_process_rail_show_window 0x%8.8x %x", window_id,
        rwso.show_state);
    rv = libxrdp_orders_init(self->wm->session);
    if (rv == 0)
    {
        rv = libxrdp_window_new_update(self->wm->session, window_id, &rwso, flags);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(self->wm->session);
    }
    return rv;
}

/*****************************************************************************/
/* returns error
   process rail update window (title) order */
static int
xrdp_mm_process_rail_update_window_text(struct xrdp_mm *self, struct stream *s)
{
    int size;
    int flags;
    int rv;
    int window_id;
    struct rail_window_state_order rwso;

    LOG(LOG_LEVEL_DEBUG, "xrdp_mm_process_rail_update_window_text:");
    in_uint32_le(s, window_id);
    in_uint32_le(s, flags);
    LOG(LOG_LEVEL_DEBUG, "  update window title info: 0x%8.8x", window_id);

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, size); /* title size */
    if (size < 0 || !s_check_rem(s, size))
    {
        LOG(LOG_LEVEL_ERROR, "%s : invalid window text size %d",
            __func__, size);
        return 1;
    }
    rwso.title_info = g_new(char, size + 1);
    in_uint8a(s, rwso.title_info, size);
    rwso.title_info[size] = 0;
    LOG(LOG_LEVEL_DEBUG, "  set window title %s size %d 0x%8.8x", rwso.title_info, size, flags);
    rv = libxrdp_orders_init(self->wm->session);
    if (rv == 0)
    {
        rv = libxrdp_window_new_update(self->wm->session, window_id, &rwso, flags);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(self->wm->session);
    }
    LOG(LOG_LEVEL_DEBUG, "  set window title %s %d", rwso.title_info, rv);

    g_free(rwso.title_info);

    return rv;
}

/*****************************************************************************/
/* returns error
   process alternate secondary drawing orders for rail channel */
static int
xrdp_mm_process_rail_drawing_orders(struct xrdp_mm *self, struct stream *s)
{
    int order_type;
    int rv;

    rv = 0;
    in_uint32_le(s, order_type);

    switch (order_type)
    {
        case 2: /* create_window */
            xrdp_mm_process_rail_create_window(self, s);
            break;
        case 4: /* destroy_window */
            xrdp_mm_process_rail_destroy_window(self, s);
            break;
        case 6: /* show_window */
            rv = xrdp_mm_process_rail_show_window(self, s);
            break;
        case 8: /* update title info */
            rv = xrdp_mm_process_rail_update_window_text(self, s);
            break;
        default:
            break;
    }

    return rv;
}

#define GFX_PLANAR_BYTES (32 * 1024)

/******************************************************************************/
int
xrdp_mm_egfx_send_planar_bitmap(struct xrdp_mm *self,
                                struct xrdp_bitmap *bitmap,
                                struct xrdp_rect *rect, int surface_id,
                                int x, int y)
{
    struct xrdp_egfx_rect gfx_rect;
    struct stream *comp_s;
    struct stream *temp_s;
    char *pixels;
    char *src8;
    char *dst8;
    int index;
    int lines;
    int comp_bytes;
    int xindex;
    int yindex;
    int bwidth;
    int bheight;
    int cx;
    int cy;
    int rv = 0;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_send_planar_bitmap:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_egfx_send_planar_bitmap: "
              "surface_id %d rect %d %d %d %d x %d y %d",
              surface_id, rect->left, rect->top, rect->right, rect->bottom,
              x, y);
    bwidth = rect->right - rect->left;
    bheight = rect->bottom - rect->top;
    if ((bwidth < 1) || (bheight < 1))
    {
        return 0;
    }
    if (bwidth < 64)
    {
        cx = bwidth;
        cy = 4096 / cx;
    }
    else if (bheight < 64)
    {
        cy = bheight;
        cx = 4096 / cy;
    }
    else
    {
        cx = 64;
        cy = 64;
    }
    while (cx * cy < 4096)
    {
        if (cx < cy)
        {
            cx++;
            cy = 4096 / cx;
        }
        else
        {
            cy++;
            cx = 4096 / cy;
        }
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_send_planar_bitmap: cx %d cy %d", cx, cy);
    pixels = g_new(char, GFX_PLANAR_BYTES);
    make_stream(comp_s);
    init_stream(comp_s, GFX_PLANAR_BYTES);
    make_stream(temp_s);
    init_stream(temp_s, GFX_PLANAR_BYTES);
    if (xrdp_egfx_send_frame_start(self->egfx, 1, 0) != 0)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_send_planar_bitmap: "
            "xrdp_egfx_send_frame_start error");
        rv = 1;
        goto cleanup;
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_send_planar_bitmap: left %d top %d right %d "
              "bottom %d", rect->left, rect->top, rect->right, rect->bottom);
    for (yindex = rect->top; yindex < rect->bottom; yindex += cy)
    {
        bheight = rect->bottom - yindex;
        bheight = MIN(bheight, cy);
        for (xindex = rect->left; xindex < rect->right; xindex += cx)
        {
            bwidth = rect->right - xindex;
            bwidth = MIN(bwidth, cx);
            LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_send_planar_bitmap: xindex %d "
                      "yindex %d, bwidth %d bheight %d",
                      xindex, yindex, bwidth, bheight);
            src8 = bitmap->data + bitmap->line_size * yindex + xindex * 4;
            dst8 = pixels + (bheight - 1) * bwidth * 4;
            for (index = 0; index < bheight; index++)
            {
                g_memcpy(dst8, src8, bwidth * 4);
                src8 += bitmap->line_size;
                dst8 -= bwidth * 4;
            }
            lines = libxrdp_planar_compress(pixels, bwidth, bheight, comp_s,
                                            32, GFX_PLANAR_BYTES, bheight - 1,
                                            temp_s, 0, 0x10);
            comp_s->end = comp_s->p;
            comp_s->p = comp_s->data;
            if (lines != bheight)
            {
                LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_send_planar_bitmap: "
                    "lines(%d) != bheight(%d) error", lines, bheight);
            }
            else
            {
                comp_bytes = (int)(comp_s->end - comp_s->data);
                LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_send_planar_bitmap: lines %d "
                          "comp_bytes %d", lines, comp_bytes);
                gfx_rect.x1 = xindex - x;
                gfx_rect.y1 = yindex - y;
                gfx_rect.x2 = gfx_rect.x1 + bwidth;
                gfx_rect.y2 = gfx_rect.y1 + bheight;
                if (xrdp_egfx_send_wire_to_surface1(self->egfx, surface_id,
                                                    XR_RDPGFX_CODECID_PLANAR,
                                                    XR_PIXEL_FORMAT_XRGB_8888,
                                                    &gfx_rect, comp_s->data,
                                                    comp_bytes) != 0)
                {
                    LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_send_planar_bitmap: "
                        "xrdp_egfx_send_wire_to_surface1 error");
                    rv = 1;
                    goto cleanup;
                }
            }
        }
    }
    if (xrdp_egfx_send_frame_end(self->egfx, 1) != 0)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_send_planar_bitmap: "
            "xrdp_egfx_send_frame_end error");
        rv = 1;
    }
cleanup:
    g_free(pixels);
    free_stream(comp_s);
    free_stream(temp_s);
    return rv;
}

/******************************************************************************/
static int
xrdp_mm_egfx_invalidate_wm_screen(struct xrdp_mm *self)
{
    int error = 0;

    LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_invalidate_wm_screen:");

    // Only invalidate the WM screen if the module is using the WM screen,
    // or we haven't loaded the module yet.
    //
    // Otherwise we may send client updates which conflict with the
    // updates sent directly from the module via the encoder.
    if (self->mod_uses_wm_screen_for_gfx || self->mod_handle == 0)
    {
        struct xrdp_bitmap *screen = self->wm->screen;
        struct xrdp_rect xr_rect;

        xr_rect.left = 0;
        xr_rect.top = 0;
        xr_rect.right = screen->width;
        xr_rect.bottom = screen->height;
        if (self->wm->screen_dirty_region == NULL)
        {
            self->wm->screen_dirty_region = xrdp_region_create(self->wm);
        }
        error = xrdp_region_add_rect(self->wm->screen_dirty_region, &xr_rect);
    }
    return error;
}

/******************************************************************************/
static int
dynamic_monitor_open_response(intptr_t id, int chan_id, int creation_status)
{
    struct xrdp_process *pro;
    struct xrdp_wm *wm;
    struct stream *s;
    int bytes;

    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_open_response: chan_id %d "
              "creation_status 0x%8.8x", chan_id, creation_status);
    if (creation_status != 0)
    {
        LOG(LOG_LEVEL_ERROR, "dynamic_monitor_open_response: error");
        return 1;
    }
    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    make_stream(s);
    init_stream(s, 1024);
    out_uint32_le(s, 5); /* DISPLAYCONTROL_PDU_TYPE_CAPS */
    out_uint32_le(s, 8 + 12);
    out_uint32_le(s, CLIENT_MONITOR_DATA_MAXIMUM_MONITORS); /* MaxNumMonitors */
    out_uint32_le(s, 4096); /* MaxMonitorAreaFactorA */
    out_uint32_le(s, 2048); /* MaxMonitorAreaFactorB */
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    libxrdp_drdynvc_data(wm->session, chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/******************************************************************************/
static int
dynamic_monitor_close_response(intptr_t id, int chan_id)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_close_response:");
    return 0;
}

/******************************************************************************/
static int
dynamic_monitor_data_first(intptr_t id, int chan_id, char *data, int bytes,
                           int total_bytes)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_data_first:");
    return 0;
}

/******************************************************************************/
int
advance_resize_state_machine(struct xrdp_mm *mm,
                             enum display_resize_state new_state)
{
    struct display_control_monitor_layout_data *description = mm->resize_data;
    LOG_DEVEL(LOG_LEVEL_INFO,
              "advance_resize_state_machine:"
              " Processing resize to: %d x %d."
              " Advancing state from %s to %s."
              " Previous state took %d MS.",
              description->description.session_width,
              description->description.session_height,
              XRDP_DISPLAY_RESIZE_STATE_TO_STR(description->state),
              XRDP_DISPLAY_RESIZE_STATE_TO_STR(new_state),
              g_time3() - description->last_state_update_timestamp);
    description->state = new_state;
    description->last_state_update_timestamp = g_time3();
    g_set_wait_obj(mm->resize_ready);
    return 0;
}

struct ver_flags_t
{
    int version;
    int flags;
};

/******************************************************************************/
static int
cmpverfunc (const void *a, const void *b)
{
    return ((struct ver_flags_t *)a)->version -
           ((struct ver_flags_t *)b)->version;
}

/******************************************************************************/
static int
xrdp_mm_egfx_create_surfaces(struct xrdp_mm *self)
{
    int surface_id;
    int index;
    int count;
    int left;
    int top;
    int width;
    int height;
    struct monitor_info *mi;
    struct xrdp_bitmap *screen;

    screen = self->wm->screen;
    count = self->wm->client_info->display_sizes.monitorCount;
    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_mm_egfx_create_surfaces: "
              "monitor count %d", count);
    if (count < 1)
    {
        left = 0;
        top = 0;
        width = screen->width;
        height = screen->height;
        xrdp_egfx_send_create_surface(self->egfx, self->egfx->surface_id,
                                      width, height,
                                      XR_PIXEL_FORMAT_XRGB_8888);
        xrdp_egfx_send_map_surface(self->egfx, self->egfx->surface_id,
                                   left, top);
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_create_surfaces: map "
            "surface_id %d left %d top %d width %d height %d",
            self->egfx->surface_id, left, top, width, height);
        return 0;
    }
    for (index = 0; index < count; index++)
    {
        surface_id = index;
        mi = self->wm->client_info->display_sizes.minfo_wm + index;
        left = mi->left;
        top = mi->top;
        width = mi->right - mi->left + 1;
        height = mi->bottom - mi->top + 1;
        xrdp_egfx_send_create_surface(self->egfx, surface_id,
                                      width, height,
                                      XR_PIXEL_FORMAT_XRGB_8888);
        xrdp_egfx_send_map_surface(self->egfx, surface_id, left, top);
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_create_surfaces: map "
            "surface_id %d left %d top %d width %d height %d",
            surface_id, left, top, width, height);
    }
    return 0;
}

/******************************************************************************/
static int
xrdp_mm_egfx_caps_advertise(void *user, int caps_count,
                            int *versions, int *flagss)
{
    struct xrdp_mm *self;
    struct xrdp_bitmap *screen;
    int index;
    int best_index;
    int best_h264_index;
    int best_pro_index;
    int error;
    int version;
    int flags;
    struct ver_flags_t *ver_flags;

    LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise:");
    self = (struct xrdp_mm *) user;
    screen = self->wm->screen;
    if (screen->data == NULL)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise: can not do gfx");
    }
    /* create copy for sorting */
    ver_flags = g_new(struct ver_flags_t, caps_count);
    if (ver_flags == NULL)
    {
        return 1;
    }
    for (index = 0; index < caps_count; index++)
    {
        ver_flags[index].version = versions[index];
        ver_flags[index].flags = flagss[index];
    }
    /* sort by version */
    g_qsort(ver_flags, caps_count, sizeof(struct ver_flags_t), cmpverfunc);
    best_index = -1;
    best_h264_index = -1;
    best_pro_index = -1;
    for (index = 0; index < caps_count; index++)
    {
        version = ver_flags[index].version;
        flags = ver_flags[index].flags;
        LOG(LOG_LEVEL_INFO, "  version 0x%8.8x flags 0x%8.8x (index: %d)",
            version, flags, index);
        switch (version)
        {
            case XR_RDPGFX_CAPVERSION_8: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_101:
                best_pro_index = index;
                break;
            case XR_RDPGFX_CAPVERSION_81:
                if (flags & XR_RDPGFX_CAPS_FLAG_AVC420_ENABLED)
                {
                    best_h264_index = index;
                }
                best_pro_index = index;
                break;
            case XR_RDPGFX_CAPVERSION_10:
                if (!(flags & XR_RDPGFX_CAPS_FLAG_AVC_DISABLED))
                {
                    best_h264_index = index;
                }
                best_pro_index = index;
                break;
            case XR_RDPGFX_CAPVERSION_102: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_103: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_104: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_105: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_106: /* FALLTHROUGH */
            case XR_RDPGFX_CAPVERSION_107:
                if (!(flags & XR_RDPGFX_CAPS_FLAG_AVC_DISABLED))
                {
                    best_h264_index = index;
                }
                best_pro_index = index;
                break;
            default:
                /* just skip unknwown */
                LOG(LOG_LEVEL_INFO, "unknown version 0x%8.8x", version);
                break;
        }
    }
    if (best_pro_index >= 0)
    {
        best_index = best_pro_index;
        self->egfx_flags = XRDP_EGFX_RFX_PRO;
    }
    /* prefer h264, todo use setting in xrdp.ini for this */
    if (best_h264_index >= 0)
    {
#if defined(XRDP_X264) || defined(XRDP_NVENC)
        best_index = best_h264_index;
        self->egfx_flags = XRDP_EGFX_H264;
#endif
    }
    if (best_index >= 0)
    {
        LOG(LOG_LEVEL_INFO, "  replying version 0x%8.8x flags 0x%8.8x",
            ver_flags[best_index].version, ver_flags[best_index].flags);
        error = xrdp_egfx_send_capsconfirm(self->egfx,
                                           ver_flags[best_index].version,
                                           ver_flags[best_index].flags);
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise: xrdp_egfx_send_capsconfirm "
            "error %d best_index %d", error, best_index);
        error = xrdp_egfx_send_reset_graphics(self->egfx,
                                              screen->width, screen->height,
                                              self->wm->client_info->display_sizes.monitorCount,
                                              self->wm->client_info->display_sizes.minfo_wm);
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise: xrdp_egfx_send_reset_graphics "
            "error %d monitorCount %d",
            error, self->wm->client_info->display_sizes.monitorCount);
        self->egfx_up = 1;
        xrdp_mm_egfx_create_surfaces(self);
        self->encoder = xrdp_encoder_create(self);
        xrdp_mm_egfx_invalidate_wm_screen(self);

        if (self->resize_data != NULL
                && self->resize_data->state == WMRZ_EGFX_INITALIZING)
        {
            advance_resize_state_machine(self, WMRZ_EGFX_INITIALIZED);
        }
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise: egfx created.");
        if (self->gfx_delay_autologin)
        {
            self->gfx_delay_autologin = 0;
            xrdp_wm_set_login_state(self->wm, WMLS_START_CONNECT);
        }
    }
    else
    {
        struct xrdp_rect lrect;

        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_caps_advertise: no good gfx, canceling");
        lrect.left = 0;
        lrect.top = 0;
        lrect.right = screen->width;
        lrect.bottom = screen->height;
        self->wm->client_info->gfx = 0;
        xrdp_encoder_delete(self->encoder);
        self->encoder = xrdp_encoder_create(self);
        xrdp_bitmap_invalidate(screen, &lrect);
    }
    g_free(ver_flags);
    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_update_module_frame_ack(struct xrdp_mm *self)
{
    int fif;
    struct xrdp_encoder *encoder;

    encoder = self->encoder;
    fif = encoder->frames_in_flight;
    if (encoder->frame_id_client + fif > encoder->frame_id_server)
    {
        if (encoder->frame_id_server > encoder->frame_id_server_sent)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_update_module_ack: "
                      "frame_id_server %d", encoder->frame_id_server);
            encoder->frame_id_server_sent = encoder->frame_id_server;
            self->mod->mod_frame_ack(self->mod, 0, encoder->frame_id_server);
        }
    }
    return 0;
}

static int
xrdp_mm_egfx_frame_ack(void *user, uint32_t queue_depth, int frame_id,
                       int frames_decoded)
{
    struct xrdp_mm *self;
    struct xrdp_encoder *encoder;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_frame_ack:");
    self = (struct xrdp_mm *) user;
    encoder = self->encoder;
    if (encoder == NULL)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_frame_ack: encoder is nil");
        return 0;
    }
    if (queue_depth == XR_SUSPEND_FRAME_ACKNOWLEDGEMENT)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_frame_ack: "
            "queue_depth %d frame_id %d frames_decoded %d",
            queue_depth, frame_id, frames_decoded);
        if (encoder->gfx_ack_off == 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_mm_egfx_frame_ack: "
                "client request turn off frame acks.");
            encoder->gfx_ack_off = 1;
            frame_id = -1;
        }
    }
    else
    {
        if (encoder->gfx_ack_off)
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_frame_ack: "
                      "client request turn on frame acks");
            encoder->gfx_ack_off = 0;
        }
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_egfx_frame_ack: "
              "incoming %d, client %d, server %d",
              frame_id, encoder->frame_id_client, encoder->frame_id_server);
    if (frame_id < 0 || frame_id > encoder->frame_id_server)
    {
        /* if frame_id is negative or bigger then what server last sent
           just ack all sent frames */
        /* some clients can send big number just to clear all
           pending frames */
        encoder->frame_id_client = encoder->frame_id_server;
    }
    else
    {
        /* frame acks can come out of order so ignore older one */
        encoder->frame_id_client = MAX(frame_id, encoder->frame_id_client);
    }
    xrdp_mm_update_module_frame_ack(self);
    return 0;
}

/******************************************************************************/
int
egfx_initialize(struct xrdp_mm *self)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "egfx_initialize");
    if (!(self->wm->client_info->gfx))
    {
        return 0;
    }
    LOG_DEVEL(LOG_LEVEL_INFO, "egfx_initialize: gfx capable client");
    if (xrdp_egfx_create(self, &(self->egfx)) == 0)
    {
        self->egfx->user = self;
        self->egfx->caps_advertise = xrdp_mm_egfx_caps_advertise;
        self->egfx->frame_ack = xrdp_mm_egfx_frame_ack;
        return 0;
    }
    LOG_DEVEL(LOG_LEVEL_INFO, "egfx_initialize: xrdp_egfx_create failed");
    return 1;
}

/******************************************************************************/
static const int MAXIMUM_MONITOR_SIZE
    = sizeof(struct monitor_info) * CLIENT_MONITOR_DATA_MAXIMUM_MONITORS;

/******************************************************************************/
static void
sync_dynamic_monitor_data(struct xrdp_wm *wm,
                          struct display_size_description *description)
{
    struct display_size_description *display_sizes
        = &(wm->client_info->display_sizes);

    display_sizes->monitorCount = description->monitorCount;
    display_sizes->session_width = description->session_width;
    display_sizes->session_height = description->session_height;
    g_memcpy(display_sizes->minfo,
             description->minfo,
             MAXIMUM_MONITOR_SIZE);
    g_memcpy(display_sizes->minfo_wm,
             description->minfo_wm,
             MAXIMUM_MONITOR_SIZE);
}

/******************************************************************************/
static int
dynamic_monitor_data(intptr_t id, int chan_id, char *data, int bytes)
{
    int error = 0;
    struct stream ls;
    struct stream *s;
    int msg_type;
    int msg_length;
    struct xrdp_process *pro;
    struct xrdp_wm *wm;
    int monitor_layout_size;
    struct display_size_description *display_size_data;

    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_data:");
    pro = (struct xrdp_process *) id;
    wm = pro->wm;

    if (OUTPUT_SUPPRESSED_FOR_REASON(wm->client_info,
                                     XSO_REASON_CLIENT_REQUEST))
    {
        LOG(LOG_LEVEL_INFO, "dynamic_monitor_data: Not allowing resize."
            " Suppress output requested by client");
        return error;
    }

    g_memset(&ls, 0, sizeof(ls));
    ls.data = data;
    ls.p = ls.data;
    ls.size = bytes;
    ls.end = ls.data + bytes;
    s = &ls;
    in_uint32_le(s, msg_type);
    in_uint32_le(s, msg_length);
    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "dynamic_monitor_data: msg_type %d msg_length %d",
              msg_type, msg_length);

    if (msg_type != DISPLAYCONTROL_PDU_TYPE_MONITOR_LAYOUT
            && msg_length >= 0)
    {
        // Unsupported message types
        switch (msg_type)
        {
            case DISPLAYCONTROL_PDU_TYPE_CAPS:
                LOG(LOG_LEVEL_ERROR, "dynamic_monitor_data: Received"
                    " DISPLAYCONTROL_PDU_TYPE_CAPS. Per MS-RDPEDISP 2.2.2.1,"
                    " this is a server-to-client message, client should never"
                    " send this. Ignoring message");
                break;
            default:
                LOG(LOG_LEVEL_ERROR, "dynamic_monitor_data: Received neither"
                    " DISPLAYCONTROL_PDU_TYPE_MONITOR_LAYOUT nor"
                    " DISPLAYCONTROL_PDU_TYPE_CAPS. Ignoring message.");
                break;
        }
        return 0;
    }
    in_uint32_le(s, monitor_layout_size);
    if (monitor_layout_size != 40)
    {
        /* 2.2.2.2 DISPLAYCONTROL_MONITOR_LAYOUT_PDU */
        LOG(LOG_LEVEL_ERROR, "dynamic_monitor_data: monitor_layout_size"
            " is %d. Per spec ([MS-RDPEDISP] 2.2.2.2"
            " DISPLAYCONTROL_MONITOR_LAYOUT_PDU) it must be 40.",
            monitor_layout_size);
        return 1;
    }

    display_size_data = (struct display_size_description *)
                        g_malloc(sizeof(struct display_size_description), 1);
    if (!display_size_data)
    {
        return 1;
    }
    error = libxrdp_process_monitor_stream(s, display_size_data, 1);
    if (error)
    {
        LOG(LOG_LEVEL_ERROR, "dynamic_monitor_data:"
            " libxrdp_process_monitor_stream"
            " failed with error %d.", error);
        g_free(display_size_data);
        return error;
    }
    list_add_item(wm->mm->resize_queue, (tintptr)display_size_data);
    g_set_wait_obj(wm->mm->resize_ready);
    LOG(LOG_LEVEL_DEBUG, "dynamic_monitor_data:"
        " received width %d, received height %d.",
        display_size_data->session_width, display_size_data->session_height);
    return 0;
}

/******************************************************************************/
static int
advance_error(int error,
              struct xrdp_mm *mm)
{
    advance_resize_state_machine(mm, WMRZ_ERROR);
    return error;
}

/******************************************************************************/
static int
process_display_control_monitor_layout_data(struct xrdp_wm *wm)
{
    int error = 0;
    struct xrdp_mm *mm;
    struct xrdp_mod *module;
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;
    int in_progress;

    LOG_DEVEL(LOG_LEVEL_TRACE, "process_display_control_monitor_layout_data:");

    if (wm == NULL)
    {
        return 1;
    }
    mm = wm->mm;
    if (mm == NULL)
    {
        return 1;
    }
    module = mm->mod;
    if (module == NULL)
    {
        return 1;
    }

    if (!xrdp_wm_can_resize(wm))
    {
        return 0;
    }

    struct display_control_monitor_layout_data *description
            = mm->resize_data;
    const unsigned int desc_width = description->description.session_width;
    const unsigned int desc_height = description->description.session_height;

    switch (description->state)
    {
        case WMRZ_ENCODER_DELETE:
            // Stop any output from the module
            rdp = (struct xrdp_rdp *) (wm->session->rdp);
            xrdp_rdp_suppress_output(rdp,
                                     1, XSO_REASON_DYNAMIC_RESIZE,
                                     0, 0, 0, 0);
            // Disable the encoder until the resize is complete.
            if (mm->encoder != NULL)
            {
                xrdp_encoder_delete(mm->encoder);
                mm->encoder = NULL;
            }
            if (mm->resize_data->using_egfx == 0)
            {
                advance_resize_state_machine(mm, WMRZ_SERVER_MONITOR_RESIZE);
            }
            else
            {
                advance_resize_state_machine(mm, WMRZ_EGFX_DELETE_SURFACE);
            }
            break;
        case WMRZ_EGFX_DELETE_SURFACE:
            if (error == 0 && module != 0)
            {
                xrdp_egfx_shutdown_delete_surface(mm->egfx);
            }
            advance_resize_state_machine(mm, WMRZ_EGFX_CONN_CLOSE);
            break;
        case WMRZ_EGFX_CONN_CLOSE:
            if (error == 0 && module != 0)
            {
                xrdp_egfx_shutdown_close_connection(wm->mm->egfx);
                mm->egfx_up = 0;
            }
            advance_resize_state_machine(mm, WMRZ_EGFX_CONN_CLOSING);
            break;
        // Also processed in xrdp_egfx_close_response
        case WMRZ_EGFX_CONN_CLOSING:
            rdp = (struct xrdp_rdp *) (wm->session->rdp);
            sec = rdp->sec_layer;
            chan = sec->chan_layer;

            // Continue to check to see if the connection is closed. If it
            // ever is, advance the state machine!
            if (chan->drdynvcs[mm->egfx->channel_id].status
                    == XRDP_DRDYNVC_STATUS_CLOSED
                    || (g_time3() - description->last_state_update_timestamp) > 100)
            {
                advance_resize_state_machine(mm, WMRZ_EGFX_CONN_CLOSED);
                break;
            }
            g_set_wait_obj(mm->resize_ready);
            break;
        case WMRZ_EGFX_CONN_CLOSED:
            advance_resize_state_machine(mm, WRMZ_EGFX_DELETE);
            break;
        case WRMZ_EGFX_DELETE:
            if (error == 0 && module != 0)
            {
                xrdp_egfx_shutdown_delete(wm->mm->egfx);
                mm->egfx = NULL;
            }
            advance_resize_state_machine(mm, WMRZ_SERVER_MONITOR_RESIZE);
            break;
        case WMRZ_SERVER_MONITOR_RESIZE:
            error = module->mod_server_monitor_resize(
                        module, desc_width, desc_height,
                        description->description.monitorCount,
                        description->description.minfo,
                        &in_progress);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " mod_server_monitor_resize failed %d", error);
                return advance_error(error, mm);
            }
            else if (in_progress)
            {
                // Call is proceeding asynchronously
                advance_resize_state_machine(
                    mm, WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING);
            }
            else
            {
                // Call is done
                advance_resize_state_machine(
                    mm, WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED);
            }
            break;
        // Not processed here. Processed in client_monitor_resize
        // case WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING:
        case WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED:
            advance_resize_state_machine(mm, WMRZ_XRDP_CORE_RESET);
            break;
        case WMRZ_XRDP_CORE_RESET:
            sync_dynamic_monitor_data(wm, &(description->description));
            error = libxrdp_reset(wm->session);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " libxrdp_reset failed %d", error);
                return advance_error(error, mm);
            }
            /* reset cache */
            error = xrdp_cache_reset(wm->cache, wm->client_info);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " xrdp_cache_reset failed %d", error);
                return advance_error(error, mm);
            }
            advance_resize_state_machine(mm, WMRZ_XRDP_CORE_RESET_PROCESSING);
            break;

        // Not processed here. Processed in xrdp_mm_up_and_running()
        // case WMRZ_XRDP_CORE_RESET_PROCESSING:
        case WMRZ_XRDP_CORE_RESET_PROCESSED:
            /* load some stuff */
            error = xrdp_wm_load_static_colors_plus(wm, 0);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " xrdp_wm_load_static_colors_plus failed %d", error);
                return advance_error(error, mm);
            }

            error = xrdp_wm_load_static_pointers(wm);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " xrdp_wm_load_static_pointers failed %d", error);
                return advance_error(error, mm);
            }
            /* resize the main window */
            error = xrdp_bitmap_resize(
                        wm->screen, desc_width, desc_height);
            if (error != 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "process_display_control_monitor_layout_data:"
                          " xrdp_bitmap_resize failed %d", error);
                return advance_error(error, mm);
            }
            advance_resize_state_machine(mm, WMRZ_EGFX_INITIALIZE);
            break;
        case WMRZ_EGFX_INITIALIZE:
            if (mm->resize_data->using_egfx)
            {
                egfx_initialize(mm);
                advance_resize_state_machine(mm, WMRZ_EGFX_INITALIZING);
            }
            else
            {
                advance_resize_state_machine(mm, WMRZ_EGFX_INITIALIZED);
            }
            break;
        // Not processed here. Processed in xrdp_mm_egfx_caps_advertise
        // case WMRZ_EGFX_INITALIZING:
        case WMRZ_EGFX_INITIALIZED:
            advance_resize_state_machine(mm, WMRZ_ENCODER_CREATE);
            break;
        case WMRZ_ENCODER_CREATE:
            if (mm->encoder == NULL)
            {
                mm->encoder = xrdp_encoder_create(mm);
            }
            advance_resize_state_machine(mm, WMRZ_SERVER_INVALIDATE);
            break;
        case WMRZ_SERVER_INVALIDATE:
            if (module != 0)
            {
                // Ack all frames to speed up resize.
                module->mod_frame_ack(module, 0, INT_MAX);
            }
            // Restart module output after invalidating
            // the screen. This causes an automatic redraw.
            xrdp_bitmap_invalidate(wm->screen, 0);
            rdp = (struct xrdp_rdp *) (wm->session->rdp);
            xrdp_rdp_suppress_output(rdp,
                                     0, XSO_REASON_DYNAMIC_RESIZE,
                                     0, 0, desc_width, desc_height);
            advance_resize_state_machine(mm, WMRZ_COMPLETE);
            break;
        default:
            break;
    }
    return 0;
}

/******************************************************************************/
static int
dynamic_monitor_process_queue(struct xrdp_mm *self)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_process_queue:");

    if (self == 0)
    {
        return 0;
    }

    struct xrdp_wm *wm = self->wm;

    if (!xrdp_wm_can_resize(wm))
    {
        return 0;
    }

    if (self->resize_data == NULL && self->resize_queue != NULL)
    {
        if  (self->resize_queue->count <= 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "Resize queue is empty.");
            return 0;
        }
        LOG_DEVEL(LOG_LEVEL_DEBUG, "dynamic_monitor_process_queue: Queue is"
                  " not empty. Filling out description.");
        const struct display_size_description *queue_head =
            (struct display_size_description *)
            list_get_item(self->resize_queue, 0);

        const int invalid_dimensions = queue_head->session_width <= 0
                                       || queue_head->session_height <= 0;

        if (invalid_dimensions)
        {
            LOG(LOG_LEVEL_DEBUG,
                "dynamic_monitor_process_queue: Not allowing"
                " resize due to invalid dimensions (w: %d x h: %d)",
                queue_head->session_width,
                queue_head->session_height);
        }

        const struct display_size_description *current_size
                = &wm->client_info->display_sizes;

        const int already_this_size = queue_head->session_width
                                      == current_size->session_width
                                      && queue_head->session_height
                                      == current_size->session_height;

        if (already_this_size)
        {
            LOG(LOG_LEVEL_DEBUG,
                "dynamic_monitor_process_queue: Not resizing."
                " Already this size. (w: %d x h: %d)",
                queue_head->session_width,
                queue_head->session_height);
        }

        if (!invalid_dimensions && !already_this_size)
        {
            const int LAYOUT_DATA_SIZE =
                sizeof(struct display_control_monitor_layout_data);
            self->resize_data = (struct display_control_monitor_layout_data *)
                                g_malloc(LAYOUT_DATA_SIZE, 1);
            g_memcpy(&(self->resize_data->description), queue_head,
                     sizeof(struct display_size_description));
            const int time = g_time3();
            self->resize_data->start_time = time;
            self->resize_data->last_state_update_timestamp = time;
            self->resize_data->using_egfx = (self->egfx != NULL);
            advance_resize_state_machine(self, WMRZ_ENCODER_DELETE);
        }
        else
        {
            g_set_wait_obj(self->resize_ready);
        }
        list_remove_item(self->resize_queue, 0);
        return 0;
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "dynamic_monitor_process_queue:"
                  " Resize data is not null.");
    }

    if (self->resize_data == NULL)
    {
        return 0;
    }

    if (self->resize_data->state == WMRZ_COMPLETE)
    {
        LOG(LOG_LEVEL_INFO, "dynamic_monitor_process_queue: Clearing"
            " completed resize (w: %d x h: %d). It took %d milliseconds.",
            self->resize_data->description.session_width,
            self->resize_data->description.session_height,
            g_time3() - self->resize_data->start_time);
        g_set_wait_obj(self->resize_ready);
    }
    else if (self->resize_data->state == WMRZ_ERROR)
    {
        LOG(LOG_LEVEL_INFO, "dynamic_monitor_process_queue: Clearing"
            " failed request to resize to: (w: %d x h: %d)",
            self->resize_data->description.session_width,
            self->resize_data->description.session_height);
        g_set_wait_obj(self->resize_ready);
    }
    else
    {
        // No errors, process it!
        return process_display_control_monitor_layout_data(self->wm);
    }
    g_free(self->resize_data);
    self->resize_data = NULL;
    return 0;
}

/******************************************************************************/
int
dynamic_monitor_initialize(struct xrdp_mm *self)
{
    struct xrdp_drdynvc_procs d_procs;
    int flags;
    int error;
    char buf[1024];
    int pid;

    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_initialize:");

    g_memset(&d_procs, 0, sizeof(d_procs));
    d_procs.open_response = dynamic_monitor_open_response;
    d_procs.close_response = dynamic_monitor_close_response;
    d_procs.data_first = dynamic_monitor_data_first;
    d_procs.data = dynamic_monitor_data;
    flags = 0;
    error = libxrdp_drdynvc_open(self->wm->session,
                                 "Microsoft::Windows::RDS::DisplayControl",
                                 flags, &d_procs,
                                 &(self->dynamic_monitor_chanid));
    if (error != 0)
    {
        LOG(LOG_LEVEL_ERROR, "dynamic_monitor_initialize: "
            "libxrdp_drdynvc_open failed %d", error);
        return error;
    }

    // Initialize xrdp_mm specific variables.
    self->resize_queue = list_create();
    self->resize_queue->auto_free = 1;
    pid = g_getpid();
    /* setup wait objects for signaling */
    g_snprintf(buf, sizeof(buf), "xrdp_%8.8x_resize_ready", pid);
    self->resize_ready = g_create_wait_obj(buf);
    self->resize_data = NULL;

    return error;
}

/******************************************************************************/
int
xrdp_mm_drdynvc_up(struct xrdp_mm *self)
{
    struct display_control_monitor_layout_data *ignore_marker;
    const char *enable_dynamic_resize;
    int error = 0;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_drdynvc_up:");

    error = egfx_initialize(self);
    if (error != 0)
    {
        return error;
    }

    enable_dynamic_resize = xrdp_mm_get_value(self, "enable_dynamic_resizing");
    /*
     * User can disable dynamic resizing if necessary
     */
    if (enable_dynamic_resize != NULL && enable_dynamic_resize[0] != '\0' &&
            !g_text2bool(enable_dynamic_resize))
    {
        LOG(LOG_LEVEL_INFO, "User has disabled dynamic resizing.");
        return error;
    }
    error = dynamic_monitor_initialize(self);
    if (error != 0)
    {
        LOG(LOG_LEVEL_INFO, "Dynamic monitor initialize failed."
            " Client likely does not support it.");
        return error;
    }
    ignore_marker = (struct display_control_monitor_layout_data *)
                    g_malloc(sizeof(struct display_control_monitor_layout_data),
                             1);
    list_add_item(self->resize_queue, (tintptr)ignore_marker);
    return error;
}

/******************************************************************************/
int
xrdp_mm_suppress_output(struct xrdp_mm *self, int suppress,
                        int left, int top, int right, int bottom)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_suppress_output: suppress %d "
              "left %d top %d right %d bottom %d",
              suppress, left, top, right, bottom);
    if (self->mod != NULL)
    {
        if (self->mod->mod_suppress_output != NULL)
        {
            self->mod->mod_suppress_output(self->mod, suppress,
                                           left, top, right, bottom);
        }
    }
    return 0;
}

/******************************************************************************/
int
xrdp_mm_up_and_running(struct xrdp_mm *self)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_up_and_running:");
    if (self->resize_data != NULL &&
            self->resize_data->state == WMRZ_XRDP_CORE_RESET_PROCESSING)
    {
        LOG(LOG_LEVEL_INFO,
            "xrdp_mm_up_and_running: Core reset done.");
        advance_resize_state_machine(self, WMRZ_XRDP_CORE_RESET_PROCESSED);
    }
    return 0;
}

/*****************************************************************************/
/* open response from client going to channel server */
static int
xrdp_mm_drdynvc_open_response(intptr_t id, int chan_id, int creation_status)
{
    struct trans *trans;
    struct stream *s;
    struct xrdp_wm *wm;
    struct xrdp_process *pro;
    int chansrv_chan_id;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_drdynvc_open_response: "
              " chan_id %d creation_status %d",
              chan_id, creation_status);
    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    trans = wm->mm->chan_trans;
    s = trans_get_out_s(trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 24); /* size */
    out_uint32_le(s, 13); /* msg id */
    out_uint32_le(s, 16); /* size */
    chansrv_chan_id = wm->mm->xr2cr_cid_map[chan_id];
    out_uint32_le(s, chansrv_chan_id);
    out_uint32_le(s, creation_status); /* status */
    s_mark_end(s);
    return trans_write_copy(trans);
}

/*****************************************************************************/
/* close response from client going to channel server */
static int
xrdp_mm_drdynvc_close_response(intptr_t id, int chan_id)
{
    struct trans *trans;
    struct stream *s;
    struct xrdp_wm *wm;
    struct xrdp_process *pro;
    int chansrv_chan_id;

    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    trans = wm->mm->chan_trans;
    s = trans_get_out_s(trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 20); /* size */
    out_uint32_le(s, 15); /* msg id */
    out_uint32_le(s, 12); /* size */
    chansrv_chan_id = wm->mm->xr2cr_cid_map[chan_id];
    out_uint32_le(s, chansrv_chan_id);
    s_mark_end(s);
    return trans_write_copy(trans);
}

/*****************************************************************************/
/* part data from client going to channel server */
static int
xrdp_mm_drdynvc_data_first(intptr_t id, int chan_id, char *data,
                           int bytes, int total_bytes)
{
    struct trans *trans;
    struct stream *s;
    struct xrdp_wm *wm;
    struct xrdp_process *pro;
    int chansrv_chan_id;

    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    trans = wm->mm->chan_trans;
    s = trans_get_out_s(trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 4 + 4 + 4 + bytes);
    out_uint32_le(s, 17); /* msg id */
    out_uint32_le(s, 8 + 4 + 4 + 4 + bytes);
    chansrv_chan_id = wm->mm->xr2cr_cid_map[chan_id];
    out_uint32_le(s, chansrv_chan_id);
    out_uint32_le(s, bytes);
    out_uint32_le(s, total_bytes);
    out_uint8a(s, data, bytes);
    s_mark_end(s);
    return trans_write_copy(trans);
}

/*****************************************************************************/
/* data from client going to channel server */
static int
xrdp_mm_drdynvc_data(intptr_t id, int chan_id, char *data, int bytes)
{
    struct trans *trans;
    struct stream *s;
    struct xrdp_wm *wm;
    struct xrdp_process *pro;
    int chansrv_chan_id;

    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    trans = wm->mm->chan_trans;
    s = trans_get_out_s(trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 4 + 4 + bytes);
    out_uint32_le(s, 19); /* msg id */
    out_uint32_le(s, 8 + 4 + 4 + bytes);
    chansrv_chan_id = wm->mm->xr2cr_cid_map[chan_id];
    out_uint32_le(s, chansrv_chan_id);
    out_uint32_le(s, bytes);
    out_uint8a(s, data, bytes);
    s_mark_end(s);
    return trans_write_copy(trans);
}

/*****************************************************************************/
/* open message from channel server going to client */
static int
xrdp_mm_trans_process_drdynvc_channel_open(struct xrdp_mm *self,
        struct stream *s)
{
    int name_bytes;
    int flags;
    int error;
    int chan_id;
    int chansrv_chan_id;
    char name[1024 + 1];
    struct xrdp_drdynvc_procs procs;

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint32_le(s, name_bytes);
    if ((name_bytes < 1) || (name_bytes > (int)(sizeof(name) - 1)))
    {
        return 1;
    }
    if (!s_check_rem(s, name_bytes))
    {
        return 1;
    }
    in_uint8a(s, name, name_bytes);
    name[name_bytes] = 0;
    if (!s_check_rem(s, 8))
    {
        return 1;
    }
    in_uint32_le(s, flags);
    in_uint32_le(s, chansrv_chan_id);
    if (chansrv_chan_id < 0 || chansrv_chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to open invalid chansrv channel %d",
            chansrv_chan_id);
        return 1;
    }

    if (flags == 0)
    {
        /* open static channel, not supported */
        return 1;
    }
    else
    {
        /* dynamic channel */
        g_memset(&procs, 0, sizeof(procs));
        procs.open_response = xrdp_mm_drdynvc_open_response;
        procs.close_response = xrdp_mm_drdynvc_close_response;
        procs.data_first = xrdp_mm_drdynvc_data_first;
        procs.data = xrdp_mm_drdynvc_data;
        chan_id = 0;
        error = libxrdp_drdynvc_open(self->wm->session, name, flags, &procs,
                                     &chan_id);
        if (error != 0)
        {
            return 1;
        }
        self->xr2cr_cid_map[chan_id] = chansrv_chan_id;
        self->cs2xr_cid_map[chansrv_chan_id] = chan_id;
    }
    return 0;
}

/*****************************************************************************/
/* close message from channel server going to client */
static int
xrdp_mm_trans_process_drdynvc_channel_close(struct xrdp_mm *self,
        struct stream *s)
{
    int chansrv_chan_id;
    int chan_id;
    int error;

    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint32_le(s, chansrv_chan_id);
    if (chansrv_chan_id < 0 || chansrv_chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to close invalid chansrv channel %d",
            chansrv_chan_id);
        return 1;
    }
    chan_id = self->cs2xr_cid_map[chansrv_chan_id];
    /* close dynamic channel */
    error = libxrdp_drdynvc_close(self->wm->session, chan_id);
    if (error != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* data from channel server going to client */
static int
xrdp_mm_trans_process_drdynvc_data_first(struct xrdp_mm *self,
        struct stream *s)
{
    int chansrv_chan_id;
    int chan_id;
    int error;
    int data_bytes;
    int total_bytes;
    char *data;

    if (!s_check_rem(s, 12))
    {
        return 1;
    }
    in_uint32_le(s, chansrv_chan_id);
    in_uint32_le(s, data_bytes);
    in_uint32_le(s, total_bytes);
    if ((!s_check_rem(s, data_bytes)))
    {
        return 1;
    }
    in_uint8p(s, data, data_bytes);
    chan_id = self->cs2xr_cid_map[chansrv_chan_id];
    error = libxrdp_drdynvc_data_first(self->wm->session, chan_id, data,
                                       data_bytes, total_bytes);
    if (error != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* data from channel server going to client */
static int
xrdp_mm_trans_process_drdynvc_data(struct xrdp_mm *self,
                                   struct stream *s)
{
    int chansrv_chan_id;
    int chan_id;
    int error;
    int data_bytes;
    char *data;

    if (!s_check_rem(s, 8))
    {
        return 1;
    }
    in_uint32_le(s, chansrv_chan_id);
    in_uint32_le(s, data_bytes);
    if ((!s_check_rem(s, data_bytes)))
    {
        return 1;
    }
    in_uint8p(s, data, data_bytes);
    chan_id = self->cs2xr_cid_map[chansrv_chan_id];
    error = libxrdp_drdynvc_data(self->wm->session, chan_id, data, data_bytes);
    if (error != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error
   process a message for the channel handler */
static int
xrdp_mm_chan_process_msg(struct xrdp_mm *self, struct trans *trans,
                         struct stream *s)
{
    int rv;
    int id;
    int size;
    char *next_msg;
    char *s_end;

    rv = 0;

    while (s_check_rem(s, 8))
    {
        next_msg = s->p;
        in_uint32_le(s, id);
        in_uint32_le(s, size);
        if (size < 8)
        {
            return 1;
        }
        if (!s_check_rem(s, size - 8))
        {
            return 1;
        }
        next_msg += size;
        s_end = s->end;
        s->end = next_msg;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_chan_process_msg: got msg id %d", id);
        switch (id)
        {
            case 8: /* channel data */
                rv = xrdp_mm_trans_process_channel_data(self, s);
                break;
            case 10: /* rail alternate secondary drawing orders */
                rv = xrdp_mm_process_rail_drawing_orders(self, s);
                break;
            case 12:
                rv = xrdp_mm_trans_process_drdynvc_channel_open(self, s);
                break;
            case 14:
                rv = xrdp_mm_trans_process_drdynvc_channel_close(self, s);
                break;
            case 16:
                rv = xrdp_mm_trans_process_drdynvc_data_first(self, s);
                break;
            case 18:
                rv = xrdp_mm_trans_process_drdynvc_data(self, s);
                break;
            default:
                LOG(LOG_LEVEL_ERROR, "xrdp_mm_chan_process_msg: unknown id %d", id);
                break;
        }
        s->end = s_end;
        if (rv != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_mm_chan_process_msg: error rv %d id %d", rv, id);
            rv = 0;
        }

        s->p = next_msg;
    }

    return rv;
}

/*****************************************************************************/
/* this is callback from trans obj
   returns error */
static int
xrdp_mm_chan_data_in(struct trans *trans)
{
    struct xrdp_mm *self;
    struct stream *s;
    int size;
    int error;

    if (trans == NULL)
    {
        return 1;
    }

    self = (struct xrdp_mm *)(trans->callback_data);
    s = trans_get_in_s(trans);

    if (s == 0)
    {
        return 1;
    }

    if (trans->extra_flags == 0)
    {
        in_uint8s(s, 4); /* id */
        in_uint32_le(s, size);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_chan_data_in: got header, size %d", size);
        if (size > 8)
        {
            self->chan_trans->header_size = size;
            trans->extra_flags = 1;
            return 0;
        }
    }
    /* here, the entire message block is read in, process it */
    error = xrdp_mm_chan_process_msg(self, trans, s);
    self->chan_trans->header_size = 8;
    trans->extra_flags = 0;
    init_stream(s, 0);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_chan_data_in: got whole message, reset for "
              "next header");
    return error;
}

/*****************************************************************************/
/* does the section in xrdp.ini has any channel.*=true | false */
static int
xrdp_mm_update_allowed_channels(struct xrdp_mm *self)
{
    int index;
    int count;
    int chan_id;
    int disabled;
    const char *name;
    const char *value;
    const char *chan_name;
    struct xrdp_session *session;

    session = self->wm->session;
    count = self->login_names->count;
    for (index = 0; index < count; index++)
    {
        name = (const char *) list_get_item(self->login_names, index);
        if (g_strncasecmp(name, "channel.", 8) == 0)
        {
            value = (const char *) list_get_item(self->login_values, index);
            chan_name = name + 8;
            chan_id = libxrdp_get_channel_id(session, chan_name);
            disabled = !g_text2bool(value);
            libxrdp_disable_channel(session, chan_id, disabled);
            if (disabled)
            {
                LOG(LOG_LEVEL_INFO, "xrdp_mm_update_allowed_channels: channel %s "
                    "channel id %d is disabled", chan_name, chan_id);
            }
            else
            {
                LOG(LOG_LEVEL_INFO, "xrdp_mm_update_allowed_channels: channel %s "
                    "channel id %d is allowed", chan_name, chan_id);
            }
        }
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_get_sesman_port(char *port, int port_bytes)
{
    int fd;
    int error;
    int index;
    char *val;
    char cfg_file[256];
    struct list *names;
    struct list *values;

    g_memset(cfg_file, 0, sizeof(char) * 256);
    /* default to port 3350 */
    g_strncpy(port, "3350", port_bytes - 1);
    /* see if port is in sesman.ini file */
    g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);
    fd = g_file_open_ro(cfg_file);

    if (fd >= 0)
    {
        names = list_create();
        names->auto_free = 1;
        values = list_create();
        values->auto_free = 1;

        if (file_read_section(fd, "Globals", names, values) == 0)
        {
            for (index = 0; index < names->count; index++)
            {
                val = (char *)list_get_item(names, index);

                if (val != 0)
                {
                    if (g_strcasecmp(val, "ListenPort") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        error = g_atoi(val);

                        if ((error > 0) && (error < 65000))
                        {
                            g_strncpy(port, val, port_bytes - 1);
                        }

                        break;
                    }
                }
            }
        }

        list_delete(names);
        list_delete(values);
        g_file_close(fd);
    }

    return 0;
}

/*****************************************************************************/
/* returns error
   data coming from client that need to go to channel handler */
int
xrdp_mm_process_channel_data(struct xrdp_mm *self, tbus param1, tbus param2,
                             tbus param3, tbus param4)
{
    struct stream *s;
    int rv;
    int length;
    int total_length;
    int flags;
    int id;
    char *data;

    rv = 0;

    if ((self->chan_trans != 0) && self->chan_trans->status == TRANS_STATUS_UP)
    {
        s = trans_get_out_s(self->chan_trans, 8192);

        if (s != 0)
        {
            id = LOWORD(param1);
            flags = HIWORD(param1);
            length = param2;
            data = (char *)param3;
            total_length = param4;

            if (total_length < length)
            {
                LOG(LOG_LEVEL_WARNING, "WARNING in xrdp_mm_process_channel_data(): total_len < length");
                total_length = length;
            }

            out_uint32_le(s, 0); /* version */
            out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + length);
            out_uint32_le(s, 5); /* msg id */
            out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + length);
            out_uint16_le(s, id);
            out_uint16_le(s, flags);
            out_uint16_le(s, length);
            out_uint32_le(s, total_length);
            out_uint8a(s, data, length);
            s_mark_end(s);
            rv = trans_force_write(self->chan_trans);
        }
    }

    return rv;
}

/*****************************************************************************/
static int
xrdp_mm_process_login_response(struct xrdp_mm *self)
{
    enum scp_login_status login_result;
    int rv;
    int server_closed;

    self->mmcs_expecting_msg = 0;

    rv = scp_get_login_response(self->sesman_trans, &login_result,
                                &server_closed, &self->uid);
    if (rv == 0)
    {
        if (login_result != E_SCP_LOGIN_OK)
        {
            char buff[128];
            scp_login_status_to_str(login_result, buff, sizeof(buff));
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s", buff);

            if (login_result == E_SCP_LOGIN_NOT_AUTHENTICATED &&
                    self->wm->pamerrortxt[0] != '\0')
            {
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s",
                                self->wm->pamerrortxt);
            }

            if (server_closed)
            {
                if (login_result == E_SCP_LOGIN_NOT_AUTHENTICATED)
                {
                    xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s",
                                    "Login retry limit reached");
                }
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s",
                                "Close the log window to exit.");
                self->wm->fatal_error_in_log_window = 1;
                /* Transport can be deleted now */
                self->delete_sesman_trans = 1;
            }
            /* If the server hasn't closed, inform the window manager
             * of the fail, but leave the sesman connection open for
             * further login attempts */
            xrdp_wm_mod_connect_done(self->wm, 1);
        }
        else
        {
            /* login successful */
            xrdp_mm_connect_sm(self);
        }
    }

    return rv;
}

/*****************************************************************************/
static int
xrdp_mm_process_create_session_response(struct xrdp_mm *self)
{
    enum scp_screate_status status;
    int display;
    struct guid guid;

    int rv;

    self->mmcs_expecting_msg = 0;

    rv = scp_get_create_session_response(self->sesman_trans, &status,
                                         &display, &guid);
    if (rv == 0)
    {
        const char *username;

        /* Sort out some logging information */
        if ((username = xrdp_mm_get_value(self, "username")) == NULL)
        {
            username = "???";
        }

        if (status == E_SCP_SCREATE_OK)
        {
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                            "session is available on display %d for user %s",
                            display, username);

            /* Carry on with the connect state machine */
            self->display = display;
            self->guid = guid;
            xrdp_mm_connect_sm(self);
        }
        else
        {
            char buff[128];
            scp_screate_status_to_str(status, buff, sizeof(buff));
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                            "Can't create session for user %s - %s",
                            username, buff);
            /* Leave the sesman connection open for further login attenpts */
            xrdp_wm_mod_connect_done(self->wm, 1);
        }
    }

    return rv;
}

/*****************************************************************************/
/* This is the callback registered for sesman communication replies. */
static int
xrdp_mm_scp_data_in(struct trans *trans)
{
    int rv = 0;
    int available;

    rv = scp_msg_in_check_available(trans, &available);
    if (rv == 0 && available)
    {
        struct xrdp_mm *self = (struct xrdp_mm *)(trans->callback_data);
        enum scp_msg_code msgno;

        switch ((msgno = scp_msg_in_get_msgno(trans)))
        {
            case E_SCP_LOGIN_RESPONSE:
                rv = xrdp_mm_process_login_response(self);
                break;

            case E_SCP_CREATE_SESSION_RESPONSE:
                rv = xrdp_mm_process_create_session_response(self);
                break;

            default:
            {
                char buff[64];
                scp_msgno_to_str(msgno, buff, sizeof(buff));
                LOG(LOG_LEVEL_ERROR, "Ignored SCP message %s from sesman",
                    buff);
            }
        }

        scp_msg_in_reset(trans);
    }

    return rv;
}

/*****************************************************************************/
/* This routine clears all states to make sure that our next login will be
 * as expected. If the user does not press ok on the log window and try to
 * connect again we must make sure that no previous information is stored.
 *
 * This routine does not clear a sesman_trans if it is allocated, as this
 * would break the password retry limit mechanism */
static void
cleanup_states(struct xrdp_mm *self)
{
    if (self != NULL)
    {
        self->connect_state = MMCS_CONNECT_TO_SESMAN;
        self->use_sesman = 0; /* true if this is a sesman session */
        self->use_chansrv = 0; /* true if chansrvport is set in xrdp.ini or using sesman */
        self->use_gw_login = 0; /* true if we're to use the gateway login facility */
        //self->sesman_trans = NULL; /* connection to sesman */
        self->chan_trans = NULL; /* connection to chansrv */
        self->delete_sesman_trans = 0;
        self->display = 0; /* 10 for :10.0, 11 for :11.0, etc */
        guid_clear(&self->guid);
        self->code = 0; /* ???_SESSION_CODE value */
    }
}

/*************************************************************************//**
 * Parses a chansrvport string
 *
 * This will be in one of the following formats:-
 * <path>         UNIX path to a domain socket
 * DISPLAY(<num>) Use chansrv on X Display <num>
 *
 * @param value assigned to chansrvport
 * @param dest Output buffer
 * @param dest_size Total size of output buffer, including terminator space
 * @param uid of destination
 *
 * Pass in dest of NULL, dest_size of 0 and uid of -1 to simply see if
 * the string parses OK.
 *
 * @return 0 for success
 */

static int
parse_chansrvport(const char *value, char *dest, int dest_size, int uid)
{
    int rv = 0;

    if (g_strncmp(value, "DISPLAY(", 8) == 0)
    {
        const char *p = value + 8;
        const char *end = p;

        /* Check next chars are digits followed by ')' */
        while (isdigit(*end))
        {
            ++end;
        }

        if (end == p || *end != ')')
        {
            LOG(LOG_LEVEL_WARNING, "Ignoring invalid chansrvport string '%s'",
                value);
            rv = -1;
        }
        else
        {
            g_snprintf(dest, dest_size, XRDP_CHANSRV_STR, uid, g_atoi(p));
        }
    }
    else
    {
        g_strncpy(dest, value, dest_size - 1);
    }

    return rv;
}

/*****************************************************************************/
static struct trans *
xrdp_mm_scp_connect(struct xrdp_mm *self)
{
    char port[128];
    char port_description[128];
    struct trans *t;

    xrdp_mm_get_sesman_port(port, sizeof(port));
    scp_port_to_display_string(port,
                               port_description, sizeof(port_description));

    xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                    "connecting to sesman on %s", port_description);
    t = scp_connect(port, "xrdp", g_is_term);
    if (t != NULL)
    {
        /* fully connected */
        t->trans_data_in = xrdp_mm_scp_data_in;
        t->callback_data = self;

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "sesman connect ok");
    }
    else
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "Error connecting to sesman on %s", port_description);
        trans_delete(t);
        t = NULL;
    }
    return t;
}

/*****************************************************************************/
static int
xrdp_mm_sesman_connect(struct xrdp_mm *self)
{
    trans_delete(self->sesman_trans);
    self->sesman_trans = xrdp_mm_scp_connect(self);

    return (self->sesman_trans == NULL); /* 0 for success */
}

/*****************************************************************************/
static int
xrdp_mm_chansrv_connect(struct xrdp_mm *self, const char *port)
{
    if (self->wm->client_info->channels_allowed == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "%s: "
            "skip connecting to chansrv because all channels are disabled",
            __func__);
        return 0;
    }

    /* connect channel redir */
    self->chan_trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);

    self->chan_trans->is_term = g_is_term;
    self->chan_trans->si = &(self->wm->session->si);
    self->chan_trans->my_source = XRDP_SOURCE_CHANSRV;
    self->chan_trans->trans_data_in = xrdp_mm_chan_data_in;
    self->chan_trans->header_size = 8;
    self->chan_trans->callback_data = self;
    self->chan_trans->no_stream_init_on_data_in = 1;
    self->chan_trans->extra_flags = 0;

    /* try to connect for up to 10 seconds */
    trans_connect(self->chan_trans, NULL, port, 10 * 1000);
    if (self->chan_trans->status != TRANS_STATUS_UP)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_mm_chansrv_connect: error in "
            "trans_connect chan");
    }
    else if (xrdp_mm_trans_send_channel_setup(self, self->chan_trans) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_mm_chansrv_connect: error in "
            "xrdp_mm_trans_send_channel_setup");
        trans_delete(self->chan_trans);
        self->chan_trans = NULL;
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "xrdp_mm_chansrv_connect: chansrv "
            "connect successful");
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_user_session_connect(struct xrdp_mm *self)
{
    int rv = 0;

    if (xrdp_mm_setup_mod1(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Failure setting up module");
        xrdp_mm_module_cleanup(self);
        rv = 1;
    }
    else if (xrdp_mm_setup_mod2(self) != 0)
    {
        /* connect error */
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "Error connecting to user session");
        xrdp_mm_module_cleanup(self);
        rv = 1; /* failure */
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "return value from %s %d", __func__, rv);

    return rv;
}

/**************************************************************************//**
 * Initialise and start the connect sequence
 *
 * @param self This object
 */
void
xrdp_mm_connect(struct xrdp_mm *self)
{
    const char *port = xrdp_mm_get_value(self, "port");
    const char *gw_username = xrdp_mm_get_value(self, "pamusername");

    /* make sure we start in correct state */
    cleanup_states(self);

    self->code = xrdp_mm_get_value_int(self, "code", 0);

    /* Look at our module parameters to decide if we need to connect
     * to sesman or not */

    if (port != NULL && g_strcmp(port, "-1") == 0)
    {
        self->use_sesman = 1;
        /* Connecting to a remote sesman is no longer supported. For purely
         * local session types, this setting could be removed.
         * The 'ip' value is still used for Xvnc sessions, to find the TCP
         * address that the X server is listening on */
        if (xrdp_mm_get_value(self, "ip") != NULL)
        {
            if (self->code == XORG_SESSION_CODE)
            {
                xrdp_wm_log_msg(self->wm,
                                LOG_LEVEL_WARNING,
                                "'ip' is not needed for this connection"
                                " - please remove");
            }
        }
    }

    if (gw_username != NULL)
    {
        /* Connecting to a remote sesman is no longer supported */
        if (xrdp_mm_get_value(self, "pamsessionmng") != NULL)
        {
            xrdp_wm_log_msg(self->wm,
                            LOG_LEVEL_WARNING,
                            "Parameter 'pamsessionmng' is obsolete."
                            " Please remove from config");
        }
        self->use_gw_login = 1;
    }

    /* Will we need chansrv ? We use it unconditionally for a
     * sesman session, but the user can also request it separately */
    if (self->use_sesman)
    {
        self->use_chansrv = 1;
    }
    else
    {
        const char *csp = xrdp_mm_get_value(self, "chansrvport");
        /* It's defined, but is it a valid string? */
        if (csp != NULL && parse_chansrvport(csp, NULL, 0, -1) == 0)
        {
            self->use_chansrv = 1;
        }
    }

    xrdp_mm_connect_sm(self);
}

/*****************************************************************************/
static void
xrdp_mm_connect_sm(struct xrdp_mm *self)
{
    int status = 0;

    /* we set self->mmcs_expecting_msg in the loop when we've send a
       message to sesman, and we need to wait for a response */
    self->mmcs_expecting_msg = 0;

    while (status == 0 && !self->mmcs_expecting_msg &&
            self->connect_state != MMCS_DONE)
    {
        switch (self->connect_state)
        {
            case MMCS_CONNECT_TO_SESMAN:
            {
                if (self->sesman_trans == NULL &&
                        (self->use_sesman || self->use_gw_login))
                {
                    /* Synchronous call */
                    status = xrdp_mm_sesman_connect(self);
                }
            }
            break;

            case MMCS_GATEWAY_LOGIN:
            {
                if (self->use_gw_login)
                {
                    const char *gw_username;
                    const char *gw_password;

                    gw_username = xrdp_mm_get_value(self, "pamusername");
                    gw_password = xrdp_mm_get_value(self, "pampassword");
                    if (!g_strcmp(gw_username, "same"))
                    {
                        gw_username = xrdp_mm_get_value(self, "username");
                    }

                    if (gw_password == NULL ||
                            !g_strcmp(gw_password, "same"))
                    {
                        gw_password = xrdp_mm_get_value(self, "password");
                    }

                    if (gw_username == NULL || gw_password == NULL)
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                        "Can't determine username and/or "
                                        "password for gateway login");
                        status = 1;
                    }
                    else
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                        "Performing access control for %s",
                                        gw_username);

                        status = xrdp_mm_send_sys_login_request(self,
                                                                gw_username,
                                                                gw_password);
                        if (status == 0)
                        {
                            /* Now waiting for a reply from sesman - see
                               xrdp_mm_process_login_response() */
                            self->mmcs_expecting_msg = 1;
                        }
                    }
                }
            }
            break;

            case MMCS_SESSION_LOGIN:
            {
                // Finished with the gateway login
                if (self->use_gw_login)
                {
                    xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                    "access control check was successful");
                    // No reply needed for this one
                    status = scp_send_logout_request(self->sesman_trans);
                    self->uid = -1;
                }

                if (status == 0 && self->use_sesman)
                {
                    const char *username;
                    const char *password;

                    username = xrdp_mm_get_value(self, "username");
                    password = xrdp_mm_get_value(self, "password");
                    if (username == NULL || username[0] == '\0')
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                        "No username is available");
                        status = 1;
                    }
                    else if (password == NULL)
                    {
                        /* Can't find a password definition at all - even
                         * an empty one */
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                        "No password field is available");
                        status = 1;
                    }
                    else
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                        "Performing login request for %s",
                                        username);
                        status = xrdp_mm_send_sys_login_request(self,
                                                                username,
                                                                password);
                        if (status == 0)
                        {
                            /* Now waiting for a reply from sesman - see
                               xrdp_mm_process_create_session_response() */
                            self->mmcs_expecting_msg = 1;
                        }
                    }
                }
            }
            break;

            case MMCS_CREATE_SESSION:
                if (self->use_sesman)
                {
                    xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                    "login was successful - creating session");
                    if ((status = xrdp_mm_create_session(self)) == 0)
                    {
                        /* Now waiting for a reply from sesman. Note that
                         * when it arrives, sesman is expecting us to
                         * close the connection - we can do nothing else
                         * with it */
                        self->mmcs_expecting_msg = 1;
                    }
                }
                break;

            case MMCS_CONNECT_TO_SESSION:
            {
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                "Connecting to session");
                /* This is synchronous - no reply message expected */
                status = xrdp_mm_user_session_connect(self);
            }
            break;

            case MMCS_CONNECT_TO_CHANSRV:
            {
                if (self->use_chansrv)
                {
                    char portbuff[XRDP_SOCKETS_MAXPATH];

                    xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                    "Connecting to chansrv");
                    if (self->use_sesman)
                    {
                        g_snprintf(portbuff, sizeof(portbuff),
                                   XRDP_CHANSRV_STR, self->uid, self->display);
                    }
                    else
                    {
                        const char *cp = xrdp_mm_get_value(self, "chansrvport");
                        portbuff[0] = '\0';
                        parse_chansrvport(cp, portbuff, sizeof(portbuff),
                                          self->uid);

                    }
                    xrdp_mm_update_allowed_channels(self);
                    xrdp_mm_chansrv_connect(self, portbuff);
                }
            }
            break;

            case MMCS_DONE:
            {
                /* Shouldn't get here */
                LOG(LOG_LEVEL_ERROR, "xrdp_mm_connect_sm: state machine error");
                status = 1;
            }
            break;
        }

        /* Move to the next state */
        if (self->connect_state < MMCS_DONE)
        {
            self->connect_state = (enum mm_connect_state)
                                  (self->connect_state + 1);
        }
    }

    if (!self->mmcs_expecting_msg)
    {
        /* We don't need the sesman transport anymore */
        if (self->sesman_trans != NULL)
        {
            self->delete_sesman_trans = 1;
        }
        xrdp_wm_mod_connect_done(self->wm, status);
        /* Make sure the module is cleaned up if we weren't successful */
        if (status != 0)
        {
            xrdp_mm_module_cleanup(self);
        }
    }
}

#define MIN_MS_BETWEEN_FRAMES 40
#define MIN_MS_TO_WAIT_FOR_MORE_UPDATES 0
/*****************************************************************************/
int
xrdp_mm_get_wait_objs(struct xrdp_mm *self,
                      tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    int rv = 0;

    if (self == 0)
    {
        return 0;
    }

    rv = 0;

    if (self->sesman_trans != 0 &&
            self->sesman_trans->status == TRANS_STATUS_UP)
    {
        trans_get_wait_objs(self->sesman_trans, read_objs, rcount);
    }

    if ((self->chan_trans != 0) && self->chan_trans->status == TRANS_STATUS_UP)
    {
        trans_get_wait_objs_rw(self->chan_trans, read_objs, rcount,
                               write_objs, wcount, timeout);
    }

    if (self->mod != 0)
    {
        if (self->mod->mod_get_wait_objs != 0)
        {
            rv = self->mod->mod_get_wait_objs(self->mod, read_objs, rcount,
                                              write_objs, wcount, timeout);
        }
    }

    if (self->encoder != 0)
    {
        read_objs[(*rcount)++] = self->encoder->xrdp_encoder_event_processed;
    }

    if (self->resize_queue != 0)
    {
        read_objs[(*rcount)++] = self->resize_ready;
    }

    if (self->wm->screen_dirty_region != NULL)
    {
        if (xrdp_region_not_empty(self->wm->screen_dirty_region))
        {
            int now = g_time3();
            int next_screen_draw_time = self->wm->last_screen_draw_time +
                                        MIN_MS_BETWEEN_FRAMES;
            int diff = next_screen_draw_time - now;
            int ltimeout = *timeout;
            diff = MAX(diff, MIN_MS_TO_WAIT_FOR_MORE_UPDATES);
            diff = MIN(diff, MIN_MS_BETWEEN_FRAMES);
            LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_get_wait_objs:"
                      " not empty diff %d", diff);
            if ((ltimeout < 0) || (ltimeout > diff))
            {
                *timeout = diff;
            }
        }
    }
    return rv;
}

#define DUMP_JPEG 0

#if DUMP_JPEG

/*****************************************************************************/
static int
xrdp_mm_dump_jpeg(struct xrdp_mm *self, XRDP_ENC_DATA_DONE *enc_done)
{
    static tbus ii;
    static int jj;
    struct _header
    {
        char tag[4];
        int width;
        int height;
        int bytes_follow;
    } header;
    tui16 *pheader_bytes;
    int cx;
    int cy;

    pheader_bytes = (tui16 *) (enc_done->comp_pad_data + enc_done->pad_bytes);

    cx = enc_done->enc->crects[enc_done->index * 4 + 2];
    cy = enc_done->enc->crects[enc_done->index * 4 + 3];

    header.tag[0] = 'B';
    header.tag[1] = 'E';
    header.tag[2] = 'E';
    header.tag[3] = 'F';
    header.width = cx;
    header.height = cy;
    header.bytes_follow = enc_done->comp_bytes - (2 + pheader_bytes[0]);
    if (ii == 0)
    {
        ii = g_file_open_rw("/tmp/jpeg.beef.bin");
        if (ii == -1)
        {
            ii = 0;
        }
    }
    if (ii != 0)
    {
        g_file_write(ii, (char *)&header, sizeof(header));
        g_file_write(ii, enc_done->comp_pad_data +
                     enc_done->pad_bytes + 2 + pheader_bytes[0],
                     enc_done->comp_bytes - (2 + pheader_bytes[0]));
        jj++;
        LOG(LOG_LEVEL_INFO, "dumping jpeg index %d", jj);
    }
    return 0;
}

#endif

/*****************************************************************************/
int
xrdp_mm_check_chan(struct xrdp_mm *self)
{
    LOG(LOG_LEVEL_TRACE, "xrdp_mm_check_chan:");
    if ((self->chan_trans != 0) && self->chan_trans->status == TRANS_STATUS_UP)
    {
        if (trans_check_wait_objs(self->chan_trans) != 0)
        {
            /* This is safe to do here, as we're not in a chansrv
             * transport callback */
            trans_delete(self->chan_trans);
            self->chan_trans = 0;
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_process_enc_done(struct xrdp_mm *self)
{
    XRDP_ENC_DATA *enc;
    XRDP_ENC_DATA_DONE *enc_done;
    int x;
    int y;
    int cx;
    int cy;
    int is_gfx;
    int got_frame_id;
    int client_ack;

    LOG(LOG_LEVEL_TRACE, "xrdp_mm_process_enc_done:");

    while (1)
    {
        tc_mutex_lock(self->encoder->mutex);
        enc_done = (XRDP_ENC_DATA_DONE *)
                   fifo_remove_item(self->encoder->fifo_processed);
        tc_mutex_unlock(self->encoder->mutex);
        if (enc_done == NULL)
        {
            break;
        }
        is_gfx = ENC_IS_BIT_SET(enc_done->flags, ENC_DONE_FLAGS_GFX_BIT);
        if (is_gfx)
        {
            got_frame_id = ENC_IS_BIT_SET(enc_done->flags,
                                          ENC_DONE_FLAGS_FRAME_ID_BIT);
            client_ack = self->encoder->gfx_ack_off == 0;
        }
        else
        {
            got_frame_id = 1;
            client_ack = self->wm->client_info->use_frame_acks;
        }
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_process_enc_done: message back "
                  "bytes %d", enc_done->comp_bytes);
        if (enc_done->comp_bytes > 0)
        {
            if (is_gfx)
            {
                xrdp_egfx_send_data(self->egfx,
                                    enc_done->comp_pad_data +
                                    enc_done->pad_bytes,
                                    enc_done->comp_bytes);
            }
            else
            {
                x = enc_done->x;
                y = enc_done->y;
                cx = enc_done->cx;
                cy = enc_done->cy;
                if (!enc_done->continuation)
                {
                    libxrdp_fastpath_send_frame_marker(self->wm->session, 0,
                                                       enc_done->frame_id);
                }
                libxrdp_fastpath_send_surface(self->wm->session,
                                              enc_done->comp_pad_data,
                                              enc_done->pad_bytes,
                                              enc_done->comp_bytes,
                                              x, y, x + cx, y + cy,
                                              32, self->encoder->codec_id,
                                              cx, cy);
                if (enc_done->last)
                {
                    libxrdp_fastpath_send_frame_marker(self->wm->session, 1,
                                                       enc_done->frame_id);
                }
            }
        }
        /* free enc_done */
        if (enc_done->last)
        {
            enc = enc_done->enc;
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_process_enc_done: last set");
            if (got_frame_id)
            {
                if (client_ack)
                {
                    self->encoder->frame_id_server = enc_done->frame_id;
                    xrdp_mm_update_module_frame_ack(self);
                }
                else
                {
                    self->mod->mod_frame_ack(self->mod, 0,
                                             enc_done->frame_id);
                }
            }
            if (is_gfx)
            {
                g_free(enc->u.gfx.cmd);
            }
            else
            {
                g_free(enc->u.sc.drects);
                g_free(enc->u.sc.crects);
            }
            if (enc->shmem_ptr != NULL)
            {
                g_munmap(enc->shmem_ptr, enc->shmem_bytes);
            }
            g_free(enc);
        }
        g_free(enc_done->comp_pad_data);
        g_free(enc_done);
    }
    return 0;
}

/*****************************************************************************/
void
xrdp_mm_efgx_add_dirty_region_to_planar_list(struct xrdp_mm *self,
        struct xrdp_region *dirty_region)
{
    int jndex = 0;
    struct xrdp_rect rect;

    int error = xrdp_region_get_rect(dirty_region, jndex, &rect);
    if (error == 0)
    {
        if (self->wm->screen_dirty_region == NULL)
        {
            self->wm->screen_dirty_region = xrdp_region_create(self->wm);
        }

        do
        {
            xrdp_region_add_rect(self->wm->screen_dirty_region, &rect);
            jndex++;
            error = xrdp_region_get_rect(dirty_region, jndex, &rect);
        }
        while (error == 0);

        if (self->mod_handle != 0)
        {
            // Module has been writing to WM screen using GFX
            self->mod_uses_wm_screen_for_gfx = 1;
        }
    }

}

/*****************************************************************************/
static int
xrdp_mm_draw_dirty(struct xrdp_mm *self)
{
    struct xrdp_rect rect;
    struct xrdp_rect mon_rect;
    struct xrdp_region *mon_reg;
    int error;
    int index;
    int jndex;
    int count;
    int surface_id;
    struct monitor_info *mi;
    int rv = 0;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_draw_dirty:");
    count = self->wm->client_info->display_sizes.monitorCount;
    if (count < 1)
    {
        error = xrdp_region_get_bounds(self->wm->screen_dirty_region, &rect);
        if (error == 0)
        {
            rv = xrdp_mm_egfx_send_planar_bitmap(self,
                                                 self->wm->screen, &rect,
                                                 self->egfx->surface_id, 0, 0);
        }
    }
    else
    {
        for (index = 0; index < count; index++)
        {
            /* make a copy of screen_dirty_region */
            mon_reg = xrdp_region_create(self->wm);
            if (mon_reg == NULL)
            {
                return 1;
            }
            jndex = 0;
            while (xrdp_region_get_rect(self->wm->screen_dirty_region,
                                        jndex, &mon_rect) == 0)
            {
                LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_mm_draw_dirty: jndex %d "
                          "mon_rect %d %d %d %d",
                          jndex, mon_rect.left, mon_rect.top,
                          mon_rect.right, mon_rect.bottom);
                xrdp_region_add_rect(mon_reg, &mon_rect);
                jndex++;
            }
            /* intercect monitor */
            mi = self->wm->client_info->display_sizes.minfo_wm + index;
            mon_rect.left = mi->left;
            mon_rect.top = mi->top;
            mon_rect.right = mi->right + 1;
            mon_rect.bottom = mi->bottom + 1;
            xrdp_region_intersect_rect(mon_reg, &mon_rect);
            if (xrdp_region_not_empty(mon_reg))
            {
                error = xrdp_region_get_bounds(mon_reg, &rect);
                if (error == 0)
                {
                    surface_id = index;
                    rv = xrdp_mm_egfx_send_planar_bitmap(self,
                                                         self->wm->screen,
                                                         &rect,
                                                         surface_id,
                                                         mi->left, mi->top);
                }
            }
            xrdp_region_delete(mon_reg);
        }
    }
    return rv;
}

/*****************************************************************************/
int
xrdp_mm_check_wait_objs(struct xrdp_mm *self)
{
    int rv;

    if (self == 0)
    {
        return 0;
    }

    rv = 0;

    if (self->sesman_trans != NULL &&
            !self->delete_sesman_trans &&
            self->sesman_trans->status == TRANS_STATUS_UP)
    {
        if (trans_check_wait_objs(self->sesman_trans) != 0)
        {
            if (self->mmcs_expecting_msg)
            {
                /* The sesman transport has failed with an
                 * outstanding message */
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                "Unexpected sesman failure - check sesman log");
                xrdp_wm_mod_connect_done(self->wm, 1);
            }
            self->delete_sesman_trans = 1;
            if (self->wm->hide_log_window)
            {
                /* if hide_log_window, this is fatal */
                rv = 1;
            }
        }
    }
    if (self->delete_sesman_trans)
    {
        trans_delete(self->sesman_trans);
        self->sesman_trans = NULL;
    }

    if (self->chan_trans != NULL &&
            self->chan_trans->status == TRANS_STATUS_UP)
    {
        if (trans_check_wait_objs(self->chan_trans) != 0)
        {
            /* This is safe to do here, as we're not in a chansrv
             * transport callback */
            trans_delete(self->chan_trans);
            self->chan_trans = NULL;
        }
    }

    if (self->mod != NULL)
    {
        if (self->mod->mod_check_wait_objs != NULL)
        {
            rv = self->mod->mod_check_wait_objs(self->mod);
        }
    }

    if (g_is_wait_obj_set(self->resize_ready))
    {
        g_reset_wait_obj(self->resize_ready);
        dynamic_monitor_process_queue(self);
    }

    if (self->encoder != NULL)
    {
        if (g_is_wait_obj_set(self->encoder->xrdp_encoder_event_processed))
        {
            g_reset_wait_obj(self->encoder->xrdp_encoder_event_processed);
            xrdp_mm_process_enc_done(self);
        }
    }

    if (self->wm->screen_dirty_region != NULL)
    {
        if (xrdp_region_not_empty(self->wm->screen_dirty_region))
        {
            int now = g_time3();
            int diff = now - self->wm->last_screen_draw_time;
            LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_check_wait_objs: not empty diff %d", diff);
            if ((diff < 0) || (diff >= 40))
            {
                if (self->egfx_up)
                {
                    rv = xrdp_mm_draw_dirty(self);
                    xrdp_region_delete(self->wm->screen_dirty_region);
                    self->wm->screen_dirty_region = NULL;
                    self->wm->last_screen_draw_time = now;
                }
                else
                {
                    LOG(LOG_LEVEL_TRACE, "xrdp_mm_check_wait_objs: egfx is not up");
                }
            }
        }
    }

    return rv;
}

/*****************************************************************************/
/* frame ack from client */
int
xrdp_mm_frame_ack(struct xrdp_mm *self, int frame_id)
{
    struct xrdp_encoder *encoder;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_frame_ack:");
    if (self->wm->client_info->use_frame_acks == 0)
    {
        return 1;
    }
    encoder = self->encoder;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_frame_ack: "
              "incoming %d, client %d, server %d", frame_id,
              encoder->frame_id_client, encoder->frame_id_server);
    if ((frame_id < 0) || (frame_id > encoder->frame_id_server))
    {
        /* if frame_id is negative or bigger then what server last sent
           just ack all sent frames */
        /* some clients can send big number just to clear all
           pending frames */
        encoder->frame_id_client = encoder->frame_id_server;
    }
    else
    {
        /* frame acks can come out of order so ignore older one */
        encoder->frame_id_client = MAX(frame_id, encoder->frame_id_client);
    }
    xrdp_mm_update_module_frame_ack(self);
    return 0;
}

#if 0
/*****************************************************************************/
struct xrdp_painter *
get_painter(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        wm = (struct xrdp_wm *)(mod->wm);
        p = xrdp_painter_create(wm, wm->session);
        mod->painter = (tintptr)p;
    }

    return p;
}
#endif

/*****************************************************************************/
int
server_begin_update(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    wm = (struct xrdp_wm *)(mod->wm);
    p = xrdp_painter_create(wm, wm->session);
    xrdp_painter_begin_update(p);
    mod->painter = (long)p;
    return 0;
}

/*****************************************************************************/
int
server_end_update(struct xrdp_mod *mod)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    xrdp_painter_end_update(p);
    xrdp_painter_delete(p);
    mod->painter = 0;
    return 0;
}

/*****************************************************************************/
/* got bell signal... try to send to client */
int
server_bell_trigger(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_send_bell(wm);
    return 0;
}

/*****************************************************************************/
/* Chansrv in use on this configuration? */
int
server_chansrv_in_use(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return wm->mm->use_chansrv;
}


/*****************************************************************************/
int
server_fill_rect(struct xrdp_mod *mod, int x, int y, int cx, int cy)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_painter_fill_rect(p, wm->target_surface, x, y, cx, cy);
    return 0;
}

/*****************************************************************************/
int
server_screen_blt(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    p->rop = 0xcc;
    xrdp_painter_copy(p, wm->screen, wm->target_surface, x, y, cx, cy, srcx, srcy);
    return 0;
}

/*****************************************************************************/
int
server_paint_rect(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                  char *data, int width, int height, int srcx, int srcy)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *b;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    b = xrdp_bitmap_create_with_data(width, height, wm->screen->bpp, data, wm);
    xrdp_painter_copy(p, b, wm->target_surface, x, y, cx, cy, srcx, srcy);
    xrdp_bitmap_delete(b);
    return 0;
}

/*****************************************************************************/
int
server_paint_rect_bpp(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                      char *data, int width, int height, int srcx, int srcy,
                      int bpp)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *b;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);
    if (p == 0)
    {
        return 0;
    }
    wm = (struct xrdp_wm *)(mod->wm);
    b = xrdp_bitmap_create_with_data(width, height, bpp, data, wm);
    xrdp_painter_copy(p, b, wm->target_surface, x, y, cx, cy, srcx, srcy);
    xrdp_bitmap_delete(b);
    return 0;
}

/*****************************************************************************/
int
server_composite(struct xrdp_mod *mod, int srcidx, int srcformat,
                 int srcwidth, int srcrepeat, int *srctransform,
                 int mskflags, int mskidx, int mskformat, int mskwidth,
                 int mskrepeat, int op, int srcx, int srcy,
                 int mskx, int msky, int dstx, int dsty,
                 int width, int height, int dstformat)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *b;
    struct xrdp_bitmap *msk;
    struct xrdp_painter *p;
    struct xrdp_os_bitmap_item *bi;

    p = (struct xrdp_painter *)(mod->painter);
    if (p == 0)
    {
        return 0;
    }
    wm = (struct xrdp_wm *)(mod->wm);
    b = 0;
    msk = 0;
    bi = xrdp_cache_get_os_bitmap(wm->cache, srcidx);
    if (bi != 0)
    {
        b = bi->bitmap;
    }
    if (mskflags & 1)
    {
        bi = xrdp_cache_get_os_bitmap(wm->cache, mskidx);
        if (bi != 0)
        {
            msk = bi->bitmap;
        }
    }
    if (b != 0)
    {
        xrdp_painter_composite(p, b, srcformat, srcwidth, srcrepeat,
                               wm->target_surface, srctransform,
                               mskflags, msk, mskformat, mskwidth, mskrepeat,
                               op, srcx, srcy, mskx, msky, dstx, dsty,
                               width, height, dstformat);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "server_composite: error finding id %d or %d", srcidx, mskidx);
    }
    return 0;
}

/*****************************************************************************/
int
server_paint_rects(struct xrdp_mod *mod, int num_drects, short *drects,
                   int num_crects, short *crects, char *data, int width,
                   int height, int flags, int frame_id)
{
    return server_paint_rects_ex(mod, num_drects, drects, num_crects, crects,
                                 data, 0, 0, width, height, flags, frame_id,
                                 NULL, 0);
}

/*****************************************************************************/
int
server_paint_rects_ex(struct xrdp_mod *mod,
                      int num_drects, short *drects,
                      int num_crects, short *crects,
                      char *data, int left, int top,
                      int width, int height,
                      int flags, int frame_id,
                      void *shmem_ptr, int shmem_bytes)
{
    struct xrdp_wm *wm;
    struct xrdp_mm *mm;
    struct xrdp_painter *p;
    struct xrdp_bitmap *b;
    short *s;
    int index;
    XRDP_ENC_DATA *enc_data;

    wm = (struct xrdp_wm *)(mod->wm);
    mm = wm->mm;

    LOG(LOG_LEVEL_TRACE, "server_paint_rects_ex: %p", mm->encoder);

    if (mm->encoder != 0)
    {
        /* copy formal params to XRDP_ENC_DATA */
        enc_data = (XRDP_ENC_DATA *) g_malloc(sizeof(XRDP_ENC_DATA), 1);
        if (enc_data == 0)
        {
            if (shmem_ptr != NULL)
            {
                g_munmap(shmem_ptr, shmem_bytes);
            }
            return 1;
        }

        enc_data->u.sc.drects = (short *)
                                g_malloc(sizeof(short) * num_drects * 4, 0);
        if (enc_data->u.sc.drects == 0)
        {
            if (shmem_ptr != NULL)
            {
                g_munmap(shmem_ptr, shmem_bytes);
            }
            g_free(enc_data);
            return 1;
        }

        enc_data->u.sc.crects = (short *)
                                g_malloc(sizeof(short) * num_crects * 4, 0);
        if (enc_data->u.sc.crects == 0)
        {
            if (shmem_ptr != NULL)
            {
                g_munmap(shmem_ptr, shmem_bytes);
            }
            g_free(enc_data->u.sc.drects);
            g_free(enc_data);
            return 1;
        }

        g_memcpy(enc_data->u.sc.drects, drects, sizeof(short) * num_drects * 4);
        g_memcpy(enc_data->u.sc.crects, crects, sizeof(short) * num_crects * 4);

        enc_data->mod = mod;
        enc_data->u.sc.num_drects = num_drects;
        enc_data->u.sc.num_crects = num_crects;
        enc_data->u.sc.data = data;
        enc_data->u.sc.left = left;
        enc_data->u.sc.top = top;
        enc_data->u.sc.width = width;
        enc_data->u.sc.height = height;
        enc_data->u.sc.flags = flags;
        enc_data->u.sc.frame_id = frame_id;
        enc_data->shmem_ptr = shmem_ptr;
        enc_data->shmem_bytes = shmem_bytes;
        if (width == 0 || height == 0)
        {
            LOG_DEVEL(LOG_LEVEL_WARNING, "server_paint_rects: error");
        }

        /* insert into fifo for encoder thread to process */
        tc_mutex_lock(mm->encoder->mutex);
        fifo_add_item(mm->encoder->fifo_to_proc, (void *) enc_data);
        tc_mutex_unlock(mm->encoder->mutex);

        /* signal xrdp_encoder thread */
        g_set_wait_obj(mm->encoder->xrdp_encoder_event_to_proc);

        return 0;
    }

    if (wm->client_info->gfx)
    {
        LOG(LOG_LEVEL_DEBUG, "server_paint_rects: gfx session and no encoder");
        mm->mod->mod_frame_ack(mm->mod, flags, frame_id);
        return 0;
    }

    p = (struct xrdp_painter *)(mod->painter);
    if (p == 0)
    {
        return 0;
    }
    b = xrdp_bitmap_create_with_data(width, height, wm->screen->bpp,
                                     data, wm);
    s = crects;
    for (index = 0; index < num_crects; index++)
    {
        xrdp_painter_copy(p, b, wm->target_surface, s[0], s[1], s[2], s[3],
                          s[0], s[1]);
        s += 4;
    }
    xrdp_bitmap_delete(b);
    mm->mod->mod_frame_ack(mm->mod, flags, frame_id);
    if (shmem_ptr != NULL)
    {
        g_munmap(shmem_ptr, shmem_bytes);
    }
    return 0;
}

/*****************************************************************************/
int
server_session_info(struct xrdp_mod *mod, const char *data, int data_bytes)
{
    struct xrdp_wm *wm;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "server_session_info:");
    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_send_session_info(wm->session, data, data_bytes);
}

/*****************************************************************************/
int
server_egfx_cmd(struct xrdp_mod *mod,
                char *cmd, int cmd_bytes,
                char *data, int data_bytes)
{
    XRDP_ENC_DATA *enc;
    struct xrdp_wm *wm;
    struct xrdp_mm *mm;

    wm = (struct xrdp_wm *)(mod->wm);
    mm = wm->mm;
    if (mm->encoder == NULL)
    {
        // This can happen when we are in the resize state machine, if
        // there are messages queued up by the X server
        if (data != NULL)
        {
            g_munmap(data, data_bytes);
        }
        return 0;
    }
    enc = g_new0(struct xrdp_enc_data, 1);
    if (enc == NULL)
    {
        if (data != NULL)
        {
            g_munmap(data, data_bytes);
        }
        return 1;
    }
    ENC_SET_BIT(enc->flags, ENC_FLAGS_GFX_BIT);
    enc->u.gfx.cmd = g_new(char, cmd_bytes);
    if (enc->u.gfx.cmd == NULL)
    {
        if (data != NULL)
        {
            g_munmap(data, data_bytes);
        }
        g_free(enc);
        return 1;
    }
    g_memcpy(enc->u.gfx.cmd, cmd, cmd_bytes);
    enc->u.gfx.cmd_bytes = cmd_bytes;
    enc->u.gfx.data = data;
    enc->u.gfx.data_bytes = data_bytes;
    enc->shmem_ptr = data;
    enc->shmem_bytes = data_bytes;
    /* insert into fifo for encoder thread to process */
    tc_mutex_lock(mm->encoder->mutex);
    fifo_add_item(mm->encoder->fifo_to_proc, enc);
    tc_mutex_unlock(mm->encoder->mutex);
    /* signal xrdp_encoder thread */
    g_set_wait_obj(mm->encoder->xrdp_encoder_event_to_proc);
    return 0;
}

/*****************************************************************************/
int
server_set_pointer(struct xrdp_mod *mod, int x, int y,
                   char *data, char *mask)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_pointer(wm, data, mask, x, y, 0, 32, 32);
    return 0;
}

/*****************************************************************************/
int
server_set_pointer_ex(struct xrdp_mod *mod, int x, int y,
                      char *data, char *mask, int bpp)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_pointer(wm, data, mask, x, y, bpp, 32, 32);
    return 0;
}

/*****************************************************************************/
int
server_set_pointer_large(struct xrdp_mod *mod, int x, int y,
                         char *data, char *mask, int bpp,
                         int width, int height)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_pointer(wm, data, mask, x, y, bpp, width, height);
    return 0;
}

/*****************************************************************************/
int
server_palette(struct xrdp_mod *mod, int *palette)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (g_memcmp(wm->palette, palette, 255 * sizeof(int)) != 0)
    {
        g_memcpy(wm->palette, palette, 256 * sizeof(int));
        xrdp_wm_send_palette(wm);
    }

    return 0;
}

/*****************************************************************************/
int
server_msg(struct xrdp_mod *mod, const char *msg, int code)
{
    struct xrdp_wm *wm;

    if (code == 1)
    {
        LOG(LOG_LEVEL_INFO, "%s", msg);
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    return xrdp_wm_log_msg(wm, LOG_LEVEL_DEBUG, "%s", msg);
}

/*****************************************************************************/
int
server_set_clip(struct xrdp_mod *mod, int x, int y, int cx, int cy)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    return xrdp_painter_set_clip(p, x, y, cx, cy);
}

/*****************************************************************************/
int
server_reset_clip(struct xrdp_mod *mod)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    return xrdp_painter_clr_clip(p);
}

/*****************************************************************************/
int
server_set_fgcolor(struct xrdp_mod *mod, int fgcolor)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->fg_color = fgcolor;
    p->pen.color = p->fg_color;
    return 0;
}

/*****************************************************************************/
int
server_set_bgcolor(struct xrdp_mod *mod, int bgcolor)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->bg_color = bgcolor;
    return 0;
}

/*****************************************************************************/
int
server_set_opcode(struct xrdp_mod *mod, int opcode)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->rop = opcode;
    return 0;
}

/*****************************************************************************/
int
server_set_mixmode(struct xrdp_mod *mod, int mixmode)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->mix_mode = mixmode;
    return 0;
}

/*****************************************************************************/
int
server_set_brush(struct xrdp_mod *mod, int x_origin, int y_origin,
                 int style, char *pattern)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->brush.x_origin = x_origin;
    p->brush.y_origin = y_origin;
    p->brush.style = style;
    g_memcpy(p->brush.pattern, pattern, 8);
    return 0;
}

/*****************************************************************************/
int
server_set_pen(struct xrdp_mod *mod, int style, int width)
{
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    p->pen.style = style;
    p->pen.width = width;
    return 0;
}

/*****************************************************************************/
int
server_draw_line(struct xrdp_mod *mod, int x1, int y1, int x2, int y2)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    return xrdp_painter_line(p, wm->target_surface, x1, y1, x2, y2);
}

/*****************************************************************************/
int
server_add_char(struct xrdp_mod *mod, int font, int character,
                int offset, int baseline,
                int width, int height, char *data)
{
    struct xrdp_font_char fi;

    fi.offset = offset;
    fi.baseline = baseline;
    fi.width = width;
    fi.height = height;
    fi.incby = 0;
    fi.data = data;
    fi.bpp = 1;
    return libxrdp_orders_send_font(((struct xrdp_wm *)mod->wm)->session,
                                    &fi, font, character);
}

/*****************************************************************************/
int
server_draw_text(struct xrdp_mod *mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char *data, int data_len)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    return xrdp_painter_draw_text2(p, wm->target_surface, font, flags,
                                   mixmode, clip_left, clip_top,
                                   clip_right, clip_bottom,
                                   box_left, box_top,
                                   box_right, box_bottom,
                                   x, y, data, data_len);
}

/*****************************************************************************/
int
client_monitor_resize(struct xrdp_mod *mod, int width, int height,
                      int num_monitors, const struct monitor_info *monitors)
{
    int error = 0;
    struct xrdp_wm *wm;
    struct display_size_description *display_size_data;

    LOG_DEVEL(LOG_LEVEL_TRACE, "client_monitor_resize:");
    wm = (struct xrdp_wm *)(mod->wm);
    if (wm == 0 || wm->mm == 0 || wm->client_info == 0)
    {
        return 1;
    }

    if (wm->client_info->client_resize_mode == CRMODE_NONE)
    {
        LOG(LOG_LEVEL_WARNING, "Server is not allowed to resize this client");
        return 1;
    }

    if (wm->client_info->client_resize_mode == CRMODE_SINGLE_SCREEN &&
            num_monitors > 1)
    {
        LOG(LOG_LEVEL_WARNING,
            "Server cannot resize this client with multiple monitors");
        return 1;
    }

    display_size_data = g_new0(struct display_size_description, 1);
    if (display_size_data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "client_monitor_resize: Out of memory");
        return 1;
    }
    error = libxrdp_init_display_size_description(num_monitors,
            monitors,
            display_size_data);
    if (error)
    {
        LOG(LOG_LEVEL_ERROR, "client_monitor_resize:"
            " libxrdp_init_display_size_description"
            " failed with error %d.", error);
        free(display_size_data);
        return error;
    }
    list_add_item(wm->mm->resize_queue, (tintptr)display_size_data);
    g_set_wait_obj(wm->mm->resize_ready);

    return 0;
}

/*****************************************************************************/

/* Note : if this is called on a multimon setup, the client is resized
 * to a single monitor */
int
server_monitor_resize_done(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;
    struct xrdp_mm *mm;

    LOG(LOG_LEVEL_TRACE, "server_monitor_resize_done:");

    wm = (struct xrdp_wm *)(mod->wm);
    if (wm == 0)
    {
        return 1;
    }
    mm = wm->mm;
    if (mm == 0)
    {
        return 1;
    }

    if (wm->client_info == 0)
    {
        return 1;
    }

    if (mm->resize_data != NULL
            && mm->resize_data->state
            == WMRZ_SERVER_MONITOR_MESSAGE_PROCESSING)
    {
        LOG(LOG_LEVEL_INFO,
            "server_monitor_resize_done: Advancing server monitor resized.");
        advance_resize_state_machine(
            mm, WMRZ_SERVER_MONITOR_MESSAGE_PROCESSED);
    }
    return 0;
}

/*****************************************************************************/
/*return -1 if channels are controlled by chansrv */
int
server_get_channel_count(struct xrdp_mod *mod)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->mm->use_chansrv)
    {
        return -1;
    }

    return libxrdp_get_channel_count(wm->session);
}


/*****************************************************************************/
/*return 0 if the index is not found*/
int
server_query_channel(struct xrdp_mod *mod, int index, char *channel_name,
                     int *channel_flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->mm->use_chansrv)
    {
        return 1;
    }

    return libxrdp_query_channel(wm->session, index, channel_name,
                                 channel_flags);
}

/*****************************************************************************/
/* returns -1 on error */
int
server_get_channel_id(struct xrdp_mod *mod, const char *name)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->mm->use_chansrv)
    {
        return -1;
    }

    return libxrdp_get_channel_id(wm->session, name);
}

/*****************************************************************************/
int
server_send_to_channel(struct xrdp_mod *mod, int channel_id,
                       char *data, int data_len,
                       int total_data_len, int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->mm->use_chansrv)
    {
        /* Modules should not be calling this if chansrv is running -
         * they can use server_chansrv_in_use() to avoid doing this */
        LOG_DEVEL(LOG_LEVEL_ERROR,
                  "Bad call of server_send_to_channel() detected");
        return 1;
    }

    return libxrdp_send_to_channel(wm->session, channel_id, data, data_len,
                                   total_data_len, flags);
}

/*****************************************************************************/
int
server_create_os_surface(struct xrdp_mod *mod, int rdpindex,
                         int width, int height)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *bitmap;
    int error;

    wm = (struct xrdp_wm *)(mod->wm);
    bitmap = xrdp_bitmap_create(width, height, wm->screen->bpp,
                                WND_TYPE_OFFSCREEN, wm);
    error = xrdp_cache_add_os_bitmap(wm->cache, bitmap, rdpindex);

    if (error != 0)
    {
        LOG(LOG_LEVEL_ERROR, "server_create_os_surface: xrdp_cache_add_os_bitmap failed");
        return 1;
    }

    bitmap->item_index = rdpindex;
    bitmap->id = rdpindex;
    return 0;
}

/*****************************************************************************/
int
server_create_os_surface_bpp(struct xrdp_mod *mod, int rdpindex,
                             int width, int height, int bpp)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *bitmap;
    int error;

    wm = (struct xrdp_wm *)(mod->wm);
    bitmap = xrdp_bitmap_create(width, height, bpp,
                                WND_TYPE_OFFSCREEN, wm);
    error = xrdp_cache_add_os_bitmap(wm->cache, bitmap, rdpindex);
    if (error != 0)
    {
        LOG(LOG_LEVEL_ERROR, "server_create_os_surface_bpp: xrdp_cache_add_os_bitmap failed");
        return 1;
    }
    bitmap->item_index = rdpindex;
    bitmap->id = rdpindex;
    return 0;
}

/*****************************************************************************/
int
server_switch_os_surface(struct xrdp_mod *mod, int rdpindex)
{
    struct xrdp_wm *wm;
    struct xrdp_os_bitmap_item *bi;
    struct xrdp_painter *p;

    LOG(LOG_LEVEL_DEBUG, "server_switch_os_surface: id 0x%x", rdpindex);
    wm = (struct xrdp_wm *)(mod->wm);

    if (rdpindex == -1)
    {
        LOG(LOG_LEVEL_DEBUG, "server_switch_os_surface: setting target_surface to screen");
        wm->target_surface = wm->screen;
        p = (struct xrdp_painter *)(mod->painter);

        if (p != 0)
        {
            LOG(LOG_LEVEL_DEBUG, "setting target");
            wm_painter_set_target(p);
        }

        return 0;
    }

    bi = xrdp_cache_get_os_bitmap(wm->cache, rdpindex);

    if ((bi != 0) && (bi->bitmap != 0))
    {
        LOG(LOG_LEVEL_DEBUG, "server_switch_os_surface: setting target_surface to rdpid %d", rdpindex);
        wm->target_surface = bi->bitmap;
        p = (struct xrdp_painter *)(mod->painter);

        if (p != 0)
        {
            LOG(LOG_LEVEL_DEBUG, "setting target");
            wm_painter_set_target(p);
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "server_switch_os_surface: error finding id %d", rdpindex);
    }

    return 0;
}

/*****************************************************************************/
int
server_delete_os_surface(struct xrdp_mod *mod, int rdpindex)
{
    struct xrdp_wm *wm;
    struct xrdp_painter *p;

    LOG(LOG_LEVEL_DEBUG, "server_delete_os_surface: id 0x%x", rdpindex);
    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->target_surface->type == WND_TYPE_OFFSCREEN)
    {
        if (wm->target_surface->id == rdpindex)
        {
            LOG(LOG_LEVEL_DEBUG, "server_delete_os_surface: setting target_surface to screen");
            wm->target_surface = wm->screen;
            p = (struct xrdp_painter *)(mod->painter);

            if (p != 0)
            {
                LOG(LOG_LEVEL_DEBUG, "setting target");
                wm_painter_set_target(p);
            }
        }
    }

    xrdp_cache_remove_os_bitmap(wm->cache, rdpindex);
    return 0;
}

/*****************************************************************************/
int
server_paint_rect_os(struct xrdp_mod *mod, int x, int y, int cx, int cy,
                     int rdpindex, int srcx, int srcy)
{
    struct xrdp_wm *wm;
    struct xrdp_bitmap *b;
    struct xrdp_painter *p;
    struct xrdp_os_bitmap_item *bi;

    p = (struct xrdp_painter *)(mod->painter);

    if (p == 0)
    {
        return 0;
    }

    wm = (struct xrdp_wm *)(mod->wm);
    bi = xrdp_cache_get_os_bitmap(wm->cache, rdpindex);

    if (bi != 0)
    {
        b = bi->bitmap;
        xrdp_painter_copy(p, b, wm->target_surface, x, y, cx, cy, srcx, srcy);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "server_paint_rect_os: error finding id %d", rdpindex);
    }

    return 0;
}

/*****************************************************************************/
int
server_set_hints(struct xrdp_mod *mod, int hints, int mask)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (mask & 1)
    {
        if (hints & 1)
        {
            wm->hints |= 1;
        }
        else
        {
            wm->hints &= ~1;
        }
    }

    return 0;
}

/*****************************************************************************/
int
server_window_new_update(struct xrdp_mod *mod, int window_id,
                         struct rail_window_state_order *window_state,
                         int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_window_new_update(wm->session, window_id,
                                     window_state, flags);
}

/*****************************************************************************/
int
server_window_delete(struct xrdp_mod *mod, int window_id)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_window_delete(wm->session, window_id);
}

/*****************************************************************************/
int
server_window_icon(struct xrdp_mod *mod, int window_id, int cache_entry,
                   int cache_id, struct rail_icon_info *icon_info,
                   int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_window_icon(wm->session, window_id, cache_entry, cache_id,
                               icon_info, flags);
}

/*****************************************************************************/
int
server_window_cached_icon(struct xrdp_mod *mod,
                          int window_id, int cache_entry,
                          int cache_id, int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_window_cached_icon(wm->session, window_id, cache_entry,
                                      cache_id, flags);
}

/*****************************************************************************/
int
server_notify_new_update(struct xrdp_mod *mod,
                         int window_id, int notify_id,
                         struct rail_notify_state_order *notify_state,
                         int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_notify_new_update(wm->session, window_id, notify_id,
                                     notify_state, flags);
}

/*****************************************************************************/
int
server_notify_delete(struct xrdp_mod *mod, int window_id,
                     int notify_id)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_notify_delete(wm->session, window_id, notify_id);
}

/*****************************************************************************/
int
server_monitored_desktop(struct xrdp_mod *mod,
                         struct rail_monitored_desktop_order *mdo,
                         int flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    return libxrdp_monitored_desktop(wm->session, mdo, flags);
}

/*****************************************************************************/
int
server_add_char_alpha(struct xrdp_mod *mod, int font, int character,
                      int offset, int baseline,
                      int width, int height, char *data)
{
    struct xrdp_font_char fi;

    fi.offset = offset;
    fi.baseline = baseline;
    fi.width = width;
    fi.height = height;
    fi.incby = 0;
    fi.data = data;
    fi.bpp = 8;
    return libxrdp_orders_send_font(((struct xrdp_wm *)mod->wm)->session,
                                    &fi, font, character);
}
