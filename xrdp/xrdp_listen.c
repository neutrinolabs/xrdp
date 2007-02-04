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
   Copyright (C) Jay Sorg 2004-2007

   listen for incoming connection

*/

#include "xrdp.h"

/* 'g_process' is protected by the semaphore 'g_process_sem'.  One thread sets
   g_process and waits for the other to process it */
static long g_process_sem = 0;
static struct xrdp_process* g_process = 0;

/*****************************************************************************/
struct xrdp_listen* APP_CC
xrdp_listen_create(void)
{
  struct xrdp_listen* self;

  self = (struct xrdp_listen*)g_malloc(sizeof(struct xrdp_listen), 1);
  self->process_list_max = 100;
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_listen_delete(struct xrdp_listen* self)
{
  g_free(self);
}

/*****************************************************************************/
static int APP_CC
xrdp_listen_term_processes(struct xrdp_listen* self)
{
  int i;

  /* tell all xrdp processes to end */
  for (i = 0; i < self->process_list_count; i++)
  {
    if (self->process_list[i] != 0)
    {
      self->process_list[i]->term = 1;
    }
  }
  /* make sure they are done */
  for (i = 0; i < self->process_list_count; i++)
  {
    if (self->process_list[i] != 0)
    {
      while (self->process_list[i]->status > 0)
      {
        g_sleep(10);
      }
    }
  }
  /* free them all */
  for (i = 0; i < self->process_list_count; i++)
  {
    if (self->process_list[i] != 0)
    {
      xrdp_process_delete(self->process_list[i]);
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_listen_add_pro(struct xrdp_listen* self)
{
  int i;

  for (i = 0; i < self->process_list_max; i++)
  {
    /* add process in new slot */
    if (self->process_list[i] == 0)
    {
      self->process_list[i] = g_process;
      self->process_list_count++;
      return 0;
    }
    /* add process in unused slot */
    /* this shouldn't happen */
    if (self->process_list[i]->status <= 0)
    {
      xrdp_process_delete(self->process_list[i]);
      self->process_list[i] = g_process;
      return 0;
    }
  }
  return 1;
}

/*****************************************************************************/
int APP_CC
xrdp_listen_delete_pro(struct xrdp_listen* self, struct xrdp_process* pro)
{
  int i;

  for (i = 0; i < self->process_list_max; i++)
  {
    if (self->process_list[i] == pro)
    {
      DEBUG(("process deleted"));
      xrdp_process_delete(pro);
      self->process_list[i] = 0;
      return 0;
    }
  }
  return 0;
}

/*****************************************************************************/
/* i can't get stupid in_val to work, hum using global var for now */
THREAD_RV THREAD_CC
xrdp_process_run(void* in_val)
{
  struct xrdp_process* process;

  DEBUG(("process started"));
  process = g_process;
  g_process = 0;
  tc_sem_inc(g_process_sem);
  xrdp_process_main_loop(process);
  DEBUG(("process done"));
  return 0;
}

/*****************************************************************************/
/* wait for incoming connections */
int APP_CC
xrdp_listen_main_loop(struct xrdp_listen* self)
{
  int error;
  int fd;
  int index;
  char port[8];
  char* val;
  struct list* names;
  struct list* values;

  self->status = 1;
  g_process_sem = tc_sem_create(0);
  /* default to port 3389 */
  g_strncpy(port, "3389", 7);
  /* see if port is in xrdp.ini file */
  fd = g_file_open(XRDP_CFG_FILE);
  if (fd > 0)
  {
    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    if (file_read_section(fd, "globals", names, values) == 0)
    {
      for (index = 0; index < names->count; index++)
      {
        val = (char*)list_get_item(names, index);
        if (val != 0)
        {
          if (g_strncasecmp(val, "port", 5) == 0)
          {
            val = (char*)list_get_item(values, index);
            error = g_atoi(val);
            if (error > 0 && error < 65000)
            {
              g_strncpy(port, val, 7);
            }
            break;
          }
        }
      }
    }
    list_delete(names);
    list_delete(values);
    g_file_close(fd);
  }
  self->sck = g_tcp_socket();
  g_tcp_set_non_blocking(self->sck);
  error = g_tcp_bind(self->sck, port);
  if (error != 0)
  {
    g_writeln("bind error in xrdp_listen_main_loop");
    g_tcp_close(self->sck);
    self->status = -1;
    return 1;
  }
  error = g_tcp_listen(self->sck);
  if (error == 0)
  {
    while (!g_is_term() && !self->term)
    {
      error = g_tcp_accept(self->sck);
      if ((error == -1) && g_tcp_last_error_would_block(self->sck))
      {
        g_sleep(100);
        g_loop();
      }
      else if (error == -1)
      {
        break;
      }
      else
      {
        g_process = xrdp_process_create(self);
        if (xrdp_listen_add_pro(self) == 0)
        {
          /* start thread */
          g_process->sck = error;
          tc_thread_create(xrdp_process_run, 0);
          tc_sem_dec(g_process_sem); /* this will wait */
          g_sleep(250); /* just for safety */
        }
        else
        {
          xrdp_process_delete(g_process);
        }
      }
    }
  }
  else
  {
    DEBUG(("listen error in xrdp_listen_main_loop"));
  }
  xrdp_listen_term_processes(self);
  g_tcp_close(self->sck);
  tc_sem_delete(g_process_sem);
  self->status = -1;
  return 0;
}
