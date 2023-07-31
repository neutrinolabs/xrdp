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
 */

/**
 *
 * @file xrdp_main_utils.c
 * @brief Functions used by XRDP's main() routine and needed elsewhere.
 * @author Jay Sorg, Christopher Pitstick
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "log.h"

#define THREAD_WAITING 100

static long g_threadid = 0; /* main threadid */

static long g_sync_mutex = 0;
static long g_sync1_mutex = 0;
static tbus g_term_event = 0;
static tbus g_sigchld_event = 0;
static tbus g_sync_event = 0;
/* synchronize stuff */
static int g_sync_command = 0;
static long g_sync_result = 0;
static long g_sync_param1 = 0;
static long g_sync_param2 = 0;
static long (*g_sync_func)(long param1, long param2);

/*****************************************************************************/
/* This function is used to run a function from the main thread.
   Sync_func is the function pointer that will run from main thread
   The function can have two long in parameters and must return long */
long
g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1,
            long sync_param2)
{
    long sync_result;
    int sync_command;

    /* If the function is called from the main thread, the function can
     * be called directly. g_threadid= main thread ID*/
    if (tc_threadid_equal(tc_get_threadid(), g_threadid))
    {
        /* this is the main thread, call the function directly */
        /* in fork mode, this always happens too */
        sync_result = sync_func(sync_param1, sync_param2);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "g_xrdp_sync processed IN main thread -> continue");
    }
    else
    {
        /* All threads have to wait here until the main thread
         * process the function. g_process_waiting_function() is called
         * from the listening thread. g_process_waiting_function() process the function*/
        tc_mutex_lock(g_sync1_mutex);
        tc_mutex_lock(g_sync_mutex);
        g_sync_param1 = sync_param1;
        g_sync_param2 = sync_param2;
        g_sync_func = sync_func;
        /* set a value THREAD_WAITING so the g_process_waiting_function function
         * know if any function must be processed */
        g_sync_command = THREAD_WAITING;
        tc_mutex_unlock(g_sync_mutex);
        /* set this event so that the main thread know if
         * g_process_waiting_function() must be called */
        g_set_wait_obj(g_sync_event);

        do
        {
            g_sleep(100);
            tc_mutex_lock(g_sync_mutex);
            /* load new value from global to see if the g_process_waiting_function()
             * function has processed the function */
            sync_command = g_sync_command;
            sync_result = g_sync_result;
            tc_mutex_unlock(g_sync_mutex);
        }
        while (sync_command != 0); /* loop until g_process_waiting_function()
                                * has processed the request */
        tc_mutex_unlock(g_sync1_mutex);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "g_xrdp_sync processed BY main thread -> continue");
    }

    return sync_result;
}

/*****************************************************************************/
/* Signal handler for SIGCHLD in the child
 * Note: only signal safe code (eg. setting wait event) should be executed in
 * this function. For more details see `man signal-safety`
 */
static void
xrdp_child_sigchld_handler(int sig)
{
    while (g_waitchild(NULL) > 0)
    {
    }
}

/*****************************************************************************/
/* called in child just after fork */
int
xrdp_child_fork(void)
{
    int pid;
    char text[256];

    /* SIGCHLD in the child is of no interest to us */
    g_signal_child_stop(xrdp_child_sigchld_handler);        /* SIGCHLD */

    g_close_wait_obj(g_term_event);
    g_close_wait_obj(g_sigchld_event);
    g_close_wait_obj(g_sync_event);

    pid = g_getpid();
    g_snprintf(text, 255, "xrdp_%8.8x_main_term", pid);
    g_term_event = g_create_wait_obj(text);
    g_sigchld_event = -1;
    g_snprintf(text, 255, "xrdp_%8.8x_main_sync", pid);
    g_sync_event = g_create_wait_obj(text);
    return 0;
}

/*****************************************************************************/
long
g_get_sync_mutex(void)
{
    return g_sync_mutex;
}

/*****************************************************************************/
void
g_set_sync_mutex(long mutex)
{
    g_sync_mutex = mutex;
}

/*****************************************************************************/
long
g_get_sync1_mutex(void)
{
    return g_sync1_mutex;
}

/*****************************************************************************/
void
g_set_sync1_mutex(long mutex)
{
    g_sync1_mutex = mutex;
}

/*****************************************************************************/
void
g_set_term_event(tbus event)
{
    g_term_event = event;
}

/*****************************************************************************/
void
g_set_sigchld_event(tbus event)
{
    g_sigchld_event = event;
}

/*****************************************************************************/
tbus
g_get_sync_event(void)
{
    return g_sync_event;
}

/*****************************************************************************/
void
g_set_sync_event(tbus event)
{
    g_sync_event = event;
}

/*****************************************************************************/
long
g_get_threadid(void)
{
    return g_threadid;
}

/*****************************************************************************/
void
g_set_threadid(long id)
{
    g_threadid = id;
}

/*****************************************************************************/
tbus
g_get_term(void)
{
    return g_term_event;
}

/*****************************************************************************/
tbus
g_get_sigchld(void)
{
    return g_sigchld_event;
}

/*****************************************************************************/
int
g_is_term(void)
{
    return g_is_wait_obj_set(g_term_event);
}

/*****************************************************************************/
void
g_set_term(int in_val)
{
    if (in_val)
    {
        g_set_wait_obj(g_term_event);
    }
    else
    {
        g_reset_wait_obj(g_term_event);
    }
}

/*****************************************************************************/
void
g_set_sigchld(int in_val)
{
    if (in_val)
    {
        g_set_wait_obj(g_sigchld_event);
    }
    else
    {
        g_reset_wait_obj(g_sigchld_event);
    }
}

/*****************************************************************************/
/*Some function must be called from the main thread.
 if g_sync_command==THREAD_WAITING a function is waiting to be processed*/
void
g_process_waiting_function(void)
{
    tc_mutex_lock(g_sync_mutex);

    if (g_sync_command != 0)
    {
        if (g_sync_func != 0)
        {
            if (g_sync_command == THREAD_WAITING)
            {
                g_sync_result = g_sync_func(g_sync_param1, g_sync_param2);
            }
        }

        g_sync_command = 0;
    }

    tc_mutex_unlock(g_sync_mutex);
}
