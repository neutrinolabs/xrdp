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
 * @file thread.c
 * @brief thread stuff...
 * @author Simone Fedele
 * 
 */

#include "sesman.h"

#include <errno.h>
#include <signal.h>
#include <pthread.h>

extern struct config_sesman* g_cfg; /* in sesman.c */

static pthread_t g_thread_sighandler;
//static pthread_t g_thread_updater;

/* a variable to pass the socket of s connection to a thread */
int g_thread_sck;

/******************************************************************************/
int DEFAULT_CC
thread_sighandler_start(void)
{
  int ret;
  sigset_t sigmask;
  sigset_t oldmask;
  sigset_t waitmask;

  /* mask signals to be able to wait for them... */
  sigfillset(&sigmask);
  pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);

  /* unblock some signals... */
  sigemptyset(&waitmask);

  /* it is a good idea not to block SIGILL SIGSEGV */
  /* SIGFPE -- see sigaction(2) NOTES              */
  sigaddset(&waitmask, SIGILL);
  sigaddset(&waitmask, SIGSEGV);
  sigaddset(&waitmask, SIGFPE);
  pthread_sigmask(SIG_UNBLOCK, &waitmask, NULL);

  log_message(LOG_LEVEL_INFO,"starting signal handling thread...");

  ret = pthread_create(&g_thread_sighandler, NULL, sig_handler_thread, "");
  pthread_detach(g_thread_sighandler);

  if (ret == 0)
  {
    log_message(LOG_LEVEL_INFO, "signal handler thread started successfully");
    return 0;
  }

  /* if something happened while starting a new thread... */
  switch (ret)
  {
    case EINVAL:
      log_message(LOG_LEVEL_ERROR, "invalid attributes for signal handling thread (creation returned  EINVAL)");
      break;
    case EAGAIN:
      log_message(LOG_LEVEL_ERROR, "not enough resources to start signal handling thread (creation returned EAGAIN)");
      break;
    case EPERM:
      log_message(LOG_LEVEL_ERROR, "invalid permissions for signal handling thread (creation returned EPERM)");
      break;
    default:
      log_message(LOG_LEVEL_ERROR, "unknown error starting signal handling thread");
  }

  return 1;
}

#ifdef JUST_TO_AVOID_COMPILER_ERRORS
/******************************************************************************/
int DEFAULT_CC
thread_session_update_start(void)
{
  int ret;
  //starts the session update thread
  //that checks for idle time, destroys sessions, ecc...
  
#warning this thread should always request lock_fork before read or write 
#warning (so we can Fork() In Peace)
  ret = pthread_create(&g_thread_updater, NULL, , "");
  pthread_detach(g_thread_updater);

  if (ret==0) 
  {
    log_message(&(g_cfg->log), LOG_LEVEL_INFO, "session update thread started successfully");
    return 0;
  }

  /* if something happened while starting a new thread... */
  switch (ret)
  {
    case EINVAL:
      log_message(LOG_LEVEL_ERROR, "invalid attributes for session update thread (creation returned  EINVAL)");
      break;
    case EAGAIN:
      log_message(LOG_LEVEL_ERROR, "not enough resources to start session update thread (creation returned EAGAIN)");
      break;
    case EPERM:
      log_message(LOG_LEVEL_ERROR, "invalid permissions for session update thread (creation returned EPERM)");
      break;
    default:
      log_message(LOG_LEVEL_ERROR, "unknown error starting session update thread");
  }

  return 1;
}
#endif

/******************************************************************************/
int DEFAULT_CC
thread_scp_start(int skt)
{
  int ret;
  pthread_t th;

  /* blocking the use of thread_skt */
  lock_socket_acquire();
  g_thread_sck = skt;

  /* start a thread that processes a connection */
  ret = pthread_create(&th, NULL, scp_process_start, "");
  //ret = pthread_create(&th, NULL, scp_process_start, (void*) (&g_thread_sck));
  pthread_detach(th);

  if (ret == 0) 
  {
    log_message(LOG_LEVEL_INFO, "scp thread on sck %d started successfully", skt);
    return 0;
  }

  /* if something happened while starting a new thread... */
  switch (ret)
  {
    case EINVAL:
      log_message(LOG_LEVEL_ERROR, "invalid attributes for scp thread on sck %d (creation returned  EINVAL)", skt);
      break;
    case EAGAIN:
      log_message(LOG_LEVEL_ERROR, "not enough resources to start scp thread on sck %d (creation returned EAGAIN)", skt);
      break;
    case EPERM:
      log_message(LOG_LEVEL_ERROR, "invalid permissions for scp thread on sck %d (creation returned EPERM)", skt);
      break;
    default:
      log_message(LOG_LEVEL_ERROR, "unknown error starting scp thread on sck %d");
  }

  return 1;
}

