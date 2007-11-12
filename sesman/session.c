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
 * @file session.c
 * @brief Session management code
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"
#include "libscp_types.h"

#include "errno.h"

extern unsigned char g_fixedkey[8];
extern struct config_sesman g_cfg; /* config.h */
struct session_chain* g_sessions;
int g_session_count;

/******************************************************************************/
struct session_item* DEFAULT_CC
session_get_bydata(char* name, int width, int height, int bpp)
{
  struct session_chain* tmp;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp = g_sessions;

  while (tmp != 0)
  {
    if (g_strncmp(name, tmp->item->name, 255) == 0 &&
        tmp->item->width == width &&
        tmp->item->height == height &&
        tmp->item->bpp == bpp)
    {
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return tmp->item;
    }
    tmp = tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
  return 0;
}

/******************************************************************************/
/**
 *
 * @brief checks if there's a server running on a display
 * @param display the display to check
 * @return 0 if there isn't a display running, nonzero otherwise
 *
 */
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

static void DEFAULT_CC
session_start_sessvc(int xpid, int wmpid, long data)
{
  struct list* sessvc_params;
  char wmpid_str[25];
  char xpid_str[25];
  char exe_path[262];
  char cur_dir[256];
  int i;

  /* new style waiting for clients */
  g_sprintf(wmpid_str, "%d", wmpid);
  g_sprintf(xpid_str, "%d",  xpid);
  log_message(LOG_LEVEL_INFO, "starting sessvc - xpid=%s - wmpid=%s",xpid_str, wmpid_str);

  sessvc_params = list_create();
  sessvc_params->auto_free = 1;

  /* building parameters */
  g_get_current_dir(cur_dir, 255);
  g_snprintf(exe_path, 261, "%s/%s", cur_dir, "sessvc");

  list_add_item(sessvc_params, (long)g_strdup(exe_path));
  list_add_item(sessvc_params, (long)g_strdup(xpid_str));
  list_add_item(sessvc_params, (long)g_strdup(wmpid_str));
  list_add_item(sessvc_params, 0); /* mandatory */

  /* executing sessvc */
  g_execvp(exe_path, ((char**)sessvc_params->items));

  /* should not get here */
  log_message(LOG_LEVEL_ALWAYS, "error starting sessvc - pid %d - xpid=%s - wmpid=%s",
              g_getpid(), xpid_str, wmpid_str);

  /* logging parameters */
  /* no problem calling strerror for thread safety: other threads are blocked */
  log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s", errno, g_get_strerror());
  log_message(LOG_LEVEL_DEBUG,"execve parameter list:");
  for (i=0; i < (sessvc_params->count); i++)
  {
    log_message(LOG_LEVEL_DEBUG, "        argv[%d] = %s", i, (char*)list_get_item(sessvc_params, i));
  }
  list_delete(sessvc_params);

  /* keep the old waitpid if some error occurs during execlp */
  g_waitpid(wmpid);
  g_sigterm(xpid);
  g_sigterm(wmpid);
  g_sleep(1000);
  auth_end(data);
  g_exit(0);
}

/******************************************************************************/
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data, unsigned char type)
{
  int display;
  int pid;
  int wmpid;
  int xpid;
  int i;
  char geometry[32];
  char depth[32];
  char screen[32];
  char cur_dir[256];
  char text[256];
  char passwd_file[256];
  char** pp1;
  struct session_chain* temp;
  struct list* xserver_params=0;

  /*THREAD-FIX lock to control g_session_count*/
  lock_chain_acquire();
  /* check to limit concurrent sessions */
  if (g_session_count >= g_cfg.sess.max_sessions)
  {
    /*THREAD-FIX unlock chain*/
    lock_chain_release();
    log_message(LOG_LEVEL_INFO, "max concurrent session limit exceeded. login \
for user %s denied", username);
    return 0;
  }

  /*THREAD-FIX unlock chain*/
  lock_chain_release();

  temp = (struct session_chain*)g_malloc(sizeof(struct session_chain), 0);
  if (temp == 0)
  {
    log_message(LOG_LEVEL_ERROR, "cannot create new chain element - user %s",
                username);
    return 0;
  }
  temp->item = (struct session_item*)g_malloc(sizeof(struct session_item), 0);
  if (temp->item == 0)
  {
    g_free(temp);
    log_message(LOG_LEVEL_ERROR, "cannot create new session item - user %s",
                username);
    return 0;
  }

  g_get_current_dir(cur_dir, 255);
  display = 10;

  /*while (x_server_running(display) && display < 50)*/
  /* we search for a free display up to max_sessions */
  /* we should need no more displays than this       */

  /* block all the threads running to enable forking */
  scp_lock_fork_request();
  while (x_server_running(display))
  {
    display++;
    if (((display - 10) > g_cfg.sess.max_sessions) || (display >= 50))
    {
      return 0;
    }
  }
  wmpid = 0;
  pid = g_fork();
  if (pid == -1)
  {
  }
  else if (pid == 0) /* child sesman */
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
    else if (wmpid == 0) /* child (child sesman) xserver */
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
            log_message(LOG_LEVEL_ALWAYS,"error starting user wm for user %s - pid %d",
                        username, g_getpid());
            /* logging parameters */
            /* no problem calling strerror for thread safety: other threads are blocked */
            log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s", errno,
                        g_get_strerror());
            log_message(LOG_LEVEL_DEBUG,"execlp3 parameter list:");
            log_message(LOG_LEVEL_DEBUG, "        argv[0] = %s", text);
            log_message(LOG_LEVEL_DEBUG, "        argv[1] = %s", g_cfg.user_wm);
          }
        }
        /* if we're here something happened to g_execlp3
           so we try running the default window manager */
        g_sprintf(text, "%s/%s", cur_dir, g_cfg.default_wm);
        g_execlp3(text, g_cfg.default_wm, 0);

        log_message(LOG_LEVEL_ALWAYS,"error starting default wm for user %s - pid %d",
                    username, g_getpid());
        /* logging parameters */
        /* no problem calling strerror for thread safety: other threads are blocked */
        log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s", errno,
                    g_get_strerror());
        log_message(LOG_LEVEL_DEBUG,"execlp3 parameter list:");
        log_message(LOG_LEVEL_DEBUG, "        argv[0] = %s", text);
        log_message(LOG_LEVEL_DEBUG, "        argv[1] = %s", g_cfg.default_wm);

        /* still a problem starting window manager just start xterm */
        g_execlp3("xterm", "xterm", 0);

        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS,"error starting xterm for user %s - pid %d",
                    username, g_getpid());
        /* logging parameters */
        /* no problem calling strerror for thread safety: other threads are blocked */
        log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s", errno, g_get_strerror());
      }
      else
      {
        log_message(LOG_LEVEL_ERROR, "another Xserver is already active on display %d", display);
      }
      log_message(LOG_LEVEL_DEBUG,"aborting connection...");
      g_exit(0);
    }
    else /* parent (child sesman) */
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
          xserver_params = list_create();
          xserver_params->auto_free = 1;
          /* these are the must have parameters */
          list_add_item(xserver_params, (long)g_strdup("Xvnc"));
          list_add_item(xserver_params, (long)g_strdup(screen));
          list_add_item(xserver_params, (long)g_strdup("-geometry"));
          list_add_item(xserver_params, (long)g_strdup(geometry));
          list_add_item(xserver_params, (long)g_strdup("-depth"));
          list_add_item(xserver_params, (long)g_strdup(depth));
          list_add_item(xserver_params, (long)g_strdup("-rfbauth"));
          list_add_item(xserver_params, (long)g_strdup(passwd_file));

          /* additional parameters from sesman.ini file */
          //config_read_xserver_params(SESMAN_SESSION_TYPE_XVNC,
          //                           xserver_params);
          list_append_list_strdup(g_cfg.vnc_params, xserver_params, 0);

          /* make sure it ends with a zero */
          list_add_item(xserver_params, 0);
          pp1 = (char**)xserver_params->items;
          g_execvp("Xvnc", pp1);
        }
        else if (type == SESMAN_SESSION_TYPE_XRDP)
        {
          xserver_params = list_create();
          xserver_params->auto_free = 1;
          /* these are the must have parameters */
          list_add_item(xserver_params, (long)g_strdup("X11rdp"));
          list_add_item(xserver_params, (long)g_strdup(screen));
          list_add_item(xserver_params, (long)g_strdup("-geometry"));
          list_add_item(xserver_params, (long)g_strdup(geometry));
          list_add_item(xserver_params, (long)g_strdup("-depth"));
          list_add_item(xserver_params, (long)g_strdup(depth));

          /* additional parameters from sesman.ini file */
          //config_read_xserver_params(SESMAN_SESSION_TYPE_XRDP,
          //                           xserver_params);
	  list_append_list_strdup(g_cfg.rdp_params, xserver_params, 0);

          /* make sure it ends with a zero */
          list_add_item(xserver_params, 0);
          pp1 = (char**)xserver_params->items;
          g_execvp("X11rdp", pp1);
        }
        else
        {
          log_message(LOG_LEVEL_ALWAYS, "bad session type - user %s - pid %d",
                      username, g_getpid());
          g_exit(1);
        }

        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS, "error starting X server - user %s - pid %d",
                    username, g_getpid());

        /* logging parameters */
        /* no problem calling strerror for thread safety: other threads are blocked */
        log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s", errno, g_get_strerror());
        log_message(LOG_LEVEL_DEBUG, "execve parameter list: %d", (xserver_params)->count);

        for (i=0; i<(xserver_params->count); i++)
        {
          log_message(LOG_LEVEL_DEBUG, "        argv[%d] = %s", i, (char*)list_get_item(xserver_params, i));
        }
        list_delete(xserver_params);
        g_exit(1);
      }
      else /* parent (child sesman)*/
      {
        /* new style waiting for clients */
        session_start_sessvc(xpid, wmpid, data);
      }
    }
  }
  else /* parent sesman process */
  {
    /* let the other threads go on */
    scp_lock_fork_release();

    temp->item->pid = pid;
    temp->item->display = display;
    temp->item->width = width;
    temp->item->height = height;
    temp->item->bpp = bpp;
    temp->item->data = data;
    g_strncpy(temp->item->name, username, 255);

    temp->item->connect_time = g_time1();
    temp->item->disconnect_time = 0;
    temp->item->idle_time = 0;

    temp->item->type=type;
    temp->item->status=SESMAN_SESSION_STATUS_ACTIVE;

    /*THREAD-FIX lock the chain*/
    lock_chain_acquire();
    temp->next=g_sessions;
    g_sessions=temp;
    g_session_count++;
    /*THERAD-FIX free the chain*/
    lock_chain_release();

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

/******************************************************************************/
int DEFAULT_CC
session_kill(int pid)
{
  struct session_chain* tmp;
  struct session_chain* prev;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp=g_sessions;
  prev=0;

  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!",
                  pid);
      if (prev == 0)
      {
        /* prev does no exist, so it's the first element - so we set
           g_sessions */
        g_sessions = tmp->next;
      }
      else
      {
        prev->next = tmp->next;
      }
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return SESMAN_SESSION_KILL_NULLITEM;
    }

    if (tmp->item->pid == pid)
    {
      /* deleting the session */
      log_message(LOG_LEVEL_INFO, "session %d - user %s - terminated",
                  tmp->item->pid, tmp->item->name);
      g_free(tmp->item);
      if (prev == 0)
      {
        /* prev does no exist, so it's the first element - so we set
           g_sessions */
        g_sessions = tmp->next;
      }
      else
      {
        prev->next = tmp->next;
      }
      g_free(tmp);
      g_session_count--;
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return SESMAN_SESSION_KILL_OK;
    }

    /* go on */
    prev = tmp;
    tmp=tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
  return SESMAN_SESSION_KILL_NOTFOUND;
}

