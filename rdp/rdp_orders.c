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
   Copyright (C) Jay Sorg 2005-2007

   librdp orders

*/

#include "rdp.h"

/*****************************************************************************/
struct rdp_orders* APP_CC
rdp_orders_create(struct rdp_rdp* owner)
{
  struct rdp_orders* self;

  self = (struct rdp_orders*)g_malloc(sizeof(struct rdp_orders), 1);
  self->rdp_layer = owner;
  return self;
}

/*****************************************************************************/
void APP_CC
rdp_orders_delete(struct rdp_orders* self)
{
  int i;
  int j;

  if (self == 0)
  {
    return;
  }
  /* free the colormap cache */
  for (i = 0; i < 6; i++)
  {
    g_free(self->cache_colormap[i]);
  }
  /* free the bitmap cache */
  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 600; j++)
    {
      if (self->cache_bitmap[i][j] != 0)
      {
        g_free(self->cache_bitmap[i][j]->data);
      }
      g_free(self->cache_bitmap[i][j]);
    }
  }
  g_free(self);
}

/*****************************************************************************/
void APP_CC
rdp_orders_reset_state(struct rdp_orders* self)
{
  g_memset(&self->state, 0, sizeof(self->state));
}

/*****************************************************************************/
/* Read field indicating which parameters are present */
static void APP_CC
rdp_orders_in_present(struct stream* s, int* present,
                      int flags, int size)
{
  int bits;
  int i;

  if (flags & RDP_ORDER_SMALL)
  {
    size--;
  }
  if (flags & RDP_ORDER_TINY)
  {
    if (size < 2)
    {
      size = 0;
    }
    else
    {
      size -= 2;
    }
  }
  *present = 0;
  for (i = 0; i < size; i++)
  {
    in_uint8(s, bits);
    *present |= bits << (i * 8);
  }
}

/*****************************************************************************/
/* Read a co-ordinate (16-bit, or 8-bit delta) */
static void APP_CC
rdp_orders_in_coord(struct stream* s, int* coord, int delta)
{
  int change;

  if (delta)
  {
    in_sint8(s, change);
    *coord += change;
  }
  else
  {
    in_sint16_le(s, *coord);
  }
}

/*****************************************************************************/
/* Parse bounds information */
static void APP_CC
rdp_orders_parse_bounds(struct rdp_orders* self, struct stream* s)
{
  int present;

  in_uint8(s, present);
  if (present & 1)
  {
    rdp_orders_in_coord(s, &self->state.clip_left, 0);
  }
  else if (present & 16)
  {
    rdp_orders_in_coord(s, &self->state.clip_left, 1);
  }
  if (present & 2)
  {
    rdp_orders_in_coord(s, &self->state.clip_top, 0);
  }
  else if (present & 32)
  {
    rdp_orders_in_coord(s, &self->state.clip_top, 1);
  }
  if (present & 4)
  {
    rdp_orders_in_coord(s, &self->state.clip_right, 0);
  }
  else if (present & 64)
  {
    rdp_orders_in_coord(s, &self->state.clip_right, 1);
  }
  if (present & 8)
  {
    rdp_orders_in_coord(s, &self->state.clip_bottom, 0);
  }
  else if (present & 128)
  {
    rdp_orders_in_coord(s, &self->state.clip_bottom, 1);
  }
}

