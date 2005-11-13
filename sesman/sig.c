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
*/

/**
 *
 * @file sig.c
 * @brief signal handling functions
 * @author Jay Sorg, Simone Fedele
 * 
 */

#include "sesman.h"

extern int g_sck;
extern int g_pid;
extern struct config_sesman g_cfg;

/******************************************************************************/
void DEFAULT_CC
sig_sesman_shutdown(int sig)
{
  log_message(LOG_LEVEL_INFO, "shutting down sesman %d",1);
	  
  if (g_getpid() != g_pid)
  {
    LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", (g_getpid()), g_pid);
    return;
  }
  
  LOG_DBG(" - getting signal %d pid %d", sig, g_getpid());

  g_tcp_close(g_sck);
}

/******************************************************************************/
void DEFAULT_CC
sig_sesman_reload_cfg(int sig)
{
  struct config_sesman cfg;
  
  log_message(LOG_LEVEL_WARNING, "receiving SIGHUP %d", 1);

  if (g_getpid() != g_pid)
  {
    LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", g_getpid(), g_pid);
    return;
  }
  
  if (config_read(&cfg) != 0)
  {
    log_message(LOG_LEVEL_ERROR, "error reading config - keeping old cfg");
    return;
  }
  g_cfg = cfg;
  
  log_message(LOG_LEVEL_INFO, "configuration reloaded");
}

