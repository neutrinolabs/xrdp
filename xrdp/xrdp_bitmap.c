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

   bitmap, drawable

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_bitmap* xrdp_bitmap_create(int width, int height, int bpp,
                                       int type)
{
  struct xrdp_bitmap* self;
  int Bpp;

  self = (struct xrdp_bitmap*)g_malloc(sizeof(struct xrdp_bitmap), 1);
  self->type = type;
  self->width = width;
  self->height = height;
  self->bpp = bpp;
  Bpp = 4;
  switch (bpp)
  {
    case 8: Bpp = 1; break;
    case 15: Bpp = 2; break;
    case 16: Bpp = 2; break;
  }
  self->data = (char*)g_malloc(width * height * Bpp, 1);
  self->child_list = xrdp_list_create();
  self->line_size = ((width + 3) & ~3) * Bpp;
  return self;
}

/*****************************************************************************/
void xrdp_bitmap_delete(struct xrdp_bitmap* self)
{
  int i;

  if (self == 0)
    return;
  if (self->wm != 0)
  {
    if (self->wm->focused_window == self)
      self->wm->focused_window = 0;
    if (self->wm->dragging_window == self)
      self->wm->dragging_window = 0;
    if (self->wm->button_down == self)
      self->wm->button_down = 0;
  }
  for (i = 0; i < self->child_list->count; i++)
    xrdp_bitmap_delete((struct xrdp_bitmap*)self->child_list->items[i]);
  xrdp_list_delete(self->child_list);
  g_free(self->data);
  g_free(self);
}

/*****************************************************************************/
/* if focused is true focus this window else unfocus it */
/* returns error */
int xrdp_bitmap_set_focus(struct xrdp_bitmap* self, int focused)
{
  struct xrdp_painter* painter;

  if (self == 0)
    return 0;
  if (self->type != WND_TYPE_WND) /* 1 */
    return 0;
  self->focused = focused;
  painter = xrdp_painter_create(self->wm);
  xrdp_painter_begin_update(painter);
  if (focused)
  {
    /* active title bar */
    painter->fg_color = self->wm->blue;
    xrdp_painter_fill_rect(painter, self, 3, 3, self->width - 5, 18);
    painter->font->color = self->wm->white;
  }
  else
  {
    /* inactive title bar */
    painter->fg_color = self->wm->dark_grey;
    xrdp_painter_fill_rect(painter, self, 3, 3, self->width - 5, 18);
    painter->font->color = self->wm->black;
  }
  xrdp_painter_draw_text(painter, self, 4, 4, self->title);
  xrdp_painter_end_update(painter);
  xrdp_painter_delete(painter);
  return 0;
}

/*****************************************************************************/
int xrdp_bitmap_get_index(struct xrdp_bitmap* self, int* palette, int color)
{
  int i;

  for (i = 0; i < 256; i++)
  {
    if (color == palette[i])
      return i;
  }
  for (i = 1; i < 256; i++)
  {
    if (palette[i] == 0)
    {
      palette[i] = color;
      return i;
    }
  }
  g_printf("color %8.8x not found\n", color);
  return 255;
}

/*****************************************************************************/
int xrdp_bitmap_resize(struct xrdp_bitmap* self, int width, int height)
{
  int Bpp;

  g_free(self->data);
  self->width = width;
  self->height = height;
  Bpp = 4;
  switch (self->bpp)
  {
    case 8: Bpp = 1; break;
    case 15: Bpp = 2; break;
    case 16: Bpp = 2; break;
  }
  self->data = (char*)g_malloc(width * height * Bpp, 1);
  self->line_size = ((width + 3) & ~3) * Bpp;
  return 0;
}

