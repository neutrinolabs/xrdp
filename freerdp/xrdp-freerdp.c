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

#define GET_MOD(_inst) ((struct mod*)((_inst)->param1))
#define SET_MOD(_inst, _mod) ((_inst)->param1) = _mod

struct my_bitmap
{
  char* data;
  int width;
  int height;
  int bpp;
};

struct my_glyph
{
  char* data;
  int width;
  int height;
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
  mod->settings->server_depth = mod->bpp;
  mod->settings->width = mod->width;
  mod->settings->height = mod->height;
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
  switch (msg)
  {
    case 15:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_SCANCODE,
                                param4, param3, 0);
      break;
    case 16:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_SCANCODE,
                                param4, param3, 0);
      break;
    case 17:
      mod->inst->rdp_sync_input(mod->inst, param4);
      break;
    case 100:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE, MOUSE_FLAG_MOVE,
                                param1, param2);
      break;
    case 101:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON1, param1, param2);
      break;
    case 102:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN,
                                param1, param2);
      break;
    case 103:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON2, param1, param2);
      break;
    case 104:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON2 | MOUSE_FLAG_DOWN,
                                param1, param2);
      break;
    case 105:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON3, param1, param2);
      break;
    case 106:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON3 | MOUSE_FLAG_DOWN,
                                param1, param2);
      break;
    case 107:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON4, param1, param2);
      break;
    case 108:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON4 | MOUSE_FLAG_DOWN,
                                param1, param2);
      break;
    case 109:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON5, param1, param2);
      break;
    case 110:
      mod->inst->rdp_send_input(mod->inst, RDP_INPUT_MOUSE,
                                MOUSE_FLAG_BUTTON5 | MOUSE_FLAG_DOWN,
                                param1, param2);
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
    g_strncpy(mod->settings->hostname, value, sizeof(mod->settings->hostname));
  }
  else if (g_strcmp(name, "ip") == 0)
  {
    g_strncpy(mod->settings->server, value, sizeof(mod->settings->server));
  }
  else if (g_strcmp(name, "port") == 0)
  {
    mod->settings->tcp_port_rdp = g_atoi(value);
  }
  else if (g_strcmp(name, "keylayout") == 0)
  {
    mod->settings->keyboard_layout = g_atoi(value);
  }
  else if (g_strcmp(name, "name") == 0)
  {
  }
  else if (g_strcmp(name, "lib") == 0)
  {
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

  mod = GET_MOD(inst);
  bm = (struct my_bitmap*)g_malloc(sizeof(struct my_bitmap), 1);
  bm->width = width;
  bm->height = height;
  bm->bpp = mod->bpp;
  size = width * height * (bm->bpp / 8);
  bm->data = (char*)g_malloc(size, 0);
  g_memcpy(bm->data, data, size);
  return bm;
}

/******************************************************************************/
static void DEFAULT_CC
ui_paint_bitmap(rdpInst* inst, int x, int y, int cx, int cy, int width,
                int height, uint8* data)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_paint_rect(mod, x, y, cx, cy, data, width, height, 0, 0);
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
ui_rect(rdpInst* inst, int x, int y, int cx, int cy, int color)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_set_fgcolor(mod, color);
  mod->server_fill_rect(mod, x, y, cx, cy);
}

