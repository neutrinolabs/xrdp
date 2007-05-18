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
   Copyright (C) Jay Sorg 2004-2007

   module manager

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_mm* APP_CC
xrdp_mm_create(struct xrdp_wm* owner)
{
  struct xrdp_mm* self;

  self = (struct xrdp_mm*)g_malloc(sizeof(struct xrdp_mm), 1);
  self->wm = owner;
  self->login_names = list_create();
  self->login_names->auto_free = 1;
  self->login_values = list_create();
  self->login_values->auto_free = 1;
  return self;
}

/*****************************************************************************/
/* called from main thread */
static long DEFAULT_CC
sync_unload(long param1, long param2)
{
  return g_free_library(param1);
}

/*****************************************************************************/
/* called from main thread */
static long DEFAULT_CC
sync_load(long param1, long param2)
{
  return g_load_library((char*)param1);
}

/*****************************************************************************/
static void APP_CC
xrdp_mm_module_cleanup(struct xrdp_mm* self)
{
  if (self->mod != 0)
  {
    if (self->mod_exit != 0)
    {
      self->mod_exit(self->mod);
    }
  }
  if (self->mod_handle != 0)
  {
    g_xrdp_sync(sync_unload, self->mod_handle, 0);
  }
  self->mod_init = 0;
  self->mod_exit = 0;
  self->mod = 0;
  self->mod_handle = 0;
}

/*****************************************************************************/
void APP_CC
xrdp_mm_delete(struct xrdp_mm* self)
{
  if (self == 0)
  {
    return;
  }
  /* free any modual stuff */
  xrdp_mm_module_cleanup(self);
  if (self->sck != 0)
  {
    g_tcp_close(self->sck);
    self->sck = 0;
  }
  list_delete(self->login_names);
  list_delete(self->login_values);
  g_free(self);
}

