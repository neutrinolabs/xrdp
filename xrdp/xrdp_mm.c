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
#include "ms-rdpedisp.h"
#include "ms-rdpbcgr.h"

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
    self->chan_trans_up = 0;
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
    self->sesman_trans_up = 0;
    list_delete(self->login_names);
    list_delete(self->login_values);
    g_free(self);
}

/*****************************************************************************/
/* Send login information to sesman */
/* FIXME : This code duplicates functionality in the sesman tools sesrun.c.
 * When SCP is reworked, a common library function should be used */

static int
xrdp_mm_send_login(struct xrdp_mm *self)
{
    struct stream *s;
    int rv;
    int index;
    int count;
    int xserverbpp;
    char *username;
    char *password;
    char *name;
    char *value;

    xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                    "sending login info to session manager, please wait...");
    username = 0;
    password = 0;
    self->code = 0;
    xserverbpp = 0;
    count = self->login_names->count;

    for (index = 0; index < count; index++)
    {
        name = (char *)list_get_item(self->login_names, index);
        value = (char *)list_get_item(self->login_values, index);

        if (g_strcasecmp(name, "username") == 0)
        {
            username = value;
        }
        else if (g_strcasecmp(name, "password") == 0)
        {
            password = value;
        }
        else if (g_strcasecmp(name, "code") == 0)
        {
            /* this code is either 0 for Xvnc, 10 for X11rdp or 20 for Xorg */
            self->code = g_atoi(value);
        }
        else if (g_strcasecmp(name, "xserverbpp") == 0)
        {
            xserverbpp = g_atoi(value);
        }
    }

    if ((username == 0) || (password == 0))
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                        "Error finding username and password");
        return 1;
    }

    s = trans_get_out_s(self->sesman_trans, 8192);
    s_push_layer(s, channel_hdr, 8);
    /* this code is either 0 for Xvnc, 10 for X11rdp or 20 for Xorg */
    out_uint16_be(s, self->code);
    index = g_strlen(username);
    out_uint16_be(s, index);
    out_uint8a(s, username, index);
    index = g_strlen(password);

    out_uint16_be(s, index);
    out_uint8a(s, password, index);
    out_uint16_be(s, self->wm->screen->width);
    out_uint16_be(s, self->wm->screen->height);

    /* select and send X server bpp */
    if (xserverbpp == 0)
    {
        if (self->code == 20)
        {
            xserverbpp = 24; /* xorgxrdp is always at 24 bpp */
        }
        else
        {
            xserverbpp = self->wm->screen->bpp; /* use client's bpp */
        }
    }
    out_uint16_be(s, xserverbpp);

    /* send domain */
    if (self->wm->client_info->domain[0] != '_')
    {
        index = g_strlen(self->wm->client_info->domain);
        out_uint16_be(s, index);
        out_uint8a(s, self->wm->client_info->domain, index);
    }
    else
    {
        out_uint16_be(s, 0);
        /* out_uint8a(s, "", 0); */
    }

    /* send program / shell */
    index = g_strlen(self->wm->client_info->program);
    out_uint16_be(s, index);
    out_uint8a(s, self->wm->client_info->program, index);

    /* send directory */
    index = g_strlen(self->wm->client_info->directory);
    out_uint16_be(s, index);
    out_uint8a(s, self->wm->client_info->directory, index);

    /* send client ip */
    index = g_strlen(self->wm->client_info->client_ip);
    out_uint16_be(s, index);
    out_uint8a(s, self->wm->client_info->client_ip, index);

    s_mark_end(s);

    s_pop_layer(s, channel_hdr);
    /* Version 0 of the protocol to sesman is currently used by XRDP */
    out_uint32_be(s, 0); /* version */
    index = (int)(s->end - s->data);
    out_uint32_be(s, index); /* size */

    rv = trans_force_write(self->sesman_trans);

    if (rv != 0)
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_WARNING,
                        "xrdp_mm_send_login: xrdp_mm_send_login failed");
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
/* this goes through the login_names looking for one called 'aname'
   then it copies the corresponding login_values item into 'dest'
   'dest' must be at least 'dest_len' + 1 bytes in size */
