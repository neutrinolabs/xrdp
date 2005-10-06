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
   Copyright (C) Jay Sorg 2005

   session manager
   linux only

*/

#include "sesman.h"

int g_sck;
int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct session_item g_session_items[100]; /* sesman.h */
struct sesman_config g_cfg; /* config.h */

/******************************************************************************/
static void DEFAULT_CC
cterm(int s)
{
  int i;
  int pid;

  if (g_getpid() != g_pid)
  {
    return;
  }
  pid = g_waitchild();
  if (pid > 0)
  {
    for (i = 0; i < 100; i++)
    {
      if (g_session_items[i].pid == pid)
      {
        g_memset(g_session_items + i, 0, sizeof(struct session_item));
      }
    }
  }
}

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int sck;
  int in_sck;
  int code;
  int i;
  int size;
  int version;
  int width;
  int height;
  int bpp;
  int display;
  int error;
  struct stream* in_s;
  struct stream* out_s;
  char* username;
  char* password;
  char user[256];
  char pass[256];
  struct session_item* s_item;
  long data;

  if (0 != config_read(&g_cfg))
  {
    g_printf("sesman: error reading config. quitting.\n\r");
    return 1;
  }
  g_memset(&g_session_items, 0, sizeof(g_session_items));
  g_pid = g_getpid();
  g_signal(1, sig_sesman_reload_cfg); /* SIGHUP */
  g_signal(2, sig_sesman_shutdown); /* SIGINT */
  g_signal(9, sig_sesman_shutdown); /* SIGKILL */
  g_signal(15, sig_sesman_shutdown); /* SIGTERM */
  g_signal_child_stop(cterm); /* SIGCHLD */
  if (argc == 1)
  {
    g_printf("xrdp session manager v0.1\n");
    g_printf("usage\n");
    g_printf("sesman wait - wait for connection\n");
    g_printf("sesman server username password width height bpp - \
start session\n");
  }
  else if (argc == 2 && g_strncmp(argv[1], "wait", 255) == 0)
  {
    make_stream(in_s);
    init_stream(in_s, 8192);
    make_stream(out_s);
    init_stream(out_s, 8192);
    g_printf("listening\n");
    g_sck = g_tcp_socket();
    g_tcp_set_non_blocking(g_sck);
    error = g_tcp_bind(g_sck, g_cfg.listen_port);
    if (error == 0)
    {
      error = g_tcp_listen(g_sck);
      if (error == 0)
      {
        in_sck = g_tcp_accept(g_sck);
        while (in_sck == -1 && g_tcp_last_error_would_block(g_sck))
        {
          g_sleep(1000);
          in_sck = g_tcp_accept(g_sck);
        }
        while (in_sck > 0)
        {
          init_stream(in_s, 8192);
          if (tcp_force_recv(in_sck, in_s->data, 8) == 0)
          {
            in_uint32_be(in_s, version);
            in_uint32_be(in_s, size);
            init_stream(in_s, 8192);
            if (tcp_force_recv(in_sck, in_s->data, size - 8) == 0)
            {
              if (version == 0)
              {
                in_uint16_be(in_s, code);
                if (code == 0) /* check username - password, start session */
                {
                  in_uint16_be(in_s, i);
                  in_uint8a(in_s, user, i);
                  user[i] = 0;
                  in_uint16_be(in_s, i);
                  in_uint8a(in_s, pass, i);
                  pass[i] = 0;
                  in_uint16_be(in_s, width);
                  in_uint16_be(in_s, height);
                  in_uint16_be(in_s, bpp);
                  data = auth_userpass(user, pass);
                  display = 0;
                  if (data)
                  {
                    s_item = session_find_item(user, width, height, bpp);
                    if (s_item != 0)
                    {
                      display = s_item->display;
                      auth_end(data);
                      /* don't set data to null here */
                    }
                    else
                    {
                      display = session_start(width, height, bpp, user, pass,
                                              data);
                    }
                    if (display == 0)
                    {
                      auth_end(data);
                      data = 0;
                    }
                  }
                  init_stream(out_s, 8192);
                  out_uint32_be(out_s, 0); /* version */
                  out_uint32_be(out_s, 14); /* size */
                  out_uint16_be(out_s, 3); /* cmd */
                  out_uint16_be(out_s, data != 0); /* data */
                  out_uint16_be(out_s, display); /* data */
                  s_mark_end(out_s);
                  tcp_force_send(in_sck, out_s->data,
                                 out_s->end - out_s->data);
                }
              }
            }
          }
          g_tcp_close(in_sck);
          in_sck = g_tcp_accept(g_sck);
          while (in_sck == -1 && g_tcp_last_error_would_block(g_sck))
          {
            g_sleep(1000);
            in_sck = g_tcp_accept(g_sck);
          }
        }
      }
      else
      {
        g_printf("listen error\n");
      }
    }
    else
    {
      g_printf("bind error\n");
    }
    g_tcp_close(g_sck);
    free_stream(in_s);
    free_stream(out_s);
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
