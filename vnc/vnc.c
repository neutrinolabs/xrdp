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

   libvnc

*/

#include "vnc.h"

/******************************************************************************/
int lib_mod_event(struct vnc* v, int msg, int param1, int param2)
{
  struct stream* s;
  int key;

  make_stream(s);
  if (msg >= 15 && msg <= 16) /* key events */
  {
    key = 0;
    if (param2 == 0xffff) /* ascii char */
    {
      key = param1;
    }
    else /* non ascii key event */
    {
      switch (param1)
      {
        case 0x0001: key = 0xff1b; break; /* ecs */
        case 0x000e: key = 0xff08; break; /* backspace */
        case 0x000f: key = 0xff09; break; /* tab */
        case 0x001c: key = 0xff0d; break; /* enter */
        /* left-right control */
        case 0x001d: key = (param2 & 0x0100) ? 0xffe3 : 0xffe4; break;
        case 0x002a: key = 0xffe1; break; /* left shift */
        case 0x0036: key = 0xffe2; break; /* right shift */
        /* left-right alt */
        case 0x0038: key = (param2 & 0x0100) ? 0xffe9 : 0xffea; break;
        case 0x003b: key = 0xffbe; break; /* F1 */
        case 0x003c: key = 0xffbf; break; /* F2 */
        case 0x003d: key = 0xffc0; break; /* F3 */
        case 0x003e: key = 0xffc1; break; /* F4 */
        case 0x003f: key = 0xffc2; break; /* F5 */
        case 0x0040: key = 0xffc3; break; /* F6 */
        case 0x0041: key = 0xffc4; break; /* F7 */
        case 0x0042: key = 0xffc5; break; /* F8 */
        case 0x0043: key = 0xffc6; break; /* F9 */
        case 0x0044: key = 0xffc7; break; /* F10 */
        case 0x0047: key = 0xff50; break; /* home */
        case 0x0048: key = 0xff52; break; /* up arrow */
        case 0x0049: key = 0xff55; break; /* page up */
        case 0x004b: key = 0xff51; break; /* left arrow */
        case 0x004d: key = 0xff53; break; /* right arrow */
        case 0x004f: key = 0xff57; break; /* end */
        case 0x0050: key = 0xff54; break; /* down arrow */
        case 0x0051: key = 0xff56; break; /* page down */
        case 0x0052: key = 0xff63; break; /* insert */
        case 0x0053: key = 0xffff; break; /* delete */
        case 0x0057: key = 0xffc8; break; /* F11 */
        case 0x0058: key = 0xffc9; break; /* F12 */
      }
    }
    if (key > 0)
    {
      init_stream(s, 8192);
      out_uint8(s, 4);
      out_uint8(s, msg == 15); /* down flag */
      out_uint8s(s, 2);
      out_uint32_be(s, key);
      if (g_tcp_force_send(v->sck, s->data, 8) != 0)
      {
        g_tcp_close(v->sck);
        v->sck = 0;
        free_stream(s);
        return 1;
      }
    }
  }
  else if (msg >= 100 && msg <= 110) /* mouse events */
  {
    switch (msg)
    {
      case 100: break; /* WM_MOUSEMOVE */
      case 101: v->mod_mouse_state &= ~1; break; /* WM_LBUTTONUP */
      case 102: v->mod_mouse_state |= 1; break; /* WM_LBUTTONDOWN */
      case 103: v->mod_mouse_state &= ~4; break; /* WM_RBUTTONUP */
      case 104: v->mod_mouse_state |= 4; break; /* WM_RBUTTONDOWN */
      case 105: v->mod_mouse_state &= ~2; break;
      case 106: v->mod_mouse_state |= 2; break;
      case 107: v->mod_mouse_state &= ~8; break;
      case 108: v->mod_mouse_state |= 8; break;
      case 109: v->mod_mouse_state &= ~16; break;
      case 110: v->mod_mouse_state |= 16; break;
    }
    init_stream(s, 8192);
    out_uint8(s, 5);
    out_uint8(s, v->mod_mouse_state);
    out_uint16_be(s, param1);
    out_uint16_be(s, param2);
    if (g_tcp_force_send(v->sck, s->data, 6) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      return 1;
    }
  }
  free_stream(s);
  return 0;
}

