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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_lock.h"
#include "thread_calls.h"

#include <pthread.h>

pthread_mutex_t lock_fork;            /* this lock protects the counters */
pthread_mutexattr_t lock_fork_attr;   /* mutex attributes */
tbus lock_fork_req;                   /* semaphore on which the process that are going to fork suspend on */
tbus lock_fork_wait;                  /* semaphore on which the suspended process wait on */
int lock_fork_forkers_count;          /* threads that want to fork */
int lock_fork_blockers_count;         /* threads that are blocking fork */
int lock_fork_waiting_count;          /* threads suspended until the fork finishes */

void
scp_lock_init(void)
{
    /* initializing fork lock */
    pthread_mutexattr_init(&lock_fork_attr);
    pthread_mutex_init(&lock_fork, &lock_fork_attr);
    lock_fork_req = tc_sem_create(0);
    lock_fork_wait = tc_sem_create(0);

    /* here we don't use locking because lock_init() should be called BEFORE */
    /* any thread is created                                                 */
    lock_fork_blockers_count = 0;
    lock_fork_waiting_count = 0;
    lock_fork_forkers_count = 0;
}

/******************************************************************************/
void
scp_lock_fork_request(void)
{
    /* lock mutex */
    pthread_mutex_lock(&lock_fork);

    if (lock_fork_blockers_count == 0)
    {
        /* if no one is blocking fork(), then we're allowed to fork */
        tc_sem_inc(lock_fork_req);
    }

    lock_fork_forkers_count++;
    pthread_mutex_unlock(&lock_fork);

    /* we wait to be allowed to fork() */
    tc_sem_dec(lock_fork_req);
}

/******************************************************************************/
void
scp_lock_fork_release(void)
{
    pthread_mutex_lock(&lock_fork);
    lock_fork_forkers_count--;

    /* if there's someone else that want to fork, we let him fork() */
    if (lock_fork_forkers_count > 0)
    {
        tc_sem_inc(lock_fork_req);
    }

    for (; lock_fork_waiting_count > 0; lock_fork_waiting_count--)
    {
        /* waking up the other processes */
        tc_sem_inc(lock_fork_wait);
    }

    pthread_mutex_unlock(&lock_fork);
}

/******************************************************************************/
void
scp_lock_fork_critical_section_end(int blocking)
{
    //LOG_DBG("lock_fork_critical_section_end()",0);
    /* lock mutex */
    pthread_mutex_lock(&lock_fork);

    if (blocking == LIBSCP_LOCK_FORK_BLOCKER)
    {
        lock_fork_blockers_count--;
    }

    /* if there's someone who wants to fork and we're the last blocking */
    /* then we let him go */
    if ((lock_fork_blockers_count == 0) && (lock_fork_forkers_count > 0))
    {
        tc_sem_inc(lock_fork_req);
    }

    pthread_mutex_unlock(&lock_fork);
}

/******************************************************************************/
int
scp_lock_fork_critical_section_start(void)
{
    //LOG_DBG("lock_fork_critical_section_start()",0);
    do
    {
        pthread_mutex_lock(&lock_fork);

        /* someone requested to fork */
        if (lock_fork_forkers_count > 0)
        {
            lock_fork_waiting_count++;
            pthread_mutex_unlock(&lock_fork);

            /* we wait until the fork finishes */
            tc_sem_dec(lock_fork_wait);

        }
        else
        {
            /* no fork, so we can go on... */
            lock_fork_blockers_count++;
            pthread_mutex_unlock(&lock_fork);

            return LIBSCP_LOCK_FORK_BLOCKER;
        }
    }
    while (1);

    /* we'll never get here */
    return LIBSCP_LOCK_FORK_WAITING;
}
