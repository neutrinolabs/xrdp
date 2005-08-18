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
static CRITICAL_SECTION g_sync_mutex;
static CRITICAL_SECTION g_sync1_mutex;
#else
static pthread_mutex_t g_term_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sync1_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int g_term = 0;
/* syncronize stuff */
static int g_sync_command = 0;
static long g_sync_result = 0;
static long g_sync_param1 = 0;
static long g_sync_param2 = 0;
static long (*g_sync_func)(long param1, long param2);

/*****************************************************************************/
long APP_CC
g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1,
            long sync_param2)
{
  long sync_result;
  int sync_command;

#if defined(_WIN32)
  EnterCriticalSection(&g_sync1_mutex);
#else
  pthread_mutex_lock(&g_sync1_mutex);
#endif
  g_lock();
  g_sync_param1 = sync_param1;
  g_sync_param2 = sync_param2;
  g_sync_func = sync_func;
  g_sync_command = 100;
  g_unlock();
  do
  {
    g_sleep(100);
    g_lock();
    sync_command = g_sync_command;
    sync_result = g_sync_result;
    g_unlock();
  }
  while (sync_command != 0);
#if defined(_WIN32)
  LeaveCriticalSection(&g_sync1_mutex);
#else
  pthread_mutex_unlock(&g_sync1_mutex);
#endif
  return sync_result;
}

/*****************************************************************************/
void DEFAULT_CC
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
  }
}

/*****************************************************************************/
int APP_CC
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
void APP_CC
g_lock(void)
{
#if defined(_WIN32)
  EnterCriticalSection(&g_sync_mutex);
#else
  pthread_mutex_lock(&g_sync_mutex);
#endif
}

/*****************************************************************************/
void APP_CC
g_unlock(void)
{
#if defined(_WIN32)
  LeaveCriticalSection(&g_sync_mutex);
#else
  pthread_mutex_unlock(&g_sync_mutex);
#endif
}

/*****************************************************************************/
void APP_CC
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
void DEFAULT_CC
pipe_sig(int sig_num)
{
  /* do nothing */
  g_printf("got SIGPIPE(%d)\n\r", sig_num);
}

/*****************************************************************************/
void APP_CC
g_loop(void)
{
  g_lock();
  if (g_sync_command != 0)
  {
    if (g_sync_func != 0)
    {
      if (g_sync_command == 100)
      {
        g_sync_result = g_sync_func(g_sync_param1, g_sync_param2);
      }
    }
    g_sync_command = 0;
  }
  g_unlock();
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int test;
  int host_be;
#if defined(_WIN32)
  WSADATA w;
#endif

  /* check compiled endian with actual edian */
  test = 1;
  host_be = !((int)(*(unsigned char*)(&test)));
#if defined(B_ENDIAN)
  if (!host_be)
#endif
#if defined(L_ENDIAN)
  if (host_be)
#endif
  {
    g_printf("endian wrong, edit arch.h\n\r");
    return 0;
  }
  /* check long, int and void* sizes */
  if (sizeof(int) != 4)
  {
    g_printf("unusable int size, must be 4\n\r");
    return 0;
  }
  if (sizeof(long) != sizeof(void*) || (sizeof(long) != 4 && sizeof(long) != 8))
  {
    g_printf("unusable long size, must be 4 or 8\n\r");
    return 0;
  }
#if defined(_WIN32)
  WSAStartup(2, &w);
  InitializeCriticalSection(&g_term_mutex);
  InitializeCriticalSection(&g_sync_mutex);
  InitializeCriticalSection(&g_sync1_mutex);
#endif
  g_threadid = g_get_threadid();
  g_listen = xrdp_listen_create();
  g_signal(2, xrdp_shutdown); /* SIGINT */
  g_signal(9, xrdp_shutdown); /* SIGKILL */
  g_signal(13, pipe_sig); /* sig pipe */
  g_signal(15, xrdp_shutdown); /* SIGTERM */
  xrdp_listen_main_loop(g_listen);
#if defined(_WIN32)
  /* I don't think it ever gets here */
  WSACleanup();
  DeleteCriticalSection(&g_term_mutex);
  DeleteCriticalSection(&g_sync_mutex);
  DeleteCriticalSection(&g_sync1_mutex);
  xrdp_listen_delete(g_listen);
#endif
  return 0;
}
