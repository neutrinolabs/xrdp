/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file sig.c
 * @brief signal handling functions
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <signal.h>

#include "sesman.h"

extern int g_sck;
extern int g_pid;
extern struct config_sesman *g_cfg; /* in sesman.c */
extern tbus g_term_event;

/******************************************************************************/
void
sig_sesman_shutdown(int sig)
{
    char pid_file[256];

    log_message(LOG_LEVEL_INFO, "shutting down sesman %d", 1);

    if (g_getpid() != g_pid)
    {
        LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", (g_getpid()), g_pid);
        return;
    }

    LOG_DBG(" - getting signal %d pid %d", sig, g_getpid());

    g_set_wait_obj(g_term_event);

    session_sigkill_all();

    g_snprintf(pid_file, 255, "%s/xrdp-sesman.pid", XRDP_PID_PATH);
    g_file_delete(pid_file);
}

/******************************************************************************/
void
sig_sesman_reload_cfg(int sig)
{
    int error;
    struct config_sesman *cfg;
    char cfg_file[256];

    log_message(LOG_LEVEL_WARNING, "receiving SIGHUP %d", 1);

    if (g_getpid() != g_pid)
    {
        LOG_DBG("g_getpid() [%d] differs from g_pid [%d]", g_getpid(), g_pid);
        return;
    }

    cfg = g_new0(struct config_sesman, 1);

    if (0 == cfg)
    {
        log_message(LOG_LEVEL_ERROR, "error creating new config:  - keeping old cfg");
        return;
    }

    if (config_read(cfg) != 0)
    {
        log_message(LOG_LEVEL_ERROR, "error reading config - keeping old cfg");
        g_free(cfg);
        return;
    }

    /* stop logging subsystem */
    log_end();

    /* free old config data */
    config_free(g_cfg);

    /* replace old config with newly read one */
    g_cfg = cfg;

    g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);

    /* start again logging subsystem */
    error = log_start(cfg_file, "xrdp-sesman");

    if (error != LOG_STARTUP_OK)
    {
        char buf[256];

        switch (error)
        {
            case LOG_ERROR_MALLOC:
                g_printf("error on malloc. cannot restart logging. log stops here, sorry.\n");
                break;
            case LOG_ERROR_FILE_OPEN:
                g_printf("error reopening log file [%s]. log stops here, sorry.\n", getLogFile(buf, 255));
                break;
        }
    }

    log_message(LOG_LEVEL_INFO, "configuration reloaded, log subsystem restarted");
}

/******************************************************************************/
void
sig_sesman_session_end(int sig)
{
    int pid;

    if (g_getpid() != g_pid)
    {
        return;
    }

    pid = g_waitchild();

    if (pid > 0)
    {
        session_kill(pid);
    }
}

/******************************************************************************/
void *
sig_handler_thread(void *arg)
{
    int recv_signal;
    sigset_t sigmask;
    sigset_t oldmask;
    sigset_t waitmask;

    /* mask signals to be able to wait for them... */
    sigfillset(&sigmask);
    /* it is a good idea not to block SIGILL SIGSEGV */
    /* SIGFPE -- see sigaction(2) NOTES              */
    pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);

    /* building the signal wait mask... */
    sigemptyset(&waitmask);
    sigaddset(&waitmask, SIGHUP);
    sigaddset(&waitmask, SIGCHLD);
    sigaddset(&waitmask, SIGTERM);
    sigaddset(&waitmask, SIGINT);

    //  sigaddset(&waitmask, SIGFPE);
    //  sigaddset(&waitmask, SIGILL);
    //  sigaddset(&waitmask, SIGSEGV);

    do
    {
        LOG_DBG("calling sigwait()");
        sigwait(&waitmask, &recv_signal);

        switch (recv_signal)
        {
            case SIGHUP:
                //reload cfg
                //we must stop & restart logging, or copy logging cfg!!!!
                LOG_DBG("sesman received SIGHUP");
                //return 0;
                break;
            case SIGCHLD:
                /* a session died */
                LOG_DBG("sesman received SIGCHLD");
                sig_sesman_session_end(SIGCHLD);
                break;
            case SIGINT:
                /* we die */
                LOG_DBG("sesman received SIGINT");
                sig_sesman_shutdown(recv_signal);
                break;
            case SIGTERM:
                /* we die */
                LOG_DBG("sesman received SIGTERM");
                sig_sesman_shutdown(recv_signal);
                break;
        }
    }
    while (1);

    return 0;
}