/*****************************************************************************/
/* Process a colormap cache order */
static void APP_CC
rdp_orders_process_colcache(struct rdp_orders* self, struct stream* s,
                            int flags)
{
  struct rdp_colormap* colormap;
  struct stream* rec_s;
  int cache_id;
  int i;

  colormap = (struct rdp_colormap*)g_malloc(sizeof(struct rdp_colormap), 1);
  in_uint8(s, cache_id);
  in_uint16_le(s, colormap->ncolors);
  for (i = 0; i < colormap->ncolors; i++)
  {
    in_uint32_le(s, colormap->colors[i]);
  }
  g_free(self->cache_colormap[cache_id]);
  self->cache_colormap[cache_id] = colormap;
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 4096);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 10);
    out_uint8(rec_s, cache_id);
    for (i = 0; i < 256; i++)
    {
      out_uint32_le(rec_s, colormap->colors[i]);
    }
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a raw bitmap cache order */
static void APP_CC
rdp_orders_process_raw_bmpcache(struct rdp_orders* self, struct stream* s,
                                int flags)
{
  int cache_idx;
  int bufsize;
  int cache_id;
  int width;
  int height;
  int bpp;
  int Bpp;
  int x;
  int y;
  char* inverted;
  char* dst;
  struct rdp_bitmap* bitmap;
  struct stream* rec_s;

  in_uint8(s, cache_id);
  in_uint8s(s, 1);
  in_uint8(s, width);
  in_uint8(s, height);
  in_uint8(s, bpp);
  Bpp = (bpp + 7) / 8;
  in_uint16_le(s, bufsize);
  in_uint16_le(s, cache_idx);
  inverted = (char*)g_malloc(width * height * Bpp, 0);
  for (y = 0; y < height; y++)
  {
    dst = inverted + (((height - y) - 1) * (width * Bpp));
    if (Bpp == 1)
    {
      for (x = 0; x < width; x++)
      {
        in_uint8(s, dst[x]);
      }
    }
    else if (Bpp == 2)
    {
      for (x = 0; x < width; x++)
      {
        in_uint16_le(s, ((unsigned short*)dst)[x]);
      }
    }
  }
  bitmap = (struct rdp_bitmap*)g_malloc(sizeof(struct rdp_bitmap), 0);
  bitmap->width = width;
  bitmap->height = height;
  bitmap->bpp = bpp;
  bitmap->data = inverted;
  if (self->cache_bitmap[cache_id][cache_idx] != 0)
  {
    g_free(self->cache_bitmap[cache_id][cache_idx]->data);
  }
  g_free(self->cache_bitmap[cache_id][cache_idx]);
  self->cache_bitmap[cache_id][cache_idx] = bitmap;
  if (self->rdp_layer->rec_mode)
  {
    y = width * height * Bpp;
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, y + 256);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 8);
    out_uint8(rec_s, cache_id);
    out_uint16_le(rec_s, cache_idx);
    out_uint16_le(rec_s, width);
    out_uint16_le(rec_s, height);
    out_uint16_le(rec_s, y);
    out_uint8a(rec_s, inverted, y);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a bitmap cache order */
static void APP_CC
rdp_orders_process_bmpcache(struct rdp_orders* self, struct stream* s,
                            int flags)
{
  char* data;
  char* bmpdata;
  int cache_idx;
  int size;
  int cache_id;
  int width;
  int height;
  int bpp;
  int Bpp;
  int bufsize;
  int pad1;
  int pad2;
  int row_size;
  int final_size;
  struct rdp_bitmap* bitmap;
  struct stream* rec_s;

  in_uint8(s, cache_id);
  in_uint8(s, pad1);
  in_uint8(s, width);
  in_uint8(s, height);
  in_uint8(s, bpp);
  Bpp = (bpp + 7) / 8;
  in_uint16_le(s, bufsize);
  in_uint16_le(s, cache_idx);
  if (flags & 1024)
  {
    size = bufsize;
  }
  else
  {
    in_uint16_le(s, pad2);
    in_uint16_le(s, size);
    in_uint16_le(s, row_size);
    in_uint16_le(s, final_size);
  }
  in_uint8p(s, data, size);
  bmpdata = (char*)g_malloc(width * height * Bpp, 0);
  if (rdp_bitmap_decompress(bmpdata, width, height, data, size, Bpp))
  {
  }
  else
  {
    /* error */
  }
  bitmap = (struct rdp_bitmap*)g_malloc(sizeof(struct rdp_bitmap), 0);
  bitmap->width = width;
  bitmap->height = height;
  bitmap->bpp = bpp;
  bitmap->data = bmpdata;
  if (self->cache_bitmap[cache_id][cache_idx] != 0)
  {
    g_free(self->cache_bitmap[cache_id][cache_idx]->data);
  }
  g_free(self->cache_bitmap[cache_id][cache_idx]);
  self->cache_bitmap[cache_id][cache_idx] = bitmap;
  if (self->rdp_layer->rec_mode)
  {
    size = width * height * Bpp;
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, size + 256);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 8);
    out_uint8(rec_s, cache_id);
    out_uint16_le(rec_s, cache_idx);
    out_uint16_le(rec_s, width);
    out_uint16_le(rec_s, height);
    out_uint16_le(rec_s, size);
    out_uint8a(rec_s, bmpdata, size);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a font cache order */
static void APP_CC
rdp_orders_process_fontcache(struct rdp_orders* self, struct stream* s,
                             int flags)
{
  struct stream* rec_s;
  int font;
  int nglyphs;
  int character;
  int offset;
  int baseline;
  int width;
  int height;
  int i;
  int datasize;
  char* data;

  in_uint8(s, font);
  in_uint8(s, nglyphs);
  for (i = 0; i < nglyphs; i++)
  {
    in_uint16_le(s, character);
    in_uint16_le(s, offset);
    in_uint16_le(s, baseline);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    datasize = (height * ((width + 7) / 8) + 3) & ~3;
    in_uint8p(s, data, datasize);
    self->rdp_layer->mod->server_add_char(self->rdp_layer->mod, font,
                                          character, offset, baseline,
                                          width, height, data);
    if (self->rdp_layer->rec_mode)
    {
      rdp_rec_check_file(self->rdp_layer);
      make_stream(rec_s);
      init_stream(rec_s, datasize + 256);
      s_push_layer(rec_s, iso_hdr, 4);
      out_uint8(rec_s, 9);
      out_uint8(rec_s, font);
      out_uint16_le(rec_s, character);
      out_uint16_le(rec_s, offset);
      out_uint16_le(rec_s, baseline);
      out_uint16_le(rec_s, width);
      out_uint16_le(rec_s, height);
      out_uint16_le(rec_s, datasize);
      out_uint8a(rec_s, data, datasize);
      rdp_rec_write_item(self->rdp_layer, rec_s);
      free_stream(rec_s);
    }
  }
}

