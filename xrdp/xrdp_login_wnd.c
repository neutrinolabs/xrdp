/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2005

   main login window and login help window

*/

#include "xrdp.h"

/*****************************************************************************/
/* all login help screen events go here */
int xrdp_wm_login_help_notify(struct xrdp_bitmap* wnd,
                              struct xrdp_bitmap* sender,
                              int msg, int param1, int param2)
{
  struct xrdp_painter* p;

  if (wnd == 0)
    return 0;
  if (sender == 0)
    return 0;
  if (wnd->owner == 0)
    return 0;
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
    p = (struct xrdp_painter*)param1;
    if (p != 0)
    {
      p->font->color = wnd->wm->black;
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

/*****************************************************************************/
int server_begin_update(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = xrdp_painter_create(wm);
  xrdp_painter_begin_update(p);
  mod->painter = (int)p;
  return 0;
}

/*****************************************************************************/
int server_end_update(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  mod->painter = 0;
  return 0;
}

/*****************************************************************************/
int server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                     int color)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  p->fg_color = color;
  xrdp_painter_fill_rect(p, wm->screen, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                      int srcx, int srcy)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_orders_init(wm->orders);
  xrdp_orders_screen_blt(wm->orders, x, y, cx, cy, srcx, srcy, 0xcc, 0);
  xrdp_orders_send(wm->orders);
  return 0;
}

