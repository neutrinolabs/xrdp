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
 * main login window and login help window
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "base64.h"
#include "xrdp.h"
#include "log.h"
#include "string_calls.h"

#define ASK "ask"
#define ASK_LEN g_strlen(ASK)
#define BASE64PREFIX "{base64}"
#define BASE64PREFIX_LEN g_strlen(BASE64PREFIX)

/*****************************************************************************/
/* all login help screen events go here */
static int
xrdp_wm_login_help_notify(struct xrdp_bitmap *wnd,
                          struct xrdp_bitmap *sender,
                          int msg, long param1, long param2)
{
    struct xrdp_painter *p;

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

    if (msg == 1) /* click */
    {
        if (sender->id == 1) /* ok button */
        {
            if (sender->owner->notify != 0)
            {
                wnd->owner->notify(wnd->owner, wnd, 100, 1, 0); /* ok */
            }
        }
    }
    else if (msg == WM_PAINT) /* 3 */
    {
        p = (struct xrdp_painter *)param1;

        if (p != 0)
        {
            const int x = 10;
            int y = xrdp_painter_font_body_height(p) * 2;
            const int row_height = xrdp_painter_font_body_height(p);
            const int end_para_height = row_height * 3 / 2;

            p->fg_color = wnd->wm->black;
            xrdp_painter_draw_text(p, wnd, x, y, "You must be authenticated \
before using this");
            y += row_height;
            xrdp_painter_draw_text(p, wnd, x, y, "session.");
            y += end_para_height;
            xrdp_painter_draw_text(p, wnd, x, y, "Enter a valid username in \
the username edit box.");
            y += end_para_height;
            xrdp_painter_draw_text(p, wnd, x, y, "Enter the password in \
the password edit box.");
            y += end_para_height;
            xrdp_painter_draw_text(p, wnd, x, y, "Both the username and \
password are case");
            y += row_height;
            xrdp_painter_draw_text(p, wnd, x, y, "sensitive.");
            y += end_para_height;
            xrdp_painter_draw_text(p, wnd, x, y, "Contact your system \
administrator if you are");
            y += row_height;
            xrdp_painter_draw_text(p, wnd, x, y, "having problems \
logging on.");
        }
    }

    return 0;
}

#if 0
/*****************************************************************************/
static int
xrdp_wm_popup_notify(struct xrdp_bitmap *wnd,
                     struct xrdp_bitmap *sender,
                     int msg, int param1, int param2)
{
    return 0;
}
#endif

/*****************************************************************************/
int
xrdp_wm_delete_all_children(struct xrdp_wm *self)
{
    int index;
    struct xrdp_bitmap *b;
    struct xrdp_rect rect;

    for (index = self->screen->child_list->count - 1; index >= 0; index--)
    {
        b = (struct xrdp_bitmap *)list_get_item(self->screen->child_list, index);
        MAKERECT(rect, b->left, b->top, b->width, b->height);
        xrdp_bitmap_delete(b);
        xrdp_bitmap_invalidate(self->screen, &rect);
    }

    return 0;
}

