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
 * simple window manager
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include "xrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"
#include "string_calls.h"



/*****************************************************************************/
struct xrdp_wm *
xrdp_wm_create(struct xrdp_process *owner,
               struct xrdp_client_info *client_info)
{
    struct xrdp_wm *self = (struct xrdp_wm *)NULL;
    char event_name[256];
    int pid = 0;

    /* initialize (zero out) local variables: */
    g_memset(event_name, 0, sizeof(char) * 256);

    self = (struct xrdp_wm *)g_malloc(sizeof(struct xrdp_wm), 1);
    self->client_info = client_info;
    self->screen = xrdp_bitmap_create(client_info->width,
                                      client_info->height,
                                      client_info->bpp,
                                      WND_TYPE_SCREEN, self);
    self->screen->wm = self;
    self->pro_layer = owner;
    self->session = owner->session;
    pid = g_getpid();
    g_snprintf(event_name, 255, "xrdp_%8.8x_wm_login_state_event_%8.8x",
               pid, owner->session_id);
    LOG(LOG_LEVEL_DEBUG, "%s", event_name);
    self->login_state_event = g_create_wait_obj(event_name);
    self->painter = xrdp_painter_create(self, self->session);
    self->cache = xrdp_cache_create(self, self->session, self->client_info);
    self->log = list_create();
    self->log->auto_free = 1;
    self->mm = xrdp_mm_create(self);
    self->default_font = xrdp_font_create(self);
    /* this will use built in keymap or load from file */
    get_keymaps(self->session->client_info->keylayout, &(self->keymap));
    xrdp_wm_set_login_state(self, WMLS_RESET);
    self->target_surface = self->screen;
    self->current_surface_index = 0xffff; /* screen */

    /* to store configuration from xrdp.ini */
    self->xrdp_config = g_new0(struct xrdp_config, 1);

    return self;
}

/*****************************************************************************/
void
xrdp_wm_delete(struct xrdp_wm *self)
{
    if (self == 0)
    {
        return;
    }

    xrdp_mm_delete(self->mm);
    xrdp_cache_delete(self->cache);
    xrdp_painter_delete(self->painter);
    xrdp_bitmap_delete(self->screen);
    /* free the log */
    list_delete(self->log);
    /* free default font */
    xrdp_font_delete(self->default_font);
    g_delete_wait_obj(self->login_state_event);

    if (self->xrdp_config)
    {
        g_free(self->xrdp_config);
    }

    /* free self */
    g_free(self);
}

/*****************************************************************************/
int
xrdp_wm_send_palette(struct xrdp_wm *self)
{
    return libxrdp_send_palette(self->session, self->palette);
}

/*****************************************************************************/
int
xrdp_wm_send_bell(struct xrdp_wm *self)
{
    return libxrdp_send_bell(self->session);
}

/*****************************************************************************/
int
xrdp_wm_send_bitmap(struct xrdp_wm *self, struct xrdp_bitmap *bitmap,
                    int x, int y, int cx, int cy)
{
    return libxrdp_send_bitmap(self->session, bitmap->width, bitmap->height,
                               bitmap->bpp, bitmap->data, x, y, cx, cy);
}

/*****************************************************************************/
int
xrdp_wm_set_focused(struct xrdp_wm *self, struct xrdp_bitmap *wnd)
{
    struct xrdp_bitmap *focus_out_control;
    struct xrdp_bitmap *focus_in_control;

    if (self == 0)
    {
        return 0;
    }

    if (self->focused_window == wnd)
    {
        return 0;
    }

    focus_out_control = 0;
    focus_in_control = 0;

    if (self->focused_window != 0)
    {
        xrdp_bitmap_set_focus(self->focused_window, 0);
        focus_out_control = self->focused_window->focused_control;
    }

    self->focused_window = wnd;

    if (self->focused_window != 0)
    {
        xrdp_bitmap_set_focus(self->focused_window, 1);
        focus_in_control = self->focused_window->focused_control;
    }

    xrdp_bitmap_invalidate(focus_out_control, 0);
    xrdp_bitmap_invalidate(focus_in_control, 0);
    return 0;
}

/******************************************************************************/
static int
xrdp_wm_get_pixel(char *data, int x, int y, int width, int bpp)
{
    int start;
    int shift;

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;
        return (data[start] & (0x80 >> shift)) != 0;
    }
    else if (bpp == 4)
    {
        width = (width + 1) / 2;
        start = y * width + x / 2;
        shift = x % 2;

        if (shift == 0)
        {
            return (data[start] & 0xf0) >> 4;
        }
        else
        {
            return data[start] & 0x0f;
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_pointer(struct xrdp_wm *self, char *data, char *mask, int x, int y,
                int bpp)
{
    int bytes;
    struct xrdp_pointer_item pointer_item;

    if (bpp == 0)
    {
        bpp = 24;
    }
    bytes = ((bpp + 7) / 8) * 32 * 32;
    g_memset(&pointer_item, 0, sizeof(struct xrdp_pointer_item));
    pointer_item.x = x;
    pointer_item.y = y;
    pointer_item.bpp = bpp;
    g_memcpy(pointer_item.data, data, bytes);
    g_memcpy(pointer_item.mask, mask, 32 * 32 / 8);
    self->screen->pointer = xrdp_cache_add_pointer(self->cache, &pointer_item);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_wm_load_pointer(struct xrdp_wm *self, char *file_name, char *data,
                     char *mask, int *x, int *y)
{
    int fd;
    int len;
    int bpp;
    int w;
    int h;
    int i;
    int j;
    int pixel;
    int palette[16];
    struct stream *fs;

    if (!g_file_exist(file_name))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_wm_load_pointer: error pointer file [%s] does not exist",
            file_name);
        return 1;
    }

    make_stream(fs);
    init_stream(fs, 8192);
    fd = g_file_open(file_name);

    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_wm_load_pointer: error loading pointer from file [%s]",
            file_name);
        xstream_free(fs);
        return 1;
    }

    len = g_file_read(fd, fs->data, 8192);
    g_file_close(fd);
    if (len <= 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_wm_load_pointer: read error from file [%s]",
            file_name);
        xstream_free(fs);
        return 1;
    }
    fs->end = fs->data + len;
    in_uint8s(fs, 6);
    in_uint8(fs, w);
    in_uint8(fs, h);
    in_uint8s(fs, 2);
    in_uint16_le(fs, *x);
    in_uint16_le(fs, *y);
    in_uint8s(fs, 22);
    in_uint8(fs, bpp);
    in_uint8s(fs, 25);

    if (w == 32 && h == 32)
    {
        if (bpp == 1)
        {
            for (i = 0; i < 2; i++)
            {
                in_uint32_le(fs, pixel);
                palette[i] = pixel;
            }
            for (i = 0; i < 32; i++)
            {
                for (j = 0; j < 32; j++)
                {
                    pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 1)];
                    *data = pixel;
                    data++;
                    *data = pixel >> 8;
                    data++;
                    *data = pixel >> 16;
                    data++;
                }
            }

            in_uint8s(fs, 128);
        }
        else if (bpp == 4)
        {
            for (i = 0; i < 16; i++)
            {
                in_uint32_le(fs, pixel);
                palette[i] = pixel;
            }
            for (i = 0; i < 32; i++)
            {
                for (j = 0; j < 32; j++)
                {
                    pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 1)];
                    *data = pixel;
                    data++;
                    *data = pixel >> 8;
                    data++;
                    *data = pixel >> 16;
                    data++;
                }
            }

            in_uint8s(fs, 512);
        }

        g_memcpy(mask, fs->p, 128); /* mask */
    }

    free_stream(fs);
    return 0;
}