//******************************************************************************
int get_pixel_safe(char* data, int x, int y, int width, int height, int bpp)
{
  int start;
  int shift;

  if (x < 0)
    return 0;
  if (y < 0)
    return 0;
  if (x >= width)
    return 0;
  if (y >= height)
    return 0;
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
  else if (bpp == 8)
  {
    return *(((unsigned char*)data) + (y * width + x));
  }
  else if (bpp == 15 || bpp == 16)
  {
    return *(((unsigned short*)data) + (y * width + x));
  }
  return 0;
}

/******************************************************************************/
void set_pixel_safe(char* data, int x, int y, int width, int height, int bpp,
                    int pixel)
{
  int start;
  int shift;

  if (x < 0)
    return;
  if (y < 0)
    return;
  if (x >= width)
    return;
  if (y >= height)
    return;
  if (bpp == 1)
  {
    width = (width + 7) / 8;
    start = (y * width) + x / 8;
    shift = x % 8;
    if (pixel & 1)
      data[start] = data[start] | (0x80 >> shift);
    else
      data[start] = data[start] & ~(0x80 >> shift);
  }
  else if (bpp == 15 || bpp == 16)
  {
    *(((unsigned short*)data) + (y * width + x)) = pixel;
  }
  else if (bpp == 24)
  {
    *(data + (3 * (y * width + x)) + 0) = pixel >> 0;
    *(data + (3 * (y * width + x)) + 1) = pixel >> 8;
    *(data + (3 * (y * width + x)) + 2) = pixel >> 16;
  }
}

/******************************************************************************/
int split_color(int pixel, int* r, int* g, int* b, int bpp, int* palette)
{
  if (bpp == 8)
  {
    if (pixel >= 0 && pixel < 256 && palette != 0)
    {
      *r = (palette[pixel] >> 16) & 0xff;
      *g = (palette[pixel] >> 8) & 0xff;
      *b = (palette[pixel] >> 0) & 0xff;
    }
  }
  else if (bpp == 16)
  {
    *r = ((pixel >> 8) & 0xf8) | ((pixel >> 13) & 0x7);
    *g = ((pixel >> 3) & 0xfc) | ((pixel >> 9) & 0x3);
    *b = ((pixel << 3) & 0xf8) | ((pixel >> 2) & 0x7);
  }
  return 0;
}

/******************************************************************************/
int make_color(int r, int g, int b, int bpp)
{
  if (bpp == 24)
  {
    return (r << 16) | (g << 8) | b;
  }
  return 0;
}

