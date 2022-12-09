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

#include "libscp_connection.h"

#ifdef USE_PAM
#if defined(HAVE__PAM_TYPES_H)
#define LINUXPAM 1
#include <security/_pam_types.h>
#elif defined(HAVE_PAM_CONSTANTS_H)
#define OPENPAM 1
#include <security/pam_constants.h>
#endif
#endif /* USE_PAM */
#include <ctype.h>

#include "xrdp_encoder.h"
#include "xrdp_sockets.h"


/* Forward declarations */
static const char *
getPAMError(const int pamError, char *text, int text_bytes);
static const char *
getPAMAdditionalErrorInfo(const int pamError, struct xrdp_mm *self);
static int
xrdp_mm_chansrv_connect(struct xrdp_mm *self, const char *ip, const char *port);
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

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_mm_create: bpp %d mcs_connection_type %d "
              "jpeg_codec_id %d v3_codec_id %d rfx_codec_id %d "
              "h264_codec_id %d",
              self->wm->client_info->bpp,
              self->wm->client_info->mcs_connection_type,
              self->wm->client_info->jpeg_codec_id,
              self->wm->client_info->v3_codec_id,
              self->wm->client_info->rfx_codec_id,
              self->wm->client_info->h264_codec_id);

    self->encoder = xrdp_encoder_create(self);

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
    trans_delete(self->pam_auth_trans);
    self->pam_auth_trans = 0;
    list_delete(self->login_names);
    list_delete(self->login_values);
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
xrdp_mm_send_gateway_login(struct xrdp_mm *self, const char *username,
                           const char *password)
{
    int rv = 0;
    enum SCP_CLIENT_STATES_E e;

    xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                    "sending login info to session manager, please wait...");

    e = scp_v0c_gateway_request(self->pam_auth_trans, username, password);

    if (e != SCP_CLIENT_STATE_OK)
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_WARNING,
                        "Error sending gateway login request to sesman [%s]",
                        scp_client_state_to_str(e));
        rv = 1;
    }

    return rv;
}