static int
xrdp_mm_get_value(struct xrdp_mm *self, const char *aname, char *dest,
                  int dest_len)
{
    char *name;
    char *value;
    int index;
    int count;
    int rv;

    rv = 1;
    /* find the library name */
    dest[0] = 0;
    count = self->login_names->count;

    for (index = 0; index < count; index++)
    {
        name = (char *)list_get_item(self->login_names, index);
        value = (char *)list_get_item(self->login_values, index);

        if ((name == 0) || (value == 0))
        {
            break;
        }

        if (g_strcasecmp(name, aname) == 0)
        {
            g_strncpy(dest, value, dest_len);
            rv = 0;
        }
    }

    return rv;
}

/*****************************************************************************/
static int
xrdp_mm_setup_mod1(struct xrdp_mm *self)
{
    void *func;
    char lib[256];
    char text[256];

    if (self == 0)
    {
        return 1;
    }

    lib[0] = 0;

    if (xrdp_mm_get_value(self, "lib", lib, 255) != 0)
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
        g_snprintf(text, 255, "%s/%s", XRDP_MODULE_PATH, lib);
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
xrdp_mm_setup_mod2(struct xrdp_mm *self, tui8 *guid)
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

                if (xrdp_mm_get_value(self, "ip", text, 255) == 0)
                {
                    if (g_strcmp(text, "127.0.0.1") != 0)
                    {
                        use_uds = 0;
                    }
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
        if (guid != 0)
        {
            self->mod->mod_set_param(self->mod, "guid", (char *) guid);
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
    int size;
    int total_size;
    int chan_id;
    int chan_flags;
    int rv;

    in_uint16_le(s, chan_id);
    in_uint16_le(s, chan_flags);
    in_uint16_le(s, size);
    in_uint32_le(s, total_size);
    rv = 0;

    if (rv == 0)
    {
        rv = libxrdp_send_to_channel(self->wm->session, chan_id, s->p, size, total_size,
                                     chan_flags);
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
    if ((session_width > 0) && (session_height > 0))
    {
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
        if (v != 0)
        {
            v->mod_server_version_message(v);
            v->mod_server_monitor_resize(v, session_width, session_height);
            v->mod_server_monitor_full_invalidate(v, session_width, session_height);
        }
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
    char enable_dynamic_resize[32];
    int error = 0;

    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mm_drdynvc_up:");

    xrdp_mm_get_value(self, "enable_dynamic_resizing",
                      enable_dynamic_resize,
                      sizeof(enable_dynamic_resize) - 1);

    /*
     * User can disable dynamic resizing if necessary
     */
    if (enable_dynamic_resize[0] != '\0' && !g_text2bool(enable_dynamic_resize))
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
    char *name;
    struct xrdp_drdynvc_procs procs;

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint32_le(s, name_bytes);
    if ((name_bytes < 1) || (name_bytes > 1024))
    {
        return 1;
    }
    name = g_new(char, name_bytes + 1);
    if (name == NULL)
    {
        return 1;
    }
    if (!s_check_rem(s, name_bytes))
    {
        g_free(name);
        return 1;
    }
    in_uint8a(s, name, name_bytes);
    name[name_bytes] = 0;
    if (!s_check_rem(s, 8))
    {
        g_free(name);
        return 1;
    }
    in_uint32_le(s, flags);
    in_uint32_le(s, chansrv_chan_id);
    if (flags == 0)
    {
        /* open static channel, not supported */
        g_free(name);
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
            g_free(name);
            return 1;
        }
        self->xr2cr_cid_map[chan_id] = chansrv_chan_id;
        self->cs2xr_cid_map[chansrv_chan_id] = chan_id;
    }
    g_free(name);
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

    if (trans == 0)
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
/* connect to chansrv */
static int
xrdp_mm_connect_chansrv(struct xrdp_mm *self, const char *ip, const char *port)
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
            self->chan_trans_up = 1;
            break;
        }
        if (g_is_term())
        {
            break;
        }
        g_sleep(1000);
        LOG(LOG_LEVEL_WARNING, "xrdp_mm_connect_chansrv: connect failed "
            "trying again...");
    }

    if (!(self->chan_trans_up))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_mm_connect_chansrv: error in "
            "trans_connect chan");
    }

    if (self->chan_trans_up)
    {
        if (xrdp_mm_trans_send_channel_setup(self, self->chan_trans) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_mm_connect_chansrv: error in "
                "xrdp_mm_trans_send_channel_setup");
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG, "xrdp_mm_connect_chansrv: chansrv "
                "connect successful");
        }
    }

    return 0;
}

