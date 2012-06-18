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
   Copyright (C) Jay Sorg 2004-2010

   main rdp process

*/

#include "xrdp.h"

static int g_session_id = 0;

/*****************************************************************************/
/* always called from xrdp_listen thread */
struct xrdp_process* APP_CC
xrdp_process_create(struct xrdp_listen* owner, tbus done_event)
{
  struct xrdp_process* self;
  char event_name[256];
  int pid;

  self = (struct xrdp_process*)g_malloc(sizeof(struct xrdp_process), 1);
  self->lis_layer = owner;
  self->done_event = done_event;
  g_session_id++;
  self->session_id = g_session_id;
  pid = g_getpid();
  g_snprintf(event_name, 255, "xrdp_%8.8x_process_self_term_event_%8.8x",
             pid, self->session_id);
  self->self_term_event = g_create_wait_obj(event_name);
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_process_delete(struct xrdp_process* self)
{
  if (self == 0)
  {
    return;
  }
  g_delete_wait_obj(self->self_term_event);
  libxrdp_exit(self->session);
  xrdp_wm_delete(self->wm);
  trans_delete(self->server_trans);
  g_free(self);
}

/*****************************************************************************/
static int APP_CC
xrdp_process_loop(struct xrdp_process* self)
{
  int rv;

  rv = 0;
  if (self->session != 0)
  {
    rv = libxrdp_process_data(self->session);
  }
  if ((self->wm == 0) && (self->session->up_and_running) && (rv == 0))
  {
    DEBUG(("calling xrdp_wm_init and creating wm"));
    self->wm = xrdp_wm_create(self, self->session->client_info);
    /* at this point the wm(window manager) is create and wm::login_mode is
       zero and login_mode_event is set so xrdp_wm_init should be called by
       xrdp_wm_check_wait_objs */
  }
  return rv;
}

/*****************************************************************************/
/* returns boolean */
/* this is so libxrdp.so can known when to quit looping */
static int DEFAULT_CC
xrdp_is_term(void)
{
  return g_is_term();
}

/*****************************************************************************/
static int APP_CC
xrdp_process_mod_end(struct xrdp_process* self)
{
  if (self->wm != 0)
  {
    if (self->wm->mm != 0)
    {
      if (self->wm->mm->mod != 0)
      {
        if (self->wm->mm->mod->mod_end != 0)
        {
          return self->wm->mm->mod->mod_end(self->wm->mm->mod);
        }
      }
    }
  }
  return 0;
}

/*****************************************************************************/
static int DEFAULT_CC
xrdp_process_data_in(struct trans* self)
{
  struct xrdp_process* pro;

  DEBUG(("xrdp_process_data_in"));
  pro = (struct xrdp_process*)(self->callback_data);
  if (xrdp_process_loop(pro) != 0)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_process_main_loop(struct xrdp_process* self)
{
  int robjs_count;
  int wobjs_count;
  int cont;
  int timeout = 0;
  tbus robjs[32];
  tbus wobjs[32];
  tbus term_obj;

  DEBUG(("xrdp_process_main_loop"));
  self->status = 1;
  self->server_trans->trans_data_in = xrdp_process_data_in;
  self->server_trans->callback_data = self;
  self->session = libxrdp_init((tbus)self, self->server_trans);
  /* this callback function is in xrdp_wm.c */
  self->session->callback = callback;
  /* this function is just above */
  self->session->is_term = xrdp_is_term;
  if (libxrdp_process_incomming(self->session) == 0)
  {
    term_obj = g_get_term_event();
    cont = 1;
    while (cont)
    {
      /* build the wait obj list */
      timeout = -1;
      robjs_count = 0;
      wobjs_count = 0;
      robjs[robjs_count++] = term_obj;
      robjs[robjs_count++] = self->self_term_event;
      xrdp_wm_get_wait_objs(self->wm, robjs, &robjs_count,
                            wobjs, &wobjs_count, &timeout);
      trans_get_wait_objs(self->server_trans, robjs, &robjs_count);
      /* wait */
      if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
      {
        /* error, should not get here */
        g_sleep(100);
      }
      if (g_is_wait_obj_set(term_obj)) /* term */
      {
        break;
      }
      if (g_is_wait_obj_set(self->self_term_event))
      {
        break;
      }
      if (xrdp_wm_check_wait_objs(self->wm) != 0)
      {
        break;
      }
      if (trans_check_wait_objs(self->server_trans) != 0)
      {
        break;
      }
    }
    libxrdp_disconnect(self->session);
  }
  else
  {
    g_writeln("xrdp_process_main_loop: libxrdp_process_incomming failed");
  }
  xrdp_process_mod_end(self);
  libxrdp_exit(self->session);
  self->session = 0;
  self->status = -1;
  g_set_wait_obj(self->done_event);
  return 0;
}