/*****************************************************************************/
/* Send login information to sesman */
static int
xrdp_mm_send_login(struct xrdp_mm *self)
{
    enum SCP_CLIENT_STATES_E e;
    int rv = 0;
    int xserverbpp;
    const char *username;
    const char *password;

    username = xrdp_mm_get_value(self, "username");
    password = xrdp_mm_get_value(self, "password");
    if (username == NULL || username[0] == '\0')
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR, "No username is available");
        rv = 1;
    }
    else if (password == NULL)
    {
        /* Can't find a password definition at all - even an empty one */
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "No password field is available");
        rv = 1;
    }
    else
    {
        const char *domain;

        /* this code is either 0 for Xvnc, 10 for X11rdp or 20 for Xorg */
        self->code = xrdp_mm_get_value_int(self, "code", 0);

        xserverbpp = xrdp_mm_get_value_int(self, "xserverbpp",
                                           self->wm->screen->bpp);

        domain = self->wm->client_info->domain;
        /* Don't send domains starting with '_' - see
         * xrdp_login_wnd.c:xrdp_wm_parse_domain_information()
         */
        if (domain[0] == '_')
        {
            domain = "";
        }

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                        "sending login info to session manager. "
                        "Please wait...");
        e = scp_v0c_create_session_request(self->sesman_trans,
                                           username,
                                           password,
                                           self->code,
                                           self->wm->screen->width,
                                           self->wm->screen->height,
                                           xserverbpp,
                                           domain,
                                           self->wm->client_info->program,
                                           self->wm->client_info->directory,
                                           self->wm->client_info->connection_description);

        if (e != SCP_CLIENT_STATE_OK)
        {
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_WARNING,
                            "Error sending create session to sesman [%s]",
                            scp_client_state_to_str(e));
            rv = 1;
        }
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
            self->mod->server_is_term = server_is_term;
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
            self->mod->server_reset = server_reset;
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
    int use_uds;

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
            if (self->code == 0) /* Xvnc */
            {
                g_snprintf(text, 255, "%d", 5900 + self->display);
            }
            else if (self->code == 10 || self->code == 20) /* X11rdp/Xorg */
            {
                use_uds = 1;

                if ((value = xrdp_mm_get_value(self, "ip")) != NULL &&
                        g_strcmp(value, "127.0.0.1") != 0)
                {
                    use_uds = 0;
                }

                if (use_uds)
                {
                    g_snprintf(text, 255, XRDP_X11RDP_STR, self->display);
                }
                else
                {
                    g_snprintf(text, 255, "%d", 6200 + self->display);
                }
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
            list_add_item(self->login_names, (long)g_strdup("port"));
            list_add_item(self->login_values, (long)g_strdup(text));
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
                self->mod->mod_event(self->mod, 17, key_flags, device_flags,
                                     key_flags, device_flags);
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

/******************************************************************************/
static int
dynamic_monitor_open_response(intptr_t id, int chan_id, int creation_status)
{
    struct xrdp_process *pro;
    struct xrdp_wm *wm;
    struct stream *s;
    int bytes;

    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_open_response: chan_id %d creation_status 0x%8.8x", chan_id, creation_status);
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
static int
dynamic_monitor_data(intptr_t id, int chan_id, char *data, int bytes)
{
    struct stream ls;
    struct stream *s;
    int msg_type;
    int msg_length;
    int monitor_index;
    struct xrdp_process *pro;
    struct xrdp_wm *wm;

    int MonitorLayoutSize;
    int NumMonitor;

    struct dynamic_monitor_layout monitor_layouts[CLIENT_MONITOR_DATA_MAXIMUM_MONITORS];
    struct dynamic_monitor_layout *monitor_layout;

    struct xrdp_rect rect;
    int session_width;
    int session_height;

    LOG_DEVEL(LOG_LEVEL_TRACE, "dynamic_monitor_data:");
    pro = (struct xrdp_process *) id;
    wm = pro->wm;
    g_memset(&ls, 0, sizeof(ls));
    ls.data = data;
    ls.p = ls.data;
    ls.size = bytes;
    ls.end = ls.data + bytes;
    s = &ls;
    in_uint32_le(s, msg_type);
    in_uint32_le(s, msg_length);
    LOG(LOG_LEVEL_DEBUG, "dynamic_monitor_data: msg_type %d msg_length %d",
        msg_type, msg_length);

    rect.left = 8192;
    rect.top = 8192;
    rect.right = -8192;
    rect.bottom = -8192;

    if (msg_type == DISPLAYCONTROL_PDU_TYPE_MONITOR_LAYOUT)
    {
        in_uint32_le(s, MonitorLayoutSize);
        in_uint32_le(s, NumMonitor);
        LOG(LOG_LEVEL_DEBUG, "  MonitorLayoutSize %d NumMonitor %d",
            MonitorLayoutSize, NumMonitor);
        for (monitor_index = 0; monitor_index < NumMonitor; monitor_index++)
        {
            monitor_layout = monitor_layouts + monitor_index;
            in_uint32_le(s, monitor_layout->flags);
            in_uint32_le(s, monitor_layout->left);
            in_uint32_le(s, monitor_layout->top);
            in_uint32_le(s, monitor_layout->width);
            in_uint32_le(s, monitor_layout->height);
            in_uint32_le(s, monitor_layout->physical_width);
            in_uint32_le(s, monitor_layout->physical_height);
            in_uint32_le(s, monitor_layout->orientation);
            in_uint32_le(s, monitor_layout->desktop_scale_factor);
            in_uint32_le(s, monitor_layout->device_scale_factor);
            LOG_DEVEL(LOG_LEVEL_DEBUG, "    Flags 0x%8.8x Left %d Top %d "
                      "Width %d Height %d PhysicalWidth %d PhysicalHeight %d "
                      "Orientation %d DesktopScaleFactor %d DeviceScaleFactor %d",
                      monitor_layout->flags, monitor_layout->left, monitor_layout->top,
                      monitor_layout->width, monitor_layout->height,
                      monitor_layout->physical_width, monitor_layout->physical_height,
                      monitor_layout->orientation, monitor_layout->desktop_scale_factor,
                      monitor_layout->device_scale_factor);

            rect.left = MIN(monitor_layout->left, rect.left);
            rect.top = MIN(monitor_layout->top, rect.top);
            rect.right = MAX(rect.right, monitor_layout->left + monitor_layout->width);
            rect.bottom = MAX(rect.bottom, monitor_layout->top + monitor_layout->height);
        }
    }
    session_width = rect.right - rect.left;
    session_height = rect.bottom - rect.top;

    if (session_width <= 0 && session_height <= 0)
    {
        return 0;
    }
    if (session_width == wm->client_info->width
            && session_height == wm->client_info->height)
    {
        return 0;
    }

    // TODO: Unify this logic with server_reset
    libxrdp_reset(wm->session, session_width, session_height, wm->screen->bpp);
    /* reset cache */
    xrdp_cache_reset(wm->cache, wm->client_info);
    /* resize the main window */
    xrdp_bitmap_resize(wm->screen, session_width, session_height);
    /* load some stuff */
    xrdp_wm_load_static_colors_plus(wm, 0);
    xrdp_wm_load_static_pointers(wm);
    /* redraw */
    xrdp_bitmap_invalidate(wm->screen, 0);

    struct xrdp_mod *v = wm->mm->mod;
    if (v == 0)
    {
        return 0;
    }
    v->mod_server_version_message(v);
    v->mod_server_monitor_resize(v, session_width, session_height);
    v->mod_server_monitor_full_invalidate(v, session_width, session_height);

    // Need to recreate the encoder for connections that use it.
    if (wm->mm->encoder != NULL)
    {
        xrdp_encoder_delete(wm->mm->encoder);
        wm->mm->encoder = NULL;
    }
    if (wm->mm->encoder == NULL)
    {
        wm->mm->encoder = xrdp_encoder_create(wm->mm);
    }
    return 0;
}

/******************************************************************************/
int
dynamic_monitor_initialize(struct xrdp_mm *self)
{
    struct xrdp_drdynvc_procs d_procs;
    int flags;
    int error;

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
        LOG(LOG_LEVEL_ERROR, "xrdp_mm_drdynvc_up: "
            "libxrdp_drdynvc_open failed %d", error);
    }
    return error;
}

/******************************************************************************/
int
xrdp_mm_drdynvc_up(struct xrdp_mm *self)
{
    const char *enable_dynamic_resize;
    int error = 0;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_drdynvc_up:");

    enable_dynamic_resize = xrdp_mm_get_value(self, "enable_dynamic_resizing");
    /*
     * User can disable dynamic resizing if necessary
     */
    if (enable_dynamic_resize != NULL && enable_dynamic_resize[0] != '\0' &&
            !g_text2bool(enable_dynamic_resize))
    {
        LOG(LOG_LEVEL_INFO, "User has disabled dynamic resizing.");
    }
    else
    {
        error = dynamic_monitor_initialize(self);
    }
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

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_drdynvc_open_response: chan_id %d creation_status %d",
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

static void cleanup_sesman_connection(struct xrdp_mm *self)
{
    /* Don't delete these transports here - we may be in
     * an auth callback from one of them */
    self->delete_sesman_trans = 1;
    self->delete_pam_auth_trans = 1;

    if (self->wm->login_state != WMLS_CLEANUP)
    {
        xrdp_wm_set_login_state(self->wm, WMLS_INACTIVE);
        xrdp_mm_module_cleanup(self);
    }
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
    fd = g_file_open(cfg_file);

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
static void
xrdp_mm_scp_process_msg(struct xrdp_mm *self,
                        const struct scp_v0_reply_type *msg)
{
    if (msg->is_gw_auth_response)
    {
        const char *additionalError;
        char pam_error[128];

        /* We no longer need the pam_auth transport - it's only used
         * for the one message */
        self->delete_pam_auth_trans = 1;

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                        "Reply from access control: %s",
                        getPAMError(msg->auth_result,
                                    pam_error, sizeof(pam_error)));

        if (msg->auth_result != 0)
        {
            additionalError = getPAMAdditionalErrorInfo(msg->auth_result, self);
            if (additionalError && additionalError[0])
            {
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s",
                                additionalError);
            }

            /* TODO : Check this is displayed */
            cleanup_sesman_connection(self);
            xrdp_wm_mod_connect_done(self->wm, 1);
        }
        else
        {
            /* Authentication successful */
            xrdp_mm_connect_sm(self);
        }
    }
    else
    {
        const char *username;
        char displayinfo[64];
        int auth_successful = (msg->auth_result != 0);

        /* Sort out some logging information */
        if ((username = xrdp_mm_get_value(self, "username")) == NULL)
        {
            username = "???";
        }

        if (msg->display == 0)
        {
            /* A returned display of zero doesn't mean anything useful, and
             * can confuse the user. It's most likely authentication has
             * failed and no display was allocated */
            displayinfo[0] = '\0';
        }
        else
        {
            g_snprintf(displayinfo, sizeof(displayinfo),
                       " on display %d", msg->display);
        }

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                        "login %s for user %s%s",
                        (auth_successful ? "successful" : "failed"),
                        username, displayinfo);

        if (!auth_successful)
        {
            /* Authentication failure */
            cleanup_sesman_connection(self);
            xrdp_wm_mod_connect_done(self->wm, 1);
        }
        else
        {
            /* Authentication successful - carry on with the connect
             * state machine */
            self->display = msg->display;
            self->guid = msg->guid;
            xrdp_mm_connect_sm(self);
        }
    }
}

