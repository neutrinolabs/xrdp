
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
   Copyright (C) Jay Sorg 2004

   xrdp: A Remote Desktop Protocol server.
   simple window manager

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_wm* xrdp_wm_create(struct xrdp_process* owner)
{
  struct xrdp_wm* self;

  self = (struct xrdp_wm*)g_malloc(sizeof(struct xrdp_wm), 1);
  self->screen = xrdp_bitmap_create(owner->rdp_layer->width,
                                    owner->rdp_layer->height,
                                    owner->rdp_layer->bpp, 2);
  self->screen->wm = self;
  self->pro_layer = owner;
  self->orders = owner->orders;
  self->painter = xrdp_painter_create(self);
  self->rdp_layer = owner->rdp_layer;
  self->cache = xrdp_cache_create(self, self->orders);
  return self;
}

/*****************************************************************************/
void xrdp_wm_delete(struct xrdp_wm* self)
{
  if (self == 0)
    return;
  xrdp_cache_delete(self->cache);
  xrdp_painter_delete(self->painter);
  xrdp_bitmap_delete(self->screen);
  g_free(self);
}

/*****************************************************************************/
int xrdp_wm_send_palette(struct xrdp_wm* self)
{
  int i;
  int color;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  xrdp_rdp_init_data(self->rdp_layer, s);
  out_uint16_le(s, RDP_UPDATE_PALETTE);
  out_uint16_le(s, 0);
  out_uint16_le(s, 256); /* # of colors */
  out_uint16_le(s, 0);
  for (i = 0; i < 256; i++)
  {
    color = self->palette[i];
    out_uint8(s, color >> 16);
    out_uint8(s, color >> 8);
    out_uint8(s, color);
  }
  s_mark_end(s);
  xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_UPDATE);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* todo */
int xrdp_wm_send_bitmap(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                        int x, int y, int cx, int cy)
{
  int data_size;
  int line_size;
  int i;
  int total_lines;
  int lines_sending;
  char* p;
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  data_size = bitmap->width * bitmap->height * ((bitmap->bpp + 7) / 8);
  line_size = bitmap->width * ((bitmap->bpp + 7) / 8);
  total_lines = data_size / line_size;
  i = 0;
  p = bitmap->data;
  lines_sending = 4096 / line_size;
  if (lines_sending > total_lines)
    lines_sending = total_lines;
  while (i < total_lines)
  {
    xrdp_rdp_init_data(self->rdp_layer, s);
    out_uint16_le(s, RDP_UPDATE_BITMAP);
    out_uint16_le(s, 1); /* num updates */
    out_uint16_le(s, x);
    out_uint16_le(s, y + i);
    out_uint16_le(s, (x + cx) - 1);
    out_uint16_le(s, (y + i + cy) - 1);
    out_uint16_le(s, bitmap->width);
    out_uint16_le(s, lines_sending);
    out_uint16_le(s, bitmap->bpp); /* bpp */
    out_uint16_le(s, 0); /* compress */
    out_uint16_le(s, line_size * lines_sending); /* bufsize */
    out_uint8a(s, p, line_size * lines_sending);
    s_mark_end(s);
    xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_UPDATE);
    p = p + line_size * lines_sending;
    i = i + lines_sending;
    if (i + lines_sending > total_lines)
      lines_sending = total_lines - i;
    if (lines_sending <= 0)
      break;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_color15(int r, int g, int b)
{
  r = r >> 3;
  g = g >> 3;
  b = b >> 3;
  return (r << 10) | (g << 5) | b;
}

/*****************************************************************************/
int xrdp_wm_color16(int r, int g, int b)
{
  r = r >> 3;
  g = g >> 2;
  b = b >> 3;
  return (r << 11) | (g << 5) | b;
}

/*****************************************************************************/
int xrdp_wm_color24(int r, int g, int b)
{
  return r | (g << 8) | (b << 16);
}

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
  else if (msg == 3) /* paint */
  {
    p = (struct xrdp_painter*)param1;
    if (p != 0)
    {
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
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_set_focused(struct xrdp_wm* self, struct xrdp_bitmap* wnd)
{
  if (self == 0)
    return 0;
  if (self->focused_window == wnd)
    return 0;
  if (self->focused_window != 0)
  {
    xrdp_bitmap_set_focus(self->focused_window, 0);
  }
  self->focused_window = wnd;
  if (self->focused_window != 0)
  {
    xrdp_bitmap_set_focus(self->focused_window, 1);
  }
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
  struct xrdp_rect rect;
  int i;

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
      g_strcpy(help->title, "Logon help");
      /* ok button */
      but = xrdp_bitmap_create(60, 25, wnd->wm->screen->bpp, 3);
      xrdp_list_insert_item(help->child_list, 0, (int)but);
      but->parent = help;
      but->owner = help;
      but->wm = wnd->wm;
      but->left = 120;
      but->top = 260;
      but->id = 1;
      g_strcpy(but->title, "OK");
      /* draw it */
      xrdp_bitmap_invalidate(help, 0);
      xrdp_wm_set_focused(wnd->wm, help);
    }
    else if (sender->id == 2) /* cancel button */
    {
      /*if (wnd != 0)
        if (wnd->wm != 0)
          if (wnd->wm->pro_layer != 0)
            wnd->wm->pro_layer->term = 1;*/
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
      xrdp_wm_rect(&rect, b->left, b->top, b->width, b->height);
      xrdp_bitmap_invalidate(wnd->wm->screen, &rect);
      xrdp_bitmap_delete(sender);
      wnd->modal_dialog = 0;
      xrdp_wm_set_focused(wnd->wm, wnd);
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_set_cursor(struct xrdp_wm* self, int cache_idx)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  xrdp_rdp_init_data(self->rdp_layer, s);
  out_uint16_le(s, RDP_POINTER_CACHED);
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, cache_idx); /* cache_idx */
  s_mark_end(s);
  xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_POINTER);
  free_stream(s);
  return 0;
}

//******************************************************************************
int xrdp_wm_get_pixel(char* data, int x, int y, int width, int bpp)
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
      return (data[start] & 0xf0) >> 4;
    else
      return data[start] & 0x0f;
  }
  return 0;
}


