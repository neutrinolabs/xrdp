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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <grp.h>

#include "d3des.h"

#include <security/pam_userpass.h>

#include "arch.h"
#include "parse.h"
#include "os_calls.h"

#define SERVICE "xrdp"

struct session_item
{
  char name[256];
  int pid; // pid of sesman waiting for wm to end
  int display;
  int width;
  int height;
  int bpp;
};

static unsigned char s_fixedkey[8] = {23, 82, 107, 6, 35, 78, 88, 7};

struct session_item session_items[100];

/*****************************************************************************/
int tcp_force_recv(int sck, char* data, int len)
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
int tcp_force_send(int sck, char* data, int len)
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
struct session_item* find_session_item(char* name, int width,
                                       int height, int bpp)
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
struct session_item* find_session_item_by_name(char* name)
{
  int i;

  for (i = 0; i < 100; i++)
  {
    if (g_strcmp(name, session_items[i].name) == 0)
    {
      return session_items + i;
    }
  }
  return 0;
}

/******************************************************************************/
struct session_item* find_session_item_by_display(int display)
{
  int i;

  for (i = 0; i < 100; i++)
  {
    if (session_items[i].display == display)
    {
      return session_items + i;
    }
  }
  return 0;
}

/******************************************************************************/
int x_server_running(int display)
{
  char text[256];

  g_sprintf(text, "/tmp/.X11-unix/X%d", display);
  return access(text, F_OK) == 0;
}

/******************************************************************************/
/* returns boolean */
int auth_pam_userpass(const char* user, const char* pass)
{
  pam_handle_t* pamh;
  pam_userpass_t userpass;
  struct pam_conv conv = {pam_userpass_conv, &userpass};
  const void* template1;
  int status;

  userpass.user = user;
  userpass.pass = pass;
  if (pam_start(SERVICE, user, &conv, &pamh) != PAM_SUCCESS)
  {
    return 0;
  }
  status = pam_authenticate(pamh, 0);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  status = pam_acct_mgmt(pamh, 0);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  status = pam_get_item(pamh, PAM_USER, &template1);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  if (pam_end(pamh, PAM_SUCCESS) != PAM_SUCCESS)
  {
    return 0;
  }
  return 1;
}

/******************************************************************************/
void cterm(int s)
{
  int i;
  int pid;
  int wstat;

  pid = waitpid(0, &wstat, WNOHANG);
  if (pid > 0)
  {
    for (i = 0; i < 100; i++)
    {
      if (session_items[i].pid == pid)
      {
        g_memset(session_items + i, 0, sizeof(struct session_item));
      }
    }
  }
}

/******************************************************************************/
/* ge the next available X display */
int get_next_display(void)
{
  int i;

  for (i = 10; i < 100; i++)
  {
    if (!x_server_running(i))
    {
      return i;
    }
  }
  return -1;
}

/******************************************************************************/
int check_password_file(char* filename, char* password)
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
  chmod(filename, S_IRUSR | S_IWUSR);
  return 0;
}

/******************************************************************************/
int start_session(int width, int height, int bpp, char* username,
                  char* password)
{
  int display;
  int pid;
  int uid;
  int wmpid;
  int xpid;
  int error;
  struct passwd* pwd_1;
  char text[256];
  char passwd_file[256];
  char geometry[32];
  char depth[32];
  char screen[32];
  char cur_dir[256];

  getcwd(cur_dir, 255);
  display = 10;
  while (x_server_running(display) && display < 50)
  {
    display++;
  }
  if (display >= 50)
  {
    return 0;
  }
  wmpid = 0;
  pid = fork();
  if (pid == -1)
  {
  }
  else if (pid == 0) // child
  {
    pwd_1 = getpwnam(username);
    if (pwd_1 != 0)
    {
      /* set uid and groups */
      error = initgroups(pwd_1->pw_name, pwd_1->pw_gid);
      if (error == 0)
      {
        error = setgid(pwd_1->pw_gid);
      }
      if (error == 0)
      {
        uid = pwd_1->pw_uid;
        error = setuid(uid);
      }
      if (error == 0)
      {
        clearenv();
        setenv("PATH", "/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin", 1);
        setenv("USER", username, 1);
        g_sprintf(text, "%d", uid);
        setenv("UID", text, 1);
        setenv("HOME", pwd_1->pw_dir, 1);
        chdir(pwd_1->pw_dir);
        g_sprintf(text, ":%d.0", display);
        setenv("DISPLAY", text, 1);
        g_sprintf(geometry, "%dx%d", width, height);
        g_sprintf(depth, "%d", bpp);
        g_sprintf(screen, ":%d", display);
        mkdir(".vnc", S_IRWXU);
        g_sprintf(passwd_file, "%s/.vnc/sesman_passwd", pwd_1->pw_dir);
        check_password_file(passwd_file, password);
        wmpid = fork();
        if (wmpid == -1)
        {
        }
        else if (wmpid == 0) // child
        {
          // give X a bit to start
          g_sleep(500);
          if (x_server_running(display))
          {
            g_sprintf(text, "%s/startwm.sh", cur_dir);
            execlp(text, "startwm.sh", NULL);
            // should not get here
          }
          g_printf("error\n");
          _exit(0);
        }
        else // parent
        {
          xpid = fork();
          if (xpid == -1)
          {
          }
          else if (xpid == 0) // child
          {
            execlp("Xvnc", "Xvnc", screen, "-geometry", geometry,
                   "-depth", depth, "-bs", "-rfbauth", passwd_file,
                   NULL);
            // should not get here
            g_printf("error\n");
            _exit(0);
          }
          else // parent
          {
            waitpid(wmpid, 0, 0);
            kill(xpid, SIGTERM);
            kill(wmpid, SIGTERM);
            _exit(0);
          }
        }
      }
    }
  }
  else // parent
  {
    signal(SIGCHLD, cterm);
    session_items[display].pid = pid;
    g_strcpy(session_items[display].name, username);
    session_items[display].display = display;
    session_items[display].width = width;
    session_items[display].height = height;
    session_items[display].bpp = bpp;
    g_sleep(5000);
  }
  return display;
}