/*****************************************************************************/
/* Process a secondary order */
static int APP_CC
rdp_orders_process_secondary_order(struct rdp_orders* self, struct stream* s)
{
  short length;
  int flags;
  int type;
  char* next_order;

  in_uint16_le(s, length);
  in_uint16_le(s, flags);
  in_uint8(s, type);
  next_order = s->p + length + 7;
  switch (type)
  {
    case RDP_ORDER_COLCACHE:
      rdp_orders_process_colcache(self, s, flags);
      break;
    case RDP_ORDER_RAW_BMPCACHE:
      rdp_orders_process_raw_bmpcache(self, s, flags);
      break;
    case RDP_ORDER_BMPCACHE:
      rdp_orders_process_bmpcache(self, s, flags);
      break;
    case RDP_ORDER_FONTCACHE:
      rdp_orders_process_fontcache(self, s, flags);
      break;
    default:
      /* error, unknown order */
      break;
  }
  s->p = next_order;
  return 0;
}

/*****************************************************************************/
/* Read a color entry */
static void APP_CC
rdp_orders_in_color(struct stream* s, int* color)
{
  int i;

  in_uint8(s, i);
  *color = i;
  in_uint8(s, i);
  *color |= i << 8;
  in_uint8(s, i);
  *color |= i << 16;
}

/*****************************************************************************/
/* Parse a brush */
static void APP_CC
rdp_orders_parse_brush(struct stream* s, struct rdp_brush* brush, int present)
{
  if (present & 1)
  {
    in_uint8(s, brush->xorigin);
  }
  if (present & 2)
  {
    in_uint8(s, brush->yorigin);
  }
  if (present & 4)
  {
    in_uint8(s, brush->style);
  }
  if (present & 8)
  {
    in_uint8(s, brush->pattern[0]);
  }
  if (present & 16)
  {
    in_uint8a(s, brush->pattern + 1, 7);
  }
}

/*****************************************************************************/
/* Parse a pen */
static void APP_CC
rdp_orders_parse_pen(struct stream* s, struct rdp_pen* pen, int present)
{
  if (present & 1)
  {
    in_uint8(s, pen->style);
  }
  if (present & 2)
  {
    in_uint8(s, pen->width);
  }
  if (present & 4)
  {
    rdp_orders_in_color(s, &pen->color);
  }
}