/*****************************************************************************/
/* send a cursor from a file */
int xrdp_send_cursor(struct xrdp_wm* self, char* file_name, int cache_idx)
{
  int fd;
  int w;
  int h;
  int x;
  int y;
  int bpp;
  int i;
  int j;
  int rv;
  int pixel;
  int palette[16];
  struct stream* fs;
  struct stream* s;

  rv = 1;
  make_stream(s);
  init_stream(s, 8192);
  make_stream(fs);
  init_stream(fs, 8192);
  fd = g_file_open(file_name);
  g_file_read(fd, fs->data, 8192);
  g_file_close(fd);
  in_uint8s(fs, 6);
  in_uint8(fs, w);
  in_uint8(fs, h);
  in_uint8s(fs, 2);
  in_uint16_le(fs, x);
  in_uint16_le(fs, y);
  in_uint8s(fs, 22);
  in_uint8(fs, bpp);
  in_uint8s(fs, 25);
  if (w == 32 && h == 32)
  {
    xrdp_rdp_init_data(self->rdp_layer, s);
    out_uint16_le(s, RDP_POINTER_COLOR);
    out_uint16_le(s, 0); /* pad */
    out_uint16_le(s, cache_idx); /* cache_idx */
    out_uint16_le(s, x);
    out_uint16_le(s, y);
    out_uint16_le(s, w);
    out_uint16_le(s, h);
    out_uint16_le(s, 128);
    out_uint16_le(s, 3072);
    if (bpp == 1)
    {
      in_uint8a(fs, palette, 8);
      for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 32; j++)
        {
          pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 1)];
          out_uint8(s, pixel);
          out_uint8(s, pixel >> 8);
          out_uint8(s, pixel >> 16);
        }
      }
      in_uint8s(fs, 128);
    }
    else if (bpp == 4)
    {
      in_uint8a(fs, palette, 64);
      for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 32; j++)
        {
          pixel = palette[xrdp_wm_get_pixel(fs->p, j, i, 32, 4)];
          out_uint8(s, pixel);
          out_uint8(s, pixel >> 8);
          out_uint8(s, pixel >> 16);
        }
      }
      in_uint8s(fs, 512);
    }
    out_uint8a(s, fs->p, 128); /* mask */
    s_mark_end(s);
    xrdp_rdp_send_data(self->rdp_layer, s, RDP_DATA_PDU_POINTER);
    rv = 0;
  }
  free_stream(s);
  free_stream(fs);
  return rv;
}