/*****************************************************************************/
/* This is the callback registered for sesman communication replies. */
static int
xrdp_mm_scp_data_in(struct trans *trans)
{
    int rv = 0;

    if (trans == NULL)
    {
        rv = 1;
    }
    else if (scp_v0c_reply_available(trans))
    {
        struct scp_v0_reply_type reply;
        struct xrdp_mm *self = (struct xrdp_mm *)(trans->callback_data);
        enum SCP_CLIENT_STATES_E e = scp_v0c_get_reply(trans, &reply);
        if (e != SCP_CLIENT_STATE_OK)
        {
            const char *src = (trans == self->pam_auth_trans)
                              ? "PAM authenticator"
                              : "sesman";
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                            "Error reading response from %s [%s]",
                            src, scp_client_state_to_str(e));
            rv = 1;
        }
        else
        {
            xrdp_mm_scp_process_msg(self, &reply);
        }
    }

    return rv;
}

/*****************************************************************************/
/* This routine clears all states to make sure that our next login will be
 * as expected. If the user does not press ok on the log window and try to
 * connect again we must make sure that no previous information is stored.*/
static void
cleanup_states(struct xrdp_mm *self)
{
    if (self != NULL)
    {
        self->connect_state = MMCS_CONNECT_TO_SESMAN;
        self->use_sesman = 0; /* true if this is a sesman session */
        self->use_chansrv = 0; /* true if chansrvport is set in xrdp.ini or using sesman */
        self->use_pam_auth = 0; /* true if we're to use the PAM authentication facility */
        self->sesman_trans = NULL; /* connection to sesman */
        self->pam_auth_trans = NULL; /* connection to PAM authenticator */
        self->chan_trans = NULL; /* connection to chansrv */
        self->delete_sesman_trans = 0;
        self->delete_pam_auth_trans = 0;
        self->display = 0; /* 10 for :10.0, 11 for :11.0, etc */
        guid_clear(&self->guid);
        self->code = 0; /* 0 Xvnc session, 10 X11rdp session, 20 Xorg session */
    }
}

