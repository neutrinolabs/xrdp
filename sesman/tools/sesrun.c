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
*/

/**
 *
 * @file sesrun.c
 * @brief An utility to start a session
 * @author Jay Sorg, Simone Fedele
 * 
 */

#include "sesman.h"
#include "tcp.h"

int g_sck;
int g_pid;
struct config_sesman g_cfg; /* config.h */

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int sck;
  int code;
  int i;
  int size;
  int version;
  int width;
  int height;
  int bpp;
  int display;
  struct stream* in_s;
  struct stream* out_s;
  char* username;
  char* password;
  long data;

  if (0 != config_read(&g_cfg))
  {
    g_printf("sesrun: error reading config. quitting.\n");
    return 1;
  }

  g_pid = g_getpid();
  if (argc == 1)
  {
    g_printf("xrdp session starter v0.1\n");
    g_printf("\nusage:\n");
    g_printf("sesrun <server> <username> <password> <width> <height> <bpp>\n");
  }
  else if (argc == 7)
  {
    username = argv[2];
    password = argv[3];
    width = g_atoi(argv[4]);
    height = g_atoi(argv[5]);
    bpp = g_atoi(argv[6]);
    make_stream(in_s);
    init_stream(in_s, 8192);
    make_stream(out_s);
    init_stream(out_s, 8192);
    sck = g_tcp_socket();
    if (g_tcp_connect(sck, argv[1], "3350") == 0)
    {
      s_push_layer(out_s, channel_hdr, 8);
      out_uint16_be(out_s, 0); /* code */
      i = g_strlen(username);
      out_uint16_be(out_s, i);
      out_uint8a(out_s, username, i);
      i = g_strlen(password);
      out_uint16_be(out_s, i);
      out_uint8a(out_s, password, i);
      out_uint16_be(out_s, width);
      out_uint16_be(out_s, height);
      out_uint16_be(out_s, bpp);
      s_mark_end(out_s);
      s_pop_layer(out_s, channel_hdr);
      out_uint32_be(out_s, 0); /* version */
      out_uint32_be(out_s, out_s->end - out_s->data); /* size */
      tcp_force_send(sck, out_s->data, out_s->end - out_s->data);
      if (tcp_force_recv(sck, in_s->data, 8) == 0)
      {
        in_uint32_be(in_s, version);
        in_uint32_be(in_s, size);
        init_stream(in_s, 8192);
        if (tcp_force_recv(sck, in_s->data, size - 8) == 0)
        {
          if (version == 0)
          {
            in_uint16_be(in_s, code);
            if (code == 3)
            {
              in_uint16_be(in_s, data);
              in_uint16_be(in_s, display);
              g_printf("ok %d display %d\n", data, display);
            }
          }
        }
      }
    }
    else
    {
      g_printf("connect error\n");
    }
    g_tcp_close(sck);
    free_stream(in_s);
    free_stream(out_s);
  }
  return 0;
}
