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
 * @file sig.c
 * @brief signal handling functions
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

#include "signal.h"

extern int g_sck;
extern int g_pid;
extern struct config_sesman g_cfg;

/******************************************************************************/
void DEFAULT_CC
sig_sesman_shutdown(int sig)
{
  log_message(LOG_LEVEL_INFO, "shutting down sesman %d", 1);

  if (g_getpid() != g_pid)
  {
    LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", (g_getpid()), g_pid);
    return;
  }

  LOG_DBG(" - getting signal %d pid %d", sig, g_getpid());

  g_tcp_close(g_sck);

  session_sigkill_all();

  g_file_delete(SESMAN_PID_FILE);
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

/******************************************************************************/
void DEFAULT_CC
sig_sesman_session_end(int sig)
{
  int pid;

  if (g_getpid() != g_pid)
  {
    return;
  }
  pid = g_waitchild();
  if (pid > 0)
  {
    session_kill(pid);
  }
}

/******************************************************************************/
void* DEFAULT_CC
sig_handler_thread(void* arg)
{
  int recv_signal;
  sigset_t sigmask;
  sigset_t oldmask;
  sigset_t waitmask;

  /* mask signals to be able to wait for them... */
  sigfillset(&sigmask);
  /* it is a good idea not to block SIGILL SIGSEGV */
  /* SIGFPE -- see sigaction(2) NOTES              */
  pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);

  /* building the signal wait mask... */
  sigemptyset(&waitmask);
  sigaddset(&waitmask, SIGHUP);
  sigaddset(&waitmask, SIGCHLD);
  sigaddset(&waitmask, SIGTERM);
//  sigaddset(&waitmask, SIGFPE);
//  sigaddset(&waitmask, SIGILL);
//  sigaddset(&waitmask, SIGSEGV);

  do
  {
    LOG_DBG("calling sigwait()",0);
    sigwait(&waitmask, &recv_signal);

    switch (recv_signal)
    {
      case SIGHUP:
        //reload cfg
        LOG_DBG("sesman received SIGHUP",0);
        //return 0;
        break;
      case SIGCHLD:
        /* a session died */
        LOG_DBG("sesman received SIGCHLD",0);
        sig_sesman_session_end(SIGCHLD);
        break;
      /*case SIGKILL;
        / * we die * /
        LOG_DBG("sesman received SIGKILL",0);
        sig_sesman_shutdown(recv_signal);
        break;*/
      case SIGTERM:
        /* we die */
        LOG_DBG("sesman received SIGTERM",0);
        sig_sesman_shutdown(recv_signal);
        break;
    }
  } while (1);

  return 0;
}

