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

sem_t lock_socket;

void DEFAULT_CC
lock_init(void)
{
  /* initializing socket lock */
  sem_init(&lock_socket, 0, 1);

  /* initializing chain lock */
  pthread_mutexattr_init(&lock_chain_attr);
  pthread_mutex_init(&lock_chain, &lock_chain_attr);

  /* initializing config lock */
  pthread_mutexattr_init(&lock_config_attr);
  pthread_mutex_init(&lock_config, &lock_config_attr);
}

/******************************************************************************/
void DEFAULT_CC
lock_chain_acquire(void)
{
  /*lock the chain*/
  LOG_DBG("lock_chain_acquire()",0);
  pthread_mutex_lock(&lock_chain);
}

/******************************************************************************/
void DEFAULT_CC
lock_chain_release(void)
{
  /*unlock the chain*/
  LOG_DBG("lock_chain_release()",0);
  pthread_mutex_unlock(&lock_chain);
}

/******************************************************************************/
void DEFAULT_CC
lock_socket_acquire(void)
{
  /* lock socket variable */
  LOG_DBG("lock_socket_acquire()",0);
  sem_wait(&lock_socket);
}

/******************************************************************************/
void DEFAULT_CC
lock_socket_release(void)
{
  /* unlock socket variable */
  LOG_DBG("lock_socket_release()",0);
  sem_post(&lock_socket);
}

