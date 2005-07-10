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

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "xrdp.h"

static struct xrdp_listen* g_listen = 0;
static int g_threadid = 0; /* main threadid */

#if defined(_WIN32)
static CRITICAL_SECTION g_term_mutex;
#else
static pthread_mutex_t g_term_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int g_term = 0;

/*****************************************************************************/
void
xrdp_shutdown(int sig)
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
#if defined(_WIN32)
    WSACleanup();
    DeleteCriticalSection(&g_term_mutex);
#endif
  }
}

/*****************************************************************************/
int
g_is_term(void)
{
  int rv;

#if defined(_WIN32)
  EnterCriticalSection(&g_term_mutex);
  rv = g_term;
  LeaveCriticalSection(&g_term_mutex);
#else
  pthread_mutex_lock(&g_term_mutex);
  rv = g_term;
  pthread_mutex_unlock(&g_term_mutex);
#endif
  return rv;
}

/*****************************************************************************/
void
g_set_term(int in_val)
{
#if defined(_WIN32)
  EnterCriticalSection(&g_term_mutex);
  g_term = in_val;
  LeaveCriticalSection(&g_term_mutex);
#else
  pthread_mutex_lock(&g_term_mutex);
  g_term = in_val;
  pthread_mutex_unlock(&g_term_mutex);
#endif
}

/*****************************************************************************/
void
pipe_sig(int sig_num)
{
  /* do nothing */
  g_printf("got SIGPIPE(%d)\n\r", sig_num);
}

/*****************************************************************************/
int
main(int argc, char** argv)
{
#if defined(_WIN32)
  WSADATA w;

  WSAStartup(2, &w);
  InitializeCriticalSection(&g_term_mutex);
#endif
  g_threadid = g_get_threadid();
  g_listen = xrdp_listen_create();
  g_signal(2, xrdp_shutdown); /* SIGINT */
  g_signal(9, xrdp_shutdown); /* SIGKILL */
  g_signal(13, pipe_sig); /* sig pipe */
  g_signal(15, xrdp_shutdown); /* SIGTERM */
  xrdp_listen_main_loop(g_listen);
  return 0;
}