/*****************************************************************************/
int server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                      char* data)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;

  wm = (struct xrdp_wm*)mod->wm;
  b = xrdp_bitmap_create_with_data(cx, cy, wm->screen->bpp, data);
  //xrdp_wm_send_bitmap(wm, b, x, y, cx, cy);
  xrdp_painter_draw_bitmap((struct xrdp_painter*)mod->painter,
                           wm->screen, b, x, y, cx, cy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int server_set_cursor(struct xrdp_mod* mod, int x, int y,
                      char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_wm_send_cursor(wm, 2, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int server_palette(struct xrdp_mod* mod, int* palette)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  g_memcpy(wm->palette, palette, 256 * sizeof(int));
  xrdp_cache_add_palette(wm->cache, palette);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_popup_notify(struct xrdp_bitmap* wnd,
                         struct xrdp_bitmap* sender,
                         int msg, int param1, int param2)
{
  return 0;
}

/*****************************************************************************/
int server_error_popup(struct xrdp_mod* mod, char* error, char* caption)
{
#ifdef aa0
  struct xrdp_wm* wm;
  struct xrdp_bitmap* wnd;
  struct xrdp_bitmap* but;

  wm = (struct xrdp_wm*)mod->wm;
  wnd = xrdp_bitmap_create(400, 200, wm->screen->bpp, WND_TYPE_WND);
  xrdp_list_add_item(wm->screen->child_list, (int)wnd);
  wnd->parent = wm->screen;
  wnd->owner = wm->screen;
  wnd->wm = wm;
  wnd->bg_color = wm->grey;
  wnd->left = wm->screen->width / 2 - wnd->width / 2;
  wnd->top = wm->screen->height / 2 - wnd->height / 2;
  wnd->notify = xrdp_wm_popup_notify;
  g_strcpy(wnd->caption, caption);

  /* button */
  but = xrdp_bitmap_create(60, 25, wm->screen->bpp, WND_TYPE_BUTTON);
  xrdp_list_add_item(wnd->child_list, (int)but);
  but->parent = wnd;
  but->owner = wnd;
  but->wm = wm;
  but->left = 180;
  but->top = 160;
  but->id = 1;
  g_strcpy(but->caption, "OK");
  but->tab_stop = 1;

  xrdp_bitmap_invalidate(wm->screen, 0);
  //xrdp_bitmap_invalidate(wnd, 0);
  g_sleep(2000);
#endif
  return 0;
}

/*****************************************************************************/
int xrdp_wm_setup_mod(struct xrdp_wm* self,
                      struct xrdp_mod_data* mod_data)
{
  if (self != 0)
  {
    if (self->mod_handle == 0)
    {
      self->mod_handle = g_load_library(mod_data->lib);
      if (self->mod_handle != 0)
      {
        self->mod_init = (int(*)())
                  g_get_proc_address(self->mod_handle, "mod_init");
        self->mod_exit = (int(*)(int))
                  g_get_proc_address(self->mod_handle, "mod_exit");
        if (self->mod_init != 0 && self->mod_exit != 0)
        {
          self->mod = (struct xrdp_mod*)self->mod_init();
        }
      }
      if (self->mod != 0)
      {
        self->mod->wm = (int)self;
        self->mod->server_begin_update = server_begin_update;
        self->mod->server_end_update = server_end_update;
        self->mod->server_fill_rect = server_fill_rect;
        self->mod->server_screen_blt = server_screen_blt;
        self->mod->server_paint_rect = server_paint_rect;
        self->mod->server_set_cursor = server_set_cursor;
        self->mod->server_palette = server_palette;
        self->mod->server_error_popup= server_error_popup;
      }
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_delete_all_childs(struct xrdp_wm* self)
{
  int i;
  struct xrdp_bitmap* b;

  for (i = self->screen->child_list->count - 1; i >= 0; i--)
  {
    b = (struct xrdp_bitmap*)xrdp_list_get_item(self->screen->child_list, i);
    xrdp_bitmap_delete(b);
  }
  xrdp_bitmap_invalidate(self->screen, 0);
  return 0;
}

/*****************************************************************************/
/* all login screen events go here */
int xrdp_wm_login_notify(struct xrdp_bitmap* wnd,
                         struct xrdp_bitmap* sender,
                         int msg, int param1, int param2)
{
  struct xrdp_bitmap* help;
  struct xrdp_bitmap* but;
  struct xrdp_bitmap* b;
  struct xrdp_bitmap* combo;
  struct xrdp_bitmap* edit;
  struct xrdp_wm* wm;
  struct xrdp_rect rect;
  struct xrdp_mod_data con_mod;
  struct xrdp_mod_data* mod;
  int i;

  if (wnd->modal_dialog != 0 && msg != 100)
  {
    return 0;
  }
  wm = wnd->wm;
  if (msg == 1) /* click */
  {
    if (sender->id == 1) /* help button */
    {
      /* create help screen */
      help = xrdp_bitmap_create(300, 300, wnd->wm->screen->bpp, 1);
      xrdp_list_insert_item(wnd->wm->screen->child_list, 0, (int)help);
      help->parent = wnd->wm->screen;
      help->owner = wnd;
      wnd->modal_dialog = help;
      help->wm = wnd->wm;
      help->bg_color = wnd->wm->grey;
      help->left = wnd->wm->screen->width / 2 - help->width / 2;
      help->top = wnd->wm->screen->height / 2 - help->height / 2;
      help->notify = xrdp_wm_login_help_notify;
      g_strcpy(help->caption, "Login help");
      /* ok button */
      but = xrdp_bitmap_create(60, 25, wnd->wm->screen->bpp, 3);
      xrdp_list_insert_item(help->child_list, 0, (int)but);
      but->parent = help;
      but->owner = help;
      but->wm = wnd->wm;
      but->left = 120;
      but->top = 260;
      but->id = 1;
      but->tab_stop = 1;
      g_strcpy(but->caption, "OK");
      /* draw it */
      help->focused_control = but;
      //wnd->wm->focused_window = help;
      xrdp_bitmap_invalidate(help, 0);
      xrdp_wm_set_focused(wnd->wm, help);
      //xrdp_bitmap_invalidate(wnd->focused_control, 0);
    }
    else if (sender->id == 2) /* cancel button */
    {
      /*if (wnd != 0)
        if (wnd->wm != 0)
          if (wnd->wm->pro_layer != 0)
            wnd->wm->pro_layer->term = 1;*/
    }
    else if (sender->id == 3) /* ok button */
    {
      combo = xrdp_bitmap_get_child_by_id(wnd, 6);
      if (combo != 0)
      {
        mod = (struct xrdp_mod_data*)xrdp_list_get_item(combo->data_list,
                                                        combo->item_index);
        if (mod != 0)
        {
          con_mod = *mod;
          if (g_strcmp(con_mod.username, "ask") == 0)
          {
            edit = xrdp_bitmap_get_child_by_id(wnd, 4);
            if (edit != 0)
            {
              g_strcpy(con_mod.username, edit->caption);
            }
          }
          if (g_strcmp(con_mod.password, "ask") == 0)
          {
            edit = xrdp_bitmap_get_child_by_id(wnd, 5);
            if (edit != 0)
            {
              g_strcpy(con_mod.password, edit->caption);
            }
          }
          if (xrdp_wm_setup_mod(wm, mod) == 0)
          {
            xrdp_wm_delete_all_childs(wm);
            if (!wm->pro_layer->term)
            {
              if (wm->mod->mod_start(wm->mod, wm->screen->width,
                                     wm->screen->height, wm->screen->bpp) != 0)
              {
                wm->pro_layer->term = 1; /* kill session */
              }
            }
            if (!wm->pro_layer->term)
            {
              wm->mod->mod_set_param(wm->mod, "ip", con_mod.ip);
              wm->mod->mod_set_param(wm->mod, "port", con_mod.port);
              wm->mod->mod_set_param(wm->mod, "username", con_mod.username);
              wm->mod->mod_set_param(wm->mod, "password", con_mod.password);
              if (wm->mod->mod_connect(wm->mod) != 0)
              {
                wm->pro_layer->term = 1; /* kill session */
              }
            }
            if (!wm->pro_layer->term)
            {
              if (wm->mod->sck != 0)
              {
                wm->pro_layer->app_sck = wm->mod->sck;
              }
            }
          }
        }
      }
    }
  }
  else if (msg == 2) /* mouse move */
  {
  }
  else if (msg == 100) /* modal result is done */
  {
    i = xrdp_list_index_of(wnd->wm->screen->child_list, (int)sender);
    if (i >= 0)
    {
      b = (struct xrdp_bitmap*)
              xrdp_list_get_item(wnd->wm->screen->child_list, i);
      xrdp_list_remove_item(sender->wm->screen->child_list, i);
      MAKERECT(rect, b->left, b->top, b->width, b->height);
      xrdp_bitmap_invalidate(wnd->wm->screen, &rect);
      xrdp_bitmap_delete(sender);
      wnd->modal_dialog = 0;
      xrdp_wm_set_focused(wnd->wm, wnd);
    }
  }
  return 0;
}

/******************************************************************************/
int xrdp_wm_login_fill_in_combo(struct xrdp_wm* self, struct xrdp_bitmap* b)
{
  struct xrdp_list* sections;
  struct xrdp_list* section_names;
  struct xrdp_list* section_values;
  int fd;
  int i;
  int j;
  char* p;
  char* q;
  char* r;
  struct xrdp_mod_data* mod_data;

  sections = xrdp_list_create();
  sections->auto_free = 1;
  section_names = xrdp_list_create();
  section_names->auto_free = 1;
  section_values = xrdp_list_create();
  section_values->auto_free = 1;
  fd = g_file_open("xrdp.ini");
  xrdp_file_read_sections(fd, sections);
  for (i = 0; i < sections->count; i++)
  {
    p = (char*)xrdp_list_get_item(sections, i);
    xrdp_file_read_section(fd, p, section_names, section_values);
    if (g_strcmp(p, "globals") == 0)
    {
    }
    else
    {
      mod_data = (struct xrdp_mod_data*)
                     g_malloc(sizeof(struct xrdp_mod_data), 1);
      g_strcpy(mod_data->name, p);
      for (j = 0; j < section_names->count; j++)
      {
        q = (char*)xrdp_list_get_item(section_names, j);
        r = (char*)xrdp_list_get_item(section_values, j);
        if (g_strcmp("name", q) == 0)
        {
          g_strcpy(mod_data->name, r);
        }
        else if (g_strcmp("lib", q) == 0)
        {
          g_strcpy(mod_data->lib, r);
        }
        else if (g_strcmp("ip", q) == 0)
        {
          g_strcpy(mod_data->ip, r);
        }
        else if (g_strcmp("port", q) == 0)
        {
          g_strcpy(mod_data->port, r);
        }
        else if (g_strcmp("username", q) == 0)
        {
          g_strcpy(mod_data->username, r);
        }
        else if (g_strcmp("password", q) == 0)
        {
          g_strcpy(mod_data->password, r);
        }
      }
      xrdp_list_add_item(b->string_list, (int)g_strdup(mod_data->name));
      xrdp_list_add_item(b->data_list, (int)mod_data);
    }
  }
  g_file_close(fd);
  xrdp_list_delete(sections);
  xrdp_list_delete(section_names);
  xrdp_list_delete(section_values);
  return 0;
}

/******************************************************************************/
int xrdp_login_wnd_create(struct xrdp_wm* self)
{
  struct xrdp_bitmap* but;

  /* draw login window */
  self->login_window = xrdp_bitmap_create(400, 200, self->screen->bpp,
                                          WND_TYPE_WND);
  xrdp_list_add_item(self->screen->child_list, (int)self->login_window);
  self->login_window->parent = self->screen;
  self->login_window->owner = self->screen;
  self->login_window->wm = self;
  self->login_window->bg_color = self->grey;
  self->login_window->left = self->screen->width / 2 -
                             self->login_window->width / 2;
  self->login_window->top = self->screen->height / 2 -
                            self->login_window->height / 2;
  self->login_window->notify = xrdp_wm_login_notify;
  g_strcpy(self->login_window->caption, "Login to xrdp");

  /* image */
  but = xrdp_bitmap_create(4, 4, self->screen->bpp, WND_TYPE_IMAGE);
  xrdp_bitmap_load(but, "xrdp256.bmp", self->palette);
  but->parent = self->screen;
  but->owner = self->screen;
  but->wm = self;
  but->left = self->screen->width - but->width;
  but->top = self->screen->height - but->height;
  xrdp_list_add_item(self->screen->child_list, (int)but);

  /* image */
  but = xrdp_bitmap_create(4, 4, self->screen->bpp, WND_TYPE_IMAGE);
  xrdp_bitmap_load(but, "ad256.bmp", self->palette);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 10;
  but->top = 30;
  xrdp_list_add_item(self->login_window->child_list, (int)but);

  /* label */
  but = xrdp_bitmap_create(60, 20, self->screen->bpp, WND_TYPE_LABEL);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 155;
  but->top = 50;
  g_strcpy(but->caption, "Username");

  /* edit */
  but = xrdp_bitmap_create(140, 20, self->screen->bpp, WND_TYPE_EDIT);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 220;
  but->top = 50;
  but->id = 4;
  but->cursor = 1;
  but->tab_stop = 1;
  self->login_window->focused_control = but;

  /* label */
  but = xrdp_bitmap_create(60, 20, self->screen->bpp, WND_TYPE_LABEL);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 155;
  but->top = 80;
  g_strcpy(but->caption, "Password");

  /* edit */
  but = xrdp_bitmap_create(140, 20, self->screen->bpp, WND_TYPE_EDIT);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 220;
  but->top = 80;
  but->id = 5;
  but->cursor = 1;
  but->tab_stop = 1;
  but->password_char = '*';

  /* label */
  but = xrdp_bitmap_create(60, 20, self->screen->bpp, WND_TYPE_LABEL);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 155;
  but->top = 110;
  g_strcpy(but->caption, "Module");

  /* combo */
  but = xrdp_bitmap_create(140, 20, self->screen->bpp, WND_TYPE_COMBO);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 220;
  but->top = 110;
  but->id = 6;
  but->tab_stop = 1;
  xrdp_wm_login_fill_in_combo(self, but);

  /* button */
  but = xrdp_bitmap_create(60, 25, self->screen->bpp, WND_TYPE_BUTTON);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 180;
  but->top = 160;
  but->id = 3;
  g_strcpy(but->caption, "OK");
  but->tab_stop = 1;

  /* button */
  but = xrdp_bitmap_create(60, 25, self->screen->bpp, WND_TYPE_BUTTON);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 250;
  but->top = 160;
  but->id = 2;
  g_strcpy(but->caption, "Cancel");
  but->tab_stop = 1;

  /* button */
  but = xrdp_bitmap_create(60, 25, self->screen->bpp, WND_TYPE_BUTTON);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 320;
  but->top = 160;
  but->id = 1;
  g_strcpy(but->caption, "Help");
  but->tab_stop = 1;

  return 0;
}