static const char *
getPAMError(const int pamError, char *text, int text_bytes)
{
    switch (pamError)
    {
#if defined(LINUXPAM)
        case PAM_SUCCESS:
            return "Success";
        case PAM_OPEN_ERR:
            return "dlopen() failure";
        case PAM_SYMBOL_ERR:
            return "Symbol not found";
        case PAM_SERVICE_ERR:
            return "Error in service module";
        case PAM_SYSTEM_ERR:
            return "System error";
        case PAM_BUF_ERR:
            return "Memory buffer error";
        case PAM_PERM_DENIED:
            return "Permission denied";
        case PAM_AUTH_ERR:
            return "Authentication failure";
        case PAM_CRED_INSUFFICIENT:
            return "Insufficient credentials to access authentication data";
        case PAM_AUTHINFO_UNAVAIL:
            return "Authentication service cannot retrieve authentication info.";
        case PAM_USER_UNKNOWN:
            return "User not known to the underlying authentication module";
        case PAM_MAXTRIES:
            return "Have exhausted maximum number of retries for service.";
        case PAM_NEW_AUTHTOK_REQD:
            return "Authentication token is no longer valid; new one required.";
        case PAM_ACCT_EXPIRED:
            return "User account has expired";
        case PAM_CRED_UNAVAIL:
            return "Authentication service cannot retrieve user credentials";
        case PAM_CRED_EXPIRED:
            return "User credentials expired";
        case PAM_CRED_ERR:
            return "Failure setting user credentials";
        case PAM_NO_MODULE_DATA:
            return "No module specific data is present";
        case PAM_BAD_ITEM:
            return "Bad item passed to pam_*_item()";
        case PAM_CONV_ERR:
            return "Conversation error";
        case PAM_AUTHTOK_ERR:
            return "Authentication token manipulation error";
        case PAM_AUTHTOK_LOCK_BUSY:
            return "Authentication token lock busy";
        case PAM_AUTHTOK_DISABLE_AGING:
            return "Authentication token aging disabled";
        case PAM_TRY_AGAIN:
            return "Failed preliminary check by password service";
        case PAM_IGNORE:
            return "Please ignore underlying account module";
        case PAM_MODULE_UNKNOWN:
            return "Module is unknown";
        case PAM_AUTHTOK_EXPIRED:
            return "Authentication token expired";
        case PAM_CONV_AGAIN:
            return "Conversation is waiting for event";
        case PAM_INCOMPLETE:
            return "Application needs to call libpam again";
        case 32 + 1:
            return "Error connecting to PAM";
        case 32 + 3:
            return "Username okey but group problem";
#elif defined(OPENPAM)
        case PAM_SUCCESS: /* 0 */
            return "Success";
        case PAM_OPEN_ERR:
            return "dlopen() failure";
        case PAM_SYMBOL_ERR:
            return "Symbol not found";
        case PAM_SERVICE_ERR:
            return "Error in service module";
        case PAM_SYSTEM_ERR:
            return "System error";
        case PAM_BUF_ERR:
            return "Memory buffer error";
        case PAM_CONV_ERR:
            return "Conversation error";
        case PAM_PERM_DENIED:
            return "Permission denied";
        case PAM_MAXTRIES:
            return "Have exhausted maximum number of retries for service.";
        case PAM_AUTH_ERR:
            return "Authentication failure";
        case PAM_NEW_AUTHTOK_REQD: /* 10 */
            return "Authentication token is no longer valid; new one required.";
        case PAM_CRED_INSUFFICIENT:
            return "Insufficient credentials to access authentication data";
        case PAM_AUTHINFO_UNAVAIL:
            return "Authentication service cannot retrieve authentication info.";
        case PAM_USER_UNKNOWN:
            return "User not known to the underlying authentication module";
        case PAM_CRED_UNAVAIL:
            return "Authentication service cannot retrieve user credentials";
        case PAM_CRED_EXPIRED:
            return "User credentials expired";
        case PAM_CRED_ERR:
            return "Failure setting user credentials";
        case PAM_ACCT_EXPIRED:
            return "User account has expired";
        case PAM_AUTHTOK_EXPIRED:
            return "Authentication token expired";
        case PAM_SESSION_ERR:
            return "Session failure";
        case PAM_AUTHTOK_ERR: /* 20 */
            return "Authentication token manipulation error";
        case PAM_AUTHTOK_RECOVERY_ERR:
            return "Failed to recover old authentication token";
        case PAM_AUTHTOK_LOCK_BUSY:
            return "Authentication token lock busy";
        case PAM_AUTHTOK_DISABLE_AGING:
            return "Authentication token aging disabled";
        case PAM_NO_MODULE_DATA:
            return "No module specific data is present";
        case PAM_IGNORE:
            return "Please ignore underlying account module";
        case PAM_ABORT:
            return "General failure";
        case PAM_TRY_AGAIN:
            return "Failed preliminary check by password service";
        case PAM_MODULE_UNKNOWN:
            return "Module is unknown";
        case PAM_DOMAIN_UNKNOWN: /* 29 */
            return "Unknown authentication domain";
#endif
        default:
            g_snprintf(text, text_bytes, "Not defined PAM error:%d", pamError);
            return text;
    }
}

