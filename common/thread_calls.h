/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   thread calls

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
