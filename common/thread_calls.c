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
   Copyright (C) Jay Sorg 2004-2007

   thread calls

*/

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "arch.h"
#include "thread_calls.h"
#include "os_calls.h"

/*****************************************************************************/
/* returns error */
#if defined(_WIN32)
int APP_CC
tc_thread_create(unsigned long (__stdcall * start_routine)(void*), void* arg)
{
  DWORD thread_id;
  HANDLE thread;
  int rv;

  /* CreateThread returns handle or zero on error */
  thread = CreateThread(0, 0, start_routine, arg, 0, &thread_id);
  rv = !thread;
  CloseHandle(thread);
  return rv;
}
#else
int APP_CC
tc_thread_create(void* (* start_routine)(void*), void* arg)
{
  pthread_t thread;
  int rv;

  /* pthread_create returns error */
  rv = pthread_create(&thread, 0, start_routine, arg);
  pthread_detach(thread);
  return rv;
}
#endif

/*****************************************************************************/
long APP_CC
tc_get_threadid(void)
{
#if defined(_WIN32)
  return (long)GetCurrentThreadId();
#else
  return (long)pthread_self();
#endif
}

/*****************************************************************************/
long APP_CC
tc_create_mutex(void)
{
#if defined(_WIN32)
  return (long)CreateMutex(0, 0, 0);
#else
  pthread_mutex_t* lmutex;

  lmutex = (pthread_mutex_t*)g_malloc(sizeof(pthread_mutex_t), 0);
  pthread_mutex_init(lmutex, 0);
  return (long)lmutex;
#endif
}

/*****************************************************************************/
void APP_CC
tc_delete_mutex(long mutex)
{
#if defined(_WIN32)
  CloseHandle((HANDLE)mutex);
#else
  pthread_mutex_t* lmutex;

  lmutex = (pthread_mutex_t*)mutex;
  pthread_mutex_destroy(lmutex);
  g_free(lmutex);
#endif
}

/*****************************************************************************/
int APP_CC
tc_lock_mutex(long mutex)
{
#if defined(_WIN32)
  WaitForSingleObject((HANDLE)mutex, INFINITE);
  return 0;
#else
  pthread_mutex_lock((pthread_mutex_t*)mutex);
  return 0;
#endif
}

/*****************************************************************************/
int APP_CC
tc_unlock_mutex(long mutex)
{
#if defined(_WIN32)
  ReleaseMutex((HANDLE)mutex);
  return 0;
#else
  pthread_mutex_unlock((pthread_mutex_t*)mutex);
  return 0;
#endif
}