static void cleanup_sesman_connection(struct xrdp_mm *self)
{
    self->delete_sesman_trans = 1;
    self->connected_state = 0;

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
/* FIXME : This code duplicates functionality in the sesman tools sesrun.c.
 * When SCP is reworked, a common library function should be used */
static int
xrdp_mm_process_login_response(struct xrdp_mm *self, struct stream *s)
{
    int ok;
    int display;
    int rv;
    char ip[256];
    char port[256];
    tui8 guid[16];
    tui8 *pguid;

    rv = 0;
    in_uint16_be(s, ok);
    in_uint16_be(s, display);
    pguid = 0;
    if (s_check_rem(s, 16))
    {
        in_uint8a(s, guid, 16);
        pguid = guid;
    }
    if (ok)
    {
        self->display = display;
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                        "login successful for display %d", display);

        if (xrdp_mm_setup_mod1(self) == 0)
        {
            if (xrdp_mm_setup_mod2(self, pguid) == 0)
            {
                xrdp_mm_get_value(self, "ip", ip, 255);
                xrdp_wm_set_login_state(self->wm, WMLS_CLEANUP);
                self->wm->dragging = 0;

                /* connect channel redir */
                if ((g_strcmp(ip, "127.0.0.1") == 0) || (ip[0] == 0))
                {
                    g_snprintf(port, 255, XRDP_CHANSRV_STR, display);
                }
                else
                {
                    g_snprintf(port, 255, "%d", 7200 + display);
                }
                xrdp_mm_connect_chansrv(self, ip, port);
            }
        }
    }
    else
    {
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                        "login failed for display %d", display);
        xrdp_wm_show_log(self->wm);
        if (self->wm->hide_log_window)
        {
            rv = 1;
        }
    }

    cleanup_sesman_connection(self);
    return rv;
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

    if ((self->chan_trans != 0) && self->chan_trans_up)
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
/* This is the callback registered for sesman communication replies. */
static int
xrdp_mm_sesman_data_in(struct trans *trans)
{
    struct xrdp_mm *self;
    struct stream *s;
    int version;
    int size;
    int error;
    int code;

    if (trans == 0)
    {
        return 1;
    }

    self = (struct xrdp_mm *)(trans->callback_data);
    s = trans_get_in_s(trans);

    if (s == 0)
    {
        return 1;
    }

    in_uint32_be(s, version);
    in_uint32_be(s, size);
    error = trans_force_read(trans, size - 8);

    if (error == 0)
    {
        in_uint16_be(s, code);

        switch (code)
        {
            /* even when the request is denied the reply will hold 3 as the command. */
            case 3:
                error = xrdp_mm_process_login_response(self, s);
                break;
            default:
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                "Undefined reply code %d received from sesman",
                                code);
                cleanup_sesman_connection(self);
                break;
        }
    }

    return error;
}