/*****************************************************************************/
/* load a bmp file */
/* return 0 ok */
/* return 1 error */
int xrdp_bitmap_load(struct xrdp_bitmap* self, char* filename, int* palette)
{
  int fd;
  int i;
  int j;
  int k;
  int color;
  int size;
  int palette1[256];
  char type1[3];
  char* data;
  struct xrdp_bmp_header header;

  fd = g_file_open(filename);
  if (fd != -1)
  {
    /* read file type */
    if (g_file_read(fd, type1, 2) != 2)
    {
      g_file_close(fd);
      return 1;
    }
    if (type1[0] != 'B' || type1[1] != 'M')
    {
      g_file_close(fd);
      return 1;
    }
    /* read file size */
    size = 0;
    g_file_read(fd, (char*)&size, 4);
    /* read bmp header */
    g_file_seek(fd, 14);
    g_file_read(fd, (char*)&header, sizeof(header));
    if (header.bit_count != 8 && header.bit_count != 24)
    {
      g_file_close(fd);
      return 1;
    }
    if (header.bit_count == 24) /* 24 bit bitmap */
    {
      g_file_seek(fd, 14 + header.size);
    }
    if (header.bit_count == 8) /* 8 bit bitmap */
    {
      /* read palette */
      g_file_seek(fd, 14 + header.size);
      g_file_read(fd, (char*)palette1, 256 * sizeof(int));
      /* read data */
      xrdp_bitmap_resize(self, header.image_width, header.image_height);
      data = (char*)g_malloc(header.image_width * header.image_height, 1);
      for (i = header.image_height - 1; i >= 0; i--)
        g_file_read(fd, data + i * header.image_width, header.image_width);
      for (i = 0; i < self->height; i++)
      {
        for (j = 0; j < self->width; j++)
        {
          k = (unsigned char)data[i * header.image_width + j];
          color = palette1[k];
          if (self->bpp == 8)
            color = xrdp_bitmap_get_index(self, palette, color);
          else if (self->bpp == 15)
            color = COLOR15((color & 0xff0000) >> 16,
                            (color & 0x00ff00) >> 8,
                            (color & 0x0000ff) >> 0);
          else if (self->bpp == 16)
            color = COLOR16((color & 0xff0000) >> 16,
                            (color & 0x00ff00) >> 8,
                            (color & 0x0000ff) >> 0);
          else if (self->bpp == 24)
            color = COLOR24((color & 0xff0000) >> 16,
                            (color & 0x00ff00) >> 8,
                            (color & 0x0000ff) >> 0);
          xrdp_bitmap_set_pixel(self, j, i, color);
        }
      }
      g_free(data);
    }
    g_file_close(fd);
  }
  return 0;
}

/*****************************************************************************/
int xrdp_bitmap_get_pixel(struct xrdp_bitmap* self, int x, int y)
{
  if (self == 0)
    return 0;
  if (self->data == 0)
    return 0;
  if (x >= 0 && x < self->width && y >= 0 && y < self->height)
  {
    if (self->bpp == 8)
      return GETPIXEL8(self->data, x, y, self->width);
    else if (self->bpp == 15 || self->bpp == 16)
      return GETPIXEL16(self->data, x, y, self->width);
    else if (self->bpp == 24)
      return GETPIXEL32(self->data, x, y, self->width);
  }
  return 0;
}

/*****************************************************************************/
int xrdp_bitmap_set_pixel(struct xrdp_bitmap* self, int x, int y, int pixel)
{
  if (self == 0)
    return 0;
  if (self->data == 0)
    return 0;
  if (x >= 0 && x < self->width && y >= 0 && y < self->height)
  {
    if (self->bpp == 8)
      SETPIXEL8(self->data, x, y, self->width, pixel);
    else if (self->bpp == 15 || self->bpp == 16)
      SETPIXEL16(self->data, x, y, self->width, pixel);
    else if (self->bpp == 24)
      SETPIXEL32(self->data, x, y, self->width, pixel);
  }
  return 0;
}

/*****************************************************************************/
/* copy part of self at x, y to 0, o in dest */
/* returns error */
int xrdp_bitmap_copy_box(struct xrdp_bitmap* self, struct xrdp_bitmap* dest,
                         int x, int y, int cx, int cy)
{
  int i;
  int j;
  int destx;
  int desty;

  if (self == 0)
    return 1;
  if (dest == 0)
    return 1;
  if (self->type != WND_TYPE_BITMAP && self->type != WND_TYPE_IMAGE)
    return 1;
  if (dest->type != WND_TYPE_BITMAP && dest->type != WND_TYPE_IMAGE)
    return 1;
  if (self->bpp != dest->bpp)
    return 1;
  destx = 0;
  desty = 0;
  if (!check_bounds(self, &x, &y, &cx, &cy))
    return 1;
  if (!check_bounds(dest, &destx, &desty, &cx, &cy))
    return 1;
  if (self->bpp == 24)
  {
    for (i = 0; i < cy; i++)
      for (j = 0; j < cx; j++)
        SETPIXEL32(dest->data, j + destx, i + desty, dest->width,
                   GETPIXEL32(self->data, j + x, i + y, self->width));
  }
  else if (self->bpp == 15 || self->bpp == 16)
  {
    for (i = 0; i < cy; i++)
      for (j = 0; j < cx; j++)
        SETPIXEL16(dest->data, j + destx, i + desty, dest->width,
                   GETPIXEL16(self->data, j + x, i + y, self->width));
  }
  else if (self->bpp == 8)
  {
    for (i = 0; i < cy; i++)
      for (j = 0; j < cx; j++)
        SETPIXEL8(dest->data, j + destx, i + desty, dest->width,
                  GETPIXEL8(self->data, j + x, i + y, self->width));
  }
  else
    return 1;
  return 0;
}

