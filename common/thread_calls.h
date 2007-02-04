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

*/

#if !defined(THREAD_CALLS_H)
#define THREAD_CALLS_H

#include "arch.h"

int APP_CC
tc_thread_create(THREAD_RV (THREAD_CC * start_routine)(void*), void* arg);
long APP_CC
tc_get_threadid(void);
long APP_CC
tc_mutex_create(void);
void APP_CC
tc_mutex_delete(long mutex);
int APP_CC
tc_mutex_lock(long mutex);
int APP_CC
tc_mutex_unlock(long mutex);
long APP_CC
tc_sem_create(int init_count);
void APP_CC
tc_sem_delete(long sem);
int APP_CC
tc_sem_dec(long sem);
int APP_CC
tc_sem_inc(long sem);

#endif