#ifdef USE_PAM
/*********************************************************************/
/* return 0 on success */
static int
access_control(char *username, char *password, char *srv)
{
    int reply;
    int rec = 32 + 1; /* 32 is reserved for PAM failures this means connect failure */
    struct stream *in_s;
    struct stream *out_s;
    unsigned long version;
    unsigned short int dummy;
    unsigned short int pAM_errorcode;
    unsigned short int code;
    unsigned long size;
    int index;
    int socket = g_tcp_socket();
    char port[8];

    if (socket != -1)
    {
        xrdp_mm_get_sesman_port(port, sizeof(port));
        /* we use a blocking socket here */
        reply = g_tcp_connect(socket, srv, port);

        if (reply == 0)
        {
            make_stream(in_s);
            init_stream(in_s, 500);
            make_stream(out_s);
            init_stream(out_s, 500);
            s_push_layer(out_s, channel_hdr, 8);
            out_uint16_be(out_s, 4); /*0x04 means SCP_GW_AUTHENTICATION*/
            index = g_strlen(username);
            out_uint16_be(out_s, index);
            out_uint8a(out_s, username, index);

            index = g_strlen(password);
            out_uint16_be(out_s, index);
            out_uint8a(out_s, password, index);
            s_mark_end(out_s);
            s_pop_layer(out_s, channel_hdr);
            out_uint32_be(out_s, 0); /* version */
            index = (int)(out_s->end - out_s->data);
            out_uint32_be(out_s, index); /* size */
            LOG(LOG_LEVEL_DEBUG, "Number of data to send : %d", index);
            reply = g_tcp_send(socket, out_s->data, index, 0);
            free_stream(out_s);

            if (reply > 0)
            {
                /* We wait in 5 sec for a reply from sesman*/
                if (g_sck_can_recv(socket, 5000))
                {
                    reply = g_tcp_recv(socket, in_s->end, 500, 0);

                    if (reply > 0)
                    {
                        in_s->end =  in_s->end + reply;
                        in_uint32_be(in_s, version);
                        LOG(LOG_LEVEL_INFO, "Version number in reply from sesman: %lu", version);
                        in_uint32_be(in_s, size);

                        if ((size == 14) && (version == 0))
                        {
                            in_uint16_be(in_s, code);
                            in_uint16_be(in_s, pAM_errorcode); /* this variable holds the PAM error code if the variable is >32 it is a "invented" code */
                            in_uint16_be(in_s, dummy);

                            if (code != 4) /*0x04 means SCP_GW_AUTHENTICATION*/
                            {
                                LOG(LOG_LEVEL_ERROR, "Returned cmd code from "
                                    "sesman is corrupt");
                            }
                            else
                            {
                                rec = pAM_errorcode; /* here we read the reply from the access control */
                            }
                        }
                        else
                        {
                            LOG(LOG_LEVEL_ERROR, "Corrupt reply size or "
                                "version from sesman: %ld", size);
                        }
                    }
                    else
                    {
                        LOG(LOG_LEVEL_ERROR, "No data received from sesman");
                    }
                }
                else
                {
                    LOG(LOG_LEVEL_ERROR, "Timeout when waiting for sesman");
                }
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "No success sending to sesman");
            }

            free_stream(in_s);
            g_tcp_close(socket);
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "Failure connecting to socket sesman");
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "Failure creating socket - for access control");
    }

    if (socket != -1)
    {
        g_tcp_close(socket);
    }

    return rec;
}
#endif

/*****************************************************************************/
/* This routine clears all states to make sure that our next login will be
 * as expected. If the user does not press ok on the log window and try to
 * connect again we must make sure that no previous information is stored.*/
static void
cleanup_states(struct xrdp_mm *self)
{
    if (self != NULL)
    {
        self-> connected_state = 0; /* true if connected to sesman else false */
        self-> sesman_trans = NULL; /* connection to sesman */
        self-> sesman_trans_up = 0; /* true once connected to sesman */
        self-> delete_sesman_trans = 0; /* boolean set when done with sesman connection */
        self-> display = 0; /* 10 for :10.0, 11 for :11.0, etc */
        self-> code = 0; /* 0 Xvnc session, 10 X11rdp session, 20 Xorg session */
        self-> sesman_controlled = 0; /* true if this is a sesman session */
        self-> chan_trans = NULL; /* connection to chansrv */
        self-> chan_trans_up = 0; /* true once connected to chansrv */
        self-> delete_chan_trans = 0; /* boolean set when done with channel connection */
        self-> usechansrv = 0; /* true if chansrvport is set in xrdp.ini or using sesman */
    }
}

#ifdef USE_PAM
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
        default:
            g_snprintf(text, text_bytes, "Not defined PAM error:%d", pamError);
            return text;
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
        default:
            g_snprintf(text, text_bytes, "Not defined PAM error:%d", pamError);
            return text;
#endif
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
        default:
            return "No expected error";
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
        default:
            return "No expected error";
