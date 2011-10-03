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
   Copyright (C) Jay Sorg 2010

   freerdp wrapper

*/

#include "xrdp-freerdp.h"
#include "xrdp-color.h"

#define GET_MOD(_inst) ((struct mod*)((_inst)->param1))
#define SET_MOD(_inst, _mod) ((_inst)->param1) = _mod

struct my_bitmap
{
  char* data;
  int width;
  int height;
  int bpp;
};

struct my_cursor
{
  char* andmask;
  int andbpp;
  char* xormask;
  int xorbpp;
  int width;
  int height;
  int hotx;
  int hoty;
};

/*****************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_start(struct mod* mod, int w, int h, int bpp)
{
  LIB_DEBUG(mod, "in lib_mod_start");
  g_writeln("lib_mod_start: w %d h %d bpp %d", w, h, bpp);
  mod->width = w;
  mod->height = h;
  mod->bpp = bpp;
  if (bpp == 24)
  {
    mod->settings->server_depth = 32;
  }
  else
  {
    mod->settings->server_depth = mod->bpp;
  }
  mod->settings->width = mod->width;
  mod->settings->height = mod->height;
  mod->settings->nla_security = 1;
  LIB_DEBUG(mod, "out lib_mod_start");
  return 0;
}

/******************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_connect(struct mod* mod)
{
  int code;

  LIB_DEBUG(mod, "in lib_mod_connect");
  code = mod->inst->rdp_connect(mod->inst);
  g_writeln("lib_mod_connect: code %d", code);
  LIB_DEBUG(mod, "out lib_mod_connect");
  return code;
}

/******************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_event(struct mod* mod, int msg, long param1, long param2,
              long param3, long param4)
{
  LIB_DEBUG(mod, "in lib_mod_event");
  int ext;

  //g_writeln("%d %d %d %d %d", msg, param1, param2, param3, param4);
  switch (msg)
  {
    case 15:
      ext = param4 & 0x100 ? 1 : 0;
      mod->inst->rdp_send_input_scancode(mod->inst, 0, ext, param3);
      break;
    case 16:
      ext = param4 & 0x100 ? 1 : 0;
      mod->inst->rdp_send_input_scancode(mod->inst, 1, ext, param3);
      break;
    case 17:
      mod->inst->rdp_sync_input(mod->inst, param4);
      break;
    case 100:
      mod->inst->rdp_send_input_mouse(mod->inst, PTRFLAGS_MOVE,
                                      param1, param2);
      break;
    case 101:
      mod->inst->rdp_send_input_mouse(mod->inst, PTRFLAGS_BUTTON1,
                                      param1, param2);
      break;
    case 102:
      mod->inst->rdp_send_input_mouse(mod->inst,
                                      PTRFLAGS_BUTTON1 | PTRFLAGS_DOWN,
                                      param1, param2);
      break;
    case 103:
      mod->inst->rdp_send_input_mouse(mod->inst,
                                      PTRFLAGS_BUTTON2, param1, param2);
      break;
    case 104:
      mod->inst->rdp_send_input_mouse(mod->inst,
                                      PTRFLAGS_BUTTON2 | PTRFLAGS_DOWN,
                                      param1, param2);
      break;
    case 105:
      mod->inst->rdp_send_input_mouse(mod->inst,
                                      PTRFLAGS_BUTTON3, param1, param2);
      break;
    case 106:
      mod->inst->rdp_send_input_mouse(mod->inst,
                                      PTRFLAGS_BUTTON3 | PTRFLAGS_DOWN,
                                      param1, param2);
      break;
    case 107:
      //mod->inst->rdp_send_input_mouse(mod->inst,
      //                                MOUSE_FLAG_BUTTON4, param1, param2);
      break;
    case 108:
      //mod->inst->rdp_send_input_mouse(mod->inst,
      //                                MOUSE_FLAG_BUTTON4 | MOUSE_FLAG_DOWN,
      //                                param1, param2);
      break;
    case 109:
      //mod->inst->rdp_send_input_mouse(mod->inst,
      //                                MOUSE_FLAG_BUTTON5, param1, param2);
      break;
    case 110:
      //mod->inst->rdp_send_input_mouse(mod->inst,
      //                                MOUSE_FLAG_BUTTON5 | MOUSE_FLAG_DOWN,
      //                                param1, param2);
      break;
  }
  LIB_DEBUG(mod, "out lib_mod_event");
  return 0;
}

/******************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_signal(struct mod* mod)
{
  LIB_DEBUG(mod, "in lib_mod_signal");
  g_writeln("lib_mod_signal:");
  LIB_DEBUG(mod, "out lib_mod_signal");
  return 0;
}

/******************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_end(struct mod* mod)
{
  g_writeln("lib_mod_end:");
  return 0;
}

/******************************************************************************/
/* return error */
static int DEFAULT_CC
lib_mod_set_param(struct mod* mod, char* name, char* value)
{
  g_writeln("lib_mod_set_param: name [%s] value [%s]", name, value);
  if (g_strcmp(name, "hostname") == 0)
  {
    g_strncpy(mod->settings->hostname, value, sizeof(mod->settings->hostname)-1);
  }
  else if (g_strcmp(name, "ip") == 0)
  {
    g_strncpy(mod->settings->server, value, sizeof(mod->settings->server)-1);
  }
  else if (g_strcmp(name, "port") == 0)
  {
    mod->settings->tcp_port_rdp = g_atoi(value);
  }
  else if (g_strcmp(name, "keylayout") == 0)
  {
    mod->settings->keyboard_layout = g_atoi(value);
  }
  else if (g_strcmp(name, "username") == 0)
  {
    g_strncpy(mod->settings->username, value, sizeof(mod->settings->username)-1);
  }
  else if (g_strcmp(name, "domain") == 0)
  {
    g_strncpy(mod->settings->domain, value, sizeof(mod->settings->domain)-1);
  }
  else if (g_strcmp(name, "password") == 0)
  {
    g_strncpy(mod->settings->password, value, sizeof(mod->settings->password)-1);
  }
  else if (g_strcmp(name, "hostname") == 0)
  {
    g_strncpy(mod->settings->hostname, value, sizeof(mod->settings->hostname)-1);
  }
  else if (g_strcmp(name, "program") == 0)
  {
    g_strncpy(mod->settings->shell, value, sizeof(mod->settings->shell)-1);
  }
  else if (g_strcmp(name, "nla") == 0)
  {
    if ((g_strcasecmp(value, "yes") == 0) ||
        (g_strcasecmp(value, "true") == 0) ||
        (g_strcasecmp(value, "1") == 0))
    {
      mod->settings->nla_security = 1;
    }
    else
    {
      mod->settings->nla_security = 0;
    }
  }
  else if (g_strcmp(name, "name") == 0)
  {
   // entry name
  }
  else if (g_strcmp(name, "lib") == 0)
  {
   // library name
  }
  else
  {
    g_writeln("lib_mod_set_param: unknown name [%s] value [%s]", name, value);
  }

  return 0;
}

