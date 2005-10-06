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
   Copyright (C) Jay Sorg 2005

   session manager
   linux only

   sig.c: signal handling code

*/

/**
 *
 * @file: signal handling functions
 * 
 */

#include "sesman.h"

extern int g_sck;
extern int g_pid;
extern struct sesman_config g_cfg;

/**
 * 
 * Shuts down sesman
 * 
 * @param sig The received signal
 * 
 */
void DEFAULT_CC
sig_sesman_shutdown(int sig)
{
  if (g_getpid() != g_pid)
  {
    return;
  }
  g_printf("shutting down\n\r");
  g_printf("signal %d pid %d\n\r", sig, g_getpid());
  g_tcp_close(g_sck);
}

/**
 * 
 * Reloads sesman config
 * 
 * @param sig The received signal
 * 
 */
void DEFAULT_CC
sig_sesman_reload_cfg(int sig)
{
  struct sesman_config cfg;

  if (g_getpid() != g_pid)
  {
    return;
  }
  g_printf("sesman: received SIGHUP\n\r");
  if (config_read(&cfg) != 0)
  {
    g_printf("sesman: error reading config. keeping old cfg.\n\r");
    return;
  }
  g_cfg = cfg;
  g_printf("sesman: configuration reloaded\n\r");
}

