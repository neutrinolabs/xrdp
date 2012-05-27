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

   session manager
   linux only

*/

#include "sesman.h"

extern struct config_sesman* g_cfg; /* in sesman.c */

static tbus g_sync_mutex = 0;
static tbus g_lock_chain = 0;
static tbus g_sync_sem = 0;
static tbus g_lock_socket = 0;

/******************************************************************************/
void APP_CC
lock_init(void)
{
  g_sync_mutex = tc_mutex_create();
  g_lock_chain = tc_mutex_create();
  g_sync_sem = tc_sem_create(0);
  g_lock_socket = tc_sem_create(1);
}

/******************************************************************************/
void APP_CC
lock_deinit(void)
{
  tc_mutex_delete(g_sync_mutex);
  tc_mutex_delete(g_lock_chain);
  tc_sem_delete(g_sync_sem);
  tc_sem_delete(g_lock_socket);
}

/******************************************************************************/
void APP_CC
lock_chain_acquire(void)
{
  /* lock the chain */
  LOG_DBG("lock_chain_acquire()");
  tc_mutex_lock(g_lock_chain);
}

/******************************************************************************/
void APP_CC
lock_chain_release(void)
{
  /* unlock the chain */
  LOG_DBG("lock_chain_release()");
  tc_mutex_unlock(g_lock_chain);
}

/******************************************************************************/
void APP_CC
lock_socket_acquire(void)
{
  /* lock socket variable */
  LOG_DBG("lock_socket_acquire()");
  tc_sem_dec(g_lock_socket);
}

/******************************************************************************/
void APP_CC
lock_socket_release(void)
{
  /* unlock socket variable */
  LOG_DBG("lock_socket_release()");
  tc_sem_inc(g_lock_socket);
}

/******************************************************************************/
void APP_CC
lock_sync_acquire(void)
{
  /* lock sync variable */
  LOG_DBG("lock_sync_acquire()");
  tc_mutex_lock(g_sync_mutex);
}

/******************************************************************************/
void APP_CC
lock_sync_release(void)
{
  /* unlock socket variable */
  LOG_DBG("lock_sync_release()");
  tc_mutex_unlock(g_sync_mutex);
}

/******************************************************************************/
void APP_CC
lock_sync_sem_acquire(void)
{
  /* dec sem */
  LOG_DBG("lock_sync_sem_acquire()");
  tc_sem_dec(g_sync_sem);
}

/******************************************************************************/
void APP_CC
lock_sync_sem_release(void)
{
  /* inc sem */
  LOG_DBG("lock_sync_sem_release()");
  tc_sem_inc(g_sync_sem);
}
