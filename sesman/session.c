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
   Copyright (C) Jay Sorg 2005-2008
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

#include <errno.h>
//#include <time.h>

extern tbus g_sync_event;
extern unsigned char g_fixedkey[8];
extern struct config_sesman* g_cfg; /* in sesman.c */
struct session_chain* g_sessions;
int g_session_count;

static int g_sync_width;
static int g_sync_height;
static int g_sync_bpp;
static char* g_sync_username;
static char* g_sync_password;
static char* g_sync_domain;
static char* g_sync_program;
static char* g_sync_directory;
static char* g_sync_client_ip;
static tbus g_sync_data;
static tui8 g_sync_type;
static int g_sync_result;

/**
 * Creates a string consisting of all parameters that is hosted in the param list
 * @param self
 * @param outstr, allocate this buffer before you use this function
 * @param len the allocated len for outstr
 * @return 
 */ 
char* APP_CC
dumpItemsToString(struct list* self, char *outstr, int len)
{
  g_memset(outstr,0,len);  
  int index;
  tbus item;
  int totalLen= 0;

  if (self->count == 0)
  {
    g_writeln("List is empty");
  }
  for (index = 0; index < self->count; index++)
  {    
    /* +1 = one space*/
    totalLen = totalLen + g_strlen((char*)list_get_item(self, index))+1;
    if(len>totalLen)
    {
        g_strcat(outstr,(char*)list_get_item(self, index));
        g_strcat(outstr," ");
    }    
  }  
  return outstr ;
}