static const char *
getPAMAdditionalErrorInfo(const int pamError, struct xrdp_mm *self)
{
    switch (pamError)
    {
#if defined(LINUXPAM)
        case PAM_SUCCESS:
            return NULL;
        case PAM_OPEN_ERR:
        case PAM_SYMBOL_ERR:
        case PAM_SERVICE_ERR:
        case PAM_SYSTEM_ERR:
        case PAM_BUF_ERR:
        case PAM_PERM_DENIED:
        case PAM_AUTH_ERR:
        case PAM_CRED_INSUFFICIENT:
        case PAM_AUTHINFO_UNAVAIL:
        case PAM_USER_UNKNOWN:
        case PAM_CRED_UNAVAIL:
        case PAM_CRED_ERR:
        case PAM_NO_MODULE_DATA:
        case PAM_BAD_ITEM:
        case PAM_CONV_ERR:
        case PAM_AUTHTOK_ERR:
        case PAM_AUTHTOK_LOCK_BUSY:
        case PAM_AUTHTOK_DISABLE_AGING:
        case PAM_TRY_AGAIN:
        case PAM_IGNORE:
        case PAM_MODULE_UNKNOWN:
        case PAM_CONV_AGAIN:
        case PAM_INCOMPLETE:
        case _PAM_RETURN_VALUES + 1:
        case _PAM_RETURN_VALUES + 3:
            return NULL;
        case PAM_MAXTRIES:
        case PAM_NEW_AUTHTOK_REQD:
        case PAM_ACCT_EXPIRED:
        case PAM_CRED_EXPIRED:
        case PAM_AUTHTOK_EXPIRED:
            if (self->wm->pamerrortxt[0])
            {
                return self->wm->pamerrortxt;
            }
            else
            {
                return "Authentication error - Verify that user/password is valid";
            }
#elif defined(OPENPAM)
        case PAM_SUCCESS: /* 0 */
            return NULL;
        case PAM_OPEN_ERR:
        case PAM_SYMBOL_ERR:
        case PAM_SERVICE_ERR:
        case PAM_SYSTEM_ERR:
        case PAM_BUF_ERR:
        case PAM_CONV_ERR:
        case PAM_PERM_DENIED:
        case PAM_MAXTRIES:
        case PAM_AUTH_ERR:
        case PAM_NEW_AUTHTOK_REQD: /* 10 */
        case PAM_CRED_INSUFFICIENT:
        case PAM_AUTHINFO_UNAVAIL:
        case PAM_USER_UNKNOWN:
        case PAM_CRED_UNAVAIL:
        case PAM_CRED_EXPIRED:
        case PAM_CRED_ERR:
        case PAM_ACCT_EXPIRED:
        case PAM_AUTHTOK_EXPIRED:
        case PAM_SESSION_ERR:
        case PAM_AUTHTOK_ERR: /* 20 */
        case PAM_AUTHTOK_RECOVERY_ERR:
        case PAM_AUTHTOK_LOCK_BUSY:
        case PAM_AUTHTOK_DISABLE_AGING:
        case PAM_NO_MODULE_DATA:
        case PAM_IGNORE:
        case PAM_ABORT:
        case PAM_TRY_AGAIN:
        case PAM_MODULE_UNKNOWN:
        case PAM_DOMAIN_UNKNOWN: /* 29 */
            if (self->wm->pamerrortxt[0])
            {
                return self->wm->pamerrortxt;
            }
            else
            {
                return "Authentication error - Verify that user/password is valid";
            }
#endif
        default:
            return "No expected error";
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
 * @return 0 for success
 */

static int
parse_chansrvport(const char *value, char *dest, int dest_size)
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
            g_snprintf(dest, dest_size, XRDP_CHANSRV_STR, g_atoi(p));
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
xrdp_mm_scp_connect(struct xrdp_mm *self, const char *target, const char *ip)
{
    char port[128];
    struct trans *t;

    xrdp_mm_get_sesman_port(port, sizeof(port));
    xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                    "connecting to %s on %s:%s", target, ip, port);
    t = scp_connect(ip, port, g_is_term,
                    xrdp_mm_scp_data_in, self);
    if (t != NULL)
    {
        /* fully connect */
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s connect ok", target);
    }
    else
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "Error connecting to %s on %s:%s",
                        target, ip, port);
        trans_delete(t);
        t = NULL;
    }
    return t;
}

