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

/*****************************************************************************/
/* i can't get stupid in_val to work, hum using global var for now */
THREAD_RV THREAD_CC xrdp_listen_run(void* in_val)
{
  DEBUG(("listener started\n\r"));
  xrdp_listen_main_loop(g_listen);
  DEBUG(("listener done\n\r"));
  return 0;
}

//#define CLEAN_CLOSE

/*****************************************************************************/
int main(int argc, char** argv)
{
  int rv;

  g_init_system();
  rv = 0;
  g_listen = xrdp_listen_create();
#ifdef CLEAN_CLOSE
  if (g_thread_create(xrdp_listen_run, 0) == 0)
  {
    g_getchar();
    g_set_term(1);
    while (g_listen->status > 0)
    {
      g_sleep(100);
    }
  }
  else
  {
    rv = 1;
  }
#else
  xrdp_listen_main_loop(g_listen);
#endif
  xrdp_listen_delete(g_listen);
  g_exit_system();
  return rv;
}
