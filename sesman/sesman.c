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

#include "d3des.h"

#include "arch.h"
#include "parse.h"
#include "os_calls.h"

long DEFAULT_CC
auth_userpass(char* user, char* pass);
int DEFAULT_CC
auth_start_session(long in_val);
int DEFAULT_CC
auth_end(long in_val);

static int g_sck;
static int g_pid;

struct session_item
{
  char name[256];
  int pid; /* pid of sesman waiting for wm to end */
  int display;
  int width;
  int height;
  int bpp;
  long data;
};

static unsigned char s_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };

static struct session_item session_items[100];

/*****************************************************************************/
static int DEFAULT_CC
tcp_force_recv(int sck, char* data, int len)
{
  int rcvd;

  while (len > 0)
  {
    rcvd = g_tcp_recv(sck, data, len, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(sck))
      {
        g_sleep(1);
      }
      else
      {
        return 1;
      }
    }
    else if (rcvd == 0)
    {
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
static int DEFAULT_CC
tcp_force_send(int sck, char* data, int len)
{
  int sent;

  while (len > 0)
  {
    sent = g_tcp_send(sck, data, len, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(sck))
      {
        g_sleep(1);
      }
      else
      {
        return 1;
      }
    }
    else if (sent == 0)
    {
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
static struct session_item* DEFAULT_CC
find_session_item(char* name, int width, int height, int bpp)
{
  int i;

  for (i = 0; i < 100; i++)
  {
    if (g_strcmp(name, session_items[i].name) == 0 &&
        session_items[i].width == width &&
        session_items[i].height == height &&
        session_items[i].bpp == bpp)
    {
      return session_items + i;
    }
  }
  return 0;
}

/******************************************************************************/
/* returns non zero if there is an xserver running on this display */
static int DEFAULT_CC
x_server_running(int display)
{
  char text[256];

  g_sprintf(text, "/tmp/.X11-unix/X%d", display);
  return g_file_exist(text);
}

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
      if (session_items[i].pid == pid)
      {
        auth_end(session_items[i].data);
        g_memset(session_items + i, 0, sizeof(struct session_item));
      }
    }
  }
}

/******************************************************************************/
static int DEFAULT_CC
check_password_file(char* filename, char* password)
{
  char encryptedPasswd[16];
  int fd;

  g_memset(encryptedPasswd, 0, 16);
  g_strncpy(encryptedPasswd, password, 8);
  rfbDesKey(s_fixedkey, 0);
  rfbDes(encryptedPasswd, encryptedPasswd);
  fd = g_file_open(filename);
  if (fd == 0)
  {
    return 1;
  }
  g_file_write(fd, encryptedPasswd, 8);
  g_file_close(fd);
  g_set_file_rights(filename, 1, 1); /* set read and write flags */
  return 0;
}

/******************************************************************************/
static int DEFAULT_CC
start_session(int width, int height, int bpp, char* username, char* password,
              long data)
{
  int display;
  int pid;
  int uid;
  int wmpid;
  int xpid;
  int error;
  int pw_uid;
  int pw_gid;
  char pw_gecos[256];
  char pw_dir[256];
  char pw_shell[256];
  char text[256];
  char passwd_file[256];
  char geometry[32];
  char depth[32];
  char screen[32];
  char cur_dir[256];

  g_get_current_dir(cur_dir, 255);
  display = 10;
  while (x_server_running(display) && display < 50)
  {
    display++;
  }
  if (display >= 50)
  {
    return 0;
  }
  auth_start_session(data);
  wmpid = 0;
  pid = g_fork();
  if (pid == -1)
  {
  }
  else if (pid == 0) /* child */
  {
    error = g_getuser_info(username, &pw_gid, &pw_uid, pw_shell, pw_dir,
                           pw_gecos);
    if (error == 0)
    {
      error = g_setgid(pw_gid);
      if (error == 0)
      {
        uid = pw_uid;
        error = g_setuid(uid);
      }
      if (error == 0)
      {
        g_clearenv();
        g_setenv("SHELL", pw_shell, 1);
        g_setenv("PATH", "/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin", 1);
        g_setenv("USER", username, 1);
        g_sprintf(text, "%d", uid);
        g_setenv("UID", text, 1);
        g_setenv("HOME", pw_dir, 1);
        g_set_current_dir(pw_dir);
        g_sprintf(text, ":%d.0", display);
        g_setenv("DISPLAY", text, 1);
        g_sprintf(geometry, "%dx%d", width, height);
        g_sprintf(depth, "%d", bpp);
        g_sprintf(screen, ":%d", display);
        g_mkdir(".vnc");
        g_sprintf(passwd_file, "%s/.vnc/sesman_passwd", pw_dir);
        check_password_file(passwd_file, password);
        wmpid = g_fork();
        if (wmpid == -1)
        {
        }
        else if (wmpid == 0) /* child */
        {
          /* give X a bit to start */
          g_sleep(500);
          if (x_server_running(display))
          {
            g_sprintf(text, "%s/startwm.sh", cur_dir);
            g_execlp3(text, "startwm.sh", 0);
            /* should not get here */
          }
          g_printf("error\n");
          g_exit(0);
        }
        else /* parent */
        {
          xpid = g_fork();
          if (xpid == -1)
          {
          }
          else if (xpid == 0) /* child */
          {
            g_execlp11("Xvnc", "Xvnc", screen, "-geometry", geometry,
                       "-depth", depth, "-bs", "-rfbauth", passwd_file, 0);
            /* should not get here */
            g_printf("error\n");
            g_exit(0);
          }
          else /* parent */
          {
            g_waitpid(wmpid);
            g_sigterm(xpid);
            g_sigterm(wmpid);
            g_sleep(1000);
            g_exit(0);
          }
        }
      }
    }
  }
  else /* parent */
  {
    session_items[display].pid = pid;
    g_strcpy(session_items[display].name, username);
    session_items[display].display = display;
    session_items[display].width = width;
    session_items[display].height = height;
    session_items[display].bpp = bpp;
    session_items[display].data = data;
    g_sleep(5000);
  }
  return display;
}

/******************************************************************************/
static void DEFAULT_CC
sesman_shutdown(int sig)
{
  if (g_getpid() != g_pid)
  {
    return;
  }
  g_printf("shutting down\n\r");
  g_printf("signal %d pid %d\n\r", sig, g_getpid());
  g_tcp_close(g_sck);
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

  g_memset(&session_items, 0, sizeof(session_items));
  g_pid = g_getpid();
  g_signal(2, sesman_shutdown); /* SIGINT */
  g_signal(9, sesman_shutdown); /* SIGKILL */
  g_signal(15, sesman_shutdown); /* SIGTERM */
  g_signal_child_stop(cterm); /* SIGCHLD */
  if (argc == 1)
  {
    g_printf("xrdp session manager v0.1\n");
    g_printf("usage\n");
    g_printf("sesman wait - wait for connection\n");
    g_printf("sesman server username password width height bpp - \
start session\n");
  }
  else if (argc == 2 && g_strcmp(argv[1], "wait") == 0)
  {
    make_stream(in_s);
    init_stream(in_s, 8192);
    make_stream(out_s);
    init_stream(out_s, 8192);
    g_printf("listening\n");
    g_sck = g_tcp_socket();
    g_tcp_set_non_blocking(g_sck);
    error = g_tcp_bind(g_sck, "3350");
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
                    s_item = find_session_item(user, width, height, bpp);
                    if (s_item != 0)
                    {
                      display = s_item->display;
                      auth_end(data);
                      /* don't set data to null here */
                    }
                    else
                    {
                      display = start_session(width, height, bpp, user, pass,
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