/*****************************************************************************/
int xrdp_wm_init(struct xrdp_wm* self)
{
  struct xrdp_bitmap* but;

  if (self->screen->bpp == 8)
  {
    /* rgb */
    self->palette[250] = 0x00ff0000;
    self->palette[251] = 0x0000ff00;
    self->palette[252] = 0x00c0c0c0;
    self->palette[253] = 0x00808080;
    self->palette[254] = 0x000000ff;
    self->palette[255] = 0x00ffffff;
    self->black = 0;
    self->grey = 252;
    self->dark_grey = 253;
    self->blue = 254;
    self->white = 255;
    self->red = 250;
    self->green = 251;
    xrdp_wm_send_palette(self);

  }
  else if (self->screen->bpp == 15)
  {
    self->black     = xrdp_wm_color15(0, 0, 0);
    self->grey      = xrdp_wm_color15(0xc0, 0xc0, 0xc0);
    self->dark_grey = xrdp_wm_color15(0x80, 0x80, 0x80);
    self->blue      = xrdp_wm_color15(0x00, 0x00, 0xff);
    self->white     = xrdp_wm_color15(0xff, 0xff, 0xff);
    self->red       = xrdp_wm_color15(0xff, 0x00, 0x00);
    self->green     = xrdp_wm_color15(0x00, 0xff, 0x00);
  }
  else if (self->screen->bpp == 16)
  {
    self->black     = xrdp_wm_color16(0, 0, 0);
    self->grey      = xrdp_wm_color16(0xc0, 0xc0, 0xc0);
    self->dark_grey = xrdp_wm_color16(0x80, 0x80, 0x80);
    self->blue      = xrdp_wm_color16(0x00, 0x00, 0xff);
    self->white     = xrdp_wm_color16(0xff, 0xff, 0xff);
    self->red       = xrdp_wm_color16(0xff, 0x00, 0x00);
    self->green     = xrdp_wm_color16(0x00, 0xff, 0x00);
  }
  else if (self->screen->bpp == 24)
  {
    self->black     = xrdp_wm_color24(0, 0, 0);
    self->grey      = xrdp_wm_color24(0xc0, 0xc0, 0xc0);
    self->dark_grey = xrdp_wm_color24(0x80, 0x80, 0x80);
    self->blue      = xrdp_wm_color24(0x00, 0x00, 0xff);
    self->white     = xrdp_wm_color24(0xff, 0xff, 0xff);
    self->red       = xrdp_wm_color24(0xff, 0x00, 0x00);
    self->green     = xrdp_wm_color24(0x00, 0xff, 0x00);
  }
  /* draw login window */
  self->login_window = xrdp_bitmap_create(400, 200, self->screen->bpp, 1);
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
  strcpy(self->login_window->title, "Logon to xrdp");

  /* image */
  but = xrdp_bitmap_create(4, 4, self->screen->bpp, 4);
  xrdp_bitmap_load(but, "xrdp256.bmp", self->palette);
  but->parent = self->screen;
  but->owner = self->screen;
  but->wm = self;
  but->left = self->screen->width - but->width;
  but->top = self->screen->height - but->height;
  xrdp_list_add_item(self->screen->child_list, (int)but);

  /* image */
  but = xrdp_bitmap_create(4, 4, self->screen->bpp, 4);
  xrdp_bitmap_load(but, "ad256.bmp", self->palette);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 10;
  but->top = 30;
  xrdp_list_add_item(self->login_window->child_list, (int)but);

  but = xrdp_bitmap_create(60, 25, self->screen->bpp, 3);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 320;
  but->top = 160;
  but->id = 1;
  g_strcpy(but->title, "Help");

  but = xrdp_bitmap_create(60, 25, self->screen->bpp, 3);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 250;
  but->top = 160;
  but->id = 2;
  g_strcpy(but->title, "Cancel");

  but = xrdp_bitmap_create(60, 25, self->screen->bpp, 3);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 180;
  but->top = 160;
  but->id = 3;
  g_strcpy(but->title, "OK");

  but = xrdp_bitmap_create(50, 20, self->screen->bpp, 6); /* label */
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 155;
  but->top = 50;
  g_strcpy(but->title, "Username");

  but = xrdp_bitmap_create(140, 20, self->screen->bpp, 5);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 220;
  but->top = 50;
  but->id = 4;
  but->cursor = 1;

  but = xrdp_bitmap_create(50, 20, self->screen->bpp, 6); /* label */
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 155;
  but->top = 80;
  g_strcpy(but->title, "Password");

  but = xrdp_bitmap_create(140, 20, self->screen->bpp, 5);
  xrdp_list_add_item(self->login_window->child_list, (int)but);
  but->parent = self->login_window;
  but->owner = self->login_window;
  but->wm = self;
  but->left = 220;
  but->top = 80;
  but->id = 5;
  but->cursor = 1;

  /* clear screen */
  self->screen->bg_color = self->black;
  xrdp_bitmap_invalidate(self->screen, 0);

  xrdp_wm_set_focused(self, self->login_window);

  xrdp_send_cursor(self, "cursor1.cur", 1);
  xrdp_send_cursor(self, "cursor0.cur", 0);

  return 0;
}

