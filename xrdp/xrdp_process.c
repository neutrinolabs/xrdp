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
   Copyright (C) Jay Sorg 2004-2005

   main rdp process

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_process* xrdp_process_create(struct xrdp_listen* owner)
{
  struct xrdp_process* self;

  self = (struct xrdp_process*)g_malloc(sizeof(struct xrdp_process), 1);
  self->lis_layer = owner;
  return self;
}

/*****************************************************************************/
void xrdp_process_delete(struct xrdp_process* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_rdp_delete(self->rdp_layer);
  xrdp_orders_delete(self->orders);
  xrdp_wm_delete(self->wm);
  g_free(self);
}

/*****************************************************************************/
int xrdp_process_loop(struct xrdp_process* self, struct stream* s)
{
  int cont;
  int rv;
  int code;

  code = 0;
  rv = 0;
  cont = 1;
  while (cont && !self->term)
  {
    if (xrdp_rdp_recv(self->rdp_layer, s, &code) != 0)
    {
      rv = 1;
      break;
    }
    DEBUG(("xrdp_process_main_loop code %d\n\r", code));
    switch (code)
    {
      case -1:
        xrdp_rdp_send_demand_active(self->rdp_layer);
        break;
      case 0:
        break;
      case RDP_PDU_CONFIRM_ACTIVE: /* 3 */
        xrdp_rdp_process_confirm_active(self->rdp_layer, s);
        break;
      case RDP_PDU_DATA: /* 7 */
        if (xrdp_rdp_process_data(self->rdp_layer, s) != 0)
        {
          DEBUG(("xrdp_rdp_process_data returned non zero\n\r"));
          cont = 0;
          self->term = 1;
        }
        break;
      default:
        g_printf("unknown in xrdp_process_main_loop\n\r");
        break;
    }
    if (cont)
    {
      cont = s->next_packet < s->end;
    }
  }
  if (self->rdp_layer->up_and_running && self->wm == 0 && rv == 0)
  {
    /* only do this once */
    DEBUG(("xrdp_process_main_loop up and running\n\r"));
    self->orders = xrdp_orders_create(self, self->rdp_layer);
    self->wm = xrdp_wm_create(self, &self->rdp_layer->client_info);
    xrdp_wm_init(self->wm);
  }
  return rv;
}

/*****************************************************************************/
int xrdp_process_main_loop(struct xrdp_process* self)
{
#ifndef XRDP_LIB
  int i;
  struct stream* s;

  make_stream(s);
  self->status = 1;
  self->rdp_layer = xrdp_rdp_create(self, self->sck);
  g_tcp_set_non_blocking(self->sck);
  g_tcp_set_no_delay(self->sck);
  if (xrdp_rdp_incoming(self->rdp_layer) == 0)
  {
    while (!g_is_term() && !self->term)
    {
      i = g_tcp_select(self->sck, self->app_sck);
      if (i & 1)
      {
        init_stream(s, 8192);
        if (xrdp_process_loop(self, s) != 0)
        {
          break;
        }
      }
      if (i & 2) /* mod socket fired */
      {
        if (self->wm->mod == 0)
        {
          break;
        }
        if (self->wm->mod->mod_signal == 0)
        {
          break;
        }
        if (self->wm->mod->mod_signal(self->wm->mod) != 0)
        {
          break;
        }
      }
      if (i == 0) /* no data on any stream */
      {
        g_sleep(10);
      }
      else if (i < 0)
      {
        break;
      }
    }
  }
  if (self->wm->mod != 0)
  {
    if (self->wm->mod->mod_end != 0)
    {
      self->wm->mod->mod_end(self->wm->mod);
    }
  }
  xrdp_rdp_disconnect(self->rdp_layer);
  g_sleep(500);
  xrdp_rdp_delete(self->rdp_layer);
  self->rdp_layer = 0;
  g_tcp_close(self->sck);
  self->status = -1;
  xrdp_listen_delete_pro(self->lis_layer, self);
  free_stream(s);
#endif
  return 0;
}