#endif
    }
}
#endif

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
int
xrdp_mm_connect(struct xrdp_mm *self)
{
    struct list *names;
    struct list *values;
    int index;
    int count;
    int ok;
    int rv;
    char *name;
    char *value;
    char ip[256];
    char port[8];
    char chansrvport[256];
#ifdef USE_PAM
    int use_pam_auth = 0;
    char pam_auth_sessionIP[256];
    char pam_auth_password[256];
    char pam_auth_username[256];
#endif
    char username[256];
    char password[256];
    username[0] = 0;
    password[0] = 0;

    /* make sure we start in correct state */
    cleanup_states(self);
    g_memset(ip, 0, sizeof(ip));
    g_memset(port, 0, sizeof(port));
    g_memset(chansrvport, 0, sizeof(chansrvport));
    rv = 0; /* success */
    names = self->login_names;
    values = self->login_values;
    count = names->count;

    for (index = 0; index < count; index++)
    {
        name = (char *)list_get_item(names, index);
        value = (char *)list_get_item(values, index);

        if (g_strcasecmp(name, "ip") == 0)
        {
            g_strncpy(ip, value, 255);
        }
        else if (g_strcasecmp(name, "port") == 0)
        {
            if (g_strcasecmp(value, "-1") == 0)
            {
                self->sesman_controlled = 1;
                self->usechansrv = 1;
            }
        }

#ifdef USE_PAM
        else if (g_strcasecmp(name, "pamusername") == 0)
        {
            use_pam_auth = 1;
            g_strncpy(pam_auth_username, value, 255);
        }
        else if (g_strcasecmp(name, "pamsessionmng") == 0)
        {
            g_strncpy(pam_auth_sessionIP, value, 255);
        }
        else if (g_strcasecmp(name, "pampassword") == 0)
        {
            g_strncpy(pam_auth_password, value, 255);
        }
#endif
        else if (g_strcasecmp(name, "password") == 0)
        {
            g_strncpy(password, value, 255);
        }
        else if (g_strcasecmp(name, "username") == 0)
        {
            g_strncpy(username, value, 255);
        }
        else if (g_strcasecmp(name, "chansrvport") == 0)
        {
            if (parse_chansrvport(value, chansrvport, sizeof(chansrvport)) == 0)
            {
                self->usechansrv = 1;
            }
        }
    }

    xrdp_mm_update_allowed_channels(self);

#ifdef USE_PAM
    if (use_pam_auth)
    {
        int reply;
        char pam_error[128];
        const char *additionalError;
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                        "Please wait, we now perform access control...");

        LOG(LOG_LEVEL_DEBUG, "we use pam modules to check if we can approve this user");
        if (!g_strncmp(pam_auth_username, "same", 255))
        {
            LOG(LOG_LEVEL_DEBUG, "pamusername copied from username - same: %s", username);
            g_strncpy(pam_auth_username, username, 255);
        }

        if (!g_strncmp(pam_auth_password, "same", 255))
        {
            LOG(LOG_LEVEL_DEBUG, "pam_auth_password copied from username - same: %s", password);
            g_strncpy(pam_auth_password, password, 255);
        }

        /* access_control return 0 on success */
        reply = access_control(pam_auth_username, pam_auth_password, pam_auth_sessionIP);

        xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO,
                        "Reply from access control: %s",
                        getPAMError(reply, pam_error, 127));

        additionalError = getPAMAdditionalErrorInfo(reply, self);
        if (additionalError && additionalError[0])
        {
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "%s", additionalError);
        }

        if (reply != 0)
        {
            rv = 1;
            return rv;
        }
    }