/*****************************************************************************/
/* returns the number for rects visible for an area relative to a drawable */
/* putting the rects in region */
int xrdp_wm_get_vis_region(struct xrdp_wm* self, struct xrdp_bitmap* bitmap,
                           int x, int y, int cx, int cy,
                           struct xrdp_region* region)
{
  int i;
  struct xrdp_bitmap* p;
  struct xrdp_rect a;
  struct xrdp_rect b;

  /* area we are drawing */
  xrdp_wm_rect(&a, bitmap->left + x, bitmap->top + y, cx, cy);

  p = bitmap->parent;
  while (p != 0)
  {
    xrdp_wm_rect_offset(&a, p->left, p->top);
    p = p->parent;
  }

  xrdp_region_add_rect(region, &a);

  if (bitmap == self->screen)
    return 0;

  /* loop through all windows in z order */
  for (i = 0; i < self->screen->child_list->count; i++)
  {
    p = (struct xrdp_bitmap*)xrdp_list_get_item(self->screen->child_list, i);
    if (p == bitmap || p == bitmap->parent)
      return 0;
    xrdp_wm_rect(&b, p->left, p->top, p->width, p->height);
    xrdp_region_subtract_rect(region, &b);
  }

  return 0;
}

/*****************************************************************************/
/* return the window at x, y on the screen */
struct xrdp_bitmap* xrdp_wm_at_pos(struct xrdp_bitmap* wnd, int x, int y,
                                   struct xrdp_bitmap** wnd1)
{
  int i;
  struct xrdp_bitmap* p;
  struct xrdp_bitmap* q;

  /* loop through all windows in z order */
  for (i = 0; i < wnd->child_list->count; i++)
  {
    p = (struct xrdp_bitmap*)xrdp_list_get_item(wnd->child_list, i);
    if (x >= p->left && y >= p->top && x < p->left + p->width &&
        y < p->top + p->height)
    {
      if (wnd1 != 0)
        *wnd1 = p;
      q = xrdp_wm_at_pos(p, x - p->left, y - p->top, 0);
      if (q == 0)
        return p;
      else
        return q;
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_wm_xor_pat(struct xrdp_wm* self, int x, int y, int cx, int cy)
{
  self->painter->rop = 0x5a;
  xrdp_painter_begin_update(self->painter);
  self->painter->use_clip = 0;
  self->painter->brush.pattern[0] = 0xaa;
  self->painter->brush.pattern[1] = 0x55;
  self->painter->brush.pattern[2] = 0xaa;
  self->painter->brush.pattern[3] = 0x55;
  self->painter->brush.pattern[4] = 0xaa;
  self->painter->brush.pattern[5] = 0x55;
  self->painter->brush.pattern[6] = 0xaa;
  self->painter->brush.pattern[7] = 0x55;
  self->painter->brush.x_orgin = 0;
  self->painter->brush.x_orgin = 0;
  self->painter->brush.style = 3;
  self->painter->bg_color = self->black;
  self->painter->fg_color = self->white;
  /* top */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y, cx, 5);
  /* bottom */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y + (cy - 5), cx, 5);
  /* left */
  xrdp_painter_fill_rect2(self->painter, self->screen, x, y + 5, 5, cy - 10);
  /* right */
  xrdp_painter_fill_rect2(self->painter, self->screen, x + (cx - 5), y + 5, 5,
                          cy - 10);
  xrdp_painter_end_update(self->painter);
  self->painter->rop = 0xcc;
  return 0;
}

/*****************************************************************************/
/* this don't are about nothing, just copy the bits */
/* no clipping rects, no windows in the way, nothing */
int xrdp_wm_bitblt(struct xrdp_wm* self,
                   struct xrdp_bitmap* dst, int dx, int dy,
                   struct xrdp_bitmap* src, int sx, int sy,
                   int sw, int sh, int rop)
{
//  int i;
//  int line_size;
//  int Bpp;
//  char* s;
//  char* d;

//  if (sw <= 0 || sh <= 0)
//    return 0;
  if (self->screen == dst && self->screen == src)
  { /* send a screen blt */
//    Bpp = (dst->bpp + 7) / 8;
//    line_size = sw * Bpp;
//    s = src->data + (sy * src->width + sx) * Bpp;
//    d = dst->data + (dy * dst->width + dx) * Bpp;
//    for (i = 0; i < sh; i++)
//    {
//      //g_memcpy(d, s, line_size);
//      s += src->width * Bpp;
//      d += dst->width * Bpp;
//    }
    xrdp_orders_init(self->orders);
    xrdp_orders_screen_blt(self->orders, dx, dy, sw, sh, sx, sy, rop, 0);
    xrdp_orders_send(self->orders);
  }
  return 0;
}

/*****************************************************************************/
/* return true is rect is totaly exposed going in reverse z order */
/* from wnd up */
int xrdp_wm_is_rect_vis(struct xrdp_wm* self, struct xrdp_bitmap* wnd,
                        struct xrdp_rect* rect)
{
  struct xrdp_rect rect1;
  struct xrdp_rect rect2;
  struct xrdp_bitmap* b;
  int i;;