/*****************************************************************************/
int
xrdp_wm_send_pointer(struct xrdp_wm *self, int cache_idx,
                     char *data, char *mask, int x, int y, int bpp)
{
    return libxrdp_send_pointer(self->session, cache_idx, data, mask,
                                x, y, bpp);
}

/*****************************************************************************/
int
xrdp_wm_set_pointer(struct xrdp_wm *self, int cache_idx)
{
    return libxrdp_set_pointer(self->session, cache_idx);
}

/*****************************************************************************/
/* convert hex string to int */
unsigned int
xrdp_wm_htoi (const char *ptr)
{
    unsigned int value = 0;
    char ch = *ptr;

    while (ch == ' ' || ch == '\t')
    {
        ch = *(++ptr);
    }

    for (;;)
    {
        if (ch >= '0' && ch <= '9')
        {
            value = (value << 4) + (ch - '0');
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            value = (value << 4) + (ch - 'A' + 10);
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            value = (value << 4) + (ch - 'a' + 10);
        }
        else
        {
            return value;
        }

        ch = *(++ptr);
    }
}

/*****************************************************************************/
int
xrdp_wm_load_static_colors_plus(struct xrdp_wm *self, char *autorun_name)
{
    int bindex;
    int gindex;
    int rindex;

    int fd;
    int index;
    char *val;
    struct list *names;
    struct list *values;

    if (autorun_name != 0)
    {
        autorun_name[0] = 0;
    }

    /* initialize with defaults */
    self->black      = HCOLOR(self->screen->bpp, 0x000000);
    self->grey       = HCOLOR(self->screen->bpp, 0xc0c0c0);
    self->dark_grey  = HCOLOR(self->screen->bpp, 0x808080);
    self->blue       = HCOLOR(self->screen->bpp, 0x0000ff);
    self->dark_blue  = HCOLOR(self->screen->bpp, 0x00007f);
    self->white      = HCOLOR(self->screen->bpp, 0xffffff);
    self->red        = HCOLOR(self->screen->bpp, 0xff0000);
    self->green      = HCOLOR(self->screen->bpp, 0x00ff00);
    self->background = HCOLOR(self->screen->bpp, 0x000000);

    /* now load them from the globals in xrdp.ini if defined */
    fd = g_file_open(self->session->xrdp_ini);

    if (fd >= 0)
    {
        names = list_create();
        names->auto_free = 1;
        values = list_create();
        values->auto_free = 1;

        if (file_read_section(fd, "globals", names, values) == 0)
        {
            for (index = 0; index < names->count; index++)
            {
                val = (char *)list_get_item(names, index);

                if (val != 0)
                {
                    if (g_strcasecmp(val, "black") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->black = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "grey") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->grey = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "dark_grey") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->dark_grey = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "blue") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->blue = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "dark_blue") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->dark_blue = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "white") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->white = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "red") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->red = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "green") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->green = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "background") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->background = HCOLOR(self->screen->bpp, xrdp_wm_htoi(val));
                    }
                    else if (g_strcasecmp(val, "autorun") == 0)
                    {
                        val = (char *)list_get_item(values, index);

                        if (autorun_name != 0)
                        {
                            g_strncpy(autorun_name, val, 255);
                        }
                    }
                    else if (g_strcasecmp(val, "hidelogwindow") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        self->hide_log_window = g_text2bool(val);
                    }
                    else if (g_strcasecmp(val, "pamerrortxt") == 0)
                    {
                        val = (char *)list_get_item(values, index);
                        g_strncpy(self->pamerrortxt, val, 255);
                    }
                }
            }
        }

        list_delete(names);
        list_delete(values);
        g_file_close(fd);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "xrdp_wm_load_static_colors: Could not read xrdp.ini file %s", self->session->xrdp_ini);
    }

    if (self->screen->bpp == 8)
    {
        /* rgb332 */
        for (bindex = 0; bindex < 4; bindex++)
        {
            for (gindex = 0; gindex < 8; gindex++)
            {
                for (rindex = 0; rindex < 8; rindex++)
                {
                    self->palette[(bindex << 6) | (gindex << 3) | rindex] =
                        (((rindex << 5) | (rindex << 2) | (rindex >> 1)) << 16) |
                        (((gindex << 5) | (gindex << 2) | (gindex >> 1)) << 8) |
                        ((bindex << 6) | (bindex << 4) | (bindex << 2) | (bindex));
                }
            }
        }

        xrdp_wm_send_palette(self);
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_wm_load_static_pointers(struct xrdp_wm *self)
{
    struct xrdp_pointer_item pointer_item;
    char file_path[256];

    LOG_DEVEL(LOG_LEVEL_TRACE, "sending cursor");
    g_snprintf(file_path, 255, "%s/cursor1.cur", XRDP_SHARE_PATH);
    g_memset(&pointer_item, 0, sizeof(pointer_item));
    xrdp_wm_load_pointer(self, file_path, pointer_item.data,
                         pointer_item.mask, &pointer_item.x, &pointer_item.y);
    xrdp_cache_add_pointer_static(self->cache, &pointer_item, 1);
    LOG_DEVEL(LOG_LEVEL_TRACE, "sending cursor");
    g_snprintf(file_path, 255, "%s/cursor0.cur", XRDP_SHARE_PATH);
    g_memset(&pointer_item, 0, sizeof(pointer_item));
    xrdp_wm_load_pointer(self, file_path, pointer_item.data,
                         pointer_item.mask, &pointer_item.x, &pointer_item.y);
    xrdp_cache_add_pointer_static(self->cache, &pointer_item, 0);
    return 0;
}

/*****************************************************************************/
int
xrdp_wm_init(struct xrdp_wm *self)
{
    int fd;
    int index;
    struct list *names;
    struct list *values;
    char *q;
    const char *r;
    char param[256];
    char default_section_name[256];
    char section_name[256];
    char autorun_name[256];

    LOG(LOG_LEVEL_DEBUG, "in xrdp_wm_init: ");

    load_xrdp_config(self->xrdp_config, self->session->xrdp_ini,
                     self->screen->bpp);

    /* global channels allow */
    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    if (file_by_name_read_section(self->session->xrdp_ini,
                                  "Channels", names, values) == 0)
    {
        int chan_id;
        int chan_count = libxrdp_get_channel_count(self->session);
        const char *disabled_str = NULL;

        for (chan_id = 0 ; chan_id < chan_count ; ++chan_id)
        {
            char chan_name[16];
            if (libxrdp_query_channel(self->session, chan_id, chan_name,
                                      NULL) == 0)
            {
                int disabled = 1; /* Channels disabled if not found */

                for (index = 0; index < names->count; index++)
                {
                    q = (char *) list_get_item(names, index);
                    if (g_strcasecmp(q, chan_name) == 0)
                    {
                        r = (const char *) list_get_item(values, index);
                        disabled = !g_text2bool(r);
                        break;
                    }
                }
                disabled_str = (disabled) ? "disabled" : "enabled";
                LOG(LOG_LEVEL_DEBUG, "xrdp_wm_init: "
                    "channel %s channel id %d is %s",
                    chan_name, chan_id, disabled_str);

                libxrdp_disable_channel(self->session, chan_id, disabled);
            }
        }
    }
    list_delete(names);
    list_delete(values);

    xrdp_wm_load_static_colors_plus(self, autorun_name);
    xrdp_wm_load_static_pointers(self);
    self->screen->bg_color = self->xrdp_config->cfg_globals.ls_top_window_bg_color;

    if (self->session->client_info->rdp_autologin)
    {
        /*
         * NOTE: this should eventually be accessed from self->xrdp_config
         */

        fd = g_file_open(self->session->xrdp_ini);
        if (fd != -1)
        {
            names = list_create();
            names->auto_free = 1;
            values = list_create();
            values->auto_free = 1;

            /* pick up the first section name except for 'globals', 'Logging', 'channels'
             * in xrdp.ini and use it as default section name */
            file_read_sections(fd, names);
            default_section_name[0] = '\0';
            for (index = 0; index < names->count; index++)
            {
                q = (char *)list_get_item(names, index);
                if ((g_strncasecmp("globals", q, 8) != 0) &&
                        (g_strncasecmp("Logging", q, 8) != 0) &&
                        (g_strncasecmp("LoggingPerLogger", q, 17) != 0) &&
                        (g_strncasecmp("channels", q, 9) != 0))
                {
                    g_strncpy(default_section_name, q, 255);
                    break;
                }
            }

            /* look for module name to be loaded */
            if (autorun_name[0] != 0)
            {
                /* if autorun is configured in xrdp.ini, we enforce that module to be loaded */
                g_strncpy(section_name, autorun_name, 255);
            }
            else if (self->session->client_info->domain[0] != '\0' &&
                     self->session->client_info->domain[0] != '_')
            {
                /* domain names that starts with '_' are reserved for IP/DNS to
                 * simplify for the user in a proxy setup */

                /* we use the domain name as the module name to be loaded */
                g_strncpy(section_name,
                          self->session->client_info->domain, 255);
            }
            else
            {
                /* if no domain is given, and autorun is not specified in xrdp.ini
                 * use default_section_name as section_name  */
                g_strncpy(section_name, default_section_name, 255);
            }

            list_clear(names);

            /* if given section name doesn't match any sections configured
             * in xrdp.ini, fallback to default_section_name */
            if (file_read_section(fd, section_name, names, values) != 0)
            {
                LOG(LOG_LEVEL_INFO,
                    "Module \"%s\" specified by %s from %s port %s "
                    "is not configured. Using \"%s\" instead.",
                    section_name,
                    self->session->client_info->username,
                    self->session->client_info->client_addr,
                    self->session->client_info->client_port,
                    default_section_name);
                list_clear(names);
                list_clear(values);

                g_strncpy(section_name, default_section_name, 255);
            }

            /* look for the required module in xrdp.ini, fetch its parameters */
            if (file_read_section(fd, section_name, names, values) == 0)
            {
                for (index = 0; index < names->count; index++)
                {
                    q = (char *)list_get_item(names, index);
                    r = (char *)list_get_item(values, index);

                    if (g_strncmp("password", q, 255) == 0)
                    {
                        /* if the password has been asked for by the module, use what the
                           client says.
                           if the password has been manually set in the config, use that
                           instead of what the client says. */
                        if (g_strncmp("ask", r, 3) == 0)
                        {
                            r = self->session->client_info->password;
                        }
                    }
                    else if (g_strncmp("username", q, 255) == 0)
                    {
                        /* if the username has been asked for by the module, use what the
                           client says.
                           if the username has been manually set in the config, use that
                           instead of what the client says. */
                        if (g_strncmp("ask", r, 3) == 0)
                        {
                            r = self->session->client_info->username;
                        }
                    }
                    else if (g_strncmp("ip", q, 255) == 0)
                    {
                        /* if the ip has been asked for by the module, use what the
                         client says (target ip should be in 'domain' field, when starting with "_")
                         if the ip has been manually set in the config, use that
                         instead of what the client says. */
                        if (g_strncmp("ask", r, 3) == 0)
                        {
                            if (self->session->client_info->domain[0] == '_')
                            {
                                g_strncpy(param, &self->session->client_info->domain[1], 255);
                                r = param;
                            }

                        }
                    }
                    else if (g_strncmp("port", q, 255) == 0)
                    {
                        if (g_strncmp("ask3389", r, 7) == 0)
                        {
                            r = "3389"; /* use default */
                        }
                    }

                    list_add_item(self->mm->login_names, (long)g_strdup(q));
                    list_add_item(self->mm->login_values, (long)g_strdup(r));
                }

                /*
                 * Skip the login box and go straight to the connection phase
                 */
                xrdp_wm_set_login_state(self, WMLS_START_CONNECT);
            }
            else
            {
                /* Hopefully, we never reach here. */
                LOG(LOG_LEVEL_WARNING,
                    "Control should never reach %s:%d", __FILE__, __LINE__);
            }

            list_delete(names);
            list_delete(values);
            g_file_close(fd);
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "xrdp_wm_init: Could not read xrdp.ini file %s",
                self->session->xrdp_ini);
        }
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "   xrdp_wm_init: no autologin / auto run detected, draw login window");
        xrdp_login_wnd_create(self);
        /* clear screen */
        xrdp_bitmap_invalidate(self->screen, 0);
        xrdp_wm_set_focused(self, self->login_window);
        xrdp_wm_set_login_state(self, WMLS_USER_PROMPT);
    }

    LOG(LOG_LEVEL_DEBUG, "out xrdp_wm_init: ");
    return 0;
}