/*****************************************************************************/
/* Process a text order */
static void APP_CC
rdp_orders_process_text2(struct rdp_orders* self, struct stream* s,
                         int present, int delta)
{
  int fgcolor;
  int bgcolor;
  struct stream* rec_s;

  if (present & 0x000001)
  {
    in_uint8(s, self->state.text_font);
  }
  if (present & 0x000002)
  {
    in_uint8(s, self->state.text_flags);
  }
  if (present & 0x000004)
  {
    in_uint8(s, self->state.text_opcode);
  }
  if (present & 0x000008)
  {
    in_uint8(s, self->state.text_mixmode);
  }
  if (present & 0x000010)
  {
    rdp_orders_in_color(s, &self->state.text_fgcolor);
  }
  if (present & 0x000020)
  {
    rdp_orders_in_color(s, &self->state.text_bgcolor);
  }
  if (present & 0x000040)
  {
    in_sint16_le(s, self->state.text_clipleft);
  }
  if (present & 0x000080)
  {
    in_sint16_le(s, self->state.text_cliptop);
  }
  if (present & 0x000100)
  {
    in_sint16_le(s, self->state.text_clipright);
  }
  if (present & 0x000200)
  {
    in_sint16_le(s, self->state.text_clipbottom);
  }
  if (present & 0x000400)
  {
    in_sint16_le(s, self->state.text_boxleft);
  }
  if (present & 0x000800)
  {
    in_sint16_le(s, self->state.text_boxtop);
  }
  if (present & 0x001000)
  {
    in_sint16_le(s, self->state.text_boxright);
  }
  if (present & 0x002000)
  {
    in_sint16_le(s, self->state.text_boxbottom);
  }
  rdp_orders_parse_brush(s, &self->state.text_brush, present >> 14);
  if (present & 0x080000)
  {
    in_sint16_le(s, self->state.text_x);
  }
  if (present & 0x100000)
  {
    in_sint16_le(s, self->state.text_y);
  }
  if (present & 0x200000)
  {
    in_uint8(s, self->state.text_length);
    in_uint8a(s, self->state.text_text, self->state.text_length);
  }
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                          self->state.text_opcode);
  fgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.text_fgcolor,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_fgcolor(self->rdp_layer->mod, fgcolor);
  bgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.text_bgcolor,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_bgcolor(self->rdp_layer->mod, bgcolor);
  self->rdp_layer->mod->server_draw_text(self->rdp_layer->mod,
                                         self->state.text_font,
                                         self->state.text_flags,
                                         self->state.text_mixmode,
                                         self->state.text_clipleft,
                                         self->state.text_cliptop,
                                         self->state.text_clipright,
                                         self->state.text_clipbottom,
                                         self->state.text_boxleft,
                                         self->state.text_boxtop,
                                         self->state.text_boxright,
                                         self->state.text_boxbottom,
                                         self->state.text_x,
                                         self->state.text_y,
                                         self->state.text_text,
                                         self->state.text_length);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 7);
    out_uint8(rec_s, self->state.text_font);
    out_uint8(rec_s, self->state.text_flags);
    out_uint8(rec_s, self->state.text_opcode);
    out_uint8(rec_s, self->state.text_mixmode);
    out_uint32_le(rec_s, self->state.text_fgcolor);
    out_uint32_le(rec_s, self->state.text_bgcolor);
    out_uint16_le(rec_s, self->state.text_clipleft);
    out_uint16_le(rec_s, self->state.text_cliptop);
    out_uint16_le(rec_s, self->state.text_clipright);
    out_uint16_le(rec_s, self->state.text_clipbottom);
    out_uint16_le(rec_s, self->state.text_boxleft);
    out_uint16_le(rec_s, self->state.text_boxtop);
    out_uint16_le(rec_s, self->state.text_boxright);
    out_uint16_le(rec_s, self->state.text_boxbottom);
    out_uint16_le(rec_s, self->state.text_x);
    out_uint16_le(rec_s, self->state.text_y);
    out_uint16_le(rec_s, self->state.text_length);
    out_uint8a(rec_s, self->state.text_text, self->state.text_length);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a destination blt order */
static void APP_CC
rdp_orders_process_destblt(struct rdp_orders* self, struct stream* s,
                           int present, int delta)
{
  struct stream* rec_s;

  if (present & 0x01)
  {
    rdp_orders_in_coord(s, &self->state.dest_x, delta);
  }
  if (present & 0x02)
  {
    rdp_orders_in_coord(s, &self->state.dest_y, delta);
  }
  if (present & 0x04)
  {
    rdp_orders_in_coord(s, &self->state.dest_cx, delta);
  }
  if (present & 0x08)
  {
    rdp_orders_in_coord(s, &self->state.dest_cy, delta);
  }
  if (present & 0x10)
  {
    in_uint8(s, self->state.dest_opcode);
  }
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                          self->state.dest_opcode);
  self->rdp_layer->mod->server_fill_rect(self->rdp_layer->mod,
                                         self->state.dest_x,
                                         self->state.dest_y,
                                         self->state.dest_cx,
                                         self->state.dest_cy);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 6);
    out_uint16_le(rec_s, self->state.dest_x);
    out_uint16_le(rec_s, self->state.dest_y);
    out_uint16_le(rec_s, self->state.dest_cx);
    out_uint16_le(rec_s, self->state.dest_cy);
    out_uint8(rec_s, self->state.dest_opcode);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a pattern blt order */