#endif

    if (self->sesman_controlled)
    {
        ok = 0;
        trans_delete(self->sesman_trans);
        self->sesman_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
        self->sesman_trans->is_term = g_is_term;
        xrdp_mm_get_sesman_port(port, sizeof(port));
        xrdp_wm_log_msg(self->wm, LOG_LEVEL_DEBUG,
                        "connecting to sesman ip %s port %s", ip, port);
        /* xrdp_mm_sesman_data_in is the callback that is called when data arrives */
        self->sesman_trans->trans_data_in = xrdp_mm_sesman_data_in;
        self->sesman_trans->header_size = 8;
        self->sesman_trans->callback_data = self;

        /* try to connect up to 4 times */
        for (index = 0; index < 4; index++)
        {
            if (trans_connect(self->sesman_trans, ip, port, 3000) == 0)
            {
                self->sesman_trans_up = 1;
                ok = 1;
                break;
            }
            if (g_is_term())
            {
                break;
            }
            g_sleep(1000);
            LOG(LOG_LEVEL_INFO, "xrdp_mm_connect: connect failed "
                "trying again...");
        }

        if (ok)
        {
            /* fully connect */
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_INFO, "sesman connect ok");
            self->connected_state = 1;
            rv = xrdp_mm_send_login(self);
        }
        else
        {
            xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                            "Error connecting to sesman: %s port: %s",
                            ip, port);
            trans_delete(self->sesman_trans);
            self->sesman_trans = 0;
            self->sesman_trans_up = 0;
            rv = 1;
        }
    }
    else /* no sesman */
    {
        if (xrdp_mm_setup_mod1(self) == 0)
        {
            if (xrdp_mm_setup_mod2(self, 0) == 0)
            {
                xrdp_wm_set_login_state(self->wm, WMLS_CLEANUP);
                rv = 0; /*success*/
            }
            else
            {
                /* connect error */
                xrdp_wm_log_msg(self->wm, LOG_LEVEL_ERROR,
                                "Error connecting to: %s", ip);
                rv = 1; /* failure */
            }
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "Failure setting up module");
        }

        if (self->wm->login_state != WMLS_CLEANUP)
        {
            xrdp_wm_set_login_state(self->wm, WMLS_INACTIVE);
            xrdp_mm_module_cleanup(self);
            rv = 1; /* failure */
        }
    }

    if ((self->wm->login_state == WMLS_CLEANUP) && (self->sesman_controlled == 0) &&
            (self->usechansrv != 0))
    {
        /* if sesman controlled, this will connect later */
        xrdp_mm_connect_chansrv(self, "", chansrvport);
    }

    LOG(LOG_LEVEL_DEBUG, "return value from xrdp_mm_connect %d", rv);

    return rv;
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

    if ((self->sesman_trans != 0) && self->sesman_trans_up)
    {
        trans_get_wait_objs(self->sesman_trans, read_objs, rcount);
    }

    if ((self->chan_trans != 0) && self->chan_trans_up)
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
    if ((self->chan_trans != 0) && self->chan_trans_up)
    {
        if (trans_check_wait_objs(self->chan_trans) != 0)
        {
            self->delete_chan_trans = 1;
        }
    }
    if (self->delete_chan_trans)
    {
        trans_delete(self->chan_trans);
        self->chan_trans = 0;
        self->chan_trans_up = 0;
        self->delete_chan_trans = 0;
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
            libxrdp_fastpath_send_frame_marker(self->wm->session, 0,
                                               enc_done->enc->frame_id);
            libxrdp_fastpath_send_surface(self->wm->session,
                                          enc_done->comp_pad_data,
                                          enc_done->pad_bytes,
                                          enc_done->comp_bytes,
                                          x, y, x + cx, y + cy,
                                          32, self->encoder->codec_id,
                                          cx, cy);
            libxrdp_fastpath_send_frame_marker(self->wm->session, 1,
                                               enc_done->enc->frame_id);
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

    if ((self->sesman_trans != NULL) && self->sesman_trans_up)
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

    if ((self->chan_trans != NULL) && self->chan_trans_up)
    {
        if (trans_check_wait_objs(self->chan_trans) != 0)
        {
            self->delete_chan_trans = 1;
        }
    }

    if (self->mod != NULL)
    {
        if (self->mod->mod_check_wait_objs != NULL)
        {
            rv = self->mod->mod_check_wait_objs(self->mod);
        }
    }

    if (self->delete_sesman_trans)
    {
        trans_delete(self->sesman_trans);
        self->sesman_trans = NULL;
        self->sesman_trans_up = 0;
        self->delete_sesman_trans = 0;
    }

    if (self->delete_chan_trans)
    {
        trans_delete(self->chan_trans);
        self->chan_trans = NULL;
        self->chan_trans_up = 0;
        self->delete_chan_trans = 0;
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
    return wm->mm->usechansrv;
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
server_msg(struct xrdp_mod *mod, char *msg, int code)
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
/*return 0 if the index is not found*/
int
server_query_channel(struct xrdp_mod *mod, int index, char *channel_name,
                     int *channel_flags)
{
    struct xrdp_wm *wm;

    wm = (struct xrdp_wm *)(mod->wm);

    if (wm->mm->usechansrv)
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

    if (wm->mm->usechansrv)
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

    if (wm->mm->usechansrv)
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
