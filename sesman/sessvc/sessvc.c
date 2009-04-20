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
   Copyright (C) Jay Sorg 2005-2009
*/

/**
 *
 * @file sessvc.c
 * @brief Session supervisor
 * @author Simone Fedele
 * 
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif
#include "file_loc.h"
#include "os_calls.h"
#include "arch.h"

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int ret;
  int chansrv_pid;
  int wm_pid;
  int x_pid;
  char exe_path[262];

  if (argc < 3)
  {
    g_writeln("xrdp-sessvc: exiting, not enough params");
    return 1;
  }
  x_pid = g_atoi(argv[1]);
  wm_pid = g_atoi(argv[2]);
  g_writeln("xrdp-sessvc: waiting for X (pid %d) and WM (pid %d)",
             x_pid, wm_pid);
  /* run xrdp-chansrv as a seperate process */
  chansrv_pid = g_fork();
  if (chansrv_pid == -1)
  {
    g_writeln("xrdp-sessvc: fork error");
    return 1;
  }
  else if (chansrv_pid == 0) /* child */
  {
    g_snprintf(exe_path, 261, "%s/xrdp-chansrv", XRDP_SBIN_PATH);
    g_execvp(exe_path, 0);
    /* should not get here */
    return 1;
  }
  /* wait for window manager to get done */
  ret = g_waitpid(wm_pid);
  /* kill X server */
  g_sigterm(x_pid);
  /* kill channel server */
  g_sigterm(chansrv_pid);
  g_writeln("xrdp-sessvc: WM is dead (waitpid said %d, errno is %d) "
            "exiting...", ret, g_get_errno());
  return 0;
}
