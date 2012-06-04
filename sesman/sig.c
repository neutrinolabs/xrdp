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
   Copyright (C) Jay Sorg 2005-2010
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
extern struct config_sesman* g_cfg; /* in sesman.c */
extern tbus g_term_event;

/******************************************************************************/
void DEFAULT_CC
sig_sesman_shutdown(int sig)
{
  char pid_file[256];

  log_message(LOG_LEVEL_INFO, "shutting down sesman %d", 1);

  if (g_getpid() != g_pid)
  {
    LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", (g_getpid()), g_pid);
    return;
  }

  LOG_DBG(" - getting signal %d pid %d", sig, g_getpid());

  g_set_wait_obj(g_term_event);

  g_tcp_close(g_sck);

  session_sigkill_all();

  g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
  g_file_delete(pid_file);
}

/******************************************************************************/
void DEFAULT_CC
sig_sesman_reload_cfg(int sig)
{
  int error;
  struct config_sesman *cfg;
  char cfg_file[256];

  log_message(LOG_LEVEL_WARNING, "receiving SIGHUP %d", 1);

  if (g_getpid() != g_pid)
  {
    LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", g_getpid(), g_pid);
    return;
  }

  cfg = g_malloc(sizeof(struct config_sesman), 1);
  if (0 == cfg)
  {
    log_message(LOG_LEVEL_ERROR, "error creating new config:  - keeping old cfg");
    return;
  }

  if (config_read(cfg) != 0)
  {
    log_message(LOG_LEVEL_ERROR, "error reading config - keeping old cfg");
    return;
  }

  /* stop logging subsystem */
  log_end();

  /* replace old config with new readed one */
  g_cfg = cfg;
  
  g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);

  /* start again logging subsystem */
  error = log_start(cfg_file,"XRDP-sesman");

  if (error != LOG_STARTUP_OK)
  {
    char buf[256];
    switch (error)
    {
      case LOG_ERROR_MALLOC:
        g_printf("error on malloc. cannot restart logging. log stops here, sorry.\n");
        break;
      case LOG_ERROR_FILE_OPEN:
        g_printf("error reopening log file [%s]. log stops here, sorry.\n", getLogFile(buf,255));
        break;
    }
  }

  log_message(LOG_LEVEL_INFO, "configuration reloaded, log subsystem restarted");
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
  sigaddset(&waitmask, SIGKILL);
  sigaddset(&waitmask, SIGINT);

//  sigaddset(&waitmask, SIGFPE);
//  sigaddset(&waitmask, SIGILL);
//  sigaddset(&waitmask, SIGSEGV);

  do
  {
    LOG_DBG(&(g_cfg->log), "calling sigwait()",0);
    sigwait(&waitmask, &recv_signal);
    switch (recv_signal)
    {
      case SIGHUP:
        //reload cfg
	//we must stop & restart logging, or copy logging cfg!!!!
        LOG_DBG("sesman received SIGHUP", 0);
        //return 0;
        break;
      case SIGCHLD:
        /* a session died */
        LOG_DBG("sesman received SIGCHLD", 0);
        sig_sesman_session_end(SIGCHLD);
        break;
      case SIGINT:
        /* we die */
        LOG_DBG("sesman received SIGINT", 0);
        sig_sesman_shutdown(recv_signal);
        break;
      case SIGKILL:
        /* we die */
        LOG_DBG("sesman received SIGKILL", 0);
        sig_sesman_shutdown(recv_signal);
        break;
      case SIGTERM:
        /* we die */
        LOG_DBG("sesman received SIGTERM", 0);
        sig_sesman_shutdown(recv_signal);
        break;
    }
  } while (1);

  return 0;
}