/******************************************************************************/
void DEFAULT_CC
session_sigkill_all()
{
  struct session_chain* tmp;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp=g_sessions;

  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "found null session descriptor!");
    }
    else
    {
      g_sigterm(tmp->item->pid);
    }

    /* go on */
    tmp=tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
}

/******************************************************************************/
struct session_item* DEFAULT_CC
session_get_bypid(int pid)
{
  struct session_chain* tmp;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp = g_sessions;
  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!",
                  pid);
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return 0;
    }

    if (tmp->item->pid == pid)
    {
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return tmp->item;
    }

    /* go on */
    tmp=tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
  return 0;
}

/******************************************************************************/
struct SCP_DISCONNECTED_SESSION*
session_get_byuser(char* user, int* cnt)
{
  struct session_chain* tmp;
  struct SCP_DISCONNECTED_SESSION* sess;
  int count;
  int index;

  count=0;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp = g_sessions;
  while (tmp != 0)
  {
#warning FIXME: we should get only disconnected sessions!
    if (!g_strncasecmp(user, tmp->item->name, 256))
    {
      count++;
    }

    /* go on */
    tmp=tmp->next;
  }

  if (count==0)
  {
    (*cnt)=0;
    /*THREAD-FIX release chain lock */
    lock_chain_release();
    return 0;
  }

  /* malloc() an array of disconnected sessions */
  sess=g_malloc(count * sizeof(struct SCP_DISCONNECTED_SESSION),1);
  if (sess==0)
  {
    (*cnt)=0;
    /*THREAD-FIX release chain lock */
    lock_chain_release();
    return 0;
  }

  tmp = g_sessions;
  index = 0;
  while (tmp != 0)
  {
#warning FIXME: we should get only disconnected sessions!
    if (!g_strncasecmp(user, tmp->item->name, 256))
    {
      (sess[index]).SID=tmp->item->pid;
      (sess[index]).type=tmp->item->type;
      (sess[index]).height=tmp->item->height;
      (sess[index]).width=tmp->item->width;
      (sess[index]).bpp=tmp->item->bpp;
#warning FIXME: setting idle times and such
      (sess[index]).idle_days=0;
      (sess[index]).idle_hours=0;
      (sess[index]).idle_minutes=0;
      index++;
    }

    /* go on */
    tmp=tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
  (*cnt)=count;
  return sess;
}

