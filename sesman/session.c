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

#include <stdlib.h>

#include "sesman.h"

extern unsigned char g_fixedkey[8];
extern struct config_sesman g_cfg; /* config.h */
//extern int g_server_type;
#ifdef OLDSESSION
extern struct session_item g_session_items[100]; /* sesman.h */
#else
struct session_chain* g_sessions;
#endif
int g_session_count;

/******************************************************************************/
struct session_item* DEFAULT_CC
session_get_bydata(char* name, int width, int height, int bpp)
{
#ifdef OLDSESSION
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
#else
  struct session_chain* tmp;

  /*THREAD-FIX require chain lock */
  tmp=g_sessions;

  while (tmp != 0)
  {
    if (g_strncmp(name, tmp->item->name, 255) == 0 &&
        tmp->item->width == width &&
        tmp->item->height == height &&
        tmp->item->bpp == bpp)
    {
      /*THREAD-FIX release chain lock */
      return tmp->item;
    }
    tmp=tmp->next;   
  }
  
  /*THREAD-FIX release chain lock */
#endif
  return 0;
}

/******************************************************************************/
/* returns non zero if there is an xserver running on this display */
static int DEFAULT_CC
x_server_running(int display)
{
  char text[256];
  int x_running;
  int sck;

  g_sprintf(text, "/tmp/.X11-unix/X%d", display);
  x_running = g_file_exist(text);
  if (!x_running)
  {
    g_sprintf(text, "/tmp/.X%d-lock", display);
    x_running = g_file_exist(text);
  }
  if (!x_running) /* check 59xx */
  {
    sck = g_tcp_socket();
    g_sprintf(text, "59%2.2d", display);
    x_running = g_tcp_bind(sck, text);
    g_tcp_close(sck);
  }
  if (!x_running) /* check 60xx */
  {
    sck = g_tcp_socket();
    g_sprintf(text, "60%2.2d", display);
    x_running = g_tcp_bind(sck, text);
    g_tcp_close(sck);
  }
  if (!x_running) /* check 62xx */
  {
    sck = g_tcp_socket();
    g_sprintf(text, "62%2.2d", display);
    x_running = g_tcp_bind(sck, text);
    g_tcp_close(sck);
  }
  return x_running;
}

/******************************************************************************/
/* returns 0 if error else the display number the session was started on */
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data, unsigned char type)
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
#ifndef OLDSESSION
  struct session_chain* temp;
#endif

  /*THREAD-FIX lock to control g_session_count*/
  /* check to limit concurrent sessions */
  if (g_session_count >= g_cfg.sess.max_sessions)
  {
    log_message(LOG_LEVEL_INFO, "max concurrent session limit exceeded. login for user %s denied", username);
    return 0;
  }

#ifndef OLDSESSION
  temp=malloc(sizeof(struct session_chain));
  if (temp == 0)
  {
    log_message(LOG_LEVEL_ERROR, "cannot create new chain element - user %s", username);
    return 0;
  }
  temp->item = malloc(sizeof(struct session_item));
  if (temp->item == 0)
  {
    free(temp);
    log_message(LOG_LEVEL_ERROR, "cannot create new session item - user %s", username);
    return 0;
  }
#endif

  g_get_current_dir(cur_dir, 255);
  display = 10;
  /*while (x_server_running(display) && display < 50)*/
  /* we search for a free display up to max_sessions */
  /* we should need no more displays than this       */
  while (x_server_running(display))
  {
    display++;
    if ((display - 10) > g_cfg.sess.max_sessions || display >= 50)
    {
      return 0;
    }
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
        if (type == SESMAN_SESSION_TYPE_XVNC)
        {
          g_execlp11("Xvnc", "Xvnc", screen, "-geometry", geometry,
                     "-depth", depth, "-bs", "-rfbauth", passwd_file, 0);
        }
        else if (type == SESMAN_SESSION_TYPE_XRDP)
        {
          g_execlp11("Xrdp", "Xrdp", screen, "-geometry", geometry,
                     "-depth", depth, "-bs", 0, 0, 0);
        }
	else
        {
          log_message(LOG_LEVEL_ALWAYS, "bad session type - user %s - pid %d", username, g_getpid());
	  g_exit(1);
        }
        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS,"error doing execve for user %s - pid %d",username,g_getpid());
        g_exit(1);
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
#ifdef OLDSESSION
    g_session_items[display].pid = pid;
    g_strcpy(g_session_items[display].name, username);
    g_session_items[display].display = display;
    g_session_items[display].width = width;
    g_session_items[display].height = height;
    g_session_items[display].bpp = bpp;
    g_session_items[display].data = data;

    g_session_items[display].connect_time=g_time1();
    g_session_items[display].disconnect_time=(time_t) 0;
    g_session_items[display].idle_time=(time_t) 0;
   
    i/*if (type==0)
    {
      g_session_items[display].type=SESMAN_SESSION_TYPE_XVNC;
    }
    else
    {
      g_session_items[display].type=SESMAN_SESSION_TYPE_XRDP;
    }*/
    g_session_items[display].type=type;
    g_session_items[display].status=SESMAN_SESSION_STATUS_ACTIVE;
    
    g_session_count++;
