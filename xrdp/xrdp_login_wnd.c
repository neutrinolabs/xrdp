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

#include "xrdp.h"
#define ACCESS
#include "log.h"

/*****************************************************************************/
/* all login help screen events go here */
static int DEFAULT_CC
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
            p->fg_color = wnd->wm->black;
            xrdp_painter_draw_text(p, wnd, 10, 30, "You must be authenticated \
before using this");
            xrdp_painter_draw_text(p, wnd, 10, 46, "session.");
            xrdp_painter_draw_text(p, wnd, 10, 78, "Enter a valid username in \
the username edit box.");
            xrdp_painter_draw_text(p, wnd, 10, 94, "Enter the password in \
the password edit box.");
            xrdp_painter_draw_text(p, wnd, 10, 110, "Both the username and \
password are case");
            xrdp_painter_draw_text(p, wnd, 10, 126, "sensitive.");
            xrdp_painter_draw_text(p, wnd, 10, 158, "Contact your system \
administrator if you are");
            xrdp_painter_draw_text(p, wnd, 10, 174, "having problems \
logging on.");
        }
    }

    return 0;
}

#if 0
/*****************************************************************************/
static int DEFAULT_CC
xrdp_wm_popup_notify(struct xrdp_bitmap *wnd,
                     struct xrdp_bitmap *sender,
                     int msg, int param1, int param2)
{
    return 0;
}
#endif

/*****************************************************************************/
int APP_CC
xrdp_wm_delete_all_childs(struct xrdp_wm *self)
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
static int APP_CC
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
static int APP_CC
xrdp_wm_help_clicked(struct xrdp_bitmap *wnd)
{
    struct xrdp_bitmap *help;
    struct xrdp_bitmap *but;

    /* create help screen */
    help = xrdp_bitmap_create(DEFAULT_WND_HELP_W, DEFAULT_WND_HELP_H, wnd->wm->screen->bpp,
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
    but = xrdp_bitmap_create(DEFAULT_BUTTON_W, DEFAULT_BUTTON_H, wnd->wm->screen->bpp,
                             WND_TYPE_BUTTON, wnd->wm);
    list_insert_item(help->child_list, 0, (long)but);
    but->parent = help;
    but->owner = help;
    but->left = ((DEFAULT_WND_HELP_W / 2) - (DEFAULT_BUTTON_W / 2)); /* center */
    but->top = DEFAULT_WND_HELP_H - DEFAULT_BUTTON_H - 15;
    but->id = 1;
    but->tab_stop = 1;
    set_string(&but->caption1, "OK");
    /* draw it */
    help->focused_control = but;
    help->default_button = but;
    help->esc_button = but;
    xrdp_bitmap_invalidate(help, 0);
    xrdp_wm_set_focused(wnd->wm, help);
    return 0;
}

/*****************************************************************************/
static int APP_CC
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
static int APP_CC
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
            /* gota copy these cause dialog gets freed */
            list_append_list_strdup(mod_data->names, wm->mm->login_names, 0);
            list_append_list_strdup(mod_data->values, wm->mm->login_values, 0);
            xrdp_wm_set_login_mode(wm, 2);
        }
    }
    else
    {
        log_message(LOG_LEVEL_ERROR, "Combo is 0 - potential programming error");
    }

    return 0;
}

