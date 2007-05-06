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

   session manager
   linux only

*/

#include "libscp_lock.h"

#include <semaphore.h>
#include <pthread.h>

pthread_mutex_t lock_fork;            /* this lock protects the counters */
pthread_mutexattr_t lock_fork_attr;   /* mutex attributes */
sem_t lock_fork_req;                  /* semaphore on which the process that are going to fork suspend on */
sem_t lock_fork_wait;                 /* semaphore on which the suspended process wait on */
int lock_fork_forkers_count;          /* threads that want to fork */
int lock_fork_blockers_count;         /* threads thar are blocking fork */
int lock_fork_waiting_count;          /* threads suspended until the fork finishes */

void DEFAULT_CC
scp_lock_init(void)
{
  /* initializing fork lock */
  pthread_mutexattr_init(&lock_fork_attr);
  pthread_mutex_init(&lock_fork, &lock_fork_attr);
  sem_init(&lock_fork_req, 0, 0);
  sem_init(&lock_fork_wait, 0, 0);

  /* here we don't use locking because lock_init() should be called BEFORE */
  /* any thread is created                                                 */
  lock_fork_blockers_count=0;
  lock_fork_waiting_count=0;
  lock_fork_forkers_count=0;
}

/******************************************************************************/
void DEFAULT_CC
scp_lock_fork_request(void)
{
  /* lock mutex */
  pthread_mutex_lock(&lock_fork);
  if (lock_fork_blockers_count == 0)
  {
    /* if noone is blocking fork(), then we're allowed to fork */
    sem_post(&lock_fork_req);
  }
  lock_fork_forkers_count++;
  pthread_mutex_unlock(&lock_fork);

  /* we wait to be allowed to fork() */
  sem_wait(&lock_fork_req);
}

/******************************************************************************/
void DEFAULT_CC
scp_lock_fork_release(void)
{
  pthread_mutex_lock(&lock_fork);
  lock_fork_forkers_count--;

  /* if there's someone else that want to fork, we let him fork() */
  if (lock_fork_forkers_count > 0)
  {
    sem_post(&lock_fork_req);
  }

  for (;lock_fork_waiting_count > 0; lock_fork_waiting_count--)
  {
    /* waking up the other processes */
    sem_post(&lock_fork_wait);
  }
  pthread_mutex_unlock(&lock_fork);
}

/******************************************************************************/
void DEFAULT_CC
scp_lock_fork_critical_section_end(int blocking)
{
  //LOG_DBG("lock_fork_critical_secection_end()",0);
  /* lock mutex */
  pthread_mutex_lock(&lock_fork);

  if (blocking == LIBSCP_LOCK_FORK_BLOCKER)
  {
    lock_fork_blockers_count--;
  }

  /* if there's someone who wants to fork and we're the last blocking */
  /* then we let him go */
  if ((lock_fork_blockers_count == 0) && (lock_fork_forkers_count>0))
  {
    sem_post(&lock_fork_req);
  }
  pthread_mutex_unlock(&lock_fork);
}

/******************************************************************************/
int DEFAULT_CC
scp_lock_fork_critical_section_start(void)
{
  //LOG_DBG("lock_fork_critical_secection_start()",0);
  do
  {
    pthread_mutex_lock(&lock_fork);

    /* someone requested to fork */
    if (lock_fork_forkers_count > 0)
    {
      lock_fork_waiting_count++;
      pthread_mutex_unlock(&lock_fork);

      /* we wait until the fork finishes */
      sem_wait(&lock_fork_wait);

    }
    else
    {
      /* no fork, so we can go on... */
      lock_fork_blockers_count++;
      pthread_mutex_unlock(&lock_fork);

      return LIBSCP_LOCK_FORK_BLOCKER;
    }
  } while (1);

  /* we'll never get here */
  return LIBSCP_LOCK_FORK_WAITING;
}