/*****************************************************************************/
/* returns the number for rects visible for an area relative to a drawable */
/* putting the rects in region */
int
xrdp_wm_get_vis_region(struct xrdp_wm *self, struct xrdp_bitmap *bitmap,
                       int x, int y, int cx, int cy,
                       struct xrdp_region *region, int clip_children)
{
    int i;
    struct xrdp_bitmap *p;
    struct xrdp_rect a;
    struct xrdp_rect b;

    /* area we are drawing */
    MAKERECT(a, bitmap->left + x, bitmap->top + y, cx, cy);
    p = bitmap->parent;

    while (p != 0)
    {
        RECTOFFSET(a, p->left, p->top);
        p = p->parent;
    }

    a.left = MAX(self->screen->left, a.left);
    a.top = MAX(self->screen->top, a.top);
    a.right = MIN(self->screen->left + self->screen->width, a.right);
    a.bottom = MIN(self->screen->top + self->screen->height, a.bottom);
    xrdp_region_add_rect(region, &a);

    if (clip_children)
    {
        /* loop through all windows in z order */
        for (i = 0; i < self->screen->child_list->count; i++)
        {
            p = (struct xrdp_bitmap *)list_get_item(self->screen->child_list, i);

            if (p == bitmap || p == bitmap->parent)
            {
                return 0;
            }

            MAKERECT(b, p->left, p->top, p->width, p->height);
            xrdp_region_subtract_rect(region, &b);
        }
    }

    return 0;
}

/*****************************************************************************/
/* return the window at x, y on the screen */
static struct xrdp_bitmap *
xrdp_wm_at_pos(struct xrdp_bitmap *wnd, int x, int y,
               struct xrdp_bitmap **wnd1)
{
    int i;
    struct xrdp_bitmap *p;
    struct xrdp_bitmap *q;