/******************************************************************************/
/* returns error */
static int APP_CC
xrdp_mm_recv(struct xrdp_mm* self, char* data, int len)
{
  int rcvd;

  if (self->sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    rcvd = g_tcp_recv(self->sck, data, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (g_is_term())
        {
          return 1;
        }
        g_tcp_can_recv(self->sck, 10);
      }
      else
      {
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      self->sck_closed = 1;
      return 1;
    }
    else
    {
      data += rcvd;
      len -= rcvd;
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_mm_send(struct xrdp_mm* self, char* data, int len)
{
  int sent;

  if (self->sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    sent = g_tcp_send(self->sck, data, len, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (g_is_term())
        {
          return 1;
        }
        g_tcp_can_send(self->sck, 10);
      }
      else
      {
        return 1;
      }
    }
    else if (sent == 0)
    {
      self->sck_closed = 1;
      return 1;
    }
    else
    {
      data += sent;
      len -= sent;
    }
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_send_login(struct xrdp_mm* self)
{
  struct stream* s;
  int rv;
  int index;
  int count;
  char* username;
  char* password;
  char* name;
  char* value;

  xrdp_wm_log_msg(self->wm, "sending login info to sesman");
  username = 0;
  password = 0;
  self->code = 0;
  count = self->login_names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(self->login_names, index);
    value = (char*)list_get_item(self->login_values, index);
    if (g_strcasecmp(name, "username") == 0)
    {
      username = value;
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
      password = value;
    }
    else if (g_strcasecmp(name, "lib") == 0)
    {
      if ((g_strcasecmp(value, "libxup.so") == 0) ||
          (g_strcasecmp(value, "xup.dll") == 0))
      {
        self->code = 10;
      }
    }
  }
  if ((username == 0) || (password == 0))
  {
    xrdp_wm_log_msg(self->wm, "error finding username and password");
    return 1;
  }
  make_stream(s);
  init_stream(s, 8192);
  s_push_layer(s, channel_hdr, 8);
  /* this code is either 0 for Xvnc or 10 for X11rdp */
  out_uint16_be(s, self->code);
  index = g_strlen(username);
  out_uint16_be(s, index);
  out_uint8a(s, username, index);
  index = g_strlen(password);
  out_uint16_be(s, index);
  out_uint8a(s, password, index);
  out_uint16_be(s, self->wm->screen->width);
  out_uint16_be(s, self->wm->screen->height);
  out_uint16_be(s, self->wm->screen->bpp);
  s_mark_end(s);
  s_pop_layer(s, channel_hdr);
  out_uint32_be(s, 0); /* version */
  index = s->end - s->data;
  out_uint32_be(s, index); /* size */
  rv = xrdp_mm_send(self, s->data, index);
  free_stream(s);
  return rv;
}

/*****************************************************************************/
/* returns error */
/* this goes through the login_names looking for one called 'lib'
   then it copies the corisponding login_values item into 'dest'
   'dest' must be at least 'dest_len' + 1 bytes in size */
static int APP_CC
xrdp_mm_get_lib(struct xrdp_mm* self, char* dest, int dest_len)
{
  char* name;
  char* value;
  int index;
  int count;
  int rv;

  rv = 1;
  /* find the library name */
  dest[0] = 0;
  count = self->login_names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(self->login_names, index);
    value = (char*)list_get_item(self->login_values, index);
    if ((name == 0) || (value == 0))
    {
      break;
    }
    if (g_strcasecmp(name, "lib") == 0)
    {
      g_strncpy(dest, value, dest_len);
      rv = 0;
    }
  }
  return rv;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_setup_mod1(struct xrdp_mm* self)
{
  void* func;
  char lib[256];

  if (self == 0)
  {
    return 1;
  }
  lib[0] = 0;
  if (xrdp_mm_get_lib(self, lib, 255) != 0)
  {
    g_writeln("error finding lib");
    return 1;
  }
  if (lib[0] == 0)
  {
    g_writeln("error finding lib");
    return 1;
  }
  if (self->mod_handle == 0)
  {
    self->mod_handle = g_xrdp_sync(sync_load, (long)lib, 0);
    if (self->mod_handle != 0)
    {
      func = g_get_proc_address(self->mod_handle, "mod_init");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_init");
      }
      if (func == 0)
      {
        g_writeln("error finding proc mod_init in %s", lib);
      }
      self->mod_init = (struct xrdp_mod* (*)(void))func;
      func = g_get_proc_address(self->mod_handle, "mod_exit");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_exit");
      }
      if (func == 0)
      {
        g_writeln("error finding proc mod_exit in %s", lib);
      }
      self->mod_exit = (int (*)(struct xrdp_mod*))func;
      if ((self->mod_init != 0) && (self->mod_exit != 0))
      {
        self->mod = self->mod_init();
      }
    }
    else
    {
      g_writeln("error loading %s", lib);
    }
    if (self->mod != 0)
    {
      self->mod->wm = (long)(self->wm);
      self->mod->server_begin_update = server_begin_update;
      self->mod->server_end_update = server_end_update;
      self->mod->server_fill_rect = server_fill_rect;
      self->mod->server_screen_blt = server_screen_blt;
      self->mod->server_paint_rect = server_paint_rect;
      self->mod->server_set_pointer = server_set_pointer;
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
    }
  }
  /* id self->mod is null, there must be a problem */
  if (self->mod == 0)
  {
    DEBUG(("problem loading lib in xrdp_mm_setup_mod1"));
    return 1;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_setup_mod2(struct xrdp_mm* self)
{
  char text[256];
  char* name;
  char* value;
  int i;
  int rv;

  rv = 1;
  text[0] = 0;
  if (!(self->wm->pro_layer->term))
  {
    if (self->mod->mod_start(self->mod, self->wm->screen->width,
                             self->wm->screen->height,
                             self->wm->screen->bpp) != 0)
    {
      self->wm->pro_layer->term = 1; /* kill session */
    }
  }
  if (!(self->wm->pro_layer->term))
  {
    if (self->display > 0)
    {
      if (self->code == 0) /* Xvnc */
      {
        g_snprintf(text, 255, "%d", 5900 + self->display);
      }
      else if (self->code == 10) /* X11rdp */
      {
        g_snprintf(text, 255, "%d", 6200 + self->display);
      }
      else
      {
        self->wm->pro_layer->term = 1; /* kill session */
      }
    }
  }
  if (!(self->wm->pro_layer->term))
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
    name = self->wm->session->client_info->hostname;
    self->mod->mod_set_param(self->mod, "hostname", name);
    g_snprintf(text, 255, "%d", self->wm->session->client_info->keylayout);
    self->mod->mod_set_param(self->mod, "keylayout", text);
    for (i = 0; i < self->login_names->count; i++)
    {
      name = (char*)list_get_item(self->login_names, i);
      value = (char*)list_get_item(self->login_values, i);
      self->mod->mod_set_param(self->mod, name, value);
    }
    /* connect */
    if (self->mod->mod_connect(self->mod) == 0)
    {
      rv = 0;
    }
  }
  return rv;
}

/*****************************************************************************/
static int APP_CC
xrdp_mm_process_login_response(struct xrdp_mm* self, struct stream* s)
{
  int ok;
  int display;
  int rv;
  char text[256];

  rv = 0;
  in_uint16_be(s, ok);
  in_uint16_be(s, display);
  if (ok)
  {
    self->display = display;
    g_snprintf(text, 255, "login successful for display %d", display);
    xrdp_wm_log_msg(self->wm, text);
    if (xrdp_mm_setup_mod1(self) == 0)
    {
      if (xrdp_mm_setup_mod2(self) == 0)
      {
        self->wm->login_mode = 10;
        self->wm->pro_layer->app_sck = self->mod->sck;
      }
    }
  }
  else
  {
    xrdp_wm_log_msg(self->wm, "login failed");
  }
  /* close socket */
  g_tcp_close(self->sck);
  self->sck = 0;
  self->connected_state = 0;
  if (self->wm->login_mode != 10)
  {
    self->wm->pro_layer->app_sck = 0;
    self->wm->login_mode = 11;
    xrdp_mm_module_cleanup(self);
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_mm_connect(struct xrdp_mm* self)
{
  struct list* names;
  struct list* values;
  int index;
  int count;
  int use_sesman;
  int error;
  int ok;
  int rv;
  char* name;
  char* value;
  char ip[256];
  char errstr[256];
  char text[256];

  rv = 0;
  use_sesman = 0;
  names = self->login_names;
  values = self->login_values;
  count = names->count;
  for (index = 0; index < count; index++)
  {
    name = (char*)list_get_item(names, index);
    value = (char*)list_get_item(values, index);
    if (g_strcasecmp(name, "ip") == 0)
    {
      g_strncpy(ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
      if (g_strcasecmp(value, "-1") == 0)
      {
        use_sesman = 1;
      }
    }
  }
  if (use_sesman)
  {
    ok = 0;
    errstr[0] = 0;
    self->sck = g_tcp_socket();
    g_tcp_set_non_blocking(self->sck);
    g_snprintf(text, 255, "connecting to sesman ip %s port 3350", ip);
    xrdp_wm_log_msg(self->wm, text);
    error = g_tcp_connect(self->sck, ip, "3350");
    if (error == 0)
    {
      ok = 1;
    }
    else if (error == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        ok = 1;
        /* wait up to 3 seconds for connect to complete */
        index = 0;
        while (!g_tcp_can_send(self->sck, 100))
        {
          index++;
          if ((index >= 30) || g_is_term())
          {
            g_snprintf(errstr, 255, "error connect sesman - connect timeout");
            ok = 0;
            break;
          }
        }
      }
      else
      {
        name = g_get_strerror();
        g_snprintf(errstr, 255, "error connect sesman - %s", name);
      }
    }
    if (ok)
    {
      /* fully connect */
      xrdp_wm_log_msg(self->wm, "sesman connect ok");
      self->connected_state = 1;
      self->wm->pro_layer->app_sck = self->sck;
      rv = xrdp_mm_send_login(self);
    }
    else
    {
      xrdp_wm_log_msg(self->wm, errstr);
      g_tcp_close(self->sck);
      self->sck = 0;
      rv = 1;
    }
  }
  else /* no sesman */
  {
    if (xrdp_mm_setup_mod1(self) == 0)
    {
      if (xrdp_mm_setup_mod2(self) == 0)
      {
        self->wm->login_mode = 10;
        self->wm->pro_layer->app_sck = self->mod->sck;
      }
    }
    if (self->wm->login_mode != 10)
    {
      self->wm->pro_layer->app_sck = 0;
      self->wm->login_mode = 11;
      xrdp_mm_module_cleanup(self);
    }
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_mm_signal(struct xrdp_mm* self)
{
  struct stream* s;
  int error;
  int version;
  int size;
  int code;
  int rv;

  rv = 0;
  if (self->connected_state == 1)
  {
    make_stream(s);
    init_stream(s, 8192);
    error = xrdp_mm_recv(self, s->data, 8);
    if (error == 0)
    {
      in_uint32_be(s, version);
      in_uint32_be(s, size);
      init_stream(s, 8192);
      error = (size <= 8) || (size > 8000);
    }
    if (error == 0)
    {
      error = xrdp_mm_recv(self, s->data, size - 8);
    }
    if (error == 0)
    {
      in_uint16_be(s, code);
      switch (code)
      {
        case 3:
          rv = xrdp_mm_process_login_response(self, s);
          break;
        default:
          g_writeln("unknown code %d in xrdp_mm_signal", code);
          break;
      }
    }
    free_stream(s);
  }
  return rv;
}

/*****************************************************************************/
int DEFAULT_CC
server_begin_update(struct xrdp_mod* mod)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = xrdp_painter_create(wm, wm->session);
  xrdp_painter_begin_update(p);
  mod->painter = (long)p;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_end_update(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_end_update(p);
  xrdp_painter_delete(p);
  mod->painter = 0;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  xrdp_painter_fill_rect(p, wm->screen, x, y, cx, cy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  p->rop = 0xcc;
  xrdp_painter_copy(p, wm->screen, wm->screen, x, y, cx, cy, srcx, srcy);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy)
{
  struct xrdp_wm* wm;
  struct xrdp_bitmap* b;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  b = xrdp_bitmap_create_with_data(width, height, wm->screen->bpp, data, wm);
  xrdp_painter_copy(p, b, wm->screen, x, y, cx, cy, srcx, srcy);
  xrdp_bitmap_delete(b);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pointer(struct xrdp_mod* mod, int x, int y,
                   char* data, char* mask)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  xrdp_wm_pointer(wm, data, mask, x, y);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_palette(struct xrdp_mod* mod, int* palette)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  if (g_memcmp(wm->palette, palette, 255 * sizeof(int)) != 0)
  {
    g_memcpy(wm->palette, palette, 256 * sizeof(int));
    xrdp_wm_send_palette(wm);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_msg(struct xrdp_mod* mod, char* msg, int code)
{
  struct xrdp_wm* wm;

  if (code == 1)
  {
    g_writeln(msg);
    return 0;
  }
  wm = (struct xrdp_wm*)mod->wm;
  return xrdp_wm_log_msg(wm, msg);
}

/*****************************************************************************/
int DEFAULT_CC
server_is_term(struct xrdp_mod* mod)
{
  return g_is_term();
}

/*****************************************************************************/
int DEFAULT_CC
server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_set_clip(p, x, y, cx, cy);
}

/*****************************************************************************/
int DEFAULT_CC
server_reset_clip(struct xrdp_mod* mod)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_clr_clip(p);
}

/*****************************************************************************/
int DEFAULT_CC
server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->fg_color = fgcolor;
  p->pen.color = p->fg_color;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->bg_color = bgcolor;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_opcode(struct xrdp_mod* mod, int opcode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->rop = opcode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_mixmode(struct xrdp_mod* mod, int mixmode)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->mix_mode = mixmode;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->brush.x_orgin = x_orgin;
  p->brush.y_orgin = y_orgin;
  p->brush.style = style;
  g_memcpy(p->brush.pattern, pattern, 8);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  struct xrdp_painter* p;

  p = (struct xrdp_painter*)mod->painter;
  p->pen.style = style;
  p->pen.width = width;
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_line(p, wm->screen, x1, y1, x2, y2);
}

/*****************************************************************************/
int DEFAULT_CC
server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  struct xrdp_font_char fi;

  fi.offset = offset;
  fi.baseline = baseline;
  fi.width = width;
  fi.height = height;
  fi.incby = 0;
  fi.data = data;
  return libxrdp_orders_send_font(((struct xrdp_wm*)mod->wm)->session,
                                  &fi, font, charactor);
}

/*****************************************************************************/
int DEFAULT_CC
server_draw_text(struct xrdp_mod* mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len)
{
  struct xrdp_wm* wm;
  struct xrdp_painter* p;

  wm = (struct xrdp_wm*)mod->wm;
  p = (struct xrdp_painter*)mod->painter;
  return xrdp_painter_draw_text2(p, wm->screen, font, flags,
                                 mixmode, clip_left, clip_top,
                                 clip_right, clip_bottom,
                                 box_left, box_top,
                                 box_right, box_bottom,
                                 x, y, data, data_len);
}

/*****************************************************************************/
int DEFAULT_CC
server_reset(struct xrdp_mod* mod, int width, int height, int bpp)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  if (wm->client_info == 0)
  {
    return 1;
  }
  /* older client can't resize */
  if (wm->client_info->build <= 419)
  {
    return 0;
  }
  /* if same, don't need to do anything */
  if (wm->client_info->width == width &&
      wm->client_info->height == height &&
      wm->client_info->bpp == bpp)
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
  xrdp_wm_load_static_colors(wm);
  xrdp_wm_load_static_pointers(wm);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
server_query_channel(struct xrdp_mod* mod, int index, char* channel_name,
                     int* channel_flags)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  return libxrdp_query_channel(wm->session, index, channel_name,
                               channel_flags);
}

/*****************************************************************************/
int DEFAULT_CC
server_get_channel_id(struct xrdp_mod* mod, char* name)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  return libxrdp_get_channel_id(wm->session, name);
}

/*****************************************************************************/
int DEFAULT_CC
server_send_to_channel(struct xrdp_mod* mod, int channel_id,
                       char* data, int data_len)
{
  struct xrdp_wm* wm;

  wm = (struct xrdp_wm*)mod->wm;
  return libxrdp_send_to_channel(wm->session, channel_id, data, data_len);
}
