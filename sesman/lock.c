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

#include "sesman.h"

#include <semaphore.h>
#include <pthread.h>

pthread_mutex_t lock_chain;           /* session chain lock */
pthread_mutexattr_t lock_chain_attr;  /* mutex attributes */

pthread_mutex_t lock_config;          /* configuration access lock */
pthread_mutexattr_t lock_config_attr; /* mutex attributes */

pthread_mutex_t lock_fork;            /* this lock protects the counters */
pthread_mutexattr_t lock_fork_attr;   /* mutex attributes */
sem_t lock_fork_req;                  /* semaphore on which the process that are going to fork suspend on */
sem_t lock_fork_wait;                 /* semaphore on which the suspended process wait on */
int lock_fork_forkers_count;          /* threads that want to fork */
int lock_fork_blockers_count;         /* threads thar are blocking fork */
int lock_fork_waiting_count;          /* threads suspended until the fork finishes */

sem_t lock_socket;

void DEFAULT_CC
lock_init()
{
  /* initializing socket lock */
  sem_init(&lock_socket, 0, 1);

  /* initializing chain lock */
  pthread_mutexattr_init(&lock_chain_attr);
  pthread_mutex_init(&lock_chain, &lock_chain_attr);
  
  /* initializing config lock */
  pthread_mutexattr_init(&lock_config_attr);
  pthread_mutex_init(&lock_config, &lock_config_attr);
  
  /* initializing fork lock */
  pthread_mutexattr_init(&lock_fork_attr);
  pthread_mutex_init(&lock_chain, &lock_fork_attr);
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
lock_chain_acquire()
{
  /*lock the chain*/
  LOG_DBG("lock_chain_acquire()",0);
  pthread_mutex_lock(&lock_chain);
}

/******************************************************************************/
void DEFAULT_CC
lock_chain_release()
{
  /*unlock the chain*/
  LOG_DBG("lock_chain_release()",0);
  pthread_mutex_unlock(&lock_chain);
}

/******************************************************************************/
void DEFAULT_CC
lock_socket_acquire()
{
  /* lock socket variable */
  LOG_DBG("lock_socket_acquire()",0);
  sem_wait(&lock_socket);
}

/******************************************************************************/
void DEFAULT_CC
lock_socket_release()
{
  /* unlock socket variable */
  LOG_DBG("lock_socket_release()",0);
  sem_post(&lock_socket);
}

/******************************************************************************/
void DEFAULT_CC
lock_fork_request()
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
lock_fork_release()
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
lock_fork_critical_section_end(int blocking)
{
  LOG_DBG("lock_fork_critical_secection_end()",0);
  /* lock mutex */
  pthread_mutex_lock(&lock_fork);

  if (blocking == SESMAN_LOCK_FORK_BLOCKER)
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
lock_fork_critical_section_start()
{
  LOG_DBG("lock_fork_critical_secection_start()",0);
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

      return SESMAN_LOCK_FORK_BLOCKER;
    }
  } while (1);

  /* we'll never get here */
  return SESMAN_LOCK_FORK_WAITING;
}