    /* loop through all windows in z order */
    for (i = 0; i < wnd->child_list->count; i++)
    {
        p = (struct xrdp_bitmap *)list_get_item(wnd->child_list, i);

        if (x >= p->left && y >= p->top && x < p->left + p->width &&
                y < p->top + p->height)
        {
            if (wnd1 != 0)
            {
                *wnd1 = p;
            }

            q = xrdp_wm_at_pos(p, x - p->left, y - p->top, 0);

            if (q == 0)
            {
                return p;
            }
            else
            {
                return q;
            }
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_xor_pat(struct xrdp_wm *self, int x, int y, int cx, int cy)
{
    self->painter->clip_children = 0;
    self->painter->rop = 0x5a;
    xrdp_painter_begin_update(self->painter);
    self->painter->use_clip = 0;
    self->painter->mix_mode = 1;
    self->painter->brush.pattern[0] = 0xaa;
    self->painter->brush.pattern[1] = 0x55;
    self->painter->brush.pattern[2] = 0xaa;
    self->painter->brush.pattern[3] = 0x55;
    self->painter->brush.pattern[4] = 0xaa;
    self->painter->brush.pattern[5] = 0x55;
    self->painter->brush.pattern[6] = 0xaa;
    self->painter->brush.pattern[7] = 0x55;
    self->painter->brush.x_origin = 0;
    self->painter->brush.y_origin = 0;
    self->painter->brush.style = 3;
    self->painter->bg_color = self->black;
    self->painter->fg_color = self->white;
    /* top */
    xrdp_painter_fill_rect(self->painter, self->screen, x, y, cx, 5);
    /* bottom */
    xrdp_painter_fill_rect(self->painter, self->screen, x, y + (cy - 5), cx, 5);
    /* left */
    xrdp_painter_fill_rect(self->painter, self->screen, x, y + 5, 5, cy - 10);
    /* right */
    xrdp_painter_fill_rect(self->painter, self->screen, x + (cx - 5), y + 5, 5,
                           cy - 10);
    xrdp_painter_end_update(self->painter);
    self->painter->rop = 0xcc;
    self->painter->clip_children = 1;
    self->painter->mix_mode = 0;
    return 0;
}

/*****************************************************************************/
/* return true if rect is totally exposed going in reverse z order */
/* from wnd up */
static int
xrdp_wm_is_rect_vis(struct xrdp_wm *self, struct xrdp_bitmap *wnd,
                    struct xrdp_rect *rect)
{
    struct xrdp_rect wnd_rect;
    struct xrdp_bitmap *b;
    int i;;

    /* if rect is part off screen */
    if (rect->left < 0)
    {
        return 0;
    }

    if (rect->top < 0)
    {
        return 0;
    }

    if (rect->right >= self->screen->width)
    {
        return 0;
    }

    if (rect->bottom >= self->screen->height)
    {
        return 0;
    }

    i = list_index_of(self->screen->child_list, (long)wnd);
    i--;

    while (i >= 0)
    {
        b = (struct xrdp_bitmap *)list_get_item(self->screen->child_list, i);
        MAKERECT(wnd_rect, b->left, b->top, b->width, b->height);

        if (rect_intersect(rect, &wnd_rect, 0))
        {
            return 0;
        }

        i--;
    }

    return 1;
}

/*****************************************************************************/
static int
xrdp_wm_move_window(struct xrdp_wm *self, struct xrdp_bitmap *wnd,
                    int dx, int dy)
{
    struct xrdp_rect rect1;
    struct xrdp_rect rect2;
    struct xrdp_region *r;
    int i;

    MAKERECT(rect1, wnd->left, wnd->top, wnd->width, wnd->height);

    self->painter->clip_children = 0;
    if (xrdp_wm_is_rect_vis(self, wnd, &rect1))
    {
        rect2 = rect1;
        RECTOFFSET(rect2, dx, dy);

        if (xrdp_wm_is_rect_vis(self, wnd, &rect2))
        {
            xrdp_painter_begin_update(self->painter);
            xrdp_painter_copy(self->painter, self->screen, self->screen,
                              wnd->left + dx, wnd->top + dy,
                              wnd->width, wnd->height,
                              wnd->left, wnd->top);
            xrdp_painter_end_update(self->painter);

            wnd->left += dx;
            wnd->top += dy;
            r = xrdp_region_create(self);
            xrdp_region_add_rect(r, &rect1);
            xrdp_region_subtract_rect(r, &rect2);
            i = 0;

            while (xrdp_region_get_rect(r, i, &rect1) == 0)
            {
                xrdp_bitmap_invalidate(self->screen, &rect1);
                i++;
            }

            xrdp_region_delete(r);
            self->painter->clip_children = 1;
            return 0;
        }
    }
    self->painter->clip_children = 1;

    wnd->left += dx;
    wnd->top += dy;
    xrdp_bitmap_invalidate(self->screen, &rect1);
    xrdp_bitmap_invalidate(wnd, 0);
    return 0;
}


/*****************************************************************************/
static int
xrdp_wm_undraw_dragging_box(struct xrdp_wm *self, int do_begin_end)
{
    int boxx;
    int boxy;

    if (self == 0)
    {
        return 0;
    }

    if (self->dragging)
    {
        if (self->draggingxorstate)
        {
            if (do_begin_end)
            {
                xrdp_painter_begin_update(self->painter);
            }

            boxx = self->draggingx - self->draggingdx;
            boxy = self->draggingy - self->draggingdy;
            xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
            self->draggingxorstate = 0;

            if (do_begin_end)
            {
                xrdp_painter_end_update(self->painter);
            }
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_draw_dragging_box(struct xrdp_wm *self, int do_begin_end)
{
    int boxx;
    int boxy;

    if (self == 0)
    {
        return 0;
    }

    if (self->dragging)
    {
        if (!self->draggingxorstate)
        {
            if (do_begin_end)
            {
                xrdp_painter_begin_update(self->painter);
            }

            boxx = self->draggingx - self->draggingdx;
            boxy = self->draggingy - self->draggingdy;
            xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
            self->draggingxorstate = 1;

            if (do_begin_end)
            {
                xrdp_painter_end_update(self->painter);
            }
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_mouse_move(struct xrdp_wm *self, int x, int y)
{
    struct xrdp_bitmap *b;

    if (self == 0)
    {
        return 0;
    }

    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    if (x >= self->screen->width)
    {
        x = self->screen->width;
    }

    if (y >= self->screen->height)
    {
        y = self->screen->height;
    }

    self->mouse_x = x;
    self->mouse_y = y;

    if (self->dragging)
    {
        xrdp_painter_begin_update(self->painter);
        xrdp_wm_undraw_dragging_box(self, 0);
        self->draggingx = x;
        self->draggingy = y;
        xrdp_wm_draw_dragging_box(self, 0);
        xrdp_painter_end_update(self->painter);
        return 0;
    }

    b = xrdp_wm_at_pos(self->screen, x, y, 0);

    if (b == 0) /* if b is null, the movement must be over the screen */
    {
        if (self->screen->pointer != self->current_pointer)
        {
            xrdp_wm_set_pointer(self, self->screen->pointer);
            self->current_pointer = self->screen->pointer;
        }

        if (self->mm->mod != 0) /* if screen is mod controlled */
        {
            if (self->mm->mod->mod_event != 0)
            {
                self->mm->mod->mod_event(self->mm->mod, WM_MOUSEMOVE, x, y, 0, 0);
            }
        }
    }

    if (self->button_down != 0)
    {
        if (b == self->button_down && self->button_down->state == 0)
        {
            self->button_down->state = 1;
            xrdp_bitmap_invalidate(self->button_down, 0);
        }
        else if (b != self->button_down)
        {
            self->button_down->state = 0;
            xrdp_bitmap_invalidate(self->button_down, 0);
        }
    }

    if (b != 0)
    {
        if (!self->dragging)
        {
            if (b->pointer != self->current_pointer)
            {
                xrdp_wm_set_pointer(self, b->pointer);
                self->current_pointer = b->pointer;
            }

            xrdp_bitmap_def_proc(b, WM_MOUSEMOVE,
                                 xrdp_bitmap_from_screenx(b, x),
                                 xrdp_bitmap_from_screeny(b, y));

            if (self->button_down == 0)
            {
                if (b->notify != 0)
                {
                    b->notify(b->owner, b, 2, x, y);
                }
            }
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_clear_popup(struct xrdp_wm *self)
{
    int i;
    struct xrdp_rect rect;
    //struct xrdp_bitmap* b;

    //b = 0;
    if (self->popup_wnd != 0)
    {
        //b = self->popup_wnd->popped_from;
        i = list_index_of(self->screen->child_list, (long)self->popup_wnd);
        list_remove_item(self->screen->child_list, i);
        MAKERECT(rect, self->popup_wnd->left, self->popup_wnd->top,
                 self->popup_wnd->width, self->popup_wnd->height);
        xrdp_bitmap_invalidate(self->screen, &rect);
        xrdp_bitmap_delete(self->popup_wnd);
    }

    //xrdp_wm_set_focused(self, b->parent);
    return 0;
}

/*****************************************************************************/
int
xrdp_wm_mouse_click(struct xrdp_wm *self, int x, int y, int but, int down)
{
    struct xrdp_bitmap *control;
    struct xrdp_bitmap *focus_out_control;
    struct xrdp_bitmap *wnd;
    int newx;
    int newy;
    int oldx;
    int oldy;

    if (self == 0)
    {
        return 0;
    }

    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    if (x >= self->screen->width)
    {
        x = self->screen->width;
    }

    if (y >= self->screen->height)
    {
        y = self->screen->height;
    }

    if (self->dragging && but == 1 && !down && self->dragging_window != 0)
    {
        /* if done dragging */
        self->draggingx = x;
        self->draggingy = y;
        newx = self->draggingx - self->draggingdx;
        newy = self->draggingy - self->draggingdy;
        oldx = self->dragging_window->left;
        oldy = self->dragging_window->top;

        /* draw xor box one more time */
        if (self->draggingxorstate)
        {
            xrdp_wm_xor_pat(self, newx, newy, self->draggingcx, self->draggingcy);
        }

        self->draggingxorstate = 0;
        /* move screen to new location */
        xrdp_wm_move_window(self, self->dragging_window, newx - oldx, newy - oldy);
        self->dragging_window = 0;
        self->dragging = 0;
    }

    wnd = 0;
    control = xrdp_wm_at_pos(self->screen, x, y, &wnd);

    if (control == 0)
    {
        if (self->mm->mod != 0) /* if screen is mod controlled */
        {
            if (self->mm->mod->mod_event != 0)
            {
                if (down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_MOUSEMOVE, x, y, 0, 0);
                }
                if (but == 1 && down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_LBUTTONDOWN, x, y, 0, 0);
                }
                else if (but == 1 && !down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_LBUTTONUP, x, y, 0, 0);
                }

                if (but == 2 && down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_RBUTTONDOWN, x, y, 0, 0);
                }
                else if (but == 2 && !down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_RBUTTONUP, x, y, 0, 0);
                }

                if (but == 3 && down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON3DOWN, x, y, 0, 0);
                }
                else if (but == 3 && !down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON3UP, x, y, 0, 0);
                }

                if (but == 8 && down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON8DOWN, x, y, 0, 0);
                }
                else if (but == 8 && !down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON8UP, x, y, 0, 0);
                }
                if (but == 9 && down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON9DOWN, x, y, 0, 0);
                }
                else if (but == 9 && !down)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON9UP, x, y, 0, 0);
                }
                /* vertical scroll */

                if (but == 4)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON4DOWN,
                                             self->mouse_x, self->mouse_y, 0, 0);
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON4UP,
                                             self->mouse_x, self->mouse_y, 0, 0);
                }