static void APP_CC
rdp_orders_process_patblt(struct rdp_orders* self, struct stream* s,
                          int present, int delta)
{
  int fgcolor;
  int bgcolor;
  struct stream* rec_s;

  if (present & 0x0001)
  {
    rdp_orders_in_coord(s, &self->state.pat_x, delta);
  }
  if (present & 0x0002)
  {
    rdp_orders_in_coord(s, &self->state.pat_y, delta);
  }
  if (present & 0x0004)
  {
    rdp_orders_in_coord(s, &self->state.pat_cx, delta);
  }
  if (present & 0x0008)
  {
    rdp_orders_in_coord(s, &self->state.pat_cy, delta);
  }
  if (present & 0x0010)
  {
    in_uint8(s, self->state.pat_opcode);
  }
  if (present & 0x0020)
  {
    rdp_orders_in_color(s, &self->state.pat_bgcolor);
  }
  if (present & 0x0040)
  {
    rdp_orders_in_color(s, &self->state.pat_fgcolor);
  }
  rdp_orders_parse_brush(s, &self->state.pat_brush, present >> 7);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                          self->state.pat_opcode);
  self->rdp_layer->mod->server_set_mixmode(self->rdp_layer->mod, 1);
  fgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.pat_fgcolor,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_fgcolor(self->rdp_layer->mod, fgcolor);
  bgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.pat_bgcolor,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_bgcolor(self->rdp_layer->mod, bgcolor);
  self->rdp_layer->mod->server_set_brush(self->rdp_layer->mod,
                                         self->state.pat_brush.xorigin,
                                         self->state.pat_brush.yorigin,
                                         self->state.pat_brush.style,
                                         self->state.pat_brush.pattern);
  self->rdp_layer->mod->server_fill_rect(self->rdp_layer->mod,
                                         self->state.pat_x,
                                         self->state.pat_y,
                                         self->state.pat_cx,
                                         self->state.pat_cy);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
  self->rdp_layer->mod->server_set_mixmode(self->rdp_layer->mod, 0);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 5);
    out_uint16_le(rec_s, self->state.pat_x);
    out_uint16_le(rec_s, self->state.pat_y);
    out_uint16_le(rec_s, self->state.pat_cx);
    out_uint16_le(rec_s, self->state.pat_cy);
    out_uint8(rec_s, self->state.pat_opcode);
    out_uint32_le(rec_s, self->state.pat_fgcolor);
    out_uint32_le(rec_s, self->state.pat_bgcolor);
    out_uint8(rec_s, self->state.pat_brush.xorigin);
    out_uint8(rec_s, self->state.pat_brush.yorigin);
    out_uint8(rec_s, self->state.pat_brush.style);
    out_uint8a(rec_s, self->state.pat_brush.pattern, 8);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a screen blt order */
static void APP_CC
rdp_orders_process_screenblt(struct rdp_orders* self, struct stream* s,
                             int present, int delta)
{
  struct stream* rec_s;

  if (present & 0x0001)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_x, delta);
  }
  if (present & 0x0002)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_y, delta);
  }
  if (present & 0x0004)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_cx, delta);
  }
  if (present & 0x0008)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_cy, delta);
  }
  if (present & 0x0010)
  {
    in_uint8(s, self->state.screenblt_opcode);
  }
  if (present & 0x0020)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_srcx, delta);
  }
  if (present & 0x0040)
  {
    rdp_orders_in_coord(s, &self->state.screenblt_srcy, delta);
  }
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                          self->state.screenblt_opcode);
  self->rdp_layer->mod->server_screen_blt(self->rdp_layer->mod,
                                          self->state.screenblt_x,
                                          self->state.screenblt_y,
                                          self->state.screenblt_cx,
                                          self->state.screenblt_cy,
                                          self->state.screenblt_srcx,
                                          self->state.screenblt_srcy);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 4);
    out_uint16_le(rec_s, self->state.screenblt_x);
    out_uint16_le(rec_s, self->state.screenblt_y);
    out_uint16_le(rec_s, self->state.screenblt_cx);
    out_uint16_le(rec_s, self->state.screenblt_cy);
    out_uint16_le(rec_s, self->state.screenblt_srcx);
    out_uint16_le(rec_s, self->state.screenblt_srcy);
    out_uint8(rec_s, self->state.screenblt_opcode);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a line order */
static void APP_CC
rdp_orders_process_line(struct rdp_orders* self, struct stream* s,
                        int present, int delta)
{
  int bgcolor;
  int fgcolor;
  struct stream* rec_s;

  if (present & 0x0001)
  {
    in_uint16_le(s, self->state.line_mixmode);
  }
  if (present & 0x0002)
  {
    rdp_orders_in_coord(s, &self->state.line_startx, delta);
  }
  if (present & 0x0004)
  {
    rdp_orders_in_coord(s, &self->state.line_starty, delta);
  }
  if (present & 0x0008)
  {
    rdp_orders_in_coord(s, &self->state.line_endx, delta);
  }
  if (present & 0x0010)
  {
    rdp_orders_in_coord(s, &self->state.line_endy, delta);
  }
  if (present & 0x0020)
  {
    rdp_orders_in_color(s, &self->state.line_bgcolor);
  }
  if (present & 0x0040)
  {
    in_uint8(s, self->state.line_opcode);
  }
  rdp_orders_parse_pen(s, &self->state.line_pen, present >> 7);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                          self->state.line_opcode);
  bgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.line_bgcolor,
                                     self->rdp_layer->colormap.colors);
  fgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.line_pen.color,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_fgcolor(self->rdp_layer->mod, fgcolor);
  self->rdp_layer->mod->server_set_bgcolor(self->rdp_layer->mod, bgcolor);
  self->rdp_layer->mod->server_set_pen(self->rdp_layer->mod,
                                       self->state.line_pen.style,
                                       self->state.line_pen.width);
  self->rdp_layer->mod->server_draw_line(self->rdp_layer->mod,
                                         self->state.line_startx,
                                         self->state.line_starty,
                                         self->state.line_endx,
                                         self->state.line_endy);
  self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 3);
    out_uint16_le(rec_s, self->state.line_mixmode);
    out_uint16_le(rec_s, self->state.line_startx);
    out_uint16_le(rec_s, self->state.line_starty);
    out_uint16_le(rec_s, self->state.line_endx);
    out_uint16_le(rec_s, self->state.line_endy);
    out_uint32_le(rec_s, self->state.line_bgcolor);
    out_uint8(rec_s, self->state.line_opcode);
    out_uint8(rec_s, self->state.line_pen.style);
    out_uint8(rec_s, self->state.line_pen.width);
    out_uint32_le(rec_s, self->state.line_pen.color);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process an opaque rectangle order */
