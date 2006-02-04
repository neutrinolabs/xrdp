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
   Copyright (C) Jay Sorg 2005-2006

   libxup main file

*/

#include "xup.h"

/******************************************************************************/
/* returns error */
int DEFAULT_CC
lib_recv(struct mod* mod, char* data, int len)
{
  int rcvd;

  if (mod->sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    rcvd = g_tcp_recv(mod->sck, data, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(mod->sck))
      {
        if (mod->server_is_term(mod))
        {
          return 1;
        }
        g_sleep(1);
      }
      else
      {
        return 1;
      }
    }
    else if (rcvd == 0)
    {
      mod->sck_closed = 1;
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
int DEFAULT_CC
lib_send(struct mod* mod, char* data, int len)
{
  int sent;

  if (mod->sck_closed)
  {
    return 1;
  }
  while (len > 0)
  {
    sent = g_tcp_send(mod->sck, data, len, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(mod->sck))
      {
        if (mod->server_is_term(mod))
        {
          return 1;
        }
        g_sleep(1);
      }
      else
      {
        return 1;
      }
    }
    else if (sent == 0)
    {
      mod->sck_closed = 1;
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

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_start(struct mod* mod, int w, int h, int bpp)
{
  DEBUG(("in lib_mod_start\r\n"));
  mod->width = w;
  mod->height = h;
  DEBUG(("out lib_mod_start\r\n"));
  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_connect(struct mod* mod)
{
  int error;
  int len;
  struct stream* s;

  DEBUG(("in lib_mod_connect\r\n"));
  /* clear screen */
  mod->server_begin_update(mod);
  mod->server_set_fgcolor(mod, 0);
  mod->server_fill_rect(mod, 0, 0, mod->width, mod->height);
  mod->server_end_update(mod);
  mod->sck = g_tcp_socket();
  mod->sck_closed = 0;
  error = g_tcp_connect(mod->sck, "127.0.0.1", "6210");
  if (error == 0)
  {
    g_tcp_set_non_blocking(mod->sck);
    g_tcp_set_no_delay(mod->sck);
  }
  if (error == 0)
  {
    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 103);
    out_uint32_le(s, 200);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    out_uint32_le(s, mod->width);
    out_uint32_le(s, mod->height);
    s_mark_end(s);
    len = s->end - s->data;
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send(mod, s->data, len);
    free_stream(s);
  }
  DEBUG(("out lib_mod_connect error\r\n"));
  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_event(struct mod* mod, int msg, long param1, long param2,
              long param3, long param4)
{
  struct stream* s;
  int len;
  int rv;

  DEBUG(("in lib_mod_event\r\n"));
  make_stream(s);
  init_stream(s, 8192);
  s_push_layer(s, iso_hdr, 4);
  out_uint16_le(s, 103);
  out_uint32_le(s, msg);
  out_uint32_le(s, param1);
  out_uint32_le(s, param2);
  out_uint32_le(s, param3);
  out_uint32_le(s, param4);
  s_mark_end(s);
  len = s->end - s->data;
  s_pop_layer(s, iso_hdr);
  out_uint32_le(s, len);
  rv = lib_send(mod, s->data, len);
  free_stream(s);
  DEBUG(("out lib_mod_event\r\n"));
  return rv;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_signal(struct mod* mod)
{
  struct stream* s;
  int num_orders;
  int index;
  int rv;
  int len;
  int type;
  int x;
  int y;
  int cx;
  int cy;
  int fgcolor;
  int opcode;
  int width;
  int height;
  int srcx;
  int srcy;
  int len_bmpdata;
  int style;
  int x1;
  int y1;
  int x2;
  int y2;
  char* bmpdata;

  DEBUG(("in lib_mod_signal\r\n"));
  make_stream(s);
  init_stream(s, 8192);
  rv = lib_recv(mod, s->data, 8);
  if (rv == 0)
  {
    in_uint16_le(s, type);
    in_uint16_le(s, num_orders);
    in_uint32_le(s, len);
    if (type == 1)
    {
      init_stream(s, len);
      rv = lib_recv(mod, s->data, len);
      if (rv == 0)
      {
        for (index = 0; index < num_orders; index++)
        {
          in_uint16_le(s, type);
          switch (type)
          {
            case 1: /* server_begin_update */
              rv = mod->server_begin_update(mod);
              break;
            case 2: /* server_end_update */
              rv = mod->server_end_update(mod);
              break;
            case 3: /* server_fill_rect */
              in_sint16_le(s, x);
              in_sint16_le(s, y);
              in_uint16_le(s, cx);
              in_uint16_le(s, cy);
              rv = mod->server_fill_rect(mod, x, y, cx, cy);
              break;
            case 4: /* server_screen_blt */
              in_sint16_le(s, x);
              in_sint16_le(s, y);
              in_uint16_le(s, cx);
              in_uint16_le(s, cy);
              in_sint16_le(s, srcx);
              in_sint16_le(s, srcy);
              rv = mod->server_screen_blt(mod, x, y, cx, cy, srcx, srcy);
              break;
            case 5: /* server_paint_rect */
              in_sint16_le(s, x);
              in_sint16_le(s, y);
              in_uint16_le(s, cx);
              in_uint16_le(s, cy);
              in_uint32_le(s, len_bmpdata);
              in_uint8p(s, bmpdata, len_bmpdata);
              in_uint16_le(s, width);
              in_uint16_le(s, height);
              in_sint16_le(s, srcx);
              in_sint16_le(s, srcy);
              rv = mod->server_paint_rect(mod, x, y, cx, cy,
                                          bmpdata, width, height,
                                          srcx, srcy);
              break;
            case 10: /* server_set_clip */
              in_sint16_le(s, x);
              in_sint16_le(s, y);
              in_uint16_le(s, cx);
              in_uint16_le(s, cy);
              rv = mod->server_set_clip(mod, x, y, cx, cy);
              break;
            case 11: /* server_reset_clip */
              rv = mod->server_reset_clip(mod);
              break;
            case 12: /* server_set_fgcolor */
              in_uint32_le(s, fgcolor);
              rv = mod->server_set_fgcolor(mod, fgcolor);
              break;
            case 14:
              in_uint16_le(s, opcode);
              rv = mod->server_set_opcode(mod, opcode);
              break;
            case 17:
              in_uint16_le(s, style);
              in_uint16_le(s, width);
              rv = mod->server_set_pen(mod, style, width);
              break;
            case 18:
              in_sint16_le(s, x1);
              in_sint16_le(s, y1);
              in_sint16_le(s, x2);
              in_sint16_le(s, y2);
              rv = mod->server_draw_line(mod, x1, y1, x2, y2);
              break;
            default:
              rv = 1;
              break;
          }
          if (rv != 0)
          {
            break;
          }
        }
      }
    }
  }
  free_stream(s);
  DEBUG(("out lib_mod_signal\r\n"));
  return rv;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_end(struct mod* mod)
{
  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_set_param(struct mod* mod, char* name, char* value)
{
  if (g_strncasecmp(name, "username", 8) == 0)
  {
    g_strncpy(mod->username, value, 255);
  }
  else if (g_strncasecmp(name, "password", 8) == 0)
  {
    g_strncpy(mod->password, value, 255);
  }
  else if (g_strncasecmp(name, "ip", 2) == 0)
  {
    g_strncpy(mod->ip, value, 255);
  }
  else if (g_strncasecmp(name, "port", 4) == 0)
  {
    g_strncpy(mod->port, value, 255);
  }
  return 0;
}

/******************************************************************************/
struct mod* EXPORT_CC
mod_init(void)
{
  struct mod* mod;

  DEBUG(("in mod_init\r\n"));
  mod = (struct mod*)g_malloc(sizeof(struct mod), 1);
  mod->size = sizeof(struct mod);
  mod->handle = (long)mod;
  mod->mod_connect = lib_mod_connect;
  mod->mod_start = lib_mod_start;
  mod->mod_event = lib_mod_event;
  mod->mod_signal = lib_mod_signal;
  mod->mod_end = lib_mod_end;
  mod->mod_set_param = lib_mod_set_param;
  DEBUG(("out mod_init\r\n"));
  return mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct mod* mod)
{
  DEBUG(("in mod_exit\r\n"));
  if (mod == 0)
  {
    return 0;
  }
  g_tcp_close(mod->sck);
  g_free(mod);
  DEBUG(("out mod_exit\r\n"));
  return 0;
}