  /* if rect is part off screen */
  if (rect->left < 0)
    return 0;
  if (rect->top < 0)
    return 0;
  if (rect->right >= self->screen->width)
    return 0;
  if (rect->bottom >= self->screen->height)
    return 0;

  i = xrdp_list_index_of(self->screen->child_list, (int)wnd);
  i--;
  while (i >= 0)
  {
    b = (struct xrdp_bitmap*)xrdp_list_get_item(self->screen->child_list, i);
    xrdp_wm_rect(&rect1, b->left, b->top, b->width, b->height);
    if (xrdp_wm_rect_intersect(rect, &rect1, &rect2))
      return 0;
    i--;
  }
  return 1;
}

/*****************************************************************************/
int xrdp_wm_move_window(struct xrdp_wm* self, struct xrdp_bitmap* wnd,
                        int dx, int dy)
{
  struct xrdp_rect rect1;
  struct xrdp_rect rect2;
  struct xrdp_region* r;
  int i;

  xrdp_wm_rect(&rect1, wnd->left, wnd->top, wnd->width, wnd->height);
  if (xrdp_wm_is_rect_vis(self, wnd, &rect1))
  {
    rect2 = rect1;
    xrdp_wm_rect_offset(&rect2, dx, dy);
    if (xrdp_wm_is_rect_vis(self, wnd, &rect2))
    { /* if both src and dst are unobscured, we can do a bitblt move */
      xrdp_wm_bitblt(self, self->screen, wnd->left + dx, wnd->top + dy,
                     self->screen, wnd->left, wnd->top,
                     wnd->width, wnd->height, 0xcc);
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
      return 0;
    }
  }
  wnd->left += dx;
  wnd->top += dy;
  xrdp_bitmap_invalidate(self->screen, &rect1);
  xrdp_bitmap_invalidate(wnd, 0);
  return 0;
}

/*****************************************************************************/
int xrdp_wm_rect(struct xrdp_rect* r, int x, int y, int cx, int cy)
{
  r->left = x;
  r->top = y;
  r->right = x + cx;
  r->bottom = y + cy;
  return 0;
}

/*****************************************************************************/
int xrdp_wm_rect_is_empty(struct xrdp_rect* in)
{
  return (in->right <= in->left) || (in->bottom <= in->top);
}

/*****************************************************************************/
int xrdp_wm_rect_contains_pt(struct xrdp_rect* in, int x, int y)
{
  if (x < in->left)
    return 0;
  if (y < in->top)
    return 0;
  if (x >= in->right)
    return 0;
  if (y >= in->bottom)
    return 0;
  return 1;
}

/*****************************************************************************/
int xrdp_wm_rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2,
                           struct xrdp_rect* out)
{
  int rv;

  *out = *in1;
  if (in2->left > in1->left)
    out->left = in2->left;
  if (in2->top > in1->top)
    out->top = in2->top;
  if (in2->right < in1->right)
    out->right = in2->right;
  if (in2->bottom < in1->bottom)
    out->bottom = in2->bottom;
  rv = !xrdp_wm_rect_is_empty(out);
  if (!rv)
    g_memset(out, 0, sizeof(struct xrdp_rect));
  return rv;
}

/*****************************************************************************/
int xrdp_wm_rect_offset(struct xrdp_rect* in, int dx, int dy)
{
  in->left += dx;
  in->right += dx;
  in->top += dy;
  in->bottom += dy;
  return 0;
}

