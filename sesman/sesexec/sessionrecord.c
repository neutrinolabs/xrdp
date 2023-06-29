/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Emmanuel Blindauer 2017
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
 */

/**
 *
 * @file sessionrecord.c
 * @brief utmp handling code
 *
 * wtmp/lastlog/btmp is handled by PAM or (on FreeBSD) UTX
 *
 * Idea: Only implement actual utmp, i.e. utmpx for 99%.
 *       See http://80386.nl/unix/utmpx/
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif


#include "sessionrecord.h"
#include "login_info.h"
#include "log.h"

// Operational mode of add_xtmp_entry()
//
// We can't use USER_PROCESS/DEAD_PROCESS directly, as they
// won't be available for platforms without USE_UTMP
enum add_xtmp_mode
{
    MODE_LOGIN,
    MODE_LOGOUT
};

#ifdef USE_UTMP

#include <sys/time.h>
#include <unistd.h>

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
typedef struct utmpx _utmp;
#else
#include <utmp.h>
typedef struct utmp _utmp;
#endif

#endif // USE_UTMP

#include "os_calls.h"
#include "string_calls.h"

#define XRDP_LINE_FORMAT "xrdp:%d"
// ut_id is a very small field on some platforms, so use the display
// number in hex
#define XRDP_ID_FORMAT ":%x"

/*
 * Prepare the utmp struct and write it.
 * this can handle login and logout at once with the 'mode' parameter
 */

static void
add_xtmp_entry(int pid, int display, const struct login_info *login_info,
               enum add_xtmp_mode mode, const struct proc_exit_status *e)
{
#if USE_UTMP
    char idbuff[16];
    char str_display[16];

    _utmp ut;
    struct timeval tv;

    g_memset(&ut, 0, sizeof(ut));
    g_snprintf(str_display, sizeof(str_display), XRDP_LINE_FORMAT, display);
    g_snprintf(idbuff, sizeof(idbuff), XRDP_ID_FORMAT, display);
    gettimeofday(&tv, NULL);

    ut.ut_type = (mode == MODE_LOGIN) ? USER_PROCESS : DEAD_PROCESS;
    ut.ut_pid = pid;
    ut.ut_tv.tv_sec = tv.tv_sec;
    ut.ut_tv.tv_usec = tv.tv_usec;
    g_strncpy(ut.ut_line, str_display, sizeof(ut.ut_line));
    g_strncpy(ut.ut_id, idbuff, sizeof(ut.ut_id));
    if (login_info != NULL)
    {
        g_strncpy(ut.ut_user, login_info->username, sizeof(ut.ut_user));
#ifdef HAVE_UTMPX_UT_HOST
        g_strncpy(ut.ut_host, login_info->ip_addr, sizeof(ut.ut_host));
#endif
    }

#ifdef HAVE_UTMPX_UT_EXIT
    if (e != NULL && e->reason == E_PXR_STATUS_CODE)
    {
        ut.ut_exit.e_exit = e->val;
    }
    else if (e != NULL && e->reason == E_PXR_SIGNAL)
    {
        ut.ut_exit.e_termination = e->val;
    }
#endif

    /* update the utmp file */
    /* open utmp */
    setutxent();
    /* add the computed entry */
    pututxline(&ut);
    /* closes utmp */
    endutxent();

#endif // USE_UTMP
}

void
utmp_login(int pid, int display, const struct login_info *login_info)
{
    log_message(LOG_LEVEL_DEBUG,
                "adding login info for utmp: %d - %d - %s - %s",
                pid, display, login_info->username, login_info->ip_addr);

    add_xtmp_entry(pid, display, login_info, MODE_LOGIN, NULL);
}

void
utmp_logout(int pid, int display, const struct proc_exit_status *exit_status)
{

    log_message(LOG_LEVEL_DEBUG, "adding logout info for utmp: %d - %d",
                pid, display);

    add_xtmp_entry(pid, display, NULL, MODE_LOGOUT, exit_status);
}