/******************************************************************************/
static int APP_CC
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

    globals = &self->xrdp_config->cfg_globals;

    username_set = 0;

    /* free labels and edits, cause we gota create them */
    /* creation or combo changed */
    for (index = 100; index < 200; index++)
    {
        b = xrdp_bitmap_get_child_by_id(combo->parent, index);
        xrdp_bitmap_delete(b);
    }

    insert_index = list_index_of(self->login_window->child_list,
                                 (long)combo);
    insert_index++;
    mod = (struct xrdp_mod_data *)
          list_get_item(combo->data_list, combo->item_index);

    if (mod != 0)
    {
        count = 0;

        for (index = 0; index < mod->names->count; index++)
        {
            value = (char *)list_get_item(mod->values, index);

            if (g_strncmp("ask", value, 3) == 0)
            {
                /* label */
                b = xrdp_bitmap_create(95, DEFAULT_EDIT_H, self->screen->bpp,
                                       WND_TYPE_LABEL, self);
                list_insert_item(self->login_window->child_list, insert_index,
                                 (long)b);
                insert_index++;
                b->parent = self->login_window;
                b->owner = self->login_window;
                b->left = globals->ls_label_x_pos;

                b->top = globals->ls_input_y_pos + DEFAULT_COMBO_H + 5 + (DEFAULT_EDIT_H + 5) * count;
                b->id = 100 + 2 * count;
                name = (char *)list_get_item(mod->names, index);
                set_string(&b->caption1, name);

                /* edit */
                b = xrdp_bitmap_create(DEFAULT_EDIT_W, DEFAULT_EDIT_H, self->screen->bpp,
                                       WND_TYPE_EDIT, self);
                list_insert_item(self->login_window->child_list, insert_index,
                                 (long)b);
                insert_index++;
                b->parent = self->login_window;
                b->owner = self->login_window;
                b->left = globals->ls_input_x_pos;

                b->top = globals->ls_input_y_pos + DEFAULT_COMBO_H + 5 + (DEFAULT_EDIT_H + 5) * count;

                b->id = 100 + 2 * count + 1;
                b->pointer = 1;
                b->tab_stop = 1;
                b->caption1 = (char *)g_malloc(256, 1);
                g_strncpy(b->caption1, value + 3, 255);
                b->edit_pos = g_mbstowcs(0, b->caption1, 0);

                if (self->login_window->focused_control == 0)
                {
                    self->login_window->focused_control = b;
                }
		/*Use the domain name as the destination IP/DNS
		 This is useful in a gateway setup.*/
		if (g_strncmp(name, "ip", 255) == 0)
		{
		    /* If the first char in the domain name is '_' we use the domain name as IP*/
		    if(self->session->client_info->domain[0]=='_')
		    {
			g_strncpy(b->caption1, &self->session->client_info->domain[1], 255);
			b->edit_pos = g_mbstowcs(0, b->caption1, 0);
		    }

		}
                if (g_strncmp(name, "username", 255) == 0)
                {
                    g_strncpy(b->caption1, self->session->client_info->username, 255);
                    b->edit_pos = g_mbstowcs(0, b->caption1, 0);

                    if (b->edit_pos > 0)
                    {
                        username_set = 1;
                    }
                }

#ifdef ACCESS

                if ((g_strncmp(name, "password", 255) == 0) || (g_strncmp(name, "pampassword", 255) == 0))
#else
                if (g_strncmp(name, "password", 255) == 0)
#endif
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
static int DEFAULT_CC
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
static int APP_CC
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
    char cfg_file[256];
    struct xrdp_mod_data *mod_data;

    sections = list_create();
    sections->auto_free = 1;
    section_names = list_create();
    section_names->auto_free = 1;
    section_values = list_create();
    section_values->auto_free = 1;
    g_snprintf(cfg_file, 255, "%s/xrdp.ini", XRDP_CFG_PATH);
    fd = g_file_open(cfg_file); /* xrdp.ini */

    if (fd < 1)
    {
        log_message(LOG_LEVEL_ERROR, "Could not read xrdp ini file %s", cfg_file);
    }

    file_read_sections(fd, sections);

    for (i = 0; i < sections->count; i++)
    {
        p = (char *)list_get_item(sections, i);
        file_read_section(fd, p, section_names, section_values);

        if ((g_strncmp(p, "globals", 255) == 0)
                || (g_strncmp(p, "channels", 255) == 0)
                || (g_strncmp(p, "Logging", 255) == 0))
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

                list_add_item(mod_data->names, (long)g_strdup(q));
                list_add_item(mod_data->values, (long)g_strdup(r));
            }

            list_add_item(b->string_list, (long)g_strdup(name));
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
int APP_CC
xrdp_login_wnd_create(struct xrdp_wm *self)
{
    struct xrdp_bitmap      *but;
    struct xrdp_bitmap      *combo;
    struct xrdp_cfg_globals *globals;

    char buf[256];
    char buf1[256];
    int log_width;
    int log_height;
    int regular;

    globals = &self->xrdp_config->cfg_globals;

    log_width = globals->ls_width;
    log_height = globals->ls_height;
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

    /* draw login window */
    self->login_window = xrdp_bitmap_create(log_width, log_height, self->screen->bpp,
                                            WND_TYPE_WND, self);
    list_add_item(self->screen->child_list, (long)self->login_window);
    self->login_window->parent = self->screen;
    self->login_window->owner = self->screen;
    self->login_window->bg_color = globals->ls_bg_color;

    self->login_window->left = self->screen->width / 2 -
                               self->login_window->width / 2;

    self->login_window->top = self->screen->height / 2 -
                              self->login_window->height / 2;

    self->login_window->notify = xrdp_wm_login_notify;

    g_gethostname(buf1, 256);
    g_sprintf(buf, "Login to %s", buf1);
    set_string(&self->login_window->caption1, buf);

    if (regular)
    {
        /* if logo image not specified, use default */
        if (globals->ls_logo_filename[0] == 0)
            g_snprintf(globals->ls_logo_filename, 255, "%s/xrdp_logo.bmp", XRDP_SHARE_PATH);

        /* logo image */
        but = xrdp_bitmap_create(4, 4, self->screen->bpp, WND_TYPE_IMAGE, self);

        if (self->screen->bpp <= 8)
            g_snprintf(globals->ls_logo_filename, 255, "%s/ad256.bmp", XRDP_SHARE_PATH);

        xrdp_bitmap_load(but, globals->ls_logo_filename, self->palette);
        but->parent = self->login_window;
        but->owner = self->login_window;
        but->left = globals->ls_logo_x_pos;
        but->top = globals->ls_logo_y_pos;
        list_add_item(self->login_window->child_list, (long)but);
    }

    /* label */
    but = xrdp_bitmap_create(globals->ls_label_width, DEFAULT_EDIT_H, self->screen->bpp, WND_TYPE_LABEL, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_label_x_pos;
    but->top = globals->ls_input_y_pos;
    set_string(&but->caption1, "Session");

    /* combo */
    combo = xrdp_bitmap_create(globals->ls_input_width, DEFAULT_COMBO_H,
                               self->screen->bpp, WND_TYPE_COMBO, self);
    list_add_item(self->login_window->child_list, (long)combo);
    combo->parent = self->login_window;
    combo->owner = self->login_window;
    combo->left = globals->ls_input_x_pos;
    combo->top = globals->ls_input_y_pos;
    combo->id = 6;
    combo->tab_stop = 1;
    xrdp_wm_login_fill_in_combo(self, combo);

    /* OK button */
    but = xrdp_bitmap_create(globals->ls_btn_ok_width, globals->ls_btn_ok_height,
                             self->screen->bpp, WND_TYPE_BUTTON, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_btn_ok_x_pos;
    but->top = globals->ls_btn_ok_y_pos;
    but->id = 3;
    set_string(&but->caption1, "OK");
    but->tab_stop = 1;
    self->login_window->default_button = but;

    /* Cancel button */
    but = xrdp_bitmap_create(globals->ls_btn_cancel_width,
                             globals->ls_btn_cancel_height, self->screen->bpp,
                             WND_TYPE_BUTTON, self);
    list_add_item(self->login_window->child_list, (long)but);
    but->parent = self->login_window;
    but->owner = self->login_window;
    but->left = globals->ls_btn_cancel_x_pos;
    but->top = globals->ls_btn_cancel_y_pos;
    but->id = 2;
    set_string(&but->caption1, "Cancel");
    but->tab_stop = 1;
    self->login_window->esc_button = but;

    /* labels and edits */
    xrdp_wm_show_edits(self, combo);

    return 0;
}

/**
 * Load configuration from xrdp.ini file
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int APP_CC
load_xrdp_config(struct xrdp_config *config, int bpp)
{
    struct xrdp_cfg_globals  *globals;

    struct list *names;
    struct list *values;

    char *n;
    char *v;
    char  buf[256];
    int   fd;
    int   i;

    if (!config)
        return -1;

    globals = &config->cfg_globals;

    /* set default values incase we can't get them from xrdp.ini file */
    globals->ini_version = 1;
    globals->ls_top_window_bg_color = HCOLOR(bpp, xrdp_wm_htoi("009cb5"));
    globals->ls_bg_color = HCOLOR(bpp, xrdp_wm_htoi("dedede"));
    globals->ls_width = 350;
    globals->ls_height = 350;
    globals->ls_bg_color = 0xdedede;
    globals->ls_logo_x_pos = 63;
    globals->ls_logo_y_pos = 50;
    globals->ls_label_x_pos = 30;
    globals->ls_label_width = 60;
    globals->ls_input_x_pos = 110;
    globals->ls_input_width = 210;
    globals->ls_input_y_pos = 150;
    globals->ls_btn_ok_x_pos = 150;
    globals->ls_btn_ok_y_pos = 300;
    globals->ls_btn_ok_width = 85;
    globals->ls_btn_ok_height =30;
    globals->ls_btn_cancel_x_pos = 245;
    globals->ls_btn_cancel_y_pos = 300;
    globals->ls_btn_cancel_width = 85;
    globals->ls_btn_cancel_height = 30;

    /* open xrdp.ini file */
    g_snprintf(buf, 255, "%s/xrdp.ini", XRDP_CFG_PATH);
    if ((fd = g_file_open(buf)) < 0)
    {
        log_message(LOG_LEVEL_ERROR,"load_config: Could not read "
                    "xrdp.ini file %s", buf);
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
        log_message(LOG_LEVEL_ERROR,"load_config: Could not read globals "
                    "section from xrdp.ini file %s", buf);
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
            globals->ini_version = g_atoi(v);

        else if (g_strncmp(n, "bitmap_cache", 64) == 0)
            globals->use_bitmap_cache = g_text2bool(v);

        else if (g_strncmp(n, "bitmap_compression", 64) == 0)
            globals->use_bitmap_compression = g_text2bool(v);

        else if (g_strncmp(n, "port", 64) == 0)
            globals->port = g_atoi(v);

        else if (g_strncmp(n, "crypt_level", 64) == 0)
        {
            if (g_strcmp(v, "low") == 0)
                globals->crypt_level = 1;
            else if (g_strcmp(v, "medium") == 0)
                globals->crypt_level = 2;
            else
                globals->crypt_level = 3;
        }

        else if (g_strncmp(n, "allow_channels", 64) == 0)
            globals->allow_channels = g_text2bool(v);

        else if (g_strncmp(n, "max_bpp", 64) == 0)
            globals->max_bpp = g_atoi(v);

        else if (g_strncmp(n, "fork", 64) == 0)
            globals->fork = g_text2bool(v);

        else if (g_strncmp(n, "tcp_nodelay", 64) == 0)
            globals->tcp_nodelay = g_text2bool(v);

        else if (g_strncmp(n, "tcp_keepalive", 64) == 0)
            globals->tcp_keepalive = g_text2bool(v);

        else if (g_strncmp(n, "tcp_send_buffer_bytes", 64) == 0)
            globals->tcp_send_buffer_bytes = g_atoi(v);

        else if (g_strncmp(n, "tcp_recv_buffer_bytes", 64) == 0)
            globals->tcp_recv_buffer_bytes = g_atoi(v);

        /* colors */

        else if (g_strncmp(n, "grey", 64) == 0)
            globals->grey = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "black", 64) == 0)
            globals->black = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "dark_grey", 64) == 0)
            globals->dark_grey = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "blue", 64) == 0)
            globals->blue = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "dark_blue", 64) == 0)
            globals->dark_blue = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "white", 64) == 0)
            globals->white = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "red", 64) == 0)
            globals->red = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "green", 64) == 0)
            globals->green = xrdp_wm_htoi(v);

        else if (g_strncmp(n, "background", 64) == 0)
            globals->background = xrdp_wm_htoi(v);

        /* misc stuff */

        else if (g_strncmp(n, "autorun", 255) == 0)
            g_strncpy(globals->autorun, v, 255);

        else if (g_strncmp(n, "hidelogwindow", 64) == 0)
            globals->hidelogwindow = g_text2bool(v);

        else if (g_strncmp(n, "require_credentials", 64) == 0)
            globals->require_credentials = g_text2bool(v);

        else if (g_strncmp(n, "bulk_compression", 64) == 0)
            globals->bulk_compression = g_text2bool(v);

        else if (g_strncmp(n, "new_cursors", 64) == 0)
            globals->new_cursors = g_text2bool(v);

        else if (g_strncmp(n, "nego_sec_layer", 64) == 0)
            globals->nego_sec_layer = g_atoi(v);

        else if (g_strncmp(n, "allow_multimon", 64) == 0)
            globals->allow_multimon = g_text2bool(v);

        /* login screen values */
        else if (g_strncmp(n, "ls_top_window_bg_color", 64) == 0)
            globals->ls_top_window_bg_color = HCOLOR(bpp, xrdp_wm_htoi(v));

        else if (g_strncmp(n, "ls_width", 64) == 0)
            globals->ls_width = g_atoi(v);

        else if (g_strncmp(n, "ls_height", 64) == 0)
            globals->ls_height = g_atoi(v);

        else if (g_strncmp(n, "ls_bg_color", 64) == 0)
            globals->ls_bg_color = HCOLOR(bpp, xrdp_wm_htoi(v));

        else if (g_strncmp(n, "ls_logo_filename", 255) == 0)
        {
            g_strncpy(globals->ls_logo_filename, v, 255);
            globals->ls_logo_filename[255] = 0;
        }

        else if (g_strncmp(n, "ls_logo_x_pos", 64) == 0)
            globals->ls_logo_x_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_logo_y_pos", 64) == 0)
            globals->ls_logo_y_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_label_x_pos", 64) == 0)
            globals->ls_label_x_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_label_width", 64) == 0)
            globals->ls_label_width = g_atoi(v);

        else if (g_strncmp(n, "ls_input_x_pos", 64) == 0)
            globals->ls_input_x_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_input_width", 64) == 0)
            globals->ls_input_width = g_atoi(v);

        else if (g_strncmp(n, "ls_input_y_pos", 64) == 0)
            globals->ls_input_y_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_ok_x_pos", 64) == 0)
            globals->ls_btn_ok_x_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_ok_y_pos", 64) == 0)
            globals->ls_btn_ok_y_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_ok_width", 64) == 0)
            globals->ls_btn_ok_width = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_ok_height", 64) == 0)
            globals->ls_btn_ok_height = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_cancel_x_pos", 64) == 0)
            globals->ls_btn_cancel_x_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_cancel_y_pos", 64) == 0)
            globals->ls_btn_cancel_y_pos = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_cancel_width", 64) == 0)
            globals->ls_btn_cancel_width = g_atoi(v);

        else if (g_strncmp(n, "ls_btn_cancel_height", 64) == 0)
            globals->ls_btn_cancel_height = g_atoi(v);
    }

