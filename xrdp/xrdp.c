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

   main program

*/

#include "xrdp.h"

static struct xrdp_listen* g_listen = 0;
static int g_threadid = 0; /* main threadid */

/*****************************************************************************/
void shutdown(int sig)
{
  struct xrdp_listen* listen;

  if (g_get_threadid() != g_threadid)
  {
    return;
  }
  g_printf("shutting down\n\r");
  g_printf("signal %d threadid %d\n\r", sig, g_get_threadid());
  listen = g_listen;
  g_listen = 0;
  if (listen != 0)
  {
    g_set_term(1);
    g_sleep(1000);
    xrdp_listen_delete(listen);
    g_exit_system();
  }
}

/*****************************************************************************/
int main(int argc, char** argv)
{
  g_init_system();
  g_threadid = g_get_threadid();
  g_listen = xrdp_listen_create();
  g_signal(2, shutdown);
  g_signal(9, shutdown);
  g_signal(15, shutdown);
  xrdp_listen_main_loop(g_listen);
  return 0;
}