/*****************************************************************************/
static int
xrdp_mm_pam_auth_connect(struct xrdp_mm *self, const char *ip)
{
    trans_delete(self->pam_auth_trans);
    self->pam_auth_trans = xrdp_mm_scp_connect(self, "PAM authenticator", ip);

    return (self->pam_auth_trans == NULL); /* 0 for success */
}

/*****************************************************************************/
static int
xrdp_mm_sesman_connect(struct xrdp_mm *self, const char *ip)
{
    trans_delete(self->sesman_trans);
    self->sesman_trans = xrdp_mm_scp_connect(self, "sesman", ip);

    return (self->sesman_trans == NULL); /* 0 for success */
}

/*****************************************************************************/
static int
xrdp_mm_chansrv_connect(struct xrdp_mm *self, const char *ip, const char *port)
{
    int index;

    if (self->wm->client_info->channels_allowed == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "%s: "
            "skip connecting to chansrv because all channels are disabled",
            __func__);
        return 0;
    }

    /* connect channel redir */
    if ((g_strcmp(ip, "127.0.0.1") == 0) || (ip[0] == 0))
    {
        /* unix socket */
        self->chan_trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
    }
    else
    {
        /* tcp */
        self->chan_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
    }

    self->chan_trans->is_term = g_is_term;
    self->chan_trans->si = &(self->wm->session->si);
    self->chan_trans->my_source = XRDP_SOURCE_CHANSRV;
    self->chan_trans->trans_data_in = xrdp_mm_chan_data_in;
    self->chan_trans->header_size = 8;
    self->chan_trans->callback_data = self;
    self->chan_trans->no_stream_init_on_data_in = 1;
    self->chan_trans->extra_flags = 0;

    /* try to connect up to 4 times */
    for (index = 0; index < 4; index++)
    {
        if (trans_connect(self->chan_trans, ip, port, 3000) == 0)
        {
            break;
        }
        if (g_is_term())
        {
            break;
        }
        g_sleep(1000);
        LOG(LOG_LEVEL_WARNING, "xrdp_mm_chansrv_connect: connect failed "
            "trying again...");
    }

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
    const char *gateway_username = xrdp_mm_get_value(self, "pamusername");

    /* make sure we start in correct state */
    cleanup_states(self);

    /* Look at our module parameters to decide if we need to connect
     * to sesman or not */

    if (port != NULL && g_strcmp(port, "-1") == 0)
    {
        self->use_sesman = 1;
    }

    if (gateway_username != NULL)
    {
#ifdef USE_PAM
        self->use_pam_auth = 1;
#else
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_WARNING,
                        "pamusername parameter ignored - "
                        "xrdp is compiled without PAM support");