/*****************************************************************************/
int xrdp_wm_mouse_move(struct xrdp_wm* self, int x, int y)
{
  struct xrdp_bitmap* b;
  int boxx;
  int boxy;

  DEBUG(("in mouse move\n\r"));
  if (self == 0)
    return 0;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= self->screen->width)
    x = self->screen->width;
  if (y >= self->screen->height)
    y = self->screen->height;
  if (self->dragging)
  {
    xrdp_painter_begin_update(self->painter);
    boxx = self->draggingx - self->draggingdx;
    boxy = self->draggingy - self->draggingdy;
    if (self->draggingxorstate)
      xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
    self->draggingx = x;
    self->draggingy = y;
    boxx = self->draggingx - self->draggingdx;
    boxy = self->draggingy - self->draggingdy;
    xrdp_wm_xor_pat(self, boxx, boxy, self->draggingcx, self->draggingcy);
    self->draggingxorstate = 1;
    xrdp_painter_end_update(self->painter);
  }
  b = xrdp_wm_at_pos(self->screen, x, y, 0);
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
      if (b->cursor != self->current_cursor)
      {
        xrdp_set_cursor(self, b->cursor);
        self->current_cursor = b->cursor;
      }
      if (self->button_down == 0)
        if (b->notify != 0)
          b->notify(b->owner, b, 2, x, y);
    }
  }
  DEBUG(("out mouse move\n\r"));
  return 0;
}

/*****************************************************************************/
int xrdp_wm_mouse_click(struct xrdp_wm* self, int x, int y, int but, int down)
{
  struct xrdp_bitmap* b;
  struct xrdp_bitmap* b1;
  int newx;
  int newy;
  int oldx;
  int oldy;

  if (self == 0)
    return 0;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= self->screen->width)
    x = self->screen->width;
  if (y >= self->screen->height)
    y = self->screen->height;
  if (self->dragging && but == 1 && !down && self->dragging_window != 0)
  { /* if done dragging */
    self->draggingx = x;
    self->draggingy = y;
    newx = self->draggingx - self->draggingdx;
    newy = self->draggingy - self->draggingdy;
    oldx = self->dragging_window->left;
    oldy = self->dragging_window->top;
    /* draw xor box one more time */
    if (self->draggingxorstate)
      xrdp_wm_xor_pat(self, newx, newy, self->draggingcx, self->draggingcy);
    self->draggingxorstate = 0;
    /* move screen to new location */
    xrdp_wm_move_window(self, self->dragging_window, newx - oldx, newy - oldy);
    self->dragging_window = 0;
    self->dragging = 0;
  }
  b = xrdp_wm_at_pos(self->screen, x, y, &b1);
  if (b != 0)
  {
    if (b->type == 3 && but == 1 && !down && self->button_down == b)
    { /* if clicking up on a button that was clicked down */
      self->button_down = 0;
      b->state = 0;
      xrdp_bitmap_invalidate(b, 0);
      if (b->parent != 0)
        if (b->parent->notify != 0)
          /* b can be invalid after this */
          b->parent->notify(b->owner, b, 1, x, y);
    }
    else if (b->type == 3 && but == 1 && down)
    { /* if clicking down on a button */
      self->button_down = b;
      b->state = 1;
      xrdp_bitmap_invalidate(b, 0);
    }
    else if (but == 1 && down)
    {
      xrdp_wm_set_focused(self, b1);
      if (b->type == 1 && y < (b->top + 21))
      { /* if dragging */
        if (self->dragging) /* rarely happens */
        {
          newx = self->draggingx - self->draggingdx;
          newy = self->draggingy - self->draggingdy;
          if (self->draggingxorstate)
            xrdp_wm_xor_pat(self, newx, newy,
                            self->draggingcx, self->draggingcy);
          self->draggingxorstate = 0;
        }
        self->dragging = 1;
        self->dragging_window = b;
        self->draggingorgx = b->left;
        self->draggingorgy = b->top;
        self->draggingx = x;
        self->draggingy = y;
        self->draggingdx = x - b->left;
        self->draggingdy = y - b->top;
        self->draggingcx = b->width;
        self->draggingcy = b->height;
      }
    }
  }
  else
    xrdp_wm_set_focused(self, 0);

  /* no matter what, mouse is up, reset button_down */
  if (but == 1 && !down && self->button_down != 0)
    self->button_down = 0;
  return 0;
}