/******************************************************************************/
struct session_item* DEFAULT_CC
session_get_bydata(char* name, int width, int height, int bpp, int type)
{
  struct session_chain* tmp;

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp = g_sessions;

  /* convert from SCP_SESSION_TYPE namespace to SESMAN_SESSION_TYPE namespace */
  switch (type)
  {
    case SCP_SESSION_TYPE_XVNC: /* 0 */
      type = SESMAN_SESSION_TYPE_XVNC; /* 2 */
      break;
    case SCP_SESSION_TYPE_XRDP: /* 1 */
      type = SESMAN_SESSION_TYPE_XRDP; /* 1 */
      break;
    default:
      lock_chain_release();
      return 0;
  }

  while (tmp != 0)
  {
    if (type == SESMAN_SESSION_TYPE_XRDP)
    {
      /* only name and bpp need to match for X11rdp, it can resize */
      if (g_strncmp(name, tmp->item->name, 255) == 0 &&
          tmp->item->bpp == bpp &&
          tmp->item->type == type)
      {
        /*THREAD-FIX release chain lock */
        lock_chain_release();
        return tmp->item;
      }
    }
    if (g_strncmp(name, tmp->item->name, 255) == 0 &&
        tmp->item->width == width &&
        tmp->item->height == height &&
        tmp->item->bpp == bpp &&
        tmp->item->type == type)
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
x_server_running_check_ports(int display)
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
  return x_running;
}

/******************************************************************************/
static void DEFAULT_CC
session_start_sessvc(int xpid, int wmpid, long data)
{
  struct list * sessvc_params = (struct list *)NULL;
  char wmpid_str[25];
  char xpid_str[25];
  char exe_path[262];
  int i = 0;

  /* initialize (zero out) local variables: */
  g_memset(wmpid_str,0,sizeof(char) * 25);
  g_memset(xpid_str,0,sizeof(char) * 25);
  g_memset(exe_path,0,sizeof(char) * 262);

  /* new style waiting for clients */
  g_sprintf(wmpid_str, "%d", wmpid);
  g_sprintf(xpid_str, "%d",  xpid);
  log_message(LOG_LEVEL_INFO,
              "starting xrdp-sessvc - xpid=%s - wmpid=%s",
              xpid_str, wmpid_str);

  sessvc_params = list_create();
  sessvc_params->auto_free = 1;

  /* building parameters */
  g_snprintf(exe_path, 261, "%s/xrdp-sessvc", XRDP_SBIN_PATH);

  list_add_item(sessvc_params, (long)g_strdup(exe_path));
  list_add_item(sessvc_params, (long)g_strdup(xpid_str));
  list_add_item(sessvc_params, (long)g_strdup(wmpid_str));
  list_add_item(sessvc_params, 0); /* mandatory */

  /* executing sessvc */
  g_execvp(exe_path, ((char**)sessvc_params->items));

  /* should not get here */
  log_message(LOG_LEVEL_ALWAYS,
              "error starting xrdp-sessvc - pid %d - xpid=%s - wmpid=%s",
              g_getpid(), xpid_str, wmpid_str);

  /* logging parameters */
  /* no problem calling strerror for thread safety: other threads
     are blocked */
  log_message(LOG_LEVEL_DEBUG, "errno: %d, description: %s",
              errno, g_get_strerror());
  log_message(LOG_LEVEL_DEBUG, "execve parameter list:");
  for (i = 0; i < (sessvc_params->count); i++)
  {
    log_message(LOG_LEVEL_DEBUG, "        argv[%d] = %s", i,
                (char*)list_get_item(sessvc_params, i));
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
/* called with the main thread
   returns boolean */
static int APP_CC
session_is_display_in_chain(int display)
{
  struct session_chain* chain;
  struct session_item* item;

  chain = g_sessions;
  while (chain != 0)
  {
    item = chain->item;
    if (item->display == display)
    {
      return 1;
    }
    chain = chain->next;
  }
  return 0;
}

/******************************************************************************/
/* called with the main thread */
static int APP_CC
session_get_aval_display_from_chain(void)
{
  int display;

  display = g_cfg->sess.x11_display_offset;
  lock_chain_acquire();
  while ((display - g_cfg->sess.x11_display_offset) <= g_cfg->sess.max_sessions)
  {
    if (!session_is_display_in_chain(display))
    {
      if (!x_server_running_check_ports(display))
      {
        lock_chain_release();
        return display;
      }
    }
    display++;
  }
  lock_chain_release();
  log_message(LOG_LEVEL_ERROR, "X server -- no display in range is available");
  return 0;
}

/******************************************************************************/
static int APP_CC
wait_for_xserver(int display)
{
  int i;

  /* give X a bit to start */
  /* wait up to 10 secs for x server to start */
  i = 0;
  while (!x_server_running(display))
  {
    i++;
    if (i > 40)
    {
      log_message(LOG_LEVEL_ERROR,
                  "X server for display %d startup timeout",
                  display);
      break;
    }
    g_sleep(250);
  }
  return 0;
}

/******************************************************************************/
/* called with the main thread */
static int APP_CC
session_start_fork(int width, int height, int bpp, char* username,
                   char* password, tbus data, tui8 type, char* domain,
                   char* program, char* directory, char* client_ip)
{
  int display = 0;
  int pid = 0;
  int wmpid = 0;
  int xpid = 0;
  int i = 0;
  char geometry[32];
  char depth[32];
  char screen[32];
  char text[256];
  char passwd_file[256];
  char ** pp1 = (char **)NULL;
  struct session_chain * temp = (struct session_chain *)NULL;
  struct list * xserver_params = (struct list *)NULL;
  time_t ltime;
  struct tm stime;
  char execvpparams[2048];

  /* initialize (zero out) local variables: */
  g_memset(&ltime,0,sizeof(time_t));
  g_memset(&stime,0,sizeof(struct tm));
  g_memset(geometry,0,sizeof(char) * 32);
  g_memset(depth,0,sizeof(char) * 32);
  g_memset(screen,0,sizeof(char) * 32);
  g_memset(text,0,sizeof(char) * 256);
  g_memset(passwd_file,0,sizeof(char) * 256);

  /* check to limit concurrent sessions */
  if (g_session_count >= g_cfg->sess.max_sessions)
  {
    log_message(LOG_LEVEL_INFO, "max concurrent session limit "
                "exceeded. login for user %s denied", username);
    return 0;
  }

  temp = (struct session_chain*)g_malloc(sizeof(struct session_chain), 0);
  if (temp == 0)
  {
    log_message(LOG_LEVEL_ERROR, "cannot create new chain "
                "element - user %s", username);
    return 0;
  }
  temp->item = (struct session_item*)g_malloc(sizeof(struct session_item), 0);
  if (temp->item == 0)
  {
    g_free(temp);
    log_message(LOG_LEVEL_ERROR, "cannot create new session "
                "item - user %s", username);
    return 0;
  }
  display = session_get_aval_display_from_chain();
  if (display == 0)
  {
    g_free(temp->item);
    g_free(temp);
    return 0;
  }
  pid = g_fork();
  if (pid == -1)
  {
  }
  else if (pid == 0) /* child sesman */
  {
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
      wait_for_xserver(display);
      env_set_user(username, 0, display);
      if (x_server_running(display))
      {
        auth_set_env(data);
        if (directory != 0)
        {
          if (directory[0] != 0)
          {
            g_set_current_dir(directory);
          }
        }
        if (program != 0)
        {
          if (program[0] != 0)
          {
            g_execlp3(program, program, 0);
            log_message(LOG_LEVEL_ALWAYS,
                        "error starting program %s for user %s - pid %d",
                        program, username, g_getpid());
          }
        }
        /* try to execute user window manager if enabled */
        if (g_cfg->enable_user_wm)
        {
          g_sprintf(text,"%s/%s", g_getenv("HOME"), g_cfg->user_wm);
          if (g_file_exist(text))
          {
            g_execlp3(text, g_cfg->user_wm, 0);
            log_message(LOG_LEVEL_ALWAYS,"error starting user "
                        "wm for user %s - pid %d", username, g_getpid());
            /* logging parameters */
            log_message(LOG_LEVEL_DEBUG, "errno: %d, "
                        "description: %s", errno, g_get_strerror());
            log_message(LOG_LEVEL_DEBUG,"execlp3 parameter "
                        "list:");
            log_message(LOG_LEVEL_DEBUG, "        argv[0] = %s",
                        text);
            log_message(LOG_LEVEL_DEBUG, "        argv[1] = %s",
                        g_cfg->user_wm);
          }
        }
        /* if we're here something happened to g_execlp3
           so we try running the default window manager */
        g_sprintf(text, "%s/%s", XRDP_CFG_PATH, g_cfg->default_wm);
        g_execlp3(text, g_cfg->default_wm, 0);

        log_message( LOG_LEVEL_ALWAYS,"error starting default "
                    "wm for user %s - pid %d", username, g_getpid());
        /* logging parameters */
        log_message( LOG_LEVEL_DEBUG, "errno: %d, description: "
                    "%s", errno, g_get_strerror());
        log_message(LOG_LEVEL_DEBUG,"execlp3 parameter list:");
        log_message(LOG_LEVEL_DEBUG, "        argv[0] = %s",
                    text);
        log_message(LOG_LEVEL_DEBUG, "        argv[1] = %s",
                    g_cfg->default_wm);

        /* still a problem starting window manager just start xterm */
        g_execlp3("xterm", "xterm", 0);

        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS,"error starting xterm "
                    "for user %s - pid %d", username, g_getpid());
        /* logging parameters */
        log_message(LOG_LEVEL_DEBUG, "errno: %d, description: "
                    "%s", errno, g_get_strerror());
      }
      else
      {
        log_message(LOG_LEVEL_ERROR, "another Xserver might "
                    "already be active on display %d - see log", display);
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
          list_append_list_strdup(g_cfg->vnc_params, xserver_params, 0);

          /* make sure it ends with a zero */
          list_add_item(xserver_params, 0);
          pp1 = (char**)xserver_params->items;
          log_message(LOG_LEVEL_INFO,"Xvnc start:%s",dumpItemsToString(xserver_params, execvpparams, 2048));
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
          list_append_list_strdup(g_cfg->rdp_params, xserver_params, 0);

          /* make sure it ends with a zero */
          list_add_item(xserver_params, 0);
          pp1 = (char**)xserver_params->items;
          log_message(LOG_LEVEL_INFO,"X11rdp start:%s",dumpItemsToString(xserver_params, execvpparams, 2048)); 
          g_execvp("X11rdp", pp1);
        }
        else
        {
          log_message(LOG_LEVEL_ALWAYS, "bad session type - "
                      "user %s - pid %d", username, g_getpid());
          g_exit(1);
        }

        /* should not get here */
        log_message(LOG_LEVEL_ALWAYS, "error starting X server "
                    "- user %s - pid %d", username, g_getpid());

        /* logging parameters */
        log_message(LOG_LEVEL_DEBUG, "errno: %d, description: "
                    "%s", errno, g_get_strerror());
        log_message(LOG_LEVEL_DEBUG, "execve parameter list size: "
                    "%d", (xserver_params)->count);

        for (i=0; i<(xserver_params->count); i++)
        {
          log_message(LOG_LEVEL_DEBUG, "        argv[%d] = %s",
                      i, (char*)list_get_item(xserver_params, i));
        }
        list_delete(xserver_params);
        g_exit(1);
      }
      else /* parent (child sesman)*/
      {
        wait_for_xserver(display);
        g_snprintf(text, 255, "%d", display);
        g_setenv("XRDP_SESSVC_DISPLAY", text, 1);
        g_snprintf(text, 255, ":%d.0", display);
        g_setenv("DISPLAY", text, 1);
        /* new style waiting for clients */
        session_start_sessvc(xpid, wmpid, data);
      }
    }
  }
  else /* parent sesman process */
  {
    temp->item->pid = pid;
    temp->item->display = display;
    temp->item->width = width;
    temp->item->height = height;
    temp->item->bpp = bpp;
    temp->item->data = data;
    g_strncpy(temp->item->client_ip, client_ip, 255);	/* store client ip data */
    g_strncpy(temp->item->name, username, 255);

    ltime = g_time1();
    localtime_r(&ltime, &stime);
    temp->item->connect_time.year = (tui16)(stime.tm_year + 1900);
    temp->item->connect_time.month = (tui8)stime.tm_mon;
    temp->item->connect_time.day = (tui8)stime.tm_mday;
    temp->item->connect_time.hour = (tui8)stime.tm_hour;
    temp->item->connect_time.minute = (tui8)stime.tm_min;
    zero_time(&(temp->item->disconnect_time));
    zero_time(&(temp->item->idle_time));

    temp->item->type=type;
    temp->item->status=SESMAN_SESSION_STATUS_ACTIVE;

    temp->next=g_sessions;
    g_sessions=temp;
    g_session_count++;
  }
  return display;
}

