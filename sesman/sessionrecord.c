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
 * @brief utmp/wtmp handling code
 * Idea: Only implement actual utmp, i.e. utmpx for 99%.
 *       See http://80386.nl/unix/utmpx/
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "log.h"
#include "os_calls.h"
#include "sessionrecord.h"

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
typedef struct utmpx _utmp;
#else
#include <utmpx.h>
typedef struct utmp _utmp;
#endif


#define XRDP_LINE_FORMAT "xrdp:%d"

/*
 * update the wtmp file on UTMPX platforms (~ Linux)
 * but no on FreeBSD : FreeBSD uses utx to do the job
 */
#ifdef HAVE_UTMPX_H
#if !defined(__FreeBSD__)
static inline void
_updwtmp(const _utmp *ut)
{
    updwtmpx(_PATH_WTMP, ut);
}
#else
static inline void
_updwtmp(const _utmp ut)
{
}
#endif
#elif defined(HAVE_UTMP_H)
/* Does such system still exist ? */
_updwtmp(const _utmp *ut)
{
    log_message(LOG_LEVEL_DEBUG,
            "Unsupported system: HAVE_UTMP_H defined without HAVE_UTMPX_H");
    updwtmp("/var/log/wtmp", ut);
}
#endif


/*
 * Prepare the utmp struct and write it.
 * this can handle login and logout at once with the 'state' parameter
 */

void
add_xtmp_entry(int pid, const char *display_id, const char *user, const char *rhostname, const short state)
{
    _utmp ut;
    struct timeval tv;
    char *hostname = 0;

    /* The string rhostname containt too much data, only get the ip
     * the format is
     * "2001:123:12:1234:1234:1234:1234:1234:53194 - socket: 12"
     * "::ffff:99.99.9.999:51165 - socket: 12"
     * "99.99.9.999:51165 - socket: 12"
     *
     * So the IP is the string up the two last colons
     */
    int i = g_strlen(rhostname) - 1;
    while ((i > 0) && (rhostname[i] != ':'))
    {
        i--;
    }
    i--;
    while ((i > 0) && (rhostname[i] != ':'))
    {
        i--;
    }

    hostname = g_strndup(rhostname, i);

    g_memset(&ut, 0, sizeof(ut));

    ut.ut_type = state;
    ut.ut_pid = pid;
    gettimeofday(&tv, NULL);
    ut.ut_tv.tv_sec = tv.tv_sec;
    ut.ut_tv.tv_usec = tv.tv_usec;
    g_strncpy(ut.ut_line, display_id , sizeof(ut.ut_line));
    g_strncpy(ut.ut_user, user , sizeof(ut.ut_user));
    g_strncpy(ut.ut_host, hostname, sizeof(ut.ut_host));

    /* update the utmp file */
    /* open utmp */
    setutxent();
    /* add the computed entry */
    pututxline(&ut);
    /* closes utmp */
    endutxent();

    /* update the wtmp file if needed */

    _updwtmp(&ut);

    g_free(hostname);
}

void
utmp_login(int pid, int display, const char *user, const char *rhostname)
{
    char str_display[16];

    log_message(LOG_LEVEL_DEBUG,
                "adding login info for utmp/wtmp: %d - %d - %s - %s",
                pid, display, user, rhostname);
    g_snprintf(str_display, 15, XRDP_LINE_FORMAT, display);

    add_xtmp_entry(pid, str_display, user, rhostname, USER_PROCESS);
}

void
utmp_logout(int pid, int display, const char *user, const char *rhostname)
{
    char str_display[16];

    log_message(LOG_LEVEL_DEBUG,
                "adding logout info for utmp/wtmp: %d - %d - %s - %s",
                pid, display, user, rhostname);
    g_snprintf(str_display, 15, XRDP_LINE_FORMAT, display);

    add_xtmp_entry(pid, str_display, user, rhostname, DEAD_PROCESS);
}