/******************************************************************************/
static void DEFAULT_CC
ui_polygon(rdpInst* inst, uint8 opcode, uint8 fillmode, RD_POINT* point,
           int npoints, RD_BRUSH* brush, int bgcolor, int fgcolor)
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
           int cx, int cy, RD_BRUSH* brush, int bgcolor, int fgcolor)
{
  g_writeln("ui_ellipse:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_start_draw_glyphs(rdpInst* inst, int bgcolor, int fgcolor)
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
          RD_BRUSH* brush, int bgcolor, int fgcolor)
{
  struct mod* mod;

  mod = GET_MOD(inst);
  mod->server_set_opcode(mod, opcode);
  mod->server_set_fgcolor(mod, fgcolor);
  mod->server_set_bgcolor(mod, bgcolor);
  mod->server_set_mixmode(mod, 1);
  if (brush->bd != 0)
  {
    if (brush->bd->color_code == 1) /* 8x8 1 bpp */
    {
      mod->server_set_brush(mod, brush->xorigin, brush->yorigin,
                            brush->style, brush->bd->data);
    }
    else
    {
      g_writeln("ui_patblt: error color_code %d", brush->bd->color_code);
    }
  }
  else
  {
    mod->server_set_brush(mod, brush->xorigin, brush->yorigin,
                          brush->style, brush->pattern);
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
  struct my_bitmap* bm;
  struct mod* mod;

  mod = GET_MOD(inst);
  bm = (struct my_bitmap*)src;
  mod->server_paint_rect(mod, x, y, cx, cy, bm->data,
                         bm->width, bm->height, srcx, srcy);
}

/******************************************************************************/
static void DEFAULT_CC
ui_triblt(rdpInst* inst, uint8 opcode, int x, int y, int cx, int cy,
          RD_HBITMAP src, int srcx, int srcy, RD_BRUSH* brush,
          int bgcolor, int fgcolor)
{
  g_writeln("ui_triblt:");
}

/******************************************************************************/
static RD_HGLYPH DEFAULT_CC
ui_create_glyph(rdpInst* inst, int width, int height, uint8* data)
{
  struct my_glyph* gg;
  int size;

  g_writeln("ui_create_glyph:");
  gg = (struct my_glyph*)g_malloc(sizeof(struct my_glyph), 1);
  gg->width = width;
  gg->height = height;
  size = ((height * ((width + 7) / 8)) + 3) & (~3);
  gg->data = (char*)g_malloc(size, 0);
  g_memcpy(gg->data, data, size);
  return gg;
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_glyph(rdpInst* inst, RD_HGLYPH glyph)
{
  struct my_glyph* gg;

  g_writeln("ui_destroy_glyph:");
  gg = (struct my_glyph*)glyph;
  g_free(gg->data);
  g_free(gg);
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
  g_writeln("ui_set_cursor:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_destroy_cursor(rdpInst* inst, RD_HCURSOR cursor)
{
  g_writeln("ui_destroy_cursor:");
}

/******************************************************************************/
static RD_HCURSOR DEFAULT_CC
ui_create_cursor(rdpInst* inst, unsigned int x, unsigned int y,
                 int width, int height, uint8* andmask,
                 uint8* xormask, int bpp)
{
  g_writeln("ui_create_cursor:");
  return 0;
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
ui_create_colormap(rdpInst* inst, RD_PALETTE* colors)
{
  g_writeln("ui_create_colormap:");
  return 0;
}

/******************************************************************************/
static void DEFAULT_CC
ui_move_pointer(rdpInst* inst, int x, int y)
{
  g_writeln("ui_move_pointer:");
}

/******************************************************************************/
static void DEFAULT_CC
ui_set_colormap(rdpInst* inst, RD_HPALETTE map)
{
  g_writeln("ui_set_colormap:");
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
struct mod* EXPORT_CC
mod_init(void)
{
  struct mod* mod;

  freerdp_global_init();
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

  mod->settings->encryption = 1;
  mod->settings->server_depth = 16;
  mod->settings->bitmap_cache = 1;
  mod->settings->bitmap_compression = 1;
  mod->settings->performanceflags =
    PERF_DISABLE_WALLPAPER | PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS;
  mod->settings->new_cursors = 1;
  mod->settings->rdp_version = 5;

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
  mod->inst->ui_create_colormap = ui_create_colormap;
  mod->inst->ui_move_pointer = ui_move_pointer;
  mod->inst->ui_set_colormap = ui_set_colormap;
  mod->inst->ui_create_surface = ui_create_surface;
  mod->inst->ui_set_surface = ui_set_surface;
  mod->inst->ui_destroy_surface = ui_destroy_surface;
  mod->inst->ui_channel_data = ui_channel_data;
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
  freerdp_global_finish();
  return 0;
}
