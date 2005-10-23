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
#include <stdio.h>

int g_sck;
int g_pid;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct session_item g_session_items[100]; /* sesman.h */
struct config_sesman g_cfg; /* config.h */

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
  char user[256];
  char pass[256];
  struct session_item* s_item;
  long data;

  if (0 != config_read(&g_cfg))
  {
    g_printf("error reading config. quitting.\n\r");
    return 1;
  }
  
  error = log_start(g_cfg.log.program_name, g_cfg.log.log_file, g_cfg.log.log_level, 
                    g_cfg.log.enable_syslog, g_cfg.log.syslog_level);
  
  if (error != LOG_STARTUP_OK)
  {
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot start logging. quitting.\n\r");
      case LOG_ERROR_FILE_OPEN:
        g_printf("error opening log file. quitting.\n\r");
    }
    return 1;
  }
  
  /* start of daemonizing code */
  g_pid = g_fork();

  if (0!=g_pid)
  {
    g_exit(0);
  }
  
  g_file_close(0);
  g_file_close(1);
  g_file_close(2);

  g_file_open("/dev/null");
  g_file_open("/dev/null");
  g_file_open("/dev/null");
  /* end of daemonizing code */
  
  g_memset(&g_session_items, 0, sizeof(g_session_items));
  g_pid = g_getpid();
  g_signal(1, sig_sesman_reload_cfg); /* SIGHUP  */
  g_signal(2, sig_sesman_shutdown);   /* SIGINT  */
  g_signal(9, sig_sesman_shutdown);   /* SIGKILL */
  g_signal(15, sig_sesman_shutdown);  /* SIGTERM */
  g_signal_child_stop(cterm);         /* SIGCHLD */
  
  /*main program loop*/
  make_stream(in_s);
  init_stream(in_s, 8192);
  make_stream(out_s);
  init_stream(out_s, 8192);
  
  log_message(LOG_LEVEL_INFO, "listening...");
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
      log_message(LOG_LEVEL_ERROR, "listen error");
    }
  }
  else
  {
    log_message(LOG_LEVEL_ERROR, "bind error");
  }
  g_tcp_close(g_sck);
  free_stream(in_s);
  free_stream(out_s);
    
  return 0;
}