                if (but == 5)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON5DOWN,
                                             self->mouse_x, self->mouse_y, 0, 0);
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON5UP,
                                             self->mouse_x, self->mouse_y, 0, 0);
                }

                /* horizontal scroll */

                if (but == 6)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON6DOWN,
                                             self->mouse_x, self->mouse_y, 0, 0);
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON6UP,
                                             self->mouse_x, self->mouse_y, 0, 0);
                }

                if (but == 7)
                {
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON7DOWN,
                                             self->mouse_x, self->mouse_y, 0, 0);
                    self->mm->mod->mod_event(self->mm->mod, WM_BUTTON7UP,
                                             self->mouse_x, self->mouse_y, 0, 0);
                }
            }
        }
    }

    if (self->popup_wnd != 0)
    {
        if (self->popup_wnd == control && !down)
        {
            xrdp_bitmap_def_proc(self->popup_wnd, WM_LBUTTONUP, x, y);
            xrdp_wm_clear_popup(self);
            self->button_down = 0;
            return 0;
        }
        else if (self->popup_wnd != control && down)
        {
            xrdp_wm_clear_popup(self);
            self->button_down = 0;
            return 0;
        }
    }

    if (control != 0)
    {
        if (wnd != 0)
        {
            if (wnd->modal_dialog != 0) /* if window has a modal dialog */
            {
                return 0;
            }

            if (control == wnd)
            {
            }
            else if (control->tab_stop)
            {
                focus_out_control = wnd->focused_control;
                wnd->focused_control = control;
                xrdp_bitmap_invalidate(focus_out_control, 0);
                xrdp_bitmap_invalidate(control, 0);
            }
        }

        if ((control->type == WND_TYPE_BUTTON ||
                control->type == WND_TYPE_COMBO) &&
                but == 1 && !down && self->button_down == control)
        {
            /* if clicking up on a button that was clicked down */
            self->button_down = 0;
            control->state = 0;
            xrdp_bitmap_invalidate(control, 0);

            if (control->parent != 0)
            {
                if (control->parent->notify != 0)
                {
                    /* control can be invalid after this */
                    control->parent->notify(control->owner, control, 1, x, y);
                }
            }
        }
        else if ((control->type == WND_TYPE_BUTTON ||
                  control->type == WND_TYPE_COMBO) &&
                 but == 1 && down)
        {
            /* if clicking down on a button or combo */
            self->button_down = control;
            control->state = 1;
            xrdp_bitmap_invalidate(control, 0);

            if (control->type == WND_TYPE_COMBO)
            {
                xrdp_wm_pu(self, control);
            }
        }
        else if (but == 1 && down)
        {
            if (self->popup_wnd == 0)
            {
                xrdp_wm_set_focused(self, wnd);

                if (control->type == WND_TYPE_WND && y < (control->top + 21))
                {
                    /* if dragging */
                    if (self->dragging) /* rarely happens */
                    {
                        newx = self->draggingx - self->draggingdx;
                        newy = self->draggingy - self->draggingdy;

                        if (self->draggingxorstate)
                        {
                            xrdp_wm_xor_pat(self, newx, newy,
                                            self->draggingcx, self->draggingcy);
                        }

                        self->draggingxorstate = 0;
                    }

                    self->dragging = 1;
                    self->dragging_window = control;
                    self->draggingorgx = control->left;
                    self->draggingorgy = control->top;
                    self->draggingx = x;
                    self->draggingy = y;
                    self->draggingdx = x - control->left;
                    self->draggingdy = y - control->top;
                    self->draggingcx = control->width;
                    self->draggingcy = control->height;
                }
            }
        }
    }
    else
    {
        xrdp_wm_set_focused(self, 0);
    }

    /* no matter what, mouse is up, reset button_down */
    if (but == 1 && !down && self->button_down != 0)
    {
        self->button_down = 0;
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_key(struct xrdp_wm *self, int device_flags, int scan_code)
{
    int msg;
    struct xrdp_key_info *ki;

    /*g_printf("count %d\n", self->key_down_list->count);*/
    scan_code = scan_code % 128;

    if (self->popup_wnd != 0)
    {
        xrdp_wm_clear_popup(self);
        return 0;
    }

    // workaround odd shift behavior
    // see https://github.com/neutrinolabs/xrdp/issues/397
    if (scan_code == 42 && device_flags == (KBD_FLAG_UP | KBD_FLAG_EXT))
    {
        return 0;
    }

    if (device_flags & KBD_FLAG_UP) /* 0x8000 */
    {
        self->keys[scan_code] = 0;
        msg = WM_KEYUP;
    }
    else /* key down */
    {
        self->keys[scan_code] = 1 | device_flags;
        msg = WM_KEYDOWN;

        switch (scan_code)
        {
            case 58:
                self->caps_lock = !self->caps_lock;
                break; /* caps lock */
            case 69:
                self->num_lock = !self->num_lock;
                break; /* num lock */
            case 70:
                self->scroll_lock = !self->scroll_lock;
                break; /* scroll lock */
        }
    }

    if (self->mm->mod != 0)
    {
        if (self->mm->mod->mod_event != 0)
        {
            ki = get_key_info_from_scan_code
                 (device_flags, scan_code, self->keys, self->caps_lock,
                  self->num_lock, self->scroll_lock,
                  &(self->keymap));

            if (ki != 0)
            {
                self->mm->mod->mod_event(self->mm->mod, msg, ki->chr, ki->sym,
                                         scan_code, device_flags);
            }
        }
    }
    else if (self->focused_window != 0)
    {
        xrdp_bitmap_def_proc(self->focused_window,
                             msg, scan_code, device_flags);
    }

    return 0;
}

/*****************************************************************************/
/* happens when client gets focus and sends key modifier info */
int
xrdp_wm_key_sync(struct xrdp_wm *self, int device_flags, int key_flags)
{
    self->num_lock = 0;
    self->scroll_lock = 0;
    self->caps_lock = 0;

    if (key_flags & 1)
    {
        self->scroll_lock = 1;
    }

    if (key_flags & 2)
    {
        self->num_lock = 1;
    }

    if (key_flags & 4)
    {
        self->caps_lock = 1;
    }

    if (self->mm->mod != 0)
    {
        if (self->mm->mod->mod_event != 0)
        {
            self->mm->mod->mod_event(self->mm->mod, 17, key_flags, device_flags,
                                     key_flags, device_flags);
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_key_unicode(struct xrdp_wm *self, int device_flags, int unicode)
{
    int index;

    for (index = XR_MIN_KEY_CODE; index < XR_MAX_KEY_CODE; index++)
    {
        if (unicode == self->keymap.keys_noshift[index].chr)
        {
            xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
            return 0;
        }
    }

    for (index = XR_MIN_KEY_CODE; index < XR_MAX_KEY_CODE; index++)
    {
        if (unicode == self->keymap.keys_shift[index].chr)
        {
            if (device_flags & KBD_FLAG_UP)
            {
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
                xrdp_wm_key(self, KBD_FLAG_UP, XR_RDP_SCAN_LSHIFT);
            }
            else
            {
                xrdp_wm_key(self, KBD_FLAG_DOWN, XR_RDP_SCAN_LSHIFT);
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
            }
            return 0;
        }
    }

    for (index = XR_MIN_KEY_CODE; index < XR_MAX_KEY_CODE; index++)
    {
        if (unicode == self->keymap.keys_altgr[index].chr)
        {
            if (device_flags & KBD_FLAG_UP)
            {
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
                xrdp_wm_key(self, KBD_FLAG_UP | KBD_FLAG_EXT,
                            XR_RDP_SCAN_ALT);
            }
            else
            {
                xrdp_wm_key(self, KBD_FLAG_DOWN | KBD_FLAG_EXT,
                            XR_RDP_SCAN_ALT);
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
            }
            return 0;
        }
    }

    for (index = XR_MIN_KEY_CODE; index < XR_MAX_KEY_CODE; index++)
    {
        if (unicode == self->keymap.keys_shiftaltgr[index].chr)
        {
            if (device_flags & KBD_FLAG_UP)
            {
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
                xrdp_wm_key(self, KBD_FLAG_UP | KBD_FLAG_EXT, XR_RDP_SCAN_ALT);
                xrdp_wm_key(self, KBD_FLAG_UP, XR_RDP_SCAN_LSHIFT);
            }
            else
            {
                xrdp_wm_key(self, KBD_FLAG_DOWN, XR_RDP_SCAN_LSHIFT);
                xrdp_wm_key(self, KBD_FLAG_DOWN | KBD_FLAG_EXT,
                            XR_RDP_SCAN_ALT);
                xrdp_wm_key(self, device_flags, index - XR_MIN_KEY_CODE);
            }
            return 0;
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_pu(struct xrdp_wm *self, struct xrdp_bitmap *control)
{
    int x;
    int y;

    if (self == 0)
    {
        return 0;
    }

    if (control == 0)
    {
        return 0;
    }

    self->popup_wnd = xrdp_bitmap_create(control->width, DEFAULT_WND_SPECIAL_H,
                                         self->screen->bpp,
                                         WND_TYPE_SPECIAL, self);
    self->popup_wnd->popped_from = control;
    self->popup_wnd->parent = self->screen;
    self->popup_wnd->owner = self->screen;
    x = xrdp_bitmap_to_screenx(control, 0);
    y = xrdp_bitmap_to_screeny(control, 0);
    self->popup_wnd->left = x;
    self->popup_wnd->top = y + control->height;
    self->popup_wnd->item_index = control->item_index;
    list_insert_item(self->screen->child_list, 0, (long)self->popup_wnd);
    xrdp_bitmap_invalidate(self->popup_wnd, 0);
    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_process_input_mouse(struct xrdp_wm *self, int device_flags,
                            int x, int y)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "mouse event flags %4.4x x %d y %d", device_flags, x, y);

    if (device_flags & PTRFLAGS_MOVE)
    {
        xrdp_wm_mouse_move(self, x, y);
    }

    if (device_flags & PTRFLAGS_BUTTON1)
    {
        if (device_flags & PTRFLAGS_DOWN)
        {
            xrdp_wm_mouse_click(self, x, y, 1, 1);
        }
        else
        {
            xrdp_wm_mouse_click(self, x, y, 1, 0);
        }
    }

    if (device_flags & PTRFLAGS_BUTTON2)
    {
        if (device_flags & PTRFLAGS_DOWN)
        {
            xrdp_wm_mouse_click(self, x, y, 2, 1);
        }
        else
        {
            xrdp_wm_mouse_click(self, x, y, 2, 0);
        }
    }

    if (device_flags & PTRFLAGS_BUTTON3)
    {
        if (device_flags & PTRFLAGS_DOWN)
        {
            xrdp_wm_mouse_click(self, x, y, 3, 1);
        }
        else
        {
            xrdp_wm_mouse_click(self, x, y, 3, 0);
        }
    }

    /* vertical mouse wheel */
    if (device_flags & PTRFLAGS_WHEEL)
    {
        if (device_flags & PTRFLAGS_WHEEL_NEGATIVE)
        {
            xrdp_wm_mouse_click(self, 0, 0, 5, 0);
        }
        else
        {
            xrdp_wm_mouse_click(self, 0, 0, 4, 0);
        }
    }

    /* horizontal mouse wheel */

    /**
     * As mstsc does MOUSE not MOUSEX for horizontal scrolling,
     * PTRFLAGS_HWHEEL must be handled here.
     */
    if (device_flags & PTRFLAGS_HWHEEL)
    {
        if (device_flags & PTRFLAGS_WHEEL_NEGATIVE)
        {
            xrdp_wm_mouse_click(self, 0, 0, 6, 0);
        }
        else
        {
            xrdp_wm_mouse_click(self, 0, 0, 7, 0);
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_process_input_mousex(struct xrdp_wm *self, int device_flags,
                             int x, int y)
{
    if (device_flags & PTRXFLAGS_DOWN)
    {
        if (device_flags & PTRXFLAGS_BUTTON1)
        {
            xrdp_wm_mouse_click(self, x, y, 8, 1);
        }
        else if (device_flags & PTRXFLAGS_BUTTON2)
        {
            xrdp_wm_mouse_click(self, x, y, 9, 1);
        }
    }
    else
    {
        if (device_flags & PTRXFLAGS_BUTTON1)
        {
            xrdp_wm_mouse_click(self, x, y, 8, 0);
        }
        else if (device_flags & PTRXFLAGS_BUTTON2)
        {
            xrdp_wm_mouse_click(self, x, y, 9, 0);
        }
    }
    return 0;
}

/******************************************************************************/
/* param1 = MAKELONG(channel_id, flags)
   param2 = size
   param3 = pointer to data
   param4 = total size */
static int
xrdp_wm_process_channel_data(struct xrdp_wm *self,
                             tbus param1, tbus param2,
                             tbus param3, tbus param4)
{
    int rv;
    rv = 1;

    if (self->mm->mod != 0)
    {
        if (self->mm->usechansrv)
        {
            rv = xrdp_mm_process_channel_data(self->mm, param1, param2,
                                              param3, param4);
        }
        else
        {
            if (self->mm->mod->mod_event != 0)
            {
                rv = self->mm->mod->mod_event(self->mm->mod, 0x5555, param1, param2,
                                              param3, param4);
            }
        }
    }

    return rv;
}

/******************************************************************************/
/* this is the callbacks coming from libxrdp.so */
int
callback(intptr_t id, int msg, intptr_t param1, intptr_t param2,
         intptr_t param3, intptr_t param4)
{
    int rv;
    struct xrdp_wm *wm;
    struct xrdp_rect rect;

    if (id == 0) /* "id" should be "struct xrdp_process*" as long */
    {
        return 0;
    }

    wm = ((struct xrdp_process *)id)->wm;

    if (wm == 0)
    {
        return 0;
    }

    rv = 0;

    switch (msg)
    {
        case RDP_INPUT_SYNCHRONIZE:
            rv = xrdp_wm_key_sync(wm, param3, param1);
            break;
        case RDP_INPUT_SCANCODE:
            rv = xrdp_wm_key(wm, param3, param1);
            break;
        case RDP_INPUT_UNICODE:
            rv = xrdp_wm_key_unicode(wm, param3, param1);
            break;
        case RDP_INPUT_MOUSE:
            rv = xrdp_wm_process_input_mouse(wm, param3, param1, param2);
            break;
        case RDP_INPUT_MOUSEX:
            rv = xrdp_wm_process_input_mousex(wm, param3, param1, param2);
            break;
        case 0x4444: /* invalidate, this is not from RDP_DATA_PDU_INPUT */
            /* like the rest, it's from RDP_PDU_DATA with code 33 */
            /* it's the rdp client asking for a screen update */
            MAKERECT(rect, param1, param2, param3, param4);
            rv = xrdp_bitmap_invalidate(wm->screen, &rect);
            break;
        case 0x5555: /* called from xrdp_channel.c, channel data has come in,
                    pass it to module if there is one */
            rv = xrdp_wm_process_channel_data(wm, param1, param2, param3, param4);
            break;
        case 0x5556:
            rv = xrdp_mm_check_chan(wm->mm);
            break;
        case 0x5557:
            LOG(LOG_LEVEL_TRACE, "callback: frame ack %p", (void *) param1);
            xrdp_mm_frame_ack(wm->mm, param1);
            break;
        case 0x5558:
            xrdp_mm_drdynvc_up(wm->mm);
            break;
        case 0x5559:
            xrdp_mm_suppress_output(wm->mm, param1,
                                    LOWORD(param2), HIWORD(param2),
                                    LOWORD(param3), HIWORD(param3));
            break;
    }
    return rv;
}

/******************************************************************************/
/* returns error */
/* this gets called when there is nothing on any socket */
static int
xrdp_wm_login_state_changed(struct xrdp_wm *self)
{
    if (self == 0)
    {
        return 0;
    }

    LOG(LOG_LEVEL_DEBUG, "xrdp_wm_login_mode_changed: login_mode is %d", self->login_state);
    if (self->login_state == WMLS_RESET)
    {
        /* this is the initial state of the login window */
        xrdp_wm_set_login_state(self, WMLS_USER_PROMPT);
        list_clear(self->log);
        xrdp_wm_delete_all_children(self);
        self->dragging = 0;
        xrdp_wm_init(self);
    }
    else if (self->login_state == WMLS_START_CONNECT)
    {
        if (xrdp_mm_connect(self->mm) == 0)
        {
            xrdp_wm_set_login_state(self, WMLS_CONNECT_IN_PROGRESS);
            xrdp_wm_delete_all_children(self);
            self->dragging = 0;
        }
        else
        {
            /* we do nothing on connect error so far */
        }
    }
    else if (self->login_state == WMLS_CLEANUP)
    {
        xrdp_wm_delete_all_children(self);
        self->dragging = 0;
        xrdp_wm_set_login_state(self, WMLS_INACTIVE);
    }

    return 0;
}

/*****************************************************************************/
/* this is the log windows notify function */
static int
xrdp_wm_log_wnd_notify(struct xrdp_bitmap *wnd,
                       struct xrdp_bitmap *sender,
                       int msg, long param1, long param2)
{
    struct xrdp_painter *painter;
    struct xrdp_wm *wm;
    struct xrdp_rect rect;
    int index;
    char *text;

    if (wnd == 0)
    {
        return 0;
    }

    if (sender == 0)
    {
        return 0;
    }

    if (wnd->owner == 0)
    {
        return 0;
    }

    wm = wnd->wm;

    if (msg == 1) /* click */
    {
        if (sender->id == 1) /* ok button */
        {
            /* close the log window */
            MAKERECT(rect, wnd->left, wnd->top, wnd->width, wnd->height);
            xrdp_bitmap_delete(wnd);
            xrdp_bitmap_invalidate(wm->screen, &rect);

            /* if module is gone, reset the session when ok is clicked */
            if (wm->mm->mod_handle == 0)
            {
                /* make sure autologin is off */
                wm->session->client_info->rdp_autologin = 0;
                xrdp_wm_set_login_state(wm, WMLS_RESET); /* reset session */
            }
        }
    }
    else if (msg == WM_PAINT) /* 3 */
    {
        painter = (struct xrdp_painter *)param1;

        if (painter != 0)
        {
            painter->fg_color = wnd->wm->black;

            for (index = 0; index < wnd->wm->log->count; index++)
            {
                text = (char *)list_get_item(wnd->wm->log, index);
                xrdp_painter_draw_text(painter, wnd, 10, 30 + index * 15, text);
            }
        }
    }

    return 0;
}

static void
add_string_to_logwindow(const char *msg, struct list *log)
{
    const char *new_part_message;
    const char *current_pointer = msg;
    int len_done = 0;

    do
    {
        new_part_message = g_strndup(current_pointer, LOG_WINDOW_CHAR_PER_LINE);
        LOG(LOG_LEVEL_INFO, "%s", new_part_message);
        list_add_item(log, (tintptr) new_part_message);
        len_done += g_strlen(new_part_message);
        current_pointer += g_strlen(new_part_message);
    }
    while ((len_done < g_strlen(msg)) && (len_done < DEFAULT_STRING_LEN));
}

/*****************************************************************************/
int
xrdp_wm_show_log(struct xrdp_wm *self)
{
    struct xrdp_bitmap *but;
    int w;
    int h;
    int xoffset;
    int yoffset;
    int index;
    int primary_x_offset;
    int primary_y_offset;


    if (self->hide_log_window)
    {
        /* make sure autologin is off */
        self->session->client_info->rdp_autologin = 0;
        xrdp_wm_set_login_state(self, WMLS_RESET); /* reset session */
        return 0;
    }

    if (self->log_wnd == 0)
    {
        w = DEFAULT_WND_LOG_W;
        h = DEFAULT_WND_LOG_H;
        xoffset = 10;
        yoffset = 10;

        if (self->screen->width < w)
        {
            w = self->screen->width - 4;
            xoffset = 2;
        }

        if (self->screen->height < h)
        {
            h = self->screen->height - 4;
            yoffset = 2;
        }

        primary_x_offset = 0;
        primary_y_offset = 0;

        /* multimon scenario, draw log window on primary monitor */
        if (self->client_info->monitorCount > 1)
        {
            for (index = 0; index < self->client_info->monitorCount; index++)
            {
                if (self->client_info->minfo_wm[index].is_primary)
                {
                    primary_x_offset = self->client_info->minfo_wm[index].left;
                    primary_y_offset = self->client_info->minfo_wm[index].top;
                    break;
                }
            }
        }

        /* log window */
        self->log_wnd = xrdp_bitmap_create(w, h, self->screen->bpp,
                                           WND_TYPE_WND, self);
        list_add_item(self->screen->child_list, (long)self->log_wnd);
        self->log_wnd->parent = self->screen;
        self->log_wnd->owner = self->screen;
        self->log_wnd->bg_color = self->grey;
        self->log_wnd->left = primary_x_offset + xoffset;
        self->log_wnd->top = primary_y_offset + yoffset;
        set_string(&(self->log_wnd->caption1), "Connection Log");
        /* ok button */
        but = xrdp_bitmap_create(DEFAULT_BUTTON_W, DEFAULT_BUTTON_H, self->screen->bpp, WND_TYPE_BUTTON, self);
        list_insert_item(self->log_wnd->child_list, 0, (long)but);
        but->parent = self->log_wnd;
        but->owner = self->log_wnd;
        but->left = (w - DEFAULT_BUTTON_W) - xoffset;
        but->top = (h - DEFAULT_BUTTON_H) - yoffset;
        but->id = 1;
        but->tab_stop = 1;
        set_string(&but->caption1, "OK");
        self->log_wnd->focused_control = but;
        /* set notify function */
        self->log_wnd->notify = xrdp_wm_log_wnd_notify;
    }

    xrdp_wm_set_focused(self, self->log_wnd);
    xrdp_bitmap_invalidate(self->log_wnd, 0);

    return 0;
}

/*****************************************************************************/
int
xrdp_wm_log_msg(struct xrdp_wm *self, enum logLevels loglevel,
                const char *fmt, ...)
{
    va_list ap;
    char msg[256];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    LOG(loglevel, "xrdp_wm_log_msg: %s", msg);
    add_string_to_logwindow(msg, self->log);
    return 0;
}

/*****************************************************************************/
int
xrdp_wm_get_wait_objs(struct xrdp_wm *self, tbus *robjs, int *rc,
                      tbus *wobjs, int *wc, int *timeout)
{
    int i;

    if (self == 0)
    {
        return 0;
    }

    i = *rc;
    robjs[i++] = self->login_state_event;
    *rc = i;
    return xrdp_mm_get_wait_objs(self->mm, robjs, rc, wobjs, wc, timeout);
}

/******************************************************************************/
int
xrdp_wm_check_wait_objs(struct xrdp_wm *self)
{
    int rv;

    if (self == 0)
    {
        return 0;
    }

    rv = 0;

    if (g_is_wait_obj_set(self->login_state_event))
    {
        g_reset_wait_obj(self->login_state_event);
        xrdp_wm_login_state_changed(self);
    }

    if (rv == 0)
    {
        rv = xrdp_mm_check_wait_objs(self->mm);
    }

    return rv;
}

/*****************************************************************************/

static const char *
wm_login_state_to_str(enum wm_login_state login_state)
{
    const char *result = "unknown";
    /* Use a switch for this, as some compilers will warn about missing states
     */
    switch (login_state)
    {
        case WMLS_RESET:
            result = "WMLS_RESET";
            break;
        case WMLS_USER_PROMPT:
            result = "WMLS_USER_PROMPT";
            break;
        case WMLS_START_CONNECT:
            result = "WMLS_START_CONNECT";
            break;
        case WMLS_CONNECT_IN_PROGRESS:
            result = "WMLS_CONNECT_IN_PROGRESS";
            break;
        case WMLS_CLEANUP:
            result = "WMLS_CLEANUP";
            break;
        case WMLS_INACTIVE:
            result = "WMLS_INACTIVE";
    }

    return result;
}

/*****************************************************************************/
int
xrdp_wm_set_login_state(struct xrdp_wm *self, enum wm_login_state login_state)
{
    LOG(LOG_LEVEL_DEBUG, "Login state change request %s -> %s",
        wm_login_state_to_str(self->login_state),
        wm_login_state_to_str(login_state));

    self->login_state = login_state;
    g_set_wait_obj(self->login_state_event);
    return 0;
}
