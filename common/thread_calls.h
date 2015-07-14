/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * thread calls
 */

#if !defined(THREAD_CALLS_H)
#define THREAD_CALLS_H

#include "arch.h"

int APP_CC
tc_thread_create(THREAD_RV (THREAD_CC * start_routine)(void*), void* arg);
tbus APP_CC
tc_get_threadid(void);
int APP_CC
tc_threadid_equal(tbus tid1, tbus tid2);
tbus APP_CC
tc_mutex_create(void);
void APP_CC
tc_mutex_delete(tbus mutex);
int APP_CC
tc_mutex_lock(tbus mutex);
int APP_CC
tc_mutex_unlock(tbus mutex);
tbus APP_CC
tc_sem_create(int init_count);
void APP_CC
tc_sem_delete(tbus sem);
int APP_CC
tc_sem_dec(tbus sem);
int APP_CC
tc_sem_inc(tbus sem);

#endif