/*****************************************************************************/
static int
set_mod_data_item(struct xrdp_mod_data *mod, char *name, char *value)
{
    int index;

    for (index = 0; index < mod->names->count; index++)
    {
        if (g_strncmp(name, (char *)list_get_item(mod->names, index), 255) == 0)
        {
            list_remove_item(mod->values, index);
            list_insert_item(mod->values, index, (long)g_strdup(value));
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_help_clicked(struct xrdp_bitmap *wnd)
{
    struct xrdp_bitmap *help;
    struct xrdp_bitmap *but;
    const int width =
        wnd->wm->xrdp_config->cfg_globals.ls_scaled.help_wnd_width;
    const int height =
        wnd->wm->xrdp_config->cfg_globals.ls_scaled.help_wnd_height;
    const int ok_height =
        wnd->wm->xrdp_config->cfg_globals.ls_scaled.default_btn_height;
    const char *ok_string = "OK";

    /* Get a width for the OK button */
    struct xrdp_painter *p = xrdp_painter_create(wnd->wm, wnd->wm->session);
    xrdp_painter_font_needed(p);
    const int ok_width = xrdp_painter_text_width(p, ok_string) +
                         DEFAULT_BUTTON_MARGIN_W;
    xrdp_painter_delete(p);

    /* create help screen */
    help = xrdp_bitmap_create(width, height, wnd->wm->screen->bpp,
                              WND_TYPE_WND, wnd->wm);
    list_insert_item(wnd->wm->screen->child_list, 0, (long)help);
    help->parent = wnd->wm->screen;
    help->owner = wnd;
    wnd->modal_dialog = help;
    help->bg_color = wnd->wm->grey;
    help->left = wnd->wm->screen->width / 2 - help->width / 2;
    help->top = wnd->wm->screen->height / 2 - help->height / 2;
    help->notify = xrdp_wm_login_help_notify;
    set_string(&help->caption1, "Login help");
    /* ok button */
    but = xrdp_bitmap_create(ok_width, ok_height, wnd->wm->screen->bpp,
                             WND_TYPE_BUTTON, wnd->wm);
    list_insert_item(help->child_list, 0, (long)but);
    but->parent = help;
    but->owner = help;
    but->left = ((help->width / 2) - (ok_width / 2)); /* center */
    but->top = help->height - ok_height - 15;
    but->id = 1;
    but->tab_stop = 1;
    set_string(&but->caption1, ok_string);
    /* draw it */
    help->focused_control = but;
    help->default_button = but;
    help->esc_button = but;
    xrdp_bitmap_invalidate(help, 0);
    xrdp_wm_set_focused(wnd->wm, help);
    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_cancel_clicked(struct xrdp_bitmap *wnd)
{
    if (wnd != 0)
    {
        if (wnd->wm != 0)
        {
            if (wnd->wm->pro_layer != 0)
            {
                g_set_wait_obj(wnd->wm->pro_layer->self_term_event);
            }
        }
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_wm_ok_clicked(struct xrdp_bitmap *wnd)
{
    struct xrdp_bitmap *combo;
    struct xrdp_bitmap *label;
    struct xrdp_bitmap *edit;
    struct xrdp_wm *wm;
    struct xrdp_mod_data *mod_data;
    int i;

    wm = wnd->wm;
    combo = xrdp_bitmap_get_child_by_id(wnd, 6);

    if (combo != 0)
    {
        mod_data = (struct xrdp_mod_data *)
                   list_get_item(combo->data_list, combo->item_index);

        if (mod_data != 0)
        {
            /* get the user typed values */
            i = 100;
            label = xrdp_bitmap_get_child_by_id(wnd, i);
            edit = xrdp_bitmap_get_child_by_id(wnd, i + 1);

            while (label != 0 && edit != 0)
            {
                set_mod_data_item(mod_data, label->caption1, edit->caption1);
                i += 2;
                label = xrdp_bitmap_get_child_by_id(wnd, i);
                edit = xrdp_bitmap_get_child_by_id(wnd, i + 1);
            }

            list_delete(wm->mm->login_names);
            list_delete(wm->mm->login_values);
            wm->mm->login_names = list_create();
            wm->mm->login_names->auto_free = 1;
            wm->mm->login_values = list_create();
            wm->mm->login_values->auto_free = 1;
            /* will copy these cause dialog gets freed */
            list_append_list_strdup(mod_data->names, wm->mm->login_names, 0);
            list_append_list_strdup(mod_data->values, wm->mm->login_values, 0);
            xrdp_wm_set_login_state(wm, WMLS_START_CONNECT);
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "Combo is NULL - potential programming error");
    }

    return 0;
}

/*****************************************************************************/
/**
* This is an internal function in this file used to parse the domain
* information sent from the client. If the information starts
* with '_' the domain field contains the IP/DNS to connect to.
* If the domain field contains an additional '__' the char that
* follows this '__' is an index number of a preferred combo choice.
* Valid values for this choice is 0-9. But this function will only return
* index numbers between 0 and the max number of combo items -1.
* Example: _192.168.1.2__1 result in a resultbuffer containing
* 192.168.1.2  and the return value will be 1. Meaning that
* index 1 is the preferred combo choice.
*
* Users can create shortcuts where this information is configured. These
* shortcuts simplifies login.
* @param originalDomainInfo indata to this function
* @param comboMax the max number of combo choices
* @param decode if true then we perform decoding of combo choice
* @param resultBuffer must be pre allocated before calling this function.
* Holds the IP. The size of this buffer must be 256 bytes
* @return the index number of the combobox that the user prefer.
* 0 if the user does not prefer any choice.
*/
static int
xrdp_wm_parse_domain_information(char *originalDomainInfo, int comboMax,
                                 int decode, char *resultBuffer)
{
    int ret;
    int pos;
    int comboxindex;
    char index[2];

    /* If the first char in the domain name is '_' we use the domain
       name as IP*/
    ret = 0; /* default return value */
    /* resultBuffer assumed to be 256 chars */
    g_memset(resultBuffer, 0, 256);
    if (originalDomainInfo[0] == '_')
    {
        /* we try to locate a number indicating what combobox index the user
         * prefer the information is loaded from domain field, from the client
         * We must use valid chars in the domain name.
         * Underscore is a valid name in the domain.
         * Invalid chars are ignored in microsoft client therefore we use '_'
         * again. this sec '__' contains the split for index.*/
        pos = g_pos(&originalDomainInfo[1], "__");
        if (pos > 0)
        {
            /* an index is found we try to use it */
            LOG(LOG_LEVEL_DEBUG, "domain contains index char __");
            if (decode)
            {
                g_memset(index, 0, 2);
                /* we just accept values 0-9  (one figure) */
                g_strncpy(index, &originalDomainInfo[pos + 3], 1);
                comboxindex = g_htoi(index);
                LOG(LOG_LEVEL_DEBUG,
                    "index value as string: %s, as int: %d, max: %d",
                    index, comboxindex, comboMax - 1);
                /* limit to max number of items in combo box */
                if ((comboxindex > 0) && (comboxindex < comboMax))
                {
                    LOG(LOG_LEVEL_DEBUG, "domain contains a valid "
                        "index number");
                    ret = comboxindex; /* preferred index for combo box. */
                }
            }
            /* pos limit the String to only contain the IP */
            g_strncpy(resultBuffer, &originalDomainInfo[1], pos);
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG, "domain does not contain _");
            g_strncpy(resultBuffer, &originalDomainInfo[1], 255);
        }
    }
    return ret;
}

/******************************************************************************/
static int
xrdp_wm_show_edits(struct xrdp_wm *self, struct xrdp_bitmap *combo)
{
    int count;
    int index;
    int insert_index;
    int username_set;
    char *name;
    char *value;
    struct xrdp_mod_data *mod;
    struct xrdp_bitmap *b;
    struct xrdp_cfg_globals *globals;
    char resultIP[256];
    char *plain; /* base64 decoded string */
    size_t plain_length; /* length of decoded base64 string */
    size_t base64_length; /* length of base64 string */

    globals = &self->xrdp_config->cfg_globals;

    username_set = 0;

    /* free labels and edits, cause we will create them */
    /* creation or combo changed */
    for (index = 100; index < 200; index++)
    {
        b = xrdp_bitmap_get_child_by_id(combo->parent, index);
        xrdp_bitmap_delete(b);
    }

    insert_index = list_index_of(self->login_window->child_list,
                                 (long)combo); /* find combo in the list */
    insert_index++;
    mod = (struct xrdp_mod_data *)
          list_get_item(combo->data_list, combo->item_index);

    if (mod != 0)
    {
        count = 0;

        for (index = 0; index < mod->names->count; index++)
        {
            value = (char *)list_get_item(mod->values, index);

            /* if the value begins with "{base64}", decode the string following it */
            if (g_strncmp(BASE64PREFIX, value, BASE64PREFIX_LEN) == 0)
            {
                base64_length = g_strlen(value + BASE64PREFIX_LEN);
                plain = (char *)g_malloc(base64_length, 0);
                base64_decode(value + BASE64PREFIX_LEN,
                              plain, base64_length, &plain_length);
                g_strncpy(value, plain, plain_length);
                g_free(plain);
            }
            else if (g_strncmp(ASK, value, ASK_LEN) == 0)
            {
                const int combo_height =
                    self->xrdp_config->cfg_globals.ls_scaled.combo_height;
                const int edit_height =
                    self->xrdp_config->cfg_globals.ls_scaled.edit_height;
                /* label */
                b = xrdp_bitmap_create(globals->ls_scaled.label_width,
                                       edit_height, self->screen->bpp,
                                       WND_TYPE_LABEL, self);
                list_insert_item(self->login_window->child_list, insert_index,
                                 (long)b);
                insert_index++;
                b->parent = self->login_window;
                b->owner = self->login_window;
                b->left = globals->ls_scaled.label_x_pos;

                b->top = globals->ls_scaled.input_y_pos + combo_height + 5 +
                         (edit_height + 5) * count;
                b->id = 100 + 2 * count;
                name = (char *)list_get_item(mod->names, index);
                set_string(&b->caption1, name);

                /* edit */
                b = xrdp_bitmap_create(globals->ls_scaled.input_width,
                                       edit_height, self->screen->bpp,
                                       WND_TYPE_EDIT, self);
                list_insert_item(self->login_window->child_list, insert_index,
                                 (long)b);
                insert_index++;
                b->parent = self->login_window;
                b->owner = self->login_window;
                b->left = globals->ls_scaled.input_x_pos;

                b->top = globals->ls_scaled.input_y_pos + combo_height + 5 +
                         (edit_height + 5) * count;

                b->id = 100 + 2 * count + 1;
                b->pointer = 1;
                b->tab_stop = 1;
                b->caption1 = (char *)g_malloc(256, 1);
                /* ask{base64}... 3 for "ask", 8 for "{base64}" */
                if (g_strncmp(BASE64PREFIX, value + ASK_LEN, BASE64PREFIX_LEN) == 0)
                {
                    base64_length = g_strlen(value + ASK_LEN + BASE64PREFIX_LEN);
                    plain = (char *)g_malloc(base64_length, 0);
                    base64_decode(value + ASK_LEN + BASE64PREFIX_LEN,
                                  plain, base64_length, &plain_length);
                    plain[plain_length] = '\0';
                    g_strncpy(b->caption1, plain, 255);
                    g_free(plain);
                }
                else
                {
                    g_strncpy(b->caption1, value + ASK_LEN, 255);
                }
                b->edit_pos = utf8_char_count(b->caption1);

                if (self->login_window->focused_control == 0)
                {
                    self->login_window->focused_control = b;
                }

                /* Use the domain name as the destination IP/DNS
                   This is useful in a gateway setup. */
                if (g_strncasecmp(name, "ip", 255) == 0)
                {
                    /* If the first char in the domain name is '_' we use the
                       domain name as IP */
                    if (self->session->client_info->domain[0] == '_')
                    {
                        xrdp_wm_parse_domain_information(
                            self->session->client_info->domain,
                            combo->data_list->count, 0, resultIP);
                        g_strncpy(b->caption1, resultIP, 255);
                        b->edit_pos = utf8_char_count(b->caption1);
                    }

                }

                if (g_strncasecmp(name, "username", 255) == 0 &&
                        self->session->client_info->username[0])
                {
                    g_strncpy(b->caption1, self->session->client_info->username, 255);
                    b->edit_pos = utf8_char_count(b->caption1);

                    if (b->edit_pos > 0)
                    {
                        username_set = 1;
                    }
                }

                if ((g_strncasecmp(name, "password", 255) == 0) ||
                        (g_strncasecmp(name, "pampassword", 255) == 0))
                {
                    b->password_char = '*';

                    if (username_set)
                    {
                        if (b->parent != 0)
                        {
                            b->parent->focused_control = b;
                        }
                    }
                }

                count++;
            }
        }
    }

    return 0;
}

/*****************************************************************************/
/* all login screen events go here */
static int
xrdp_wm_login_notify(struct xrdp_bitmap *wnd,
                     struct xrdp_bitmap *sender,
                     int msg, long param1, long param2)
{
    struct xrdp_bitmap *b;
    struct xrdp_rect rect;
    int i;

    if (wnd->modal_dialog != 0 && msg != 100)
    {
        return 0;
    }

    if (msg == 1) /* click */
    {
        if (sender->id == 1) /* help button */
        {
            xrdp_wm_help_clicked(wnd);
        }
        else if (sender->id == 2) /* cancel button */
        {
            xrdp_wm_cancel_clicked(wnd);
        }
        else if (sender->id == 3) /* ok button */
        {
            xrdp_wm_ok_clicked(wnd);
        }
    }
    else if (msg == 2) /* mouse move */
    {
    }
    else if (msg == 100) /* modal result is done */
    {
        i = list_index_of(wnd->wm->screen->child_list, (long)sender);

        if (i >= 0)
        {
            b = (struct xrdp_bitmap *)
                list_get_item(wnd->wm->screen->child_list, i);
            list_remove_item(sender->wm->screen->child_list, i);
            MAKERECT(rect, b->left, b->top, b->width, b->height);
            xrdp_bitmap_invalidate(wnd->wm->screen, &rect);
            xrdp_bitmap_delete(sender);
            wnd->modal_dialog = 0;
            xrdp_wm_set_focused(wnd->wm, wnd);
        }
    }
    else if (msg == CB_ITEMCHANGE) /* combo box change */
    {
        xrdp_wm_show_edits(wnd->wm, sender);
        xrdp_bitmap_invalidate(wnd, 0); /* invalidate the whole dialog for now */
    }

    return 0;
}

/******************************************************************************/
static int
xrdp_wm_login_fill_in_combo(struct xrdp_wm *self, struct xrdp_bitmap *b)
{
    struct list *sections;
    struct list *section_names;
    struct list *section_values;
    int fd;
    int i;
    int j;
    char *p;
    char *q;
    char *r;
    char name[256];
    struct xrdp_mod_data *mod_data;
    const char *xrdp_ini = self->session->xrdp_ini;

    sections = list_create();
    sections->auto_free = 1;
    section_names = list_create();
    section_names->auto_free = 1;
    section_values = list_create();
    section_values->auto_free = 1;
    fd = g_file_open_ro(xrdp_ini);

    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Could not read xrdp ini file %s",
            xrdp_ini);
        list_delete(sections);
        list_delete(section_names);
        list_delete(section_values);
        return 1;
    }

    file_read_sections(fd, sections);

    for (i = 0; i < sections->count; i++)
    {
        p = (char *)list_get_item(sections, i);
        file_read_section(fd, p, section_names, section_values);

        if ((g_strncasecmp(p, "globals", 255) == 0)
                || (g_strncasecmp(p, "channels", 255) == 0)
                || (g_strncasecmp(p, "Logging", 255) == 0)
                || (g_strncasecmp(p, "LoggingPerLogger", 255) == 0))
        {
        }
        else
        {
            g_strncpy(name, p, 255);
            mod_data = (struct xrdp_mod_data *)
                       g_malloc(sizeof(struct xrdp_mod_data), 1);
            mod_data->names = list_create();
            mod_data->names->auto_free = 1;
            mod_data->values = list_create();
            mod_data->values->auto_free = 1;

            for (j = 0; j < section_names->count; j++)
            {
                q = (char *)list_get_item(section_names, j);
                r = (char *)list_get_item(section_values, j);

                if (g_strncmp("name", q, 255) == 0)
                {
                    g_strncpy(name, r, 255);
                }

                list_add_strdup(mod_data->names, q);
                list_add_strdup(mod_data->values, r);
            }

            list_add_strdup(b->string_list, name);
            list_add_item(b->data_list, (long)mod_data);
        }
    }

    g_file_close(fd);
    list_delete(sections);
    list_delete(section_names);
    list_delete(section_values);
    return 0;
}

/******************************************************************************/
unsigned int
xrdp_login_wnd_get_monitor_dpi(struct xrdp_wm *self)
{
    unsigned int result = 0;
    const struct display_size_description *display_sizes =
            &self->client_info->display_sizes;
    unsigned int height_pixels = 0;
    unsigned int height_mm = 0;

    unsigned int i;

    /* Look at the monitor data first */
    for (i = 0; i < display_sizes->monitorCount; ++i)
    {
        const struct monitor_info *mi = &display_sizes->minfo_wm[i];
        {
            if (mi->is_primary)
            {
                height_pixels = mi->bottom - mi->top + 1;
                height_mm = mi->physical_height;
                break;
            }
        }
    }

    /* No primary monitor, or values not defined - use the desktop size */
    if (height_mm == 0)
    {
        height_pixels = display_sizes->session_height;
        height_mm = self->client_info->session_physical_height;

        if (height_mm == 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "No information is available to determine login screen DPI");
        }
        else if (height_pixels < 768)
        {
            /* A bug was encountered with mstsc.exe version
               10.0.19041.1682 where the full physical monitor size was
               sent in TS_UD_CS_CORE when the desktop size was set to
               less than the screen size.
               To generate the bug, make a connection with a full-screen
               single window, cancel the login, and reconnect at
               (e.g.) 800x600.
               We can't detect that exact situation here, but if the
               session height is so small as to likely be in a window
               (rather than full screen), we should ignore the physical
               size */
            LOG(LOG_LEVEL_WARNING,
                "Ignoring unlikely physical session size %u "
                "for height of %u pixels", height_mm, height_pixels);
            height_mm = 0;
        }
    }

    if (height_mm != 0)
    {
        /*
         * DPI = height_pixels / (height_mm / 25.4)
         *     = (height_pixels * 25.4) / height_mm
         *     = (height_pixels * 127) / (height_mm * 5)
         */
        result = (height_pixels * 127 ) / (height_mm * 5);
        LOG(LOG_LEVEL_INFO,
            "Login screen monitor height is %u pixels over %u mm (%u DPI)",
            height_pixels,
            height_mm,
            result);
    }
    return result;
}


/******************************************************************************/
int
xrdp_login_wnd_create(struct xrdp_wm *self)
{
    struct xrdp_bitmap      *but;
    struct xrdp_bitmap      *combo;
    struct xrdp_cfg_globals *globals;

    char buf[256];
    char buf1[256];
    char resultIP[256];
    int log_width;
    int log_height;
    int regular;
    int primary_width;     /* Dimensions of primary screen */
    int primary_height;
    int primary_x_offset;  /* Offset of centre of primary screen */
    int primary_y_offset;
    uint32_t index;
    int x;
    int y;
    int cx;
    int cy;
    const int combo_height =
        self->xrdp_config->cfg_globals.ls_scaled.combo_height;
    const int edit_height =
        self->xrdp_config->cfg_globals.ls_scaled.edit_height;

    globals = &self->xrdp_config->cfg_globals;

    primary_width = self->screen->width;
    primary_height = self->screen->height;
    primary_x_offset = primary_width / 2;
    primary_y_offset = primary_height / 2;

    log_width = globals->ls_scaled.width;
    log_height = globals->ls_scaled.height;
    regular = 1;

    if (self->screen->width < log_width)
    {
        if (self->screen->width < 240)
        {
            log_width = self->screen->width - 4;
        }
        else
        {
            log_width = 240;
        }

        regular = 0;
    }

    /* multimon scenario, draw login window on primary monitor */
    if (self->client_info->display_sizes.monitorCount > 1)
    {
        for (index = 0; index < self->client_info->display_sizes.monitorCount; index++)
        {
            if (self->client_info->display_sizes.minfo_wm[index].is_primary)
            {
                x = self->client_info->display_sizes.minfo_wm[index].left;
                y = self->client_info->display_sizes.minfo_wm[index].top;
                cx = self->client_info->display_sizes.minfo_wm[index].right;
                cy = self->client_info->display_sizes.minfo_wm[index].bottom;

                primary_width = cx - x;
                primary_height = cy - y;
                primary_x_offset = x + (primary_width / 2);
                primary_y_offset = y + (primary_height / 2);
                break;
            }
        }
    }

    /* draw login window */
    self->login_window = xrdp_bitmap_create(log_width, log_height, self->screen->bpp,
                                            WND_TYPE_WND, self);
    list_add_item(self->screen->child_list, (long)self->login_window);
    self->login_window->parent = self->screen;
    self->login_window->owner = self->screen;
    self->login_window->bg_color = globals->ls_bg_color;

    self->login_window->left = primary_x_offset - self->login_window->width / 2;
    self->login_window->top = primary_y_offset - self->login_window->height / 2;


    self->login_window->notify = xrdp_wm_login_notify;

    /* if window title not specified, use hostname as default */
    if (globals->ls_title[0] == 0)
    {
        g_gethostname(buf1, 256);
        g_snprintf(buf, sizeof(buf), "Login to %s", buf1);
        set_string(&self->login_window->caption1, buf);
    }
    else
    {
        /*self->login_window->caption1 = globals->ls_title[0];*/
        g_snprintf(buf, sizeof(buf), "%s", globals->ls_title);
        set_string(&self->login_window->caption1, buf);
    }

    if (regular)
    {
        /* Load the background image. */
        /* If no file is specified no default image will be loaded. */
        /* We only load the image if bpp > 8, and if the user hasn't
         * disabled wallpaper in the performance settings */
        if (globals->ls_background_image[0] != 0)
        {
            if (self->screen->bpp <= 8)
            {
                LOG(LOG_LEVEL_INFO, "Login background not loaded for bpp=%d",
                    self->screen->bpp);
            }
            else if ((self->client_info->rdp5_performanceflags &
                      RDP5_NO_WALLPAPER) != 0)
            {
                LOG(LOG_LEVEL_INFO, "Login background not loaded as client "
                    "has requested PERF_DISABLE_WALLPAPER");
            }
            else
            {
                char fileName[256] ;
                but = xrdp_bitmap_create(4, 4, self->screen->bpp,
                                         WND_TYPE_IMAGE, self);
                if (globals->ls_background_image[0] == '/')
                {
                    g_snprintf(fileName, 255, "%s",
                               globals->ls_background_image);
                }
                else
                {
                    g_snprintf(fileName, 255, "%s/%s",
                               XRDP_SHARE_PATH, globals->ls_background_image);
                }
                LOG(LOG_LEVEL_DEBUG, "We try to load the following background file: %s", fileName);
                if (globals->ls_background_transform == XBLT_NONE)
                {
                    xrdp_bitmap_load(but, fileName, self->palette,
                                     globals->ls_top_window_bg_color,
                                     globals->ls_background_transform,
                                     0, 0);
                    /* Place the background in the bottom right corner */
                    but->left = primary_x_offset + (primary_width / 2) -
                                but->width;
                    but->top = primary_y_offset + (primary_height / 2) -
                               but->height;
                }
                else
                {
                    xrdp_bitmap_load(but, fileName, self->palette,
                                     globals->ls_top_window_bg_color,
                                     globals->ls_background_transform,
                                     primary_width, primary_height);
                    but->left = primary_x_offset - (primary_width / 2);
                    but->top = primary_y_offset - (primary_height / 2);
                }
                but->parent = self->screen;
                but->owner = self->screen;
                list_add_item(self->screen->child_list, (long)but);
            }
        }

        /* if logo image not specified, use default */
        if (globals->ls_logo_filename[0] == 0)
        {
#ifdef USE_IMLIB2
            g_snprintf(globals->ls_logo_filename, 255, "%s/xrdp_logo.png",
                       XRDP_SHARE_PATH);
#else
            g_snprintf(globals->ls_logo_filename, 255, "%s/xrdp_logo.bmp",
                       XRDP_SHARE_PATH);
#endif
        }

        /* logo image */
        but = xrdp_bitmap_create(4, 4, self->screen->bpp, WND_TYPE_IMAGE, self);

        if (self->screen->bpp <= 8)
        {
            g_snprintf(globals->ls_logo_filename, 255, "%s/ad256.bmp", XRDP_SHARE_PATH);
        }

        LOG(LOG_LEVEL_DEBUG, "ls_logo_filename: %s", globals->ls_logo_filename);

        xrdp_bitmap_load(but, globals->ls_logo_filename, self->palette,
                         globals->ls_bg_color,
                         globals->ls_logo_transform,
                         globals->ls_scaled.logo_width,
                         globals->ls_scaled.logo_height);
        but->parent = self->login_window;
        but->owner = self->login_window;
        but->left = globals->ls_scaled.logo_x_pos;
        but->top = globals->ls_scaled.logo_y_pos;
        list_add_item(self->login_window->child_list, (long)but);
    }

    /* label */
    but = xrdp_bitmap_create(globals->ls_scaled.label_width, edit_height,
                             self->screen->bpp, WND_TYPE_LABEL, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_scaled.label_x_pos;
    but->top = globals->ls_scaled.input_y_pos;
    set_string(&but->caption1, "Session");

    /* combo */
    combo = xrdp_bitmap_create(globals->ls_scaled.input_width, combo_height,
                               self->screen->bpp, WND_TYPE_COMBO, self);
    list_add_item(self->login_window->child_list, (long)combo);
    combo->parent = self->login_window;
    combo->owner = self->login_window;
    combo->left = globals->ls_scaled.input_x_pos;
    combo->top = globals->ls_scaled.input_y_pos;
    combo->id = 6;
    combo->tab_stop = 1;
    xrdp_wm_login_fill_in_combo(self, combo);

    /* OK button */
    but = xrdp_bitmap_create(globals->ls_scaled.btn_ok_width,
                             globals->ls_scaled.btn_ok_height,
                             self->screen->bpp, WND_TYPE_BUTTON, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_scaled.btn_ok_x_pos;
    but->top = globals->ls_scaled.btn_ok_y_pos;
    but->id = 3;
    set_string(&but->caption1, "OK");
    but->tab_stop = 1;
    self->login_window->default_button = but;

    /* Cancel button */
    but = xrdp_bitmap_create(globals->ls_scaled.btn_cancel_width,
                             globals->ls_scaled.btn_cancel_height,
                             self->screen->bpp,
                             WND_TYPE_BUTTON, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_scaled.btn_cancel_x_pos;
    but->top = globals->ls_scaled.btn_cancel_y_pos;
    but->id = 2;
    set_string(&but->caption1, "Cancel");
    but->tab_stop = 1;
    self->login_window->esc_button = but;

    /* labels and edits.
    * parameter: 1 = decode domain field index information from client.
    * We only perform this the first time for each connection.
    */
    combo->item_index = xrdp_wm_parse_domain_information(
                            self->session->client_info->domain,
                            combo->data_list->count, 1,
                            resultIP /* just a dummy place holder, we ignore */ );
    xrdp_wm_show_edits(self, combo);

    return 0;
}

/**
 * Map a bitmap transform string to a value
 *
 * @param param Param we're trying to read
 * @param str   String we're trying to map
 *
 * @return enum xrdp_bitmap_load_transform value
 *
 * A warning is logged if the string is not recognised
 *****************************************************************************/
static enum xrdp_bitmap_load_transform
bitmap_transform_str_to_val(const char *param, const char *str)
{
    enum xrdp_bitmap_load_transform rv;
    if (g_strcmp(str, "none") == 0)
    {
        rv = XBLT_NONE;
    }
    else if (g_strcmp(str, "scale") == 0)
    {
        rv = XBLT_SCALE;
    }
    else if (g_strcmp(str, "zoom") == 0)
    {
        rv = XBLT_ZOOM;
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "Param '%s' has unrecognised value '%s'"
            " - assuming 'none'", param, str);
        rv = XBLT_NONE;
    }

    return rv;
}

/**
 * Load configuration from xrdp.ini file
 *
 * @param config XRDP configuration to initialise
 * @param xrdp_ini Path to xrdp.ini
 * @param bpp bits-per-pixel for this connection
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int
load_xrdp_config(struct xrdp_config *config, const char *xrdp_ini, int bpp)
{
    struct xrdp_cfg_globals  *globals;

    struct list *names;
    struct list *values;

    char *n;
    char *v;
    int   fd;
    int   i;

    if (!config)
    {
        return -1;
    }

    globals = &config->cfg_globals;

    /* set default values in case we can't get them from xrdp.ini file */
    globals->ini_version = 1;
    globals->default_dpi = 96;

    globals->ls_top_window_bg_color = HCOLOR(bpp, xrdp_wm_htoi("009cb5"));
    globals->ls_bg_color = HCOLOR(bpp, xrdp_wm_htoi("dedede"));
    globals->ls_unscaled.width = 350;
    globals->ls_unscaled.height = 350;
    globals->ls_background_transform = XBLT_NONE;
    globals->ls_logo_transform = XBLT_NONE;
    globals->ls_unscaled.logo_x_pos = 63;
    globals->ls_unscaled.logo_y_pos = 50;
    globals->ls_unscaled.label_x_pos = 30;
    globals->ls_unscaled.label_width = 65;
    globals->ls_unscaled.input_x_pos = 110;
    globals->ls_unscaled.input_width = 210;
    globals->ls_unscaled.input_y_pos = 150;
    globals->ls_unscaled.btn_ok_x_pos = 150;
    globals->ls_unscaled.btn_ok_y_pos = 300;
    globals->ls_unscaled.btn_ok_width = 85;
    globals->ls_unscaled.btn_ok_height = 30;
    globals->ls_unscaled.btn_cancel_x_pos = 245;
    globals->ls_unscaled.btn_cancel_y_pos = 300;
    globals->ls_unscaled.btn_cancel_width = 85;
    globals->ls_unscaled.btn_cancel_height = 30;
    globals->ls_unscaled.default_btn_height =
        DEFAULT_FONT_PIXEL_SIZE + DEFAULT_BUTTON_MARGIN_H;
    globals->ls_unscaled.log_wnd_width = DEFAULT_WND_LOG_W;
    globals->ls_unscaled.log_wnd_height = DEFAULT_WND_LOG_H;
    globals->ls_unscaled.edit_height =
        DEFAULT_FONT_PIXEL_SIZE + DEFAULT_EDIT_MARGIN_H;
    globals->ls_unscaled.combo_height =
        DEFAULT_FONT_PIXEL_SIZE + DEFAULT_COMBO_MARGIN_H;
    globals->ls_unscaled.help_wnd_width = DEFAULT_WND_HELP_W;
    globals->ls_unscaled.help_wnd_height = DEFAULT_WND_HELP_H;

    /* open xrdp.ini file */
    if ((fd = g_file_open_ro(xrdp_ini)) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "load_config: Could not read "
            "xrdp.ini file %s", xrdp_ini);
        return -1;

    }

    names = list_create();
    values = list_create();
    names->auto_free = 1;
    values->auto_free = 1;

    if (file_read_section(fd, "globals", names, values) != 0)
    {
        list_delete(names);
        list_delete(values);
        g_file_close(fd);
        LOG(LOG_LEVEL_ERROR, "load_config: Could not read globals "
            "section from xrdp.ini file %s", xrdp_ini);
        return -1;
    }

    for (i = 0; i < names->count; i++)
    {
        n = (char *) list_get_item(names, i);
        v = (char *) list_get_item(values, i);

        /*
         * parse globals section
         */

        if (g_strncmp(n, "ini_version", 64) == 0)
        {
            globals->ini_version = g_atoi(v);
        }

        else if (g_strncmp(n, "bitmap_cache", 64) == 0)
        {
            globals->use_bitmap_cache = g_text2bool(v);
        }

        else if (g_strncmp(n, "bitmap_compression", 64) == 0)
        {
            globals->use_bitmap_compression = g_text2bool(v);
        }

        else if (g_strncmp(n, "port", 64) == 0)
        {
            globals->port = g_atoi(v);
        }

        else if (g_strncmp(n, "crypt_level", 64) == 0)
        {
            if (g_strcmp(v, "low") == 0)
            {
                globals->crypt_level = 1;
            }
            else if (g_strcmp(v, "medium") == 0)
            {
                globals->crypt_level = 2;
            }
            else
            {
                globals->crypt_level = 3;
            }
        }

        else if (g_strncmp(n, "allow_channels", 64) == 0)
        {
            globals->allow_channels = g_text2bool(v);
        }

        else if (g_strncmp(n, "max_bpp", 64) == 0)
        {
            globals->max_bpp = g_atoi(v);
        }

        else if (g_strncmp(n, "fork", 64) == 0)
        {
            globals->fork = g_text2bool(v);
        }

        else if (g_strncmp(n, "tcp_nodelay", 64) == 0)
        {
            globals->tcp_nodelay = g_text2bool(v);
        }

        else if (g_strncmp(n, "tcp_keepalive", 64) == 0)
        {
            globals->tcp_keepalive = g_text2bool(v);
        }

        else if (g_strncmp(n, "tcp_send_buffer_bytes", 64) == 0)
        {
            globals->tcp_send_buffer_bytes = g_atoi(v);
        }

        else if (g_strncmp(n, "tcp_recv_buffer_bytes", 64) == 0)
        {
            globals->tcp_recv_buffer_bytes = g_atoi(v);
        }

        /* colors */

        else if (g_strncmp(n, "grey", 64) == 0)
        {
            globals->grey = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "black", 64) == 0)
        {
            globals->black = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "dark_grey", 64) == 0)
        {
            globals->dark_grey = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "blue", 64) == 0)
        {
            globals->blue = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "dark_blue", 64) == 0)
        {
            globals->dark_blue = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "white", 64) == 0)
        {
            globals->white = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "red", 64) == 0)
        {
            globals->red = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "green", 64) == 0)
        {
            globals->green = xrdp_wm_htoi(v);
        }

        else if (g_strncmp(n, "background", 64) == 0)
        {
            globals->background = xrdp_wm_htoi(v);
        }

        /* misc stuff */

        else if (g_strncmp(n, "autorun", 255) == 0)
        {
            g_strncpy(globals->autorun, v, 255);
        }

        else if (g_strncmp(n, "hidelogwindow", 64) == 0)
        {
            globals->hidelogwindow = g_text2bool(v);
        }

        else if (g_strncmp(n, "require_credentials", 64) == 0)
        {
            globals->require_credentials = g_text2bool(v);
        }

        else if (g_strncmp(n, "bulk_compression", 64) == 0)
        {
            globals->bulk_compression = g_text2bool(v);
        }

        else if (g_strncmp(n, "new_cursors", 64) == 0)
        {
            globals->new_cursors = g_text2bool(v);
        }

        else if (g_strncmp(n, "nego_sec_layer", 64) == 0)
        {
            globals->nego_sec_layer = g_atoi(v);
        }

        else if (g_strncmp(n, "allow_multimon", 64) == 0)
        {
            globals->allow_multimon = g_text2bool(v);
        }

        else if (g_strncmp(n, "enable_token_login", 64) == 0)
        {
            LOG(LOG_LEVEL_DEBUG, "Token login detection enabled x");
            globals->enable_token_login = g_text2bool(v);
        }

        /* login screen values */
        else if (g_strcmp(n, "default_dpi") == 0)
        {
            globals->default_dpi = g_atoi(v);
        }

        else if (g_strcmp(n, "fv1_select") == 0)
        {
            g_strncpy(globals->fv1_select, v, sizeof(globals->fv1_select) - 1);
        }

        else if (g_strncmp(n, "ls_top_window_bg_color", 64) == 0)
        {
            globals->ls_top_window_bg_color = HCOLOR(bpp, xrdp_wm_htoi(v));
        }

        else if (g_strncmp(n, "ls_width", 64) == 0)
        {
            globals->ls_unscaled.width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_height", 64) == 0)
        {
            globals->ls_unscaled.height = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_bg_color", 64) == 0)
        {
            globals->ls_bg_color = HCOLOR(bpp, xrdp_wm_htoi(v));
        }

        else if (g_strncmp(n, "ls_title", 255) == 0)
        {
            g_strncpy(globals->ls_title, v, 255);
            globals->ls_title[255] = 0;
        }

        else if (g_strncmp(n, "ls_background_image", 255) == 0)
        {
            g_strncpy(globals->ls_background_image, v, 255);
            globals->ls_background_image[255] = 0;
        }

        else if (g_strncmp(n, "ls_background_transform", 255) == 0)
        {
            globals->ls_background_transform =
                bitmap_transform_str_to_val(n, v);
        }

        else if (g_strncmp(n, "ls_logo_filename", 255) == 0)
        {
            g_strncpy(globals->ls_logo_filename, v, 255);
            globals->ls_logo_filename[255] = 0;
        }

        else if (g_strncmp(n, "ls_logo_transform", 255) == 0)
        {
            globals->ls_logo_transform = bitmap_transform_str_to_val(n, v);
        }

        else if (g_strncmp(n, "ls_logo_width", 64) == 0)
        {
            globals->ls_unscaled.logo_width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_logo_height", 64) == 0)
        {
            globals->ls_unscaled.logo_height = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_logo_x_pos", 64) == 0)
        {
            globals->ls_unscaled.logo_x_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_logo_y_pos", 64) == 0)
        {
            globals->ls_unscaled.logo_y_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_label_x_pos", 64) == 0)
        {
            globals->ls_unscaled.label_x_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_label_width", 64) == 0)
        {
            globals->ls_unscaled.label_width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_input_x_pos", 64) == 0)
        {
            globals->ls_unscaled.input_x_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_input_width", 64) == 0)
        {
            globals->ls_unscaled.input_width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_input_y_pos", 64) == 0)
        {
            globals->ls_unscaled.input_y_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_ok_x_pos", 64) == 0)
        {
            globals->ls_unscaled.btn_ok_x_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_ok_y_pos", 64) == 0)
        {
            globals->ls_unscaled.btn_ok_y_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_ok_width", 64) == 0)
        {
            globals->ls_unscaled.btn_ok_width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_ok_height", 64) == 0)
        {
            globals->ls_unscaled.btn_ok_height = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_cancel_x_pos", 64) == 0)
        {
            globals->ls_unscaled.btn_cancel_x_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_cancel_y_pos", 64) == 0)
        {
            globals->ls_unscaled.btn_cancel_y_pos = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_cancel_width", 64) == 0)
        {
            globals->ls_unscaled.btn_cancel_width = g_atoi(v);
        }

        else if (g_strncmp(n, "ls_btn_cancel_height", 64) == 0)
        {
            globals->ls_unscaled.btn_cancel_height = g_atoi(v);
        }
    }

    LOG(LOG_LEVEL_DEBUG, "ini_version:             %d", globals->ini_version);
    LOG(LOG_LEVEL_DEBUG, "use_bitmap_cache:        %d", globals->use_bitmap_cache);
    LOG(LOG_LEVEL_DEBUG, "use_bitmap_compression:  %d", globals->use_bitmap_compression);
    LOG(LOG_LEVEL_DEBUG, "port:                    %d", globals->port);
    LOG(LOG_LEVEL_DEBUG, "crypt_level:             %d", globals->crypt_level);
    LOG(LOG_LEVEL_DEBUG, "allow_channels:          %d", globals->allow_channels);
    LOG(LOG_LEVEL_DEBUG, "max_bpp:                 %d", globals->max_bpp);
    LOG(LOG_LEVEL_DEBUG, "fork:                    %d", globals->fork);
    LOG(LOG_LEVEL_DEBUG, "tcp_nodelay:             %d", globals->tcp_nodelay);
    LOG(LOG_LEVEL_DEBUG, "tcp_keepalive:           %d", globals->tcp_keepalive);
    LOG(LOG_LEVEL_DEBUG, "tcp_send_buffer_bytes:   %d", globals->tcp_send_buffer_bytes);
    LOG(LOG_LEVEL_DEBUG, "tcp_recv_buffer_bytes:   %d", globals->tcp_recv_buffer_bytes);
    LOG(LOG_LEVEL_DEBUG, "new_cursors:             %d", globals->new_cursors);
    LOG(LOG_LEVEL_DEBUG, "allow_multimon:          %d", globals->allow_multimon);

    LOG(LOG_LEVEL_DEBUG, "grey:                    %d", globals->grey);
    LOG(LOG_LEVEL_DEBUG, "black:                   %d", globals->black);
    LOG(LOG_LEVEL_DEBUG, "dark_grey:               %d", globals->dark_grey);
    LOG(LOG_LEVEL_DEBUG, "blue:                    %d", globals->blue);
    LOG(LOG_LEVEL_DEBUG, "dark_blue:               %d", globals->dark_blue);
    LOG(LOG_LEVEL_DEBUG, "white:                   %d", globals->white);
    LOG(LOG_LEVEL_DEBUG, "red:                     %d", globals->red);
    LOG(LOG_LEVEL_DEBUG, "green:                   %d", globals->green);
    LOG(LOG_LEVEL_DEBUG, "background:              %d", globals->background);

    LOG(LOG_LEVEL_DEBUG, "autorun:                 %s", globals->autorun);
    LOG(LOG_LEVEL_DEBUG, "hidelogwindow:           %d", globals->hidelogwindow);
    LOG(LOG_LEVEL_DEBUG, "require_credentials:     %d", globals->require_credentials);
    LOG(LOG_LEVEL_DEBUG, "bulk_compression:        %d", globals->bulk_compression);
    LOG(LOG_LEVEL_DEBUG, "new_cursors:             %d", globals->new_cursors);
    LOG(LOG_LEVEL_DEBUG, "nego_sec_layer:          %d", globals->nego_sec_layer);
    LOG(LOG_LEVEL_DEBUG, "allow_multimon:          %d", globals->allow_multimon);
    LOG(LOG_LEVEL_DEBUG, "enable_token_login:      %d", globals->enable_token_login);

    LOG(LOG_LEVEL_DEBUG, "ls_top_window_bg_color:  %x", globals->ls_top_window_bg_color);
    LOG(LOG_LEVEL_DEBUG, "ls_width (unscaled):     %d", globals->ls_unscaled.width);
    LOG(LOG_LEVEL_DEBUG, "ls_height (unscaled):    %d", globals->ls_unscaled.height);
    LOG(LOG_LEVEL_DEBUG, "ls_bg_color:             %x", globals->ls_bg_color);
    LOG(LOG_LEVEL_DEBUG, "ls_title:                %s", globals->ls_title);
    LOG(LOG_LEVEL_DEBUG, "ls_logo_filename:        %s", globals->ls_logo_filename);
    LOG(LOG_LEVEL_DEBUG, "ls_logo_x_pos :          %d", globals->ls_unscaled.logo_x_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_logo_y_pos :          %d", globals->ls_unscaled.logo_y_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_label_x_pos :         %d", globals->ls_unscaled.label_x_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_label_width :         %d", globals->ls_unscaled.label_width);
    LOG(LOG_LEVEL_DEBUG, "ls_input_x_pos :         %d", globals->ls_unscaled.input_x_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_input_width :         %d", globals->ls_unscaled.input_width);
    LOG(LOG_LEVEL_DEBUG, "ls_input_y_pos :         %d", globals->ls_unscaled.input_y_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_ok_x_pos :        %d", globals->ls_unscaled.btn_ok_x_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_ok_y_pos :        %d", globals->ls_unscaled.btn_ok_y_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_ok_width :        %d", globals->ls_unscaled.btn_ok_width);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_ok_height :       %d", globals->ls_unscaled.btn_ok_height);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_cancel_x_pos :    %d", globals->ls_unscaled.btn_cancel_x_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_cancel_y_pos :    %d", globals->ls_unscaled.btn_cancel_y_pos);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_cancel_width :    %d", globals->ls_unscaled.btn_cancel_width);
    LOG(LOG_LEVEL_DEBUG, "ls_btn_cancel_height :   %d", globals->ls_unscaled.btn_cancel_height);

    list_delete(names);
    list_delete(values);
    g_file_close(fd);
    return 0;
}

/**
 * Scale the configuration values
 *
 * After a font has been loaded, we can produce scaled versions of the
 * login screen layout parameters which will correspond to the size of the
 * font
 */
void
xrdp_login_wnd_scale_config_values(struct xrdp_wm *self)
{
    const struct xrdp_ls_dimensions *unscaled =
            &self->xrdp_config->cfg_globals.ls_unscaled;
    struct xrdp_ls_dimensions *scaled =
            &self->xrdp_config->cfg_globals.ls_scaled;

    /* Clear the scaled values, so if we add one and forget to scale it,
     * it will be obvious */
    g_memset(scaled, '\0', sizeof(*scaled));

    /* If we don't have a font, use zeros for everything */
    if (self->default_font == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't scale login values - no font available");
    }
    else
    {
        const int fheight = self->default_font->body_height;
        /* Define a Macro to scale to the nearest pixel value,
         * rounding up as appropriate */
#define SCALE_AND_ROUND(x) \
    (((x) * fheight + (DEFAULT_FONT_PIXEL_SIZE / 2)) / \
     DEFAULT_FONT_PIXEL_SIZE)

        LOG(LOG_LEVEL_DEBUG, "Login screen scale factor %f",
            (float)fheight / DEFAULT_FONT_PIXEL_SIZE);

        scaled->width = SCALE_AND_ROUND(unscaled->width);
        scaled->height = SCALE_AND_ROUND(unscaled->height);
        scaled->logo_width = SCALE_AND_ROUND(unscaled->logo_width);
        scaled->logo_height = SCALE_AND_ROUND(unscaled->logo_height);
        scaled->logo_x_pos = SCALE_AND_ROUND(unscaled->logo_x_pos);
        scaled->logo_y_pos = SCALE_AND_ROUND(unscaled->logo_y_pos);
        scaled->label_x_pos = SCALE_AND_ROUND(unscaled->label_x_pos);
        scaled->label_width = SCALE_AND_ROUND(unscaled->label_width);
        scaled->input_x_pos = SCALE_AND_ROUND(unscaled->input_x_pos);
        scaled->input_width = SCALE_AND_ROUND(unscaled->input_width);
        scaled->input_y_pos = SCALE_AND_ROUND(unscaled->input_y_pos);
        scaled->btn_ok_x_pos = SCALE_AND_ROUND(unscaled->btn_ok_x_pos);
        scaled->btn_ok_y_pos = SCALE_AND_ROUND(unscaled->btn_ok_y_pos);
        scaled->btn_ok_width = SCALE_AND_ROUND(unscaled->btn_ok_width);
        scaled->btn_ok_height = SCALE_AND_ROUND(unscaled->btn_ok_height);
        scaled->btn_cancel_x_pos = SCALE_AND_ROUND(unscaled->btn_cancel_x_pos);
        scaled->btn_cancel_y_pos = SCALE_AND_ROUND(unscaled->btn_cancel_y_pos);
        scaled->btn_cancel_width = SCALE_AND_ROUND(unscaled->btn_cancel_width);
        scaled->btn_cancel_height = SCALE_AND_ROUND(unscaled->btn_cancel_height);
        scaled->default_btn_height = fheight + DEFAULT_BUTTON_MARGIN_H;
        scaled->log_wnd_width = SCALE_AND_ROUND(unscaled->log_wnd_width);
        scaled->log_wnd_height = SCALE_AND_ROUND(unscaled->log_wnd_height);
        scaled->edit_height = fheight + DEFAULT_EDIT_MARGIN_H;
        scaled->combo_height = fheight + DEFAULT_COMBO_MARGIN_H;
        scaled->help_wnd_width = SCALE_AND_ROUND(unscaled->help_wnd_width);
        scaled->help_wnd_height = SCALE_AND_ROUND(unscaled->help_wnd_height);
#undef SCALE_AND_ROUND
    }
}
