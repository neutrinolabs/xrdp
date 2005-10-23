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

extern unsigned char g_fixedkey[8];
extern struct session_item g_session_items[100]; /* sesman.h */
extern struct config_sesman g_cfg; /* config.h */

/******************************************************************************/
struct session_item* DEFAULT_CC
session_find_item(char* name, int width, int height, int bpp)
{
  int i;

  for (i = 0; i < 100; i++)
  {
    if (g_strncmp(name, g_session_items[i].name, 255) == 0 &&
        g_session_items[i].width == width &&
        g_session_items[i].height == height &&
        g_session_items[i].bpp == bpp)
    {
      return g_session_items + i;
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
/* returns 0 if error else the display number the session was started on */
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data)
{
  int display;
  int pid;
  int wmpid;
  int xpid;
  char geometry[32];
  char depth[32];
  char screen[32];
  char cur_dir[256];
  char text[256];
  char passwd_file[256];

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
  wmpid = 0;
  pid = g_fork();
  if (pid == -1)
  {
  }
  else if (pid == 0) /* child */
  {
    g_unset_signals();
    auth_start_session(data, display);
    g_sprintf(geometry, "%dx%d", width, height);
    g_sprintf(depth, "%d", bpp);
    g_sprintf(screen, ":%d", display);
    wmpid = g_fork();
    if (wmpid == -1)
    {
    }
    else if (wmpid == 0) /* child */
    {
      /* give X a bit to start */
      g_sleep(1000);
      env_set_user(username, 0, display);
      if (x_server_running(display))
      {
        auth_set_env(data);
        /* try to execute user window manager if enabled */
        if (g_cfg.enable_user_wm)
        {
          g_sprintf(text,"%s/%s", g_getenv("HOME"), g_cfg.user_wm);
          if (g_file_exist(text))
          {
            g_execlp3(text, g_cfg.user_wm, 0);
          }
        }
        /* if we're here something happened to g_execlp3
           so we try running the default window manager */
        g_sprintf(text, "%s/%s", cur_dir, g_cfg.default_wm);
        g_execlp3(text, g_cfg.default_wm, 0);
	
        /* still a problem starting window manager just start xterm */
        g_execlp3("xterm", "xterm", 0);
        /* should not get here */
      }
      log_message(LOG_LEVEL_ALWAYS,"error starting window manager %s - pid %d", username, g_getpid());
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
        env_set_user(username, passwd_file, display);
        env_check_password_file(passwd_file, password);
        g_execlp11("Xvnc", "Xvnc", screen, "-geometry", geometry,
                   "-depth", depth, "-bs", "-rfbauth", passwd_file, 0);
	
        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS,"error doing execve for user %s - pid %d",username,g_getpid());
        g_exit(0);
      }
      else /* parent */
      {
        g_waitpid(wmpid);
        g_sigterm(xpid);
        g_sigterm(wmpid);
        g_sleep(1000);
        auth_end(data);
        g_exit(0);
      }
    }
  }
  else /* parent */
  {
    g_session_items[display].pid = pid;
    g_strcpy(g_session_items[display].name, username);
    g_session_items[display].display = display;
    g_session_items[display].width = width;
    g_session_items[display].height = height;
    g_session_items[display].bpp = bpp;
    g_session_items[display].data = data;
    g_sleep(5000);
  }
  return display;
}

