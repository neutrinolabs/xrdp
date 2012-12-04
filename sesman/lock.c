/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * session manager
 * linux only
 */

#include "sesman.h"

extern struct config_sesman *g_cfg; /* in sesman.c */

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