/******************************************************************************/
static int DEFAULT_CC
mod_session_change(struct mod* v, int a, int b)
{
  g_writeln("mod_session_change:");
  return 0;
}

/******************************************************************************/
static int DEFAULT_CC
mod_get_wait_objs(struct mod* v, tbus* read_objs, int* rcount,
                  tbus* write_objs, int* wcount, int* timeout)
{
  void** rfds;
  void** wfds;

  rfds = (void**)read_objs;
  wfds = (void**)write_objs;
  return v->inst->rdp_get_fds(v->inst, rfds, rcount, wfds, wcount);
}

/******************************************************************************/
static int DEFAULT_CC
mod_check_wait_objs(struct mod* v)
{
  return v->inst->rdp_check_fds(v->inst);
}

/******************************************************************************/
static void DEFAULT_CC
ui_error(rdpInst* inst, const char* text)
{
  g_writeln("ui_error: %s", text);
}

/******************************************************************************/
static void DEFAULT_CC
ui_warning(rdpInst* inst, const char* text)
{
  g_writeln("ui_warning: %s", text);
}

/******************************************************************************/
static void DEFAULT_CC
ui_unimpl(rdpInst* inst, const char* text)
{
  g_writeln("ui_unimpl: %s", text);
}

/******************************************************************************/
static void DEFAULT_CC
ui_begin_update(rdpInst* inst)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_begin_update(mod);
}

/******************************************************************************/
static void DEFAULT_CC
ui_end_update(rdpInst* inst)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_end_update(mod);
}