static void APP_CC
rdp_orders_process_rect(struct rdp_orders* self, struct stream* s,
                        int present, int delta)
{
  int i;
  int fgcolor;
  struct stream* rec_s;

  if (present & 0x01)
  {
    rdp_orders_in_coord(s, &self->state.rect_x, delta);
  }
  if (present & 0x02)
  {
    rdp_orders_in_coord(s, &self->state.rect_y, delta);
  }
  if (present & 0x04)
  {
    rdp_orders_in_coord(s, &self->state.rect_cx, delta);
  }
  if (present & 0x08)
  {
    rdp_orders_in_coord(s, &self->state.rect_cy, delta);
  }
  if (present & 0x10)
  {
    in_uint8(s, i);
    self->state.rect_color = (self->state.rect_color & 0xffffff00) | i;
  }
  if (present & 0x20)
  {
    in_uint8(s, i);
    self->state.rect_color = (self->state.rect_color & 0xffff00ff) | (i << 8);
  }
  if (present & 0x40)
  {
    in_uint8(s, i);
    self->state.rect_color = (self->state.rect_color & 0xff00ffff) | (i << 16);
  }
  fgcolor = rdp_orders_convert_color(self->rdp_layer->mod->rdp_bpp,
                                     self->rdp_layer->mod->xrdp_bpp,
                                     self->state.rect_color,
                                     self->rdp_layer->colormap.colors);
  self->rdp_layer->mod->server_set_fgcolor(self->rdp_layer->mod, fgcolor);
  self->rdp_layer->mod->server_fill_rect(self->rdp_layer->mod,
                                         self->state.rect_x,
                                         self->state.rect_y,
                                         self->state.rect_cx,
                                         self->state.rect_cy);
  if (self->rdp_layer->rec_mode)
  {
    rdp_rec_check_file(self->rdp_layer);
    make_stream(rec_s);
    init_stream(rec_s, 512);
    s_push_layer(rec_s, iso_hdr, 4);
    out_uint8(rec_s, 1);
    out_uint16_le(rec_s, self->state.rect_x);
    out_uint16_le(rec_s, self->state.rect_y);
    out_uint16_le(rec_s, self->state.rect_cx);
    out_uint16_le(rec_s, self->state.rect_cy);
    out_uint32_le(rec_s, self->state.rect_color);
    rdp_rec_write_item(self->rdp_layer, rec_s);
    free_stream(rec_s);
  }
}

/*****************************************************************************/
/* Process a desktop save order */
static void APP_CC
rdp_orders_process_desksave(struct rdp_orders* self, struct stream* s,
                            int present, int delta)
{
  int width;
  int height;

  if (present & 0x01)
  {
    in_uint32_le(s, self->state.desksave_offset);
  }
  if (present & 0x02)
  {
    rdp_orders_in_coord(s, &self->state.desksave_left, delta);
  }
  if (present & 0x04)
  {
    rdp_orders_in_coord(s, &self->state.desksave_top, delta);
  }
  if (present & 0x08)
  {
    rdp_orders_in_coord(s, &self->state.desksave_right, delta);
  }
  if (present & 0x10)
  {
    rdp_orders_in_coord(s, &self->state.desksave_bottom, delta);
  }
  if (present & 0x20)
  {
    in_uint8(s, self->state.desksave_action);
  }
  width = (self->state.desksave_right - self->state.desksave_left) + 1;
  height = (self->state.desksave_bottom - self->state.desksave_top) + 1;
  if (self->state.desksave_action == 0)
  {
//		ui_desktop_save(os->offset, os->left, os->top, width, height);
  }
  else
  {
//		ui_desktop_restore(os->offset, os->left, os->top, width, height);
  }
}