/******************************************************************************/
int lib_framebuffer_update(struct vnc* v)
{
  char* data;
  char* d1;
  char* d2;
  char cursor_data[32 * (32 * 3)];
  char cursor_mask[32 * (32 / 8)];
  int num_recs;
  int i;
  int j;
  int k;
  int x;
  int y;
  int cx;
  int cy;
  int srcx;
  int srcy;
  int encoding;
  int Bpp;
  int pixel;
  int r;
  int g;
  int b;
  int data_size;
  int need_size;
  struct stream* s;

  data_size = 0;
  data = 0;
  Bpp = (v->mod_bpp + 7) / 8;
  make_stream(s);
  init_stream(s, 8192);
  if (g_tcp_force_recv(v->sck, s->data, 3) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 1);
  in_uint16_be(s, num_recs);

  v->server_begin_update(v);
  for (i = 0; i < num_recs; i++)
  {
    init_stream(s, 8192);
    if (g_tcp_force_recv(v->sck, s->data, 12) != 0)
    {
      free_stream(s);
      return 1;
    }
    in_uint16_be(s, x);
    in_uint16_be(s, y);
    in_uint16_be(s, cx);
    in_uint16_be(s, cy);
    in_uint32_be(s, encoding);
    if (encoding == 0) /* raw */
    {
      need_size = cx * cy * Bpp;
      if (need_size > data_size)
      {
        g_free(data);
        data = (char*)g_malloc(need_size, 0);
        data_size = need_size;
      }
      if (g_tcp_force_recv(v->sck, data, cx * cy * Bpp) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
      if (v->server_paint_rect(v, x, y, cx, cy, data) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
    }
    else if (encoding == 1) /* copy rect */
    {
      init_stream(s, 8192);
      if (g_tcp_force_recv(v->sck, s->data, 4) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
      in_uint16_be(s, srcx);
      in_uint16_be(s, srcy);
      if (v->server_screen_blt(v, x, y, cx, cy, srcx, srcy) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
    }
    else if (encoding == 0xffffff11) /* cursor */
    {
      g_memset(cursor_data, 0, 32 * (32 * 3));
      g_memset(cursor_mask, 0, 32 * (32 / 8));
      init_stream(s, 8192);
      if (g_tcp_force_recv(v->sck, s->data,
                           cx * cy * Bpp + ((cx + 7) / 8) * cy) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
      in_uint8p(s, d1, cx * cy * Bpp);
      in_uint8p(s, d2, ((cx + 7) / 8) * cy);
      for (j = 0; j < 32; j++)
      {
        for (k = 0; k < 32; k++)
        {
          pixel = get_pixel_safe(d2, k, 31 - j, cx, cy, 1);
          set_pixel_safe(cursor_mask, k, j, 32, 32, 1, !pixel);
          if (pixel)
          {
            pixel = get_pixel_safe(d1, k, 31 - j, cx, cy, v->mod_bpp);
            split_color(pixel, &r, &g, &b, v->mod_bpp, v->palette);
            pixel = make_color(r, g, b, 24);
            set_pixel_safe(cursor_data, k, j, 32, 32, 24, pixel);
          }
        }
      }
      if (v->server_set_cursor(v, x, y, cursor_data, cursor_mask) != 0)
      {
        g_free(data);
        free_stream(s);
        return 1;
      }
    }
    else
    {
      g_printf("error in lib_framebuffer_update\n\r");
    }
  }
  v->server_end_update(v);
  g_free(data);

  /* FrambufferUpdateRequest */
  init_stream(s, 8192);
  out_uint8(s, 3);
  out_uint8(s, 1);
  out_uint16_be(s, 0);
  out_uint16_be(s, 0);
  out_uint16_be(s, v->mod_width);
  out_uint16_be(s, v->mod_height);
  if (g_tcp_force_send(v->sck, s->data, 10) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/******************************************************************************/
int lib_clip_data(struct vnc* v)
{
  struct stream* s;
  int size;

  make_stream(s);
  init_stream(s, 8192);
  if (g_tcp_force_recv(v->sck, s->data, 7) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 3);
  in_uint32_be(s, size);
  init_stream(s, 8192);
  if (g_tcp_force_recv(v->sck, s->data, size) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/******************************************************************************/
int lib_palette_update(struct vnc* v)
{
  struct stream* s;
  int first_color;
  int num_colors;
  int i;
  int r;
  int g;
  int b;

  make_stream(s);
  init_stream(s, 8192);
  if (g_tcp_force_recv(v->sck, s->data, 5) != 0)
  {
    free_stream(s);
    return 1;
  }
  in_uint8s(s, 1);
  in_uint16_be(s, first_color);
  in_uint16_be(s, num_colors);
  init_stream(s, 8192);
  if (g_tcp_force_recv(v->sck, s->data, num_colors * 6) != 0)
  {
    free_stream(s);
    return 1;
  }
  for (i = 0; i < num_colors; i++)
  {
    in_uint16_be(s, r);
    in_uint16_be(s, g);
    in_uint16_be(s, b);
    r = r >> 8;
    g = g >> 8;
    b = b >> 8;
    v->palette[first_color + i] = (r << 16) | (g << 8) | b;
  }
  v->server_begin_update(v);
  v->server_palette(v, v->palette);
  v->server_end_update(v);
  free_stream(s);
  return 0;
}

/******************************************************************************/
int lib_mod_signal(struct vnc* v)
{
  char type;

  if (g_tcp_force_recv(v->sck, &type, 1) != 0)
  {
    g_tcp_close(v->sck);
    v->sck = 0;
    return 1;
  }
  if (type == 0) /* framebuffer update */
  {
    if (lib_framebuffer_update(v) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      return 1;
    }
  }
  else if (type == 1) /* palette */
  {
    if (lib_palette_update(v) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      return 1;
    }
  }
  else if (type == 3) /* clipboard */
  {
    if (lib_clip_data(v) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      return 1;
    }
  }
  else
  {
    g_printf("unknown in lib_mod_signal %d\n\r", type);
  }
  return 0;
}

/******************************************************************************/
int lib_mod_start(struct vnc* v, int w, int h, int bpp)
{
  v->server_begin_update(v);
  v->server_fill_rect(v, 0, 0, w, h, 0);
  v->server_end_update(v);
  v->server_width = w;
  v->server_height = h;
  v->server_bpp = bpp;
  return 0;
}

/******************************************************************************/
/*
  return error
  1 - authentation failed
  2 - authentation failed
  3 - server name length received from server too long
  4 - server and client bpp do not match
  5 - no more available X desktops when spawning a new session
  6 - no ip
  7 - sck closed
*/
int lib_mod_connect(struct vnc* v)
{
  char cursor_data[32 * (32 * 3)];
  char cursor_mask[32 * (32 / 8)];
  char con_port[256];
  struct stream* s;
  struct stream* pixel_format;
  int error;
  int i;
  int check_sec_result;
  int sck;
  int version;
  int size;
  int code;
  int ok;
  int display;

  check_sec_result = 1;
  if (g_strcmp(v->ip, "") == 0)
  {
    return 6;
  }
  make_stream(s);
  if (g_strcmp(v->port, "-1") == 0)
  {
    i = 0;
    error = 0;
    init_stream(s, 8192);
    sck = g_tcp_socket();
    if (g_tcp_connect(sck, v->ip, "3350") == 0)
    {
      s_push_layer(s, channel_hdr, 8);
      out_uint16_be(s, 0); // code
      i = g_strlen(v->username);
      out_uint16_be(s, i);
      out_uint8a(s, v->username, i);
      i = g_strlen(v->password);
      out_uint16_be(s, i);
      out_uint8a(s, v->password, i);
      out_uint16_be(s, v->server_width);
      out_uint16_be(s, v->server_height);
      out_uint16_be(s, v->server_bpp);
      s_mark_end(s);
      s_pop_layer(s, channel_hdr);
      out_uint32_be(s, 0); // version
      out_uint32_be(s, s->end - s->data); // size
      if (g_tcp_force_send(sck, s->data, s->end - s->data) != 0)
      {
        g_tcp_close(sck);
        free_stream(s);
        return 7;
      }
      init_stream(s, 8192);
      if (g_tcp_force_recv(sck, s->data, 8) == 0)
      {
        in_uint32_be(s, version);
        in_uint32_be(s, size);
        init_stream(s, 8192);
        if (g_tcp_force_recv(sck, s->data, size - 8) == 0)
        {
          if (version == 0)
          {
            in_uint16_be(s, code);
            if (code == 3)
            {
              in_uint16_be(s, ok);
              in_uint16_be(s, display);
              if (ok)
              {
                i = display;
              }
            }
          }
        }
        else
        {
          g_tcp_close(sck);
          free_stream(s);
          return 7;
        }
      }
      else
      {
        g_tcp_close(sck);
        free_stream(s);
        return 7;
      }
    }
    g_tcp_close(sck);
    if (error != 0 || i == 0)
    {
      free_stream(s);
      return 5;
    }
    g_sprintf(con_port, "%d", 5900 + i);
    v->vnc_desktop = i;
  }
  else
  {
    g_sprintf(con_port, "%s", v->port);
  }
  make_stream(pixel_format);
  v->sck = g_tcp_socket();
  error = g_tcp_connect(v->sck, v->ip, con_port);
  if (error == 0)
  {
    g_tcp_set_non_blocking(v->sck);
    /* protocal version */
    init_stream(s, 8192);
    if (g_tcp_force_recv(v->sck, s->data, 12) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    if (g_tcp_force_send(v->sck, "RFB 003.003\n", 12) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    /* sec type */
    init_stream(s, 8192);
    if (g_tcp_force_recv(v->sck, s->data, 4) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    in_uint32_be(s, i);
    if (i == 1) /* none */
    {
      check_sec_result = 0;
    }
    else if (i == 2) /* dec the password and the server random */
    {
      init_stream(s, 8192);
      if (g_tcp_force_recv(v->sck, s->data, 16) != 0)
      {
        g_tcp_close(v->sck);
        v->sck = 0;
        free_stream(s);
        free_stream(pixel_format);
        return 7;
      }
      rfbEncryptBytes((unsigned char*)s->data, v->password);
      if (g_tcp_force_send(v->sck, s->data, 16) != 0)
      {
        g_tcp_close(v->sck);
        v->sck = 0;
        free_stream(s);
        free_stream(pixel_format);
        return 7;
      }
    }
    else
    {
      error = 1;
    }
  }
  if (error == 0 && check_sec_result)
  {
    /* sec result */
    init_stream(s, 8192);
    if (g_tcp_force_recv(v->sck, s->data, 4) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    in_uint32_be(s, i);
    if (i != 0)
    {
      error = 2;
    }
  }
  if (error == 0)
  {
    init_stream(s, 8192);
    s->data[0] = 1;
    if (g_tcp_force_send(v->sck, s->data, 1) != 0) /* share flag */
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    if (g_tcp_force_recv(v->sck, s->data, 4) != 0) /* server init */
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    in_uint16_be(s, v->mod_width);
    in_uint16_be(s, v->mod_height);
    init_stream(pixel_format, 8192);
    if (g_tcp_force_recv(v->sck, pixel_format->data, 16) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    in_uint8(pixel_format, v->mod_bpp);
    init_stream(s, 8192);
    if (g_tcp_force_recv(v->sck, s->data, 4) != 0) /* name len */
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    in_uint32_be(s, i);
    if (i > 255 || i < 0)
    {
      error = 3;
    }
    else
    {
      if (g_tcp_force_recv(v->sck, v->mod_name, i) != 0)
      {
        g_tcp_close(v->sck);
        v->sck = 0;
        free_stream(s);
        free_stream(pixel_format);
        return 7;
      }
      v->mod_name[i] = 0;
    }
    /* should be connected */
  }
  if (error == 0)
  {
    /* SetPixelFormat */
    init_stream(s, 8192);
    out_uint8(s, 0);
    out_uint8(s, 0);
    out_uint8(s, 0);
    out_uint8(s, 0);
    init_stream(pixel_format, 8192);
    if (v->mod_bpp == 8)
    {
      out_uint8(pixel_format, 8); /* bits per pixel */
      out_uint8(pixel_format, 8); /* depth */
      out_uint8(pixel_format, 0); /* big endian */
      out_uint8(pixel_format, 0); /* true color flag */
      out_uint16_be(pixel_format, 0); /* red max */
      out_uint16_be(pixel_format, 0); /* green max */
      out_uint16_be(pixel_format, 0); /* blue max */
      out_uint8(pixel_format, 0); /* red shift */
      out_uint8(pixel_format, 0); /* green shift */
      out_uint8(pixel_format, 0); /* blue shift */
      out_uint8s(pixel_format, 3); /* pad */
    }
    out_uint8a(s, pixel_format->data, 16);
    if (g_tcp_force_send(v->sck, s->data, 20) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    /* SetEncodings */
    init_stream(s, 8192);
    out_uint8(s, 2);
    out_uint8(s, 0);
    out_uint16_be(s, 3);
    out_uint32_be(s, 0); /* raw */
    out_uint32_be(s, 1); /* copy rect */
    out_uint32_be(s, 0xffffff11); /* cursor */
    if (g_tcp_force_send(v->sck, s->data, 4 + 3 * 4) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
    /* FrambufferUpdateRequest */
    init_stream(s, 8192);
    out_uint8(s, 3);
    out_uint8(s, 0);
    out_uint16_be(s, 0);
    out_uint16_be(s, 0);
    out_uint16_be(s, v->mod_width);
    out_uint16_be(s, v->mod_height);
    if (g_tcp_force_send(v->sck, s->data, 10) != 0)
    {
      g_tcp_close(v->sck);
      v->sck = 0;
      free_stream(s);
      free_stream(pixel_format);
      return 7;
    }
  }
  if (error == 0)
  {
    v->server_error_popup(v, "hi", "Hi");
    if (v->server_bpp != v->mod_bpp)
    {
      error = 4;
    }
  }
  /* set almost null cursor */
  g_memset(cursor_data, 0, 32 * (32 * 3));
  g_memset(cursor_data + (32 * (32 * 3) - 1 * 32 * 3), 0xff, 9);
  g_memset(cursor_data + (32 * (32 * 3) - 2 * 32 * 3), 0xff, 9);
  g_memset(cursor_data + (32 * (32 * 3) - 3 * 32 * 3), 0xff, 9);
  g_memset(cursor_mask, 0xff, 32 * (32 / 8));
  v->server_set_cursor(v, 0, 0, cursor_data, cursor_mask);
  free_stream(s);
  free_stream(pixel_format);
  if (error != 0)
  {
    g_tcp_close(v->sck);
    v->sck = 0;
  }
  return error;
}

/******************************************************************************/
int lib_mod_invalidate(struct vnc* v, int x, int y, int cx, int cy)
{
  struct stream* s;

  make_stream(s);
  /* FrambufferUpdateRequest */
  init_stream(s, 8192);
  out_uint8(s, 3);
  out_uint8(s, 0);
  out_uint16_be(s, x);
  out_uint16_be(s, y);
  out_uint16_be(s, cx);
  out_uint16_be(s, cy);
  if (g_tcp_force_send(v->sck, s->data, 10) != 0)
  {
    free_stream(s);
    g_tcp_close(v->sck);
    v->sck = 0;
    return 1;
  }
  free_stream(s);
  return 0;
}

/******************************************************************************/
int lib_mod_end(struct vnc* v)
{
  if (v->vnc_desktop != 0)
  {
  }
  return 0;
}

/******************************************************************************/
int lib_mod_set_param(struct vnc* v, char* name, char* value)
{
  if (g_strcmp(name, "username") == 0)
  {
    g_strncpy(v->username, value, 255);
  }
  else if (g_strcmp(name, "password") == 0)
  {
    g_strncpy(v->password, value, 255);
  }
  else if (g_strcmp(name, "ip") == 0)
  {
    g_strncpy(v->ip, value, 255);
  }
  else if (g_strcmp(name, "port") == 0)
  {
    g_strncpy(v->port, value, 255);
  }
  return 0;
}

/******************************************************************************/
int mod_init()
{
  struct vnc* v;

  v = (struct vnc*)g_malloc(sizeof(struct vnc), 1);
  /* set client functions */
  v->size = sizeof(struct vnc);
  v->handle = (int)v;
  v->mod_connect = lib_mod_connect;
  v->mod_start = lib_mod_start;
  v->mod_event = lib_mod_event;
  v->mod_signal = lib_mod_signal;
  v->mod_invalidate = lib_mod_invalidate;
  v->mod_end = lib_mod_end;
  v->mod_set_param = lib_mod_set_param;
  return (int)v;
}

/******************************************************************************/
int mod_exit(struct vnc* v)
{
  if (v == 0)
    return 0;
  g_tcp_close(v->sck);
  g_free(v);
  return 0;
}
