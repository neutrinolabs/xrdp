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

   main rdp process

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_process* APP_CC
xrdp_process_create(struct xrdp_listen* owner)
{
  struct xrdp_process* self;

  self = (struct xrdp_process*)g_malloc(sizeof(struct xrdp_process), 1);
  self->lis_layer = owner;
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
  libxrdp_exit(self->session);
  xrdp_wm_delete(self->wm);
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
       zero so xrdp_wm_init should be called by xrdp_wm_idle */
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
int APP_CC
xrdp_process_main_loop(struct xrdp_process* self)
{
  int sel_r;

  self->status = 1;
  self->session = libxrdp_init((long)self, self->sck);
  /* this callback function is in xrdp_wm.c */
  self->session->callback = callback;
  /* this function is just above */
  self->session->is_term = xrdp_is_term;
  g_tcp_set_non_blocking(self->sck);
  g_tcp_set_no_delay(self->sck);
  if (libxrdp_process_incomming(self->session) == 0)
  {
    while (!g_is_term() && !self->term)
    {
      sel_r = g_tcp_select(self->sck, self->app_sck);
      if (sel_r == 0) /* no data on any stream */
      {
        xrdp_wm_idle(self->wm);
      }
      else if (sel_r < 0)
      {
        break;
      }
      if (sel_r & 1)
      {
        if (xrdp_process_loop(self) != 0)
        {
          break;
        }
      }
      if (sel_r & 2) /* mod socket fired */
      {
        if (xrdp_wm_app_sck_signal(self->wm, self->app_sck) != 0)
        {
          break;
        }
      }
    }
    libxrdp_disconnect(self->session);
    g_sleep(500);
  }
  if (self->wm != 0)
  {
    if (self->wm->mm != 0)
    {
      if (self->wm->mm->mod != 0)
      {
        if (self->wm->mm->mod->mod_end != 0)
        {
          self->wm->mm->mod->mod_end(self->wm->mm->mod);
        }
      }
    }
  }
  libxrdp_exit(self->session);
  self->session = 0;
  g_tcp_close(self->sck);
  self->status = -1;
  xrdp_listen_delete_pro(self->lis_layer, self);
  return 0;
}