#endif
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
        if (csp != NULL && parse_chansrvport(csp, NULL, 0) == 0)
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
    int waiting_for_msg = 0; /* Set this to leave the sm to wait for a reply */

    while (status == 0 && !waiting_for_msg && self->connect_state != MMCS_DONE)
    {
        switch (self->connect_state)
        {
            case MMCS_CONNECT_TO_SESMAN:
            {
                if (self->use_sesman)
                {
                    /* Synchronous call */
                    const char *ip = xrdp_mm_get_value(self, "ip");
                    status = xrdp_mm_sesman_connect(self, ip);
                }

                if (status == 0 && self->use_pam_auth)
                {
                    /* Synchronous call */
                    const char *ip = xrdp_mm_get_value(self, "pamsessionmng");
                    if (ip == NULL)
                    {
                        ip = xrdp_mm_get_value(self, "ip");
                    }
                    status = xrdp_mm_pam_auth_connect(self, ip);
                }
            }
            break;

            case MMCS_PAM_AUTH:
            {
                if (self->use_pam_auth)
                {
                    const char *gateway_username;
                    const char *gateway_password;

                    gateway_username = xrdp_mm_get_value(self, "pamusername");
                    gateway_password = xrdp_mm_get_value(self, "pampassword");
                    if (!g_strcmp(gateway_username, "same"))
                    {
                        gateway_username = xrdp_mm_get_value(self, "username");
                    }

                    if (gateway_password == NULL ||
                            !g_strcmp(gateway_password, "same"))
                    {
                        gateway_password = xrdp_mm_get_value(self, "password");
                    }

                    if (gateway_username == NULL || gateway_password == NULL)
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                        "Can't determine username and/or "
                                        "password for gateway authorization");
                        status = 1;
                    }
                    else
                    {
                        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                                        "Performing access control for %s",
                                        gateway_username);

                        status = xrdp_mm_send_gateway_login(self,
                                                            gateway_username,
                                                            gateway_password);
                        if (status == 0)
                        {
                            /* Now waiting for a reply from sesman */
                            waiting_for_msg = 1;
                        }
                    }
                }
            }
            break;

            case MMCS_SESSION_AUTH:
            {
                if (self->use_sesman)
                {
                    if ((status = xrdp_mm_send_login(self)) == 0)
                    {
                        /* Now waiting for a reply from sesman */
                        waiting_for_msg = 1;
                    }
                }
            }
            break;

            case MMCS_CONNECT_TO_SESSION:
            {
                /* This is synchronous - no reply message expected */
                status = xrdp_mm_user_session_connect(self);
            }
            break;

            case MMCS_CONNECT_TO_CHANSRV:
            {
                if (self->use_chansrv)
                {
                    const char *ip = "";
                    char portbuff[256];

                    if (self->use_sesman)
                    {
                        ip = xrdp_mm_get_value(self, "ip");

                        /* connect channel redir */
                        if (ip == NULL || (ip[0] == '\0') ||
                                (g_strcmp(ip, "127.0.0.1") == 0))
                        {
                            g_snprintf(portbuff, sizeof(portbuff),
                                       XRDP_CHANSRV_STR, self->display);
                        }
                        else
                        {
                            g_snprintf(portbuff, sizeof(portbuff),
                                       "%d", 7200 + self->display);
                        }
                    }
                    else
                    {
                        const char *cp = xrdp_mm_get_value(self, "chansrvport");
                        portbuff[0] = '\0';
                        parse_chansrvport(cp, portbuff, sizeof(portbuff));

                    }
                    xrdp_mm_update_allowed_channels(self);
                    xrdp_mm_chansrv_connect(self, ip, portbuff);
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

    if (!waiting_for_msg)
    {
        xrdp_wm_mod_connect_done(self->wm, status);
        cleanup_sesman_connection(self);
    }
}


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

    if (self->pam_auth_trans != 0 &&
            self->pam_auth_trans->status == TRANS_STATUS_UP)
    {
        trans_get_wait_objs(self->pam_auth_trans, read_objs, rcount);
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
        ii = g_file_open("/tmp/jpeg.beef.bin");
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_update_module_ack: frame_id_server %d",
                      encoder->frame_id_server);
            encoder->frame_id_server_sent = encoder->frame_id_server;
            self->mod->mod_frame_ack(self->mod, 0, encoder->frame_id_server);
        }
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_mm_process_enc_done(struct xrdp_mm *self)
{
    XRDP_ENC_DATA_DONE *enc_done;
    int x;
    int y;
    int cx;
    int cy;

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
        /* do something with msg */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_process_enc_done: message back bytes %d",
                  enc_done->comp_bytes);
        x = enc_done->x;
        y = enc_done->y;
        cx = enc_done->cx;
        cy = enc_done->cy;
        if (enc_done->comp_bytes > 0)
        {
            if (!enc_done->continuation)
            {
                libxrdp_fastpath_send_frame_marker(self->wm->session, 0,
                                                   enc_done->enc->frame_id);
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
                                                   enc_done->enc->frame_id);
            }
        }
        /* free enc_done */
        if (enc_done->last)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_process_enc_done: last set");
            if (self->wm->client_info->use_frame_acks == 0)
            {
                self->mod->mod_frame_ack(self->mod,
                                         enc_done->enc->flags,
                                         enc_done->enc->frame_id);
            }
            else
            {
                self->encoder->frame_id_server = enc_done->enc->frame_id;
                xrdp_mm_update_module_frame_ack(self);
            }
            g_free(enc_done->enc->drects);
            g_free(enc_done->enc->crects);
            g_free(enc_done->enc);
        }
        g_free(enc_done->comp_pad_data);
        g_free(enc_done);
    }
    return 0;
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

    if (self->pam_auth_trans != NULL &&
            !self->delete_pam_auth_trans &&
            self->pam_auth_trans->status == TRANS_STATUS_UP)
    {
        if (trans_check_wait_objs(self->pam_auth_trans) != 0)
        {
            self->delete_pam_auth_trans = 1;
        }
    }
    if (self->delete_pam_auth_trans)
    {
        trans_delete(self->pam_auth_trans);
        self->pam_auth_trans = NULL;
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

    if (self->encoder != NULL)
    {
        if (g_is_wait_obj_set(self->encoder->xrdp_encoder_event_processed))
        {
            g_reset_wait_obj(self->encoder->xrdp_encoder_event_processed);
            xrdp_mm_process_enc_done(self);
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
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mm_frame_ack: incoming %d, client %d, server %d",
              frame_id, encoder->frame_id_client, encoder->frame_id_server);
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
    struct xrdp_wm *wm;
    struct xrdp_mm *mm;
    struct xrdp_painter *p;
    struct xrdp_bitmap *b;
    short *s;
    int index;
    XRDP_ENC_DATA *enc_data;

    wm = (struct xrdp_wm *)(mod->wm);
    mm = wm->mm;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "server_paint_rects: %p", mm->encoder);

    if (mm->encoder != 0)
    {
        /* copy formal params to XRDP_ENC_DATA */
        enc_data = (XRDP_ENC_DATA *) g_malloc(sizeof(XRDP_ENC_DATA), 1);
        if (enc_data == 0)
        {
            return 1;
        }

        enc_data->drects = (short *)
                           g_malloc(sizeof(short) * num_drects * 4, 0);
        if (enc_data->drects == 0)
        {
            g_free(enc_data);
            return 1;
        }

        enc_data->crects = (short *)
                           g_malloc(sizeof(short) * num_crects * 4, 0);
        if (enc_data->crects == 0)
        {
            g_free(enc_data->drects);
            g_free(enc_data);
            return 1;
        }

        g_memcpy(enc_data->drects, drects, sizeof(short) * num_drects * 4);
        g_memcpy(enc_data->crects, crects, sizeof(short) * num_crects * 4);

        enc_data->mod = mod;
        enc_data->num_drects = num_drects;
        enc_data->num_crects = num_crects;
        enc_data->data = data;
        enc_data->width = width;
        enc_data->height = height;
        enc_data->flags = flags;
        enc_data->frame_id = frame_id;
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

    LOG(LOG_LEVEL_TRACE, "server_paint_rects:");

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
server_set_pointer(struct xrdp_mod *mod, int x, int y,
                   char *data, char *mask)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_pointer(wm, data, mask, x, y, 0);
    return 0;
}

/*****************************************************************************/
int
server_set_pointer_ex(struct xrdp_mod *mod, int x, int y,
                      char *data, char *mask, int bpp)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);
    xrdp_wm_pointer(wm, data, mask, x, y, bpp);
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
server_is_term(struct xrdp_mod *mod)
{
    return g_is_term();
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

/* Note : if this is called on a multimon setup, the client is resized
 * to a single monitor */
int
server_reset(struct xrdp_mod *mod, int width, int height, int bpp)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->client_info == 0)
    {
        return 1;
    }

    /* older client can't resize */
    if (wm->client_info->build <= 419)
    {
        return 0;
    }

    /* if same (and only one monitor on client) don't need to do anything */
    if (wm->client_info->width == width &&
            wm->client_info->height == height &&
            wm->client_info->bpp == bpp &&
            (wm->client_info->monitorCount == 0 || wm->client_info->multimon == 0))
    {
        return 0;
    }

    /* reset lib, client_info gets updated in libxrdp_reset */
    if (libxrdp_reset(wm->session, width, height, bpp) != 0)
    {
        return 1;
    }

    /* reset cache */
    xrdp_cache_reset(wm->cache, wm->client_info);
    /* resize the main window */
    xrdp_bitmap_resize(wm->screen, wm->client_info->width,
                       wm->client_info->height);
    /* load some stuff */
    xrdp_wm_load_static_colors_plus(wm, 0);
    xrdp_wm_load_static_pointers(wm);
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