/*****************************************************************************/
/* returns true if they are the same, else returns false */
int xrdp_bitmap_compare(struct xrdp_bitmap* self, struct xrdp_bitmap* b)
{
  if (self == 0)
    return 0;
  if (b == 0)
    return 0;
  if (self->bpp != b->bpp)
    return 0;
  if (self->width != b->width)
    return 0;
  if (self->height != b->height)
    return 0;
  if (g_memcmp(self->data, b->data, b->height * b->line_size) == 0)
    return 1;
  return 0;
}

/*****************************************************************************/
/* nil for rect means the whole thing */
/* returns error */
int xrdp_bitmap_invalidate(struct xrdp_bitmap* self, struct xrdp_rect* rect)
{
  int i;
  int w;
  int h;
  struct xrdp_bitmap* b;
  struct xrdp_rect r1;
  struct xrdp_rect r2;
  struct xrdp_painter* painter;

  if (self == 0) /* if no bitmap */
    return 0;
  if (self->type == WND_TYPE_BITMAP) /* if 0, bitmap, leave */
    return 0;
  painter = xrdp_painter_create(self->wm);
  painter->rop = 0xcc; /* copy */
  if (rect == 0)
    painter->use_clip = 0;
  else
  {
    if (ISRECTEMPTY(*rect))
    {
      xrdp_painter_delete(painter);
      return 0;
    }
    painter->clip = *rect;
    painter->use_clip = 1;
  }
  xrdp_painter_begin_update(painter);
  if (self->type == WND_TYPE_WND) /* 1 */
  {
    /* draw grey background */
    painter->fg_color = self->bg_color;
    xrdp_painter_fill_rect(painter, self, 0, 0, self->width, self->height);
    /* top white line */
    painter->fg_color = self->wm->white;
    xrdp_painter_fill_rect(painter, self, 1, 1, self->width - 2, 1);
    /* left white line */
    painter->fg_color = self->wm->white;
    xrdp_painter_fill_rect(painter, self, 1, 1, 1, self->height - 2);
    /* bottom dark grey line */
    painter->fg_color = self->wm->dark_grey;
    xrdp_painter_fill_rect(painter, self, 1, self->height - 2,
                           self->width - 2, 1);
    /* right dark grey line */
    painter->fg_color = self->wm->dark_grey;
    xrdp_painter_fill_rect(painter, self, self->width - 2, 1, 1,
                           self->height - 2);
    /* bottom black line */
    painter->fg_color = self->wm->black;
    xrdp_painter_fill_rect(painter, self, 0, self->height - 1,
                           self->width, 1);
    /* right black line */
    painter->fg_color = self->wm->black;
    xrdp_painter_fill_rect(painter, self, self->width - 1, 0,
                           1, self->height);
    if (self->focused)
    {
      /* active title bar */
      painter->fg_color = self->wm->blue;
      xrdp_painter_fill_rect(painter, self, 3, 3, self->width - 5, 18);
      painter->font->color = self->wm->white;
    }
    else
    {
      /* inactive title bar */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, 3, 3, self->width - 5, 18);
      painter->font->color = self->wm->black;
    }
    xrdp_painter_draw_text(painter, self, 4, 4, self->title);
  }
  else if (self->type == WND_TYPE_SCREEN) /* 2 */
  {
    painter->fg_color = self->bg_color;
    xrdp_painter_fill_rect(painter, self, 0, 0, self->width, self->height);
  }
  else if (self->type == WND_TYPE_BUTTON) /* 3 */
  {
    if (self->state == BUTTON_STATE_UP) /* 0 */
    {
      /* gray box */
      painter->fg_color = self->wm->grey;
      xrdp_painter_fill_rect(painter, self, 0, 0, self->width, self->height);
      /* white top line */
      painter->fg_color = self->wm->white;
      xrdp_painter_fill_rect(painter, self, 0, 0, self->width, 1);
      /* white left line */
      painter->fg_color = self->wm->white;
      xrdp_painter_fill_rect(painter, self, 0, 0, 1, self->height);
      /* dark grey bottom line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, 1, self->height - 2,
                             self->width - 1, 1);
      /* dark grey right line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, self->width - 2, 1,
                             1, self->height - 1);
      /* black bottom line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, 0, self->height - 1, self->width, 1);
      /* black right line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, self->width - 1, 0, 1, self->height);
      w = xrdp_painter_text_width(painter, self->title);
      h = xrdp_painter_text_height(painter, self->title);
      painter->font->color = self->wm->black;
      xrdp_painter_draw_text(painter, self, self->width / 2 - w / 2,
                             self->height / 2 - h / 2, self->title);
    }
    else if (self->state == BUTTON_STATE_DOWN) /* 1 */
    {
      /* gray box */
      painter->fg_color = self->wm->grey;
      xrdp_painter_fill_rect(painter, self, 0, 0, self->width, self->height);
      /* black top line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, 0, 0, self->width, 1);
      /* black left line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, 0, 0, 1, self->height);
      /* dark grey top line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, 1, 1, self->width - 2, 1);
      /* dark grey left line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, 1, 1, 1, self->height - 2);
      /* dark grey bottom line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, 1, self->height - 2,
                             self->width - 1, 1);
      /* dark grey right line */
      painter->fg_color = self->wm->dark_grey;
      xrdp_painter_fill_rect(painter, self, self->width - 2, 1,
                             1, self->height - 1);
      /* black bottom line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, 0, self->height - 1, self->width, 1);
      /* black right line */
      painter->fg_color = self->wm->black;
      xrdp_painter_fill_rect(painter, self, self->width - 1, 0, 1, self->height);
      w = xrdp_painter_text_width(painter, self->title);
      h = xrdp_painter_text_height(painter, self->title);
      painter->font->color = self->wm->black;
      xrdp_painter_draw_text(painter, self, (self->width / 2 - w / 2) + 1,
                             (self->height / 2 - h / 2) + 1, self->title);
    }
  }
  else if (self->type == WND_TYPE_IMAGE) /* 4 */
  {
    xrdp_painter_draw_bitmap(painter, self, self, 0, 0, self->width,
                             self->height);
  }
  else if (self->type == WND_TYPE_EDIT) /* 5 */
  {
    /* draw gray box */
    painter->fg_color = self->wm->grey;
    xrdp_painter_fill_rect(painter, self, 0, 0, self->width, self->height);
    /* main white background */
    painter->fg_color = self->wm->white;
    xrdp_painter_fill_rect(painter, self, 1, 1, self->width - 3,
                           self->height - 3);
    /* dark grey top line */
    painter->fg_color = self->wm->dark_grey;
    xrdp_painter_fill_rect(painter, self, 0, 0, self->width, 1);
    /* dark grey left line */
    painter->fg_color = self->wm->dark_grey;
    xrdp_painter_fill_rect(painter, self, 0, 0, 1, self->height);
    /* white bottom line */
    painter->fg_color = self->wm->white;
    xrdp_painter_fill_rect(painter, self, 0, self->height- 1, self->width, 1);
    /* white right line */
    painter->fg_color = self->wm->white;
    xrdp_painter_fill_rect(painter, self, self->width - 1, 0, 1, self->height);
    /* black left line */
    painter->fg_color = self->wm->black;
    xrdp_painter_fill_rect(painter, self, 1, 1, 1, self->height - 2);
    /* black top line */
    painter->fg_color = self->wm->black;
    xrdp_painter_fill_rect(painter, self, 1, 1, self->width - 2, 1);
  }
  else if (self->type == WND_TYPE_LABEL) /* 6 */
  {
    painter->font->color = self->wm->black;
    xrdp_painter_draw_text(painter, self, 0, 0, self->title);
  }
  /* notify */
  if (self->notify != 0)
    self->notify(self, self, 3, (int)painter, 0);
  /* draw any child windows in the area */
  for (i = 0; i < self->child_list->count; i++)
  {
    b = (struct xrdp_bitmap*)xrdp_list_get_item(self->child_list, i);
    if (rect == 0)
      xrdp_bitmap_invalidate(b, 0);
    else
    {
      MAKERECT(r1, b->left, b->top, b->width, b->height);
      if (rect_intersect(rect, &r1, &r2))
      {
        RECTOFFSET(r2, -(b->left), -(b->top));
        xrdp_bitmap_invalidate(b, &r2);
      }
    }
  }
  xrdp_painter_end_update(painter);
  xrdp_painter_delete(painter);
  return 0;
}