#else
    temp->item->pid=pid;
    temp->item->display=display;
    temp->item->width=width;
    temp->item->height=height;
    temp->item->bpp=bpp;
    temp->item->data=data;
    g_strncpy(temp->item->name, username, 255);

    temp->item->connect_time=g_time1();
    temp->item->disconnect_time=(time_t) 0;
    temp->item->idle_time=(time_t) 0;

/*    if (type==0)
    {
      temp->item->type=SESMAN_SESSION_TYPE_XVNC;
    }
    else
    {
      temp->item->type=SESMAN_SESSION_TYPE_XRDP;
    }*/
	    
    temp->item->type=type;
    temp->item->status=SESMAN_SESSION_STATUS_ACTIVE;
    
    /*THREAD-FIX lock the chain*/
    temp->next=g_sessions;
    g_sessions=temp;
    g_session_count++;
    /*THERAD-FIX free the chain*/
#endif
    g_sleep(5000);
  }
  return display;
}

/*
SESMAN_SESSION_TYPE_XRDP  1
SESMAN_SESSION_TYPE_XVNC  2

SESMAN_SESSION_STATUS_ACTIVE        1
SESMAN_SESSION_STATUS_IDLE          2
SESMAN_SESSION_STATUS_DISCONNECTED  3

struct session_item
{
  char name[256];
  int pid; 
  int display;
  int width;
  int height;
  int bpp;
  long data;

  / *
  unsigned char status;
  unsigned char type;
  * / 
  
  / *
  time_t connect_time;
  time_t disconnect_time;
  time_t idle_time;
  * /
};

struct session_chain
{
  struct session_chain* next;
  struct session_item* item;
};
*/

#ifndef OLDSESSION

/******************************************************************************/
int DEFAULT_CC
session_kill(int pid)
{
  struct session_chain* tmp;
  struct session_chain* prev;
  
  /*THREAD-FIX require chain lock */
  tmp=g_sessions;
  prev=0;
  
  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!", pid);
      if (prev == 0)
      {
        /* prev does no exist, so it's the first element - so we set g_sessions */
	g_sessions = tmp->next;
      }
      else
      {
        prev->next = tmp->next;
      }
      /*THREAD-FIX release chain lock */
      return SESMAN_SESSION_KILL_NULLITEM;
    }

    if (tmp->item->pid == pid)
    {
      /* deleting the session */   
      log_message(LOG_LEVEL_INFO, "session %d - user %s - terminated", tmp->item->pid, tmp->item->name);
      free(tmp->item);
      if (prev == 0)
      {
        /* prev does no exist, so it's the first element - so we set g_sessions */
	g_sessions = tmp->next;
      }
      else
      {
        prev->next = tmp->next;
      }
      free(tmp);
      g_session_count--;
      /*THREAD-FIX release chain lock */
      return SESMAN_SESSION_KILL_OK;
    }
     
    /* go on */
    prev = tmp;
    tmp=tmp->next;
  }
  
  /*THREAD-FIX release chain lock */
  return SESMAN_SESSION_KILL_NOTFOUND;
}

/******************************************************************************/
struct session_item* DEFAULT_CC
session_get_bypid(int pid)
{
  struct session_chain* tmp;
  
  /*THREAD-FIX  require chain lock */
  tmp=g_sessions;
  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!", pid);
      /*THREAD-FIX release chain lock */
      return 0;
    }
    
    if (tmp->item->pid == pid)
    {
      /*THREAD-FIX release chain lock */
      return tmp->item;
    }

    /* go on */
    tmp=tmp->next;
  }
  
  /*THREAD-FIX release chain lock */
  return 0;
}

#endif