/******************************************************************************/
int main(int argc, char** argv)
{
  int sck;
  int in_sck;
  int code;
  int i;
  int size;
  int version;
  int ok;
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

  g_memset(&session_items, 0, sizeof(session_items));
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
    sck = g_tcp_socket();
    g_tcp_set_non_blocking(sck);
    error = g_tcp_bind(sck, "3350");
    if (error == 0)
    {
      error = g_tcp_listen(sck);
      if (error == 0)
      {
        in_sck = g_tcp_accept(sck);
        while (in_sck == -1 && g_tcp_last_error_would_block(sck))
        {
          g_sleep(1000);
          in_sck = g_tcp_accept(sck);
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
                if (code == 0) // check username - password, start session
                {
                  in_uint16_be(in_s, i);
                  in_uint8a(in_s, user, i);
                  user[i] = 0;
                  //g_printf("%s\n", user);
                  in_uint16_be(in_s, i);
                  in_uint8a(in_s, pass, i);
                  pass[i] = 0;
                  //g_printf("%s\n", pass);
                  in_uint16_be(in_s, width);
                  in_uint16_be(in_s, height);
                  in_uint16_be(in_s, bpp);
                  //g_printf("%d %d %d\n", width, height, bpp);
                  ok = auth_pam_userpass(user, pass);
                  display = 0;
                  if (ok)
                  {
                    s_item = find_session_item(user, width, height, bpp);
                    if (s_item != 0)
                    {
                      display = s_item->display;
                    }
                    else
                    {
                      display = start_session(width, height, bpp, user, pass);
                    }
                    if (display == 0)
                    {
                      ok = 0;
                    }
                  }
                  init_stream(out_s, 8192);
                  out_uint32_be(out_s, 0); // version
                  out_uint32_be(out_s, 14); // size
                  out_uint16_be(out_s, 3); // cmd
                  out_uint16_be(out_s, ok); // data
                  out_uint16_be(out_s, display); // data
                  s_mark_end(out_s);
                  tcp_force_send(in_sck, out_s->data,
                                 out_s->end - out_s->data);
                }
              }
            }
          }
          close(in_sck);
          in_sck = g_tcp_accept(sck);
          while (in_sck == -1 && g_tcp_last_error_would_block(sck))
          {
            g_sleep(1000);
            in_sck = g_tcp_accept(sck);
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
    g_tcp_close(sck);
    free_stream(in_s);
    free_stream(out_s);
  }
  else if (argc == 7)
  {
    username = argv[2];
    password = argv[3];
    width = atoi(argv[4]);
    height = atoi(argv[5]);
    bpp = atoi(argv[6]);
    make_stream(in_s);
    init_stream(in_s, 8192);
    make_stream(out_s);
    init_stream(out_s, 8192);
    sck = g_tcp_socket();
    if (g_tcp_connect(sck, argv[1], "3350") == 0)
    {
      s_push_layer(out_s, channel_hdr, 8);
      out_uint16_be(out_s, 0); // code
      i = g_strlen(username);
      out_uint16_be(out_s, i);
      out_uint8a(out_s, username, i);
      i = g_strlen(password);
      out_uint16_be(out_s, i);
      out_uint8a(out_s, password, i);
      //g_printf("%d\n", width);
      out_uint16_be(out_s, width);
      out_uint16_be(out_s, height);
      out_uint16_be(out_s, bpp);
      s_mark_end(out_s);
      s_pop_layer(out_s, channel_hdr);
      out_uint32_be(out_s, 0); // version
      out_uint32_be(out_s, out_s->end - out_s->data); // size
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
              in_uint16_be(in_s, ok);
              in_uint16_be(in_s, display);
              g_printf("ok %d display %d\n", ok, display);
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