#if 0
    g_writeln("ini_version:             %d", globals->ini_version);
    g_writeln("use_bitmap_cache:        %d", globals->use_bitmap_cache);
    g_writeln("use_bitmap_compression:  %d", globals->use_bitmap_compression);
    g_writeln("port:                    %d", globals->port);
    g_writeln("crypt_level:             %d", globals->crypt_level);
    g_writeln("allow_channels:          %d", globals->allow_channels);
    g_writeln("max_bpp:                 %d", globals->max_bpp);
    g_writeln("fork:                    %d", globals->fork);
    g_writeln("tcp_nodelay:             %d", globals->tcp_nodelay);
    g_writeln("tcp_keepalive:           %d", globals->tcp_keepalive);
    g_writeln("tcp_send_buffer_bytes:   %d", globals->tcp_send_buffer_bytes);
    g_writeln("tcp_recv_buffer_bytes:   %d", globals->tcp_recv_buffer_bytes);
    g_writeln("new_cursors:             %d", globals->new_cursors);
    g_writeln("allow_multimon:          %d", globals->allow_multimon);

    g_writeln("grey:                    %d", globals->grey);
    g_writeln("black:                   %d", globals->black);
    g_writeln("dark_grey:               %d", globals->dark_grey);
    g_writeln("blue:                    %d", globals->blue);
    g_writeln("dark_blue:               %d", globals->dark_blue);
    g_writeln("white:                   %d", globals->white);
    g_writeln("red:                     %d", globals->red);
    g_writeln("green:                   %d", globals->green);
    g_writeln("background:              %d", globals->background);

    g_writeln("autorun:                 %s", globals->autorun);
    g_writeln("hidelogwindow:           %d", globals->hidelogwindow);
    g_writeln("require_credentials:     %d", globals->require_credentials);
    g_writeln("bulk_compression:        %d", globals->bulk_compression);
    g_writeln("new_cursors:             %d", globals->new_cursors);
    g_writeln("nego_sec_layer:          %d", globals->nego_sec_layer);
    g_writeln("allow_multimon:          %d", globals->allow_multimon);

    g_writeln("ls_top_window_bg_color:  %x", globals->ls_top_window_bg_color);
    g_writeln("ls_width:                %d", globals->ls_width);
    g_writeln("ls_height:               %d", globals->ls_height);
    g_writeln("ls_bg_color:             %x", globals->ls_bg_color);
    g_writeln("ls_logo_filename:        %s", globals->ls_logo_filename);
    g_writeln("ls_logo_x_pos:           %d", globals->ls_logo_x_pos);
    g_writeln("ls_logo_y_pos:           %d", globals->ls_logo_y_pos);
    g_writeln("ls_label_x_pos:          %d", globals->ls_label_x_pos);
    g_writeln("ls_label_width:          %d", globals->ls_label_width);
    g_writeln("ls_input_x_pos:          %d", globals->ls_input_x_pos);
    g_writeln("ls_input_width:          %d", globals->ls_input_width);
    g_writeln("ls_input_y_pos:          %d", globals->ls_input_y_pos);
    g_writeln("ls_btn_ok_x_pos:         %d", globals->ls_btn_ok_x_pos);
    g_writeln("ls_btn_ok_y_pos:         %d", globals->ls_btn_ok_y_pos);
    g_writeln("ls_btn_ok_width:         %d", globals->ls_btn_ok_width);
    g_writeln("ls_btn_ok_height:        %d", globals->ls_btn_ok_height);
    g_writeln("ls_btn_cancel_x_pos:     %d", globals->ls_btn_cancel_x_pos);
    g_writeln("ls_btn_cancel_y_pos:     %d", globals->ls_btn_cancel_y_pos);
    g_writeln("ls_btn_cancel_width:     %d", globals->ls_btn_cancel_width);
    g_writeln("ls_btn_cancel_height:    %d", globals->ls_btn_cancel_height);
#endif

    list_delete(names);
    list_delete(values);
    g_file_close(fd);
    return 0;
}