/*****************************************************************************/
/* Process a memory blt order */
static void APP_CC
rdp_orders_process_memblt(struct rdp_orders* self, struct stream* s,
                          int present, int delta)
{
  struct rdp_bitmap* bitmap;
  struct stream* rec_s;
  char* bmpdata;

  if (present & 0x0001)
  {
    in_uint8(s, self->state.memblt_cache_id);
    in_uint8(s, self->state.memblt_color_table);
  }
  if (present & 0x0002)
  {
    rdp_orders_in_coord(s, &self->state.memblt_x, delta);
  }
  if (present & 0x0004)
  {
    rdp_orders_in_coord(s, &self->state.memblt_y, delta);
  }
  if (present & 0x0008)
  {
    rdp_orders_in_coord(s, &self->state.memblt_cx, delta);
  }
  if (present & 0x0010)
  {
    rdp_orders_in_coord(s, &self->state.memblt_cy, delta);
  }
  if (present & 0x0020)
  {
    in_uint8(s, self->state.memblt_opcode);
  }
  if (present & 0x0040)
  {
    rdp_orders_in_coord(s, &self->state.memblt_srcx, delta);
  }
  if (present & 0x0080)
  {
    rdp_orders_in_coord(s, &self->state.memblt_srcy, delta);
  }
  if (present & 0x0100)
  {
    in_uint16_le(s, self->state.memblt_cache_idx);
  }
  bitmap = self->cache_bitmap[self->state.memblt_cache_id]
                             [self->state.memblt_cache_idx];
  if (bitmap != 0)
  {
    self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod,
                                            self->state.memblt_opcode);
    bmpdata = rdp_orders_convert_bitmap(self->rdp_layer->mod->rdp_bpp,
                                        self->rdp_layer->mod->xrdp_bpp,
                                        bitmap->data, bitmap->width,
                                        bitmap->height,
                                        self->cache_colormap
                                   [self->state.memblt_color_table]->colors);
    self->rdp_layer->mod->server_paint_rect(self->rdp_layer->mod,
                                            self->state.memblt_x,
                                            self->state.memblt_y,
                                            self->state.memblt_cx,
                                            self->state.memblt_cy,
                                            bmpdata,
                                            bitmap->width,
                                            bitmap->height,
                                            self->state.memblt_srcx,
                                            self->state.memblt_srcy);
    self->rdp_layer->mod->server_set_opcode(self->rdp_layer->mod, 0xcc);
    if (self->rdp_layer->rec_mode)
    {
      rdp_rec_check_file(self->rdp_layer);
      make_stream(rec_s);
      init_stream(rec_s, 512);
      s_push_layer(rec_s, iso_hdr, 4);
      out_uint8(rec_s, 2);
      out_uint8(rec_s, self->state.memblt_opcode);
      out_uint16_le(rec_s, self->state.memblt_x);
      out_uint16_le(rec_s, self->state.memblt_y);
      out_uint16_le(rec_s, self->state.memblt_cx);
      out_uint16_le(rec_s, self->state.memblt_cy);
      out_uint16_le(rec_s, self->state.memblt_cache_id);
      out_uint16_le(rec_s, self->state.memblt_cache_idx);
      out_uint16_le(rec_s, self->state.memblt_srcx);
      out_uint16_le(rec_s, self->state.memblt_srcy);
      rdp_rec_write_item(self->rdp_layer, rec_s);
      free_stream(rec_s);
    }
    if (bmpdata != bitmap->data)
    {
      g_free(bmpdata);
    }
  }
}

/*****************************************************************************/
/* Process a 3-way blt order */
static void APP_CC
rdp_orders_process_triblt(struct rdp_orders* self, struct stream* s,
                          int present, int delta)
{
  /* not used */
}

/*****************************************************************************/
/* Process a polyline order */
static void APP_CC
rdp_orders_process_polyline(struct rdp_orders* self, struct stream* s,
                            int present, int delta)
{
  if (present & 0x01)
  {
    rdp_orders_in_coord(s, &self->state.polyline_x, delta);
  }
  if (present & 0x02)
  {
    rdp_orders_in_coord(s, &self->state.polyline_y, delta);
  }
  if (present & 0x04)
  {
    in_uint8(s, self->state.polyline_opcode);
  }
  if (present & 0x10)
  {
    rdp_orders_in_color(s, &self->state.polyline_fgcolor);
  }
  if (present & 0x20)
  {
    in_uint8(s, self->state.polyline_lines);
  }
  if (present & 0x40)
  {
    in_uint8(s, self->state.polyline_datasize);
    in_uint8a(s, self->state.polyline_data, self->state.polyline_datasize);
  }
  /* todo */
}

