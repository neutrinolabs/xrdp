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
 * @file sessvc.c
 * @brief Session supervisor
 * @author Simone Fedele
 * 
 */

#include "os_calls.h"
#include "arch.h"
#include "log.h"

#include <sys/types.h>
#include <errno.h>

pid_t wm_pid;
pid_t x_pid;

void DEFAULT_CC
signal_handler(int sig)
{
  g_sigterm(x_pid);
  g_sigterm(wm_pid);
  g_exit(0);
}

/******************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int ret=0;

  x_pid = g_atoi(argv[1]);
  wm_pid = g_atoi(argv[2]);

  g_printf("\n[sessvc] Setting signal handler\n"); 
  g_signal(9, signal_handler);   /* SIGKILL */
  g_signal(15, signal_handler);  /* SIGTERM */
  g_signal(17, signal_handler);  /* SIGCHLD */
 
  g_printf("\n[sessvc] Waiting for X (pid %d) and WM (pid %d)\n", x_pid, wm_pid);
  
  ret = g_waitpid(wm_pid);
  g_sigterm(x_pid);

  g_printf("\n[sessvc] WM is dead (waitpid said %d, errno is %d). exiting...\n", ret, errno);
  return 0;
}

