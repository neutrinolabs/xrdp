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
   Copyright (C) Jay Sorg 2004

   listen for incoming connection

*/

#include "xrdp.h"

static struct xrdp_process* g_process = 0;

/*****************************************************************************/
struct xrdp_listen* xrdp_listen_create(void)
{
  struct xrdp_listen* self;

  self = (struct xrdp_listen*)g_malloc(sizeof(struct xrdp_listen), 1);
  self->process_list_max = 100;
  return self;
}

/*****************************************************************************/
void xrdp_listen_delete(struct xrdp_listen* self)
{
  g_free(self);
}

/*****************************************************************************/
int xrdp_listen_term_processes(struct xrdp_listen* self)
{
  int i;

  /* tell all xrdp processes to end */
  for (i = 0; i < self->process_list_count; i++)
    if (self->process_list[i] != 0)
      self->process_list[i]->term = 1;
  /* make sure they are done */
  for (i = 0; i < self->process_list_count; i++)
    if (self->process_list[i] != 0)
      while (self->process_list[i]->status > 0)
        g_sleep(10);
  /* free them all */
  for (i = 0; i < self->process_list_count; i++)
    if (self->process_list[i] != 0)
      xrdp_process_delete(self->process_list[i]);
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_listen_add_pro(struct xrdp_listen* self)
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
int xrdp_listen_delete_pro(struct xrdp_listen* self, struct xrdp_process* pro)
{
  int i;

  for (i = 0; i < self->process_list_max; i++)
  {
    if (self->process_list[i] == pro)
    {
      DEBUG(("process deleted\n\r"));
      xrdp_process_delete(pro);
      self->process_list[i] = 0;
      return 0;
    }
  }
  return 0;
}

/*****************************************************************************/
/* i can't get stupid in_val to work, hum using global var for now */
THREAD_RV THREAD_CC xrdp_process_run(void* in_val)
{
  DEBUG(("process started\n\r"));
  xrdp_process_main_loop(g_process);
  DEBUG(("process done\n\r"));
  return 0;
}

/*****************************************************************************/
/* wait for incoming connections */
int xrdp_listen_main_loop(struct xrdp_listen* self)
{
  int error;

  self->status = 1;
  self->sck = g_tcp_socket();
  g_tcp_set_non_blocking(self->sck);
  g_tcp_bind(self->sck, "3389");
  error = g_tcp_listen(self->sck);
  if (error == 0)
  {
    while (!g_is_term() && !self->term)
    {
      error = g_tcp_accept(self->sck);
      if (error == -1 && g_tcp_last_error_would_block(self->sck))
        g_sleep(100);
      else if (error == -1)
        break;
      else
      {
        g_process = xrdp_process_create(self);
        if (xrdp_listen_add_pro(self) == 0)
        {
          /* start thread */
          g_process->sck = error;
          g_thread_create(xrdp_process_run, 0);
          g_sleep(100);
        }
        else
          xrdp_process_delete(g_process);
      }
    }
  }
  else
  {
    DEBUG(("error, listener done\n\r"));
  }
  xrdp_listen_term_processes(self);
  g_tcp_close(self->sck);
  self->status = -1;
  return 0;
}