/******************************************************************************/
static void DEFAULT_CC
ui_desktop_save(rdpInst* inst, int offset, int x, int y,
                int cx, int cy)
{
  g_writeln("ui_desktop_save:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_desktop_restore(rdpInst* inst, int offset, int x, int y,
  int cx, int cy)
{
  g_writeln("ui_desktop_restore:");
}

/******************************************************************************/
static RD_HBITMAP DEFAULT_CC
ui_create_bitmap(rdpInst* inst, int width, int height, uint8* data)
{
  struct my_bitmap* bm;
  struct mod* mod;
  int size;
  int bpp;
  char* bmpdata;

  mod = GET_MOD(inst);
  bpp = mod->bpp == 24 ? 32 : mod->bpp;
  bm = (struct my_bitmap*)g_malloc(sizeof(struct my_bitmap), 1);
  bm->width = width;
  bm->height = height;
  bm->bpp = bpp;
  bmpdata = convert_bitmap(mod->settings->server_depth, bpp,
                           data, width, height, mod->cmap);
  if (bmpdata == (char*)data)
  {
    size = width * height * ((bpp + 7) / 8);
    bm->data = (char*)g_malloc(size, 0);
    g_memcpy(bm->data, bmpdata, size);
  }
  else
  {
    bm->data = bmpdata;
  }
  return bm;
}

/******************************************************************************/
static void DEFAULT_CC
ui_paint_bitmap(rdpInst* inst, int x, int y, int cx, int cy, int width,
                int height, uint8* data)
{
  struct mod* mod;
  char* bmpdata;

  mod = GET_MOD(inst);
  bmpdata = convert_bitmap(mod->settings->server_depth, mod->bpp,
                           data, width, height, mod->cmap);
  if (bmpdata != 0)
  {
    mod->server_paint_rect(mod, x, y, cx, cy, bmpdata, width, height, 0, 0);
  }
  if (bmpdata != (char*)data)
  {
    g_free(bmpdata);
  }
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_bitmap(rdpInst* inst, RD_HBITMAP bmp)
{
  struct my_bitmap* bm;

  bm = (struct my_bitmap*)bmp;
  g_free(bm->data);
  g_free(bm);
}

/******************************************************************************/
static void DEFAULT_CC
ui_line(rdpInst* inst, uint8 opcode, int startx, int starty, int endx,
        int endy, RD_PEN* pen)
{
  g_writeln("ui_line:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_rect(rdpInst* inst, int x, int y, int cx, int cy, uint32 color)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  color = convert_color(mod->settings->server_depth, mod->bpp,
                        color, mod->cmap);
  mod->server_set_fgcolor(mod, color);
  mod->server_fill_rect(mod, x, y, cx, cy);
}

/******************************************************************************/
static void DEFAULT_CC
ui_polygon(rdpInst* inst, uint8 opcode, uint8 fillmode, RD_POINT* point,
           int npoints, RD_BRUSH* brush, uint32 bgcolor, uint32 fgcolor)
{
  g_writeln("ui_polygon:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_polyline(rdpInst* inst, uint8 opcode, RD_POINT* points, int npoints,
            RD_PEN* pen)
{
  g_writeln("ui_polyline:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_ellipse(rdpInst* inst, uint8 opcode, uint8 fillmode, int x, int y,
           int cx, int cy, RD_BRUSH* brush, uint32 bgcolor, uint32 fgcolor)
{
  g_writeln("ui_ellipse:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_add_char(rdpInst * inst, uint8 font, uint16 character, sint16 offset,
            sint16 baseline, uint16 width, uint16 height, uint8 * data)
{
  struct mod* mod;

  //g_writeln("ui_add_char:");
  mod = GET_MOD(inst);
  mod->server_add_char(mod, font, character, offset, baseline,
                       width, height, (char*)data);
}

/******************************************************************************/
static void DEFAULT_CC
ui_draw_text(rdpInst* inst, uint8 font, uint8 flags,
             uint8 opcode, int mixmode,
             int x, int y, int clipx, int clipy, int clipcx, int clipcy,
             int boxx, int boxy, int boxcx, int boxcy, RD_BRUSH * brush,
             uint32 bgcolor, uint32 fgcolor, uint8 * text, uint8 length)
{
  struct mod* mod;

  //g_writeln("ui_draw_text: flags %d mixmode %d x %d y %d", flags, mixmode, x, y);
  //g_writeln("%d %d %d %d %d %d %d %d", clipx, clipy, clipcx, clipcy, boxx, boxy, boxcx, boxcy);
  mod = GET_MOD(inst);
  fgcolor = convert_color(mod->settings->server_depth, mod->bpp,
                          fgcolor, mod->cmap);
  mod->server_set_fgcolor(mod, fgcolor);
  bgcolor = convert_color(mod->settings->server_depth, mod->bpp,
                          bgcolor, mod->cmap);
  mod->server_set_bgcolor(mod, bgcolor);
  mod->server_draw_text(mod, font, flags, mixmode,
                        clipx, clipy, clipx + clipcx, clipy + clipcy,
                        boxx, boxy, boxx + boxcx, boxy + boxcy, x, y,
                        (char*)text, length);
}

/******************************************************************************/
static void DEFAULT_CC
ui_start_draw_glyphs(rdpInst* inst, uint32 bgcolor, uint32 fgcolor)
{
  g_writeln("ui_start_draw_glyphs:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_draw_glyph(rdpInst* inst, int x, int y, int cx, int cy,
  RD_HGLYPH glyph)
{
  g_writeln("ui_draw_glyph:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_end_draw_glyphs(rdpInst* inst, int x, int y, int cx, int cy)
{
  g_writeln("ui_end_draw_glyphs:");
}

/******************************************************************************/
static uint32 DEFAULT_CC
ui_get_toggle_keys_state(rdpInst* inst)
{
  g_writeln("ui_get_toggle_keys_state:");
  return 0;
}

/******************************************************************************/
static void DEFAULT_CC
ui_bell(rdpInst* inst)
{
  g_writeln("ui_bell:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_destblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy)
{
  struct mod* mod;

  g_writeln("ui_destblt:");
  mod = GET_MOD(inst);
  mod->server_set_opcode(mod, opcode);
  mod->server_fill_rect(mod, x, y, cx, cy);
  mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void DEFAULT_CC
ui_patblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy,
          RD_BRUSH* brush, uint32 bgcolor, uint32 fgcolor)
{
  struct mod* mod;
  uint8 idata[8];
  int index;

  mod = GET_MOD(inst);
  mod->server_set_opcode(mod, opcode);
  fgcolor = convert_color(mod->settings->server_depth, mod->bpp,
                          fgcolor, mod->cmap);
  mod->server_set_fgcolor(mod, fgcolor);
  bgcolor = convert_color(mod->settings->server_depth, mod->bpp,
                          bgcolor, mod->cmap);
  mod->server_set_bgcolor(mod, bgcolor);
  mod->server_set_mixmode(mod, 1);
  if (brush->bd != 0)
  {
    if (brush->bd->color_code == 1) /* 8x8 1 bpp */
    {
      for (index = 0; index < 8; index++)
      {
        idata[index] = ~(brush->bd->data[index]);
      }
      mod->server_set_brush(mod, brush->xorigin, brush->yorigin,
                            brush->style, idata);
    }
    else
    {
      g_writeln("ui_patblt: error color_code %d", brush->bd->color_code);
    }
  }
  else
  {
    for (index = 0; index < 8; index++)
    {
      idata[index] = ~(brush->pattern[index]);
    }
    mod->server_set_brush(mod, brush->xorigin, brush->yorigin,
                          brush->style, idata);
  }
  mod->server_fill_rect(mod, x, y, cx, cy);
  mod->server_set_opcode(mod, 0xcc);
  mod->server_set_mixmode(mod, 0);
}

/******************************************************************************/
static void DEFAULT_CC
ui_screenblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy,
             int srcx, int srcy)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_set_opcode(mod, opcode);
  mod->server_screen_blt(mod, x, y, cx, cy, srcx, srcy);
  mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void DEFAULT_CC
ui_memblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy,
          RD_HBITMAP src, int srcx, int srcy)
{
  struct my_bitmap* bitmap;
  struct mod* mod;
  char* bmpdata;

  mod = GET_MOD(inst);
  bitmap = (struct my_bitmap*)src;
  mod->server_set_opcode(mod, opcode);
  mod->server_paint_rect(mod, x, y, cx, cy, bitmap->data,
                         bitmap->width, bitmap->height, srcx, srcy);
  mod->server_set_opcode(mod, 0xcc);
}

/******************************************************************************/
static void DEFAULT_CC
ui_triblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy,
          RD_HBITMAP src, int srcx, int srcy, RD_BRUSH* brush,
          uint32 bgcolor, uint32 fgcolor)
{
  g_writeln("ui_triblt:");
}

/******************************************************************************/
static RD_HGLYPH DEFAULT_CC
ui_create_glyph(rdpInst* inst, int width, int height, uint8* data)
{
  g_writeln("ui_create_glyph:");
  return 0;
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_glyph(rdpInst* inst, RD_HGLYPH glyph)
{
  g_writeln("ui_destroy_glyph:");
}

/******************************************************************************/
static int DEFAULT_CC
ui_select(rdpInst* inst, int rdp_socket)
{
  return 1;
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_clip(rdpInst* inst, int x, int y, int cx, int cy)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_set_clip(mod, x, y, cx, cy);
}

/******************************************************************************/
static void DEFAULT_CC
ui_reset_clip(rdpInst* inst)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_reset_clip(mod);
}

/******************************************************************************/
static void DEFAULT_CC
ui_resize_window(rdpInst* inst)
{
  g_writeln("ui_resize_window:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_cursor(rdpInst* inst, RD_HCURSOR cursor)
{
  struct mod* mod;
  struct my_cursor* cur;

  //g_writeln("ui_set_cursor:");
  mod = GET_MOD(inst);
  cur = (struct my_cursor*)cursor;
  if (cur != 0)
  {
    mod->server_set_cursor(mod, cur->hotx, cur->hoty,
                           cur->xormask, cur->andmask);
  }
  else
  {
    g_writeln("ui_set_cursor: nil cursor");
  }
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_cursor(rdpInst* inst, RD_HCURSOR cursor)
{
  struct my_cursor* cur;

  //g_writeln("ui_destroy_cursor:");
  cur = (struct my_cursor*)cursor;
  if (cur != 0)
  {
    g_free(cur->andmask);
    g_free(cur->xormask);
  }
  g_free(cur);
}

#define RGB24(_r, _g, _b)  \
  (_r << 16) | (_g << 8) | _b;

/******************************************************************************/
static int
l_get_pixel(tui8* data, int x, int y, int width, int height, int bpp)
{
  int start;
  int shift;
  tui16* src16;
  tui32* src32;
  int red, green, blue;

  switch (bpp)
  {
    case 1:
      width = (width + 7) / 8;
      start = (y * width) + x / 8;
      shift = x % 8;
      return (data[start] & (0x80 >> shift)) != 0;
    case 8:
      return data[y * width + x];
    case 15:
    case 16:
      src16 = (uint16*) data;
      return src16[y * width + x];
    case 24:
      data += y * width * 3;
      data += x * 3;
      red = data[0];
      green = data[1];
      blue = data[2];
      return RGB24(red, green, blue);
    case 32:
      src32 = (uint32*) data;
      return src32[y * width + x];
    default:
      g_writeln("l_get_pixel: unknown bpp %d", bpp);
      break;
  }

  return 0;
}

/******************************************************************************/
static void
l_set_pixel(tui8* data, int x, int y, int width, int height,
            int bpp, int pixel)
{
  int start;
  int shift;
  int* dst32;
  tui8* dst8;

  if (bpp == 1)
  {
    width = (width + 7) / 8;
    start = (y * width) + x / 8;
    shift = x % 8;
    if (pixel)
      data[start] = data[start] | (0x80 >> shift);
    else
      data[start] = data[start] & ~(0x80 >> shift);
  }
  else if (bpp == 24)
  {
    dst8 = data + (y * width + x) * 3;
    *(dst8++) = (pixel >> 16) & 0xff;
    *(dst8++) = (pixel >> 8) & 0xff;
    *(dst8++) = (pixel >> 0) & 0xff;
  }
  else if (bpp == 32)
  {
    dst32 = (int*) data;
    dst32[y * width + x] = pixel;
  }
  else
  {
    g_writeln("l_set_pixel: unknown bpp %d", bpp);
  }
}


/******************************************************************************/
/* andmask = mask = 32 * 32 / 8
   xormask = data = 32 * 32 * 3 */
static RD_HCURSOR DEFAULT_CC
ui_create_cursor(rdpInst* inst, unsigned int x, unsigned int y,
                 int width, int height, uint8* andmask,
                 uint8* xormask, int bpp)
{
  struct mod* mod;
  struct my_cursor* cur;
  int i;
  int j;
  int jj;
  int apixel;
  int xpixel;
  char* dst;

  mod = GET_MOD(inst);
  g_writeln("ui_create_cursor: x %d y %d width %d height %d bpp %d",
            x, y, width, height, bpp);
  cur = (struct my_cursor*)g_malloc(sizeof(struct my_cursor), 1);
  cur->width = width;
  cur->height = height;
  cur->hotx = x;
  cur->hoty = y;
  cur->andmask = g_malloc(32 * 32 * 4, 1);
  cur->xormask = g_malloc(32 * 32 * 4, 1);
  for (j = 0; j < height; j++)
  {
    jj = (bpp != 1) ? j : (height - 1) - j;
    for (i = 0; i < width; i++)
    {
      apixel = l_get_pixel(andmask, i, jj, width, height, 1);
      xpixel = l_get_pixel(xormask, i, jj, width, height, bpp);
      xpixel = convert_color(bpp, 24, xpixel, mod->cmap);
      l_set_pixel(cur->andmask, i, j, width, height, 1, apixel);
      l_set_pixel(cur->xormask, i, j, width, height, 24, xpixel);
    }
  }
  return (RD_HCURSOR)cur;
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_null_cursor(rdpInst* inst)
{
  g_writeln("ui_set_null_cursor:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_default_cursor(rdpInst* inst)
{
  g_writeln("ui_set_default_cursor:");
}

/******************************************************************************/
static RD_HPALETTE DEFAULT_CC
ui_create_palette(rdpInst* inst, RD_PALETTE* colors)
{
  struct mod* mod;
  int index;
  int red;
  int green;
  int blue;
  int pixel;
  int count;
  int* cmap;

  mod = GET_MOD(inst);
  g_writeln("ui_create_palette:");
  count = 256;
  if (count > colors->count)
  {
    count = colors->count;
  }
  cmap = (int*)g_malloc(256 * 4, 1);
  for (index = 0; index < count; index++)
  {
    red = colors->entries[index].red;
    green = colors->entries[index].green;
    blue = colors->entries[index].blue;
    pixel = COLOR24RGB(red, green, blue);
    cmap[index] = pixel;
  }
  return (RD_HPALETTE)cmap;
}

/******************************************************************************/
static void DEFAULT_CC
ui_move_pointer(rdpInst* inst, int x, int y)
{
  g_writeln("ui_move_pointer:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_palette(rdpInst* inst, RD_HPALETTE map)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  g_writeln("ui_set_palette:");
  g_memcpy(mod->cmap, map, 256 * 4);
  g_free(map);
}

/******************************************************************************/
static RD_HBITMAP DEFAULT_CC
ui_create_surface(rdpInst* inst, int width, int height, RD_HBITMAP old)
{
  g_writeln("ui_create_surface:");
  return 0;
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_surface(rdpInst* inst, RD_HBITMAP surface)
{
  g_writeln("ui_set_surface:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_surface(rdpInst* inst, RD_HBITMAP surface)
{
  g_writeln("ui_destroy_surface:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_channel_data(rdpInst* inst, int chan_id, char* data, int data_size,
  int flags, int total_size)
{
  g_writeln("ui_channel_data:");
}

/******************************************************************************/
static RD_BOOL DEFAULT_CC
ui_authenticate(rdpInst * inst)
{
  return 1;
}

/******************************************************************************/
static int DEFAULT_CC
ui_decode(rdpInst * inst, uint8 * data, int data_size)
{
  return 0;
}

/******************************************************************************/
static RD_BOOL DEFAULT_CC
ui_check_certificate(rdpInst * inst, const char * fingerprint,
                     const char * subject, const char * issuer, RD_BOOL verified)
{
  return 1;
}

/******************************************************************************/
struct mod* EXPORT_CC
mod_init(void)
{
  struct mod* mod;

  //g_writeln("1");
  //freerdp_global_init();
  //g_writeln("2");
  mod = (struct mod*)g_malloc(sizeof(struct mod), 1);
  mod->size = sizeof(struct mod);
  mod->version = CURRENT_MOD_VER;
  mod->handle = (tbus)mod;
  mod->mod_connect = lib_mod_connect;
  mod->mod_start = lib_mod_start;
  mod->mod_event = lib_mod_event;
  mod->mod_signal = lib_mod_signal;
  mod->mod_end = lib_mod_end;
  mod->mod_set_param = lib_mod_set_param;
  mod->mod_session_change = mod_session_change;
  mod->mod_get_wait_objs = mod_get_wait_objs;
  mod->mod_check_wait_objs = mod_check_wait_objs;
  mod->settings = (struct rdp_set*)g_malloc(sizeof(struct rdp_set), 1);

  mod->settings->width = 1280;
  mod->settings->height = 1024;
  mod->settings->encryption = 1;
  mod->settings->rdp_security = 1;
  mod->settings->tls_security = 1;
  mod->settings->server_depth = 16;
  mod->settings->bitmap_cache = 1;
  mod->settings->bitmap_compression = 1;
  mod->settings->performanceflags =
    PERF_DISABLE_WALLPAPER | PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS;
  mod->settings->new_cursors = 1;
  mod->settings->rdp_version = 5;
  mod->settings->text_flags = 1;

  mod->inst = freerdp_new(mod->settings);
  if (mod->inst == 0)
  {
    return 0;
  }
  SET_MOD(mod->inst, mod);
  mod->inst->ui_error = ui_error;
  mod->inst->ui_warning = ui_warning;
  mod->inst->ui_unimpl = ui_unimpl;
  mod->inst->ui_begin_update = ui_begin_update;
  mod->inst->ui_end_update = ui_end_update;
  mod->inst->ui_desktop_save = ui_desktop_save;
  mod->inst->ui_desktop_restore = ui_desktop_restore;
  mod->inst->ui_create_bitmap = ui_create_bitmap;
  mod->inst->ui_paint_bitmap = ui_paint_bitmap;
  mod->inst->ui_destroy_bitmap = ui_destroy_bitmap;
  mod->inst->ui_line = ui_line;
  mod->inst->ui_rect = ui_rect;
  mod->inst->ui_polygon = ui_polygon;
  mod->inst->ui_polyline = ui_polyline;
  mod->inst->ui_ellipse = ui_ellipse;
  mod->inst->ui_add_char = ui_add_char;
  mod->inst->ui_draw_text = ui_draw_text;
  mod->inst->ui_start_draw_glyphs = ui_start_draw_glyphs;
  mod->inst->ui_draw_glyph = ui_draw_glyph;
  mod->inst->ui_end_draw_glyphs = ui_end_draw_glyphs;
  mod->inst->ui_get_toggle_keys_state = ui_get_toggle_keys_state;
  mod->inst->ui_bell = ui_bell;
  mod->inst->ui_destblt = ui_destblt;
  mod->inst->ui_patblt = ui_patblt;
  mod->inst->ui_screenblt = ui_screenblt;
  mod->inst->ui_memblt = ui_memblt;
  mod->inst->ui_triblt = ui_triblt;
  mod->inst->ui_create_glyph = ui_create_glyph;
  mod->inst->ui_destroy_glyph = ui_destroy_glyph;
  mod->inst->ui_select = ui_select;
  mod->inst->ui_set_clip = ui_set_clip;
  mod->inst->ui_reset_clip = ui_reset_clip;
  mod->inst->ui_resize_window = ui_resize_window;
  mod->inst->ui_set_cursor = ui_set_cursor;
  mod->inst->ui_destroy_cursor = ui_destroy_cursor;
  mod->inst->ui_create_cursor = ui_create_cursor;
  mod->inst->ui_set_null_cursor = ui_set_null_cursor;
  mod->inst->ui_set_default_cursor = ui_set_default_cursor;
  mod->inst->ui_create_palette = ui_create_palette;
  mod->inst->ui_move_pointer = ui_move_pointer;
  mod->inst->ui_set_palette = ui_set_palette;
  mod->inst->ui_create_surface = ui_create_surface;
  mod->inst->ui_set_surface = ui_set_surface;
  mod->inst->ui_destroy_surface = ui_destroy_surface;
  mod->inst->ui_channel_data = ui_channel_data;

  mod->inst->ui_authenticate = ui_authenticate;
  mod->inst->ui_decode = ui_decode;
  mod->inst->ui_check_certificate = ui_check_certificate;

  return mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct mod* mod)
{
  if (mod == 0)
  {
    return 0;
  }
  freerdp_free(mod->inst);
  g_free(mod->settings);
  g_free(mod);
  //freerdp_global_finish();
  return 0;
}