/******************************************************************************/
/* called by a worker thread, ask the main thread to call session_sync_start
   and wait till done */
int DEFAULT_CC
session_start(int width, int height, int bpp, char* username, char* password,
              long data, tui8 type, char* domain, char* program,
              char* directory, char* client_ip)
{
  int display;

  /* lock mutex */
  lock_sync_acquire();
  /* set shared vars */
  g_sync_width = width;
  g_sync_height = height;
  g_sync_bpp = bpp;
  g_sync_username = username;
  g_sync_password = password;
  g_sync_domain = domain;
  g_sync_program = program;
  g_sync_directory = directory;
  g_sync_client_ip = client_ip;
  g_sync_data = data;
  g_sync_type = type;
  /* set event for main thread to see */
  g_set_wait_obj(g_sync_event);
  /* wait for main thread to get done */
  lock_sync_sem_acquire();
  /* read result(display) from shared var */
  display = g_sync_result;
  /* unlock mutex */
  lock_sync_release();
  return display;
}

/******************************************************************************/
/* called with the main thread */
int APP_CC
session_sync_start(void)
{
  g_sync_result = session_start_fork(g_sync_width, g_sync_height, g_sync_bpp,
                                     g_sync_username, g_sync_password,
                                     g_sync_data, g_sync_type, g_sync_domain,
                                     g_sync_program, g_sync_directory, g_sync_client_ip);
  lock_sync_sem_release();
  return 0;
}

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
      log_message(LOG_LEVEL_ERROR, "session descriptor for "
                  "pid %d is null!", pid);
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
      log_message(LOG_LEVEL_INFO, "++ terminated session:  username %s, display :%d.0, session_pid %d, ip %s", tmp->item->name, tmp->item->display, tmp->item->pid, tmp->item->client_ip);
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
      log_message(LOG_LEVEL_ERROR, "found null session "
                  "descriptor!");
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
  struct session_item* dummy;

  dummy = g_malloc(sizeof(struct session_item), 1);
  if (0 == dummy)
  {
    log_message(LOG_LEVEL_ERROR, "internal error", pid);
    return 0;
  }

  /*THREAD-FIX require chain lock */
  lock_chain_acquire();

  tmp = g_sessions;
  while (tmp != 0)
  {
    if (tmp->item == 0)
    {
      log_message(LOG_LEVEL_ERROR, "session descriptor for "
                  "pid %d is null!", pid);
      /*THREAD-FIX release chain lock */
      lock_chain_release();
      return 0;
    }

    if (tmp->item->pid == pid)
    {
      /*THREAD-FIX release chain lock */
      g_memcpy(dummy, tmp->item, sizeof(struct session_item));
      lock_chain_release();
      /*return tmp->item;*/
      return dummy;
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
session_get_byuser(char* user, int* cnt, unsigned char flags)
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
    LOG_DBG("user: %s", user);
    if ((NULL == user) || (!g_strncasecmp(user, tmp->item->name, 256)))
    {
      LOG_DBG("session_get_byuser: status=%d, flags=%d, "
              "result=%d", (tmp->item->status), flags,
              ((tmp->item->status) & flags));
      if ((tmp->item->status) & flags)
      {
        count++;
      }
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
    if ((NULL == user) || (!g_strncasecmp(user, tmp->item->name, 256)))
    {
      if ((tmp->item->status) & flags)
      {
        (sess[index]).SID=tmp->item->pid;
        (sess[index]).type=tmp->item->type;
        (sess[index]).height=tmp->item->height;
        (sess[index]).width=tmp->item->width;
        (sess[index]).bpp=tmp->item->bpp;
#warning FIXME: setting idle times and such
        /*(sess[index]).connect_time.year = tmp->item->connect_time.year;
        (sess[index]).connect_time.month = tmp->item->connect_time.month;
        (sess[index]).connect_time.day = tmp->item->connect_time.day;
        (sess[index]).connect_time.hour = tmp->item->connect_time.hour;
        (sess[index]).connect_time.minute = tmp->item->connect_time.minute;
        (sess[index]).disconnect_time.year = tmp->item->disconnect_time.year;
        (sess[index]).disconnect_time.month = tmp->item->disconnect_time.month;
        (sess[index]).disconnect_time.day = tmp->item->disconnect_time.day;
        (sess[index]).disconnect_time.hour = tmp->item->disconnect_time.hour;
        (sess[index]).disconnect_time.minute = tmp->item->disconnect_time.minute;
        (sess[index]).idle_time.year = tmp->item->idle_time.year;
        (sess[index]).idle_time.month = tmp->item->idle_time.month;
        (sess[index]).idle_time.day = tmp->item->idle_time.day;
        (sess[index]).idle_time.hour = tmp->item->idle_time.hour;
        (sess[index]).idle_time.minute = tmp->item->idle_time.minute;*/
        (sess[index]).conn_year = tmp->item->connect_time.year;
        (sess[index]).conn_month = tmp->item->connect_time.month;
        (sess[index]).conn_day = tmp->item->connect_time.day;
        (sess[index]).conn_hour = tmp->item->connect_time.hour;
        (sess[index]).conn_minute = tmp->item->connect_time.minute;
        (sess[index]).idle_days = tmp->item->idle_time.day;
        (sess[index]).idle_hours = tmp->item->idle_time.hour;
        (sess[index]).idle_minutes = tmp->item->idle_time.minute;

        index++;
      }
    }

    /* go on */
    tmp=tmp->next;
  }

  /*THREAD-FIX release chain lock */
  lock_chain_release();
  (*cnt)=count;
  return sess;
}