/*****************************************************************************/
int APP_CC
rdp_orders_process_orders(struct rdp_orders* self, struct stream* s,
                          int num_orders)
{
  int processed;
  int order_flags;
  int size;
  int present;
  int delta;

  processed = 0;
  while (processed < num_orders)
  {
    in_uint8(s, order_flags);
    if (!(order_flags & RDP_ORDER_STANDARD))
    {
      /* error, this should always be set */
      break;
    }
    if (order_flags & RDP_ORDER_SECONDARY)
    {
      rdp_orders_process_secondary_order(self, s);
    }
    else
    {
      if (order_flags & RDP_ORDER_CHANGE)
      {
        in_uint8(s, self->state.order_type);
      }
      switch (self->state.order_type)
      {
        case RDP_ORDER_TRIBLT:
        case RDP_ORDER_TEXT2:
          size = 3;
          break;
        case RDP_ORDER_PATBLT:
        case RDP_ORDER_MEMBLT:
        case RDP_ORDER_LINE:
          size = 2;
          break;
        default:
          size = 1;
          break;
      }
      rdp_orders_in_present(s, &present, order_flags, size);
      if (order_flags & RDP_ORDER_BOUNDS)
      {
        if (!(order_flags & RDP_ORDER_LASTBOUNDS))
        {
          rdp_orders_parse_bounds(self, s);
        }
        self->rdp_layer->mod->server_set_clip(self->rdp_layer->mod,
                                              self->state.clip_left,
                                              self->state.clip_top,
                       (self->state.clip_right - self->state.clip_left) + 1,
                       (self->state.clip_bottom - self->state.clip_top) + 1);
      }
      delta = order_flags & RDP_ORDER_DELTA;
      switch (self->state.order_type)
      {
        case RDP_ORDER_TEXT2:
          rdp_orders_process_text2(self, s, present, delta);
          break;
        case RDP_ORDER_DESTBLT:
          rdp_orders_process_destblt(self, s, present, delta);
          break;
        case RDP_ORDER_PATBLT:
          rdp_orders_process_patblt(self, s, present, delta);
          break;
        case RDP_ORDER_SCREENBLT:
          rdp_orders_process_screenblt(self, s, present, delta);
          break;
        case RDP_ORDER_LINE:
          rdp_orders_process_line(self, s, present, delta);
          break;
        case RDP_ORDER_RECT:
          rdp_orders_process_rect(self, s, present, delta);
          break;
        case RDP_ORDER_DESKSAVE:
          rdp_orders_process_desksave(self, s, present, delta);
          break;
        case RDP_ORDER_MEMBLT:
          rdp_orders_process_memblt(self, s, present, delta);
          break;
        case RDP_ORDER_TRIBLT:
          rdp_orders_process_triblt(self, s, present, delta);
          break;
        case RDP_ORDER_POLYLINE:
          rdp_orders_process_polyline(self, s, present, delta);
          break;
        default:
          /* error unknown order */
          break;
      }
      if (order_flags & RDP_ORDER_BOUNDS)
      {
        self->rdp_layer->mod->server_reset_clip(self->rdp_layer->mod);
      }
    }
    processed++;
  }
  return 0;
}

/*****************************************************************************/
/* returns pointer, it might return bmpdata if the data dosen't need to
   be converted, else it mallocs it.  The calling function must free
   it if needed */
char* APP_CC
rdp_orders_convert_bitmap(int in_bpp, int out_bpp, char* bmpdata,
                          int width, int height, int* palette)
{
  char* out;
  char* src;
  char* dst;
  int i;
  int j;
  int red;
  int green;
  int blue;
  int pixel;

  if (in_bpp == out_bpp && in_bpp == 16)
  {
    return bmpdata;
  }
  if (in_bpp == 8 && out_bpp == 8)
  {
    out = (char*)g_malloc(width * height, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((unsigned char*)src);
        pixel = palette[pixel];
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR8(red, green, blue);
        *dst = pixel;
        src++;
        dst++;
      }
    }
    return out;
  }
  if (in_bpp == 8 && out_bpp == 16)
  {
    out = (char*)g_malloc(width * height * 2, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((unsigned char*)src);
        pixel = palette[pixel];
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR16(red, green, blue);
        *((unsigned short*)dst) = pixel;
        src++;
        dst += 2;
      }
    }
    return out;
  }
  return 0;
}

/*****************************************************************************/
/* returns color or 0 */
int APP_CC
rdp_orders_convert_color(int in_bpp, int out_bpp, int in_color, int* palette)
{
  int pixel;
  int red;
  int green;
  int blue;

  if (in_bpp == out_bpp && in_bpp == 16)
  {
    return in_color;
  }
  if (in_bpp == 8 && out_bpp == 8)
  {
    pixel = palette[in_color];
    SPLITCOLOR32(red, green, blue, pixel);
    pixel = COLOR8(red, green, blue);
    return pixel;
  }
  if (in_bpp == 8 && out_bpp == 16)
  {
    pixel = palette[in_color];
    SPLITCOLOR32(red, green, blue, pixel);
    pixel = COLOR16(red, green, blue);
    return pixel;
  }
  return 0;
}
