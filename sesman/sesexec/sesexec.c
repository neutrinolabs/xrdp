/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Matt Burt 2023
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
 * @file sesexec.c
 * @brief Main program file for session executive process
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <ctype.h>
#include <stdarg.h>

#include "arch.h"
#include "eicp.h"
#include "eicp_server.h"
#include "ercp.h"
#include "ercp_server.h"
#include "login_info.h"
#include "sesexec.h"
#include "sesman_config.h"
#include "log.h"
#include "os_calls.h"
#include "session.h"
#include "string_calls.h"
#include "trans.h"
#include "xrdp_sockets.h"

struct startup_params
{
    const char *sesman_ini;
};

/*
 * Program-scope globals
 */
struct config_sesman *g_cfg;
unsigned char g_fixedkey[8] = { 23, 82, 107, 6, 35, 78, 88, 7 };
struct login_info *g_login_info;
struct session_data *g_session_data;

tintptr g_term_event = 0;
tintptr g_sigchld_event = 0;
pid_t g_pid;

/*
 * Module-scope globals
 */
static struct trans *g_ecp_trans;
static int g_terminate_loop = 0;
static int g_terminate_status = 0;

/*****************************************************************************/
/**
 * Command line argument parser
 * @param[in] argc number of command line arguments
 * @param[in] argv pointer array of commandline arguments
 * @param[out] startup_params Returned startup parameters
 * @return 0 on success
 */
static int
process_params(int argc, char **argv,
               struct startup_params *startup_params)
{
    int index;
    const char *option;
    const char *value;

    startup_params->sesman_ini = DEFAULT_SESMAN_INI;

    index = 1;

    while (index < argc)
    {
        option = argv[index];

        if (index + 1 < argc)
        {
            value = argv[index + 1];
        }
        else
        {
            value = "";
        }

        if (g_strcmp(option, "-c") == 0)
        {
            index++;
            startup_params->sesman_ini = value;
        }
        else /* unknown option */
        {
            return index;
        }

        index++;
    }

    return 0;
}

/******************************************************************************/
#if 0
static int
sesexec_scp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = scp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        struct sesman_con *sc = (struct sesman_con *)self->callback_data;
        //if ((rv = scp_process(sc)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s: scp_process failed", __func__);
        }
        scp_msg_in_reset(self);
    }

    return rv;
}
#endif

/******************************************************************************/
static int
sesexec_eicp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = eicp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        if ((rv = eicp_server(self)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s: eicp_server failed", __func__);
        }
        eicp_msg_in_reset(self);
    }

    return rv;
}

/******************************************************************************/
int
sesexec_ercp_data_in(struct trans *self)
{
    int rv;
    int available;

    rv = ercp_msg_in_check_available(self, &available);

    if (rv == 0 && available)
    {
        if ((rv = ercp_server(self)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s: ercp_server failed", __func__);
        }
        ercp_msg_in_reset(self);
    }

    return rv;
}

/******************************************************************************/
/**
 * Informs the main loop a termination signal has been received
 */
static void
set_term_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_term_event);
    }
}

/*****************************************************************************/
/* No-op signal handler.
 */
static void
sig_no_op(int sig)
{
    /* no-op */
}

/******************************************************************************/
/**
 * Informs the main loop a child exiting signal has been received
 */
static void
set_sigchld_event(int sig)
{
    /* Don't try to use a wait obj in a child process */
    if (g_getpid() == g_pid)
    {
        g_set_wait_obj(g_sigchld_event);
    }
}

/******************************************************************************/
int
sesexec_is_term(void)
{
    return g_terminate_loop || g_is_wait_obj_set(g_term_event);
}

/******************************************************************************/
void
sesexec_terminate_main_loop(int status)
{
    g_terminate_loop = 1;
    g_terminate_status = status;
}

/******************************************************************************/
static void
process_sigchld_event(void)
{
    struct proc_exit_status e;
    int pid;

    // Check for any finished children
    while ((pid = g_waitchild(&e)) > 0)
    {
        session_process_child_exit(g_session_data, pid, &e);
    }
}

/******************************************************************************/
/**
 *
 * @brief Starts sesexec main loop
 *
 */
static int
sesexec_main_loop(void)
{
    int error = 0;
    int robjs_count;
    intptr_t robjs[32];

    g_terminate_loop = 0;
    g_terminate_status = 0;
    g_login_info = NULL;

    while (!g_terminate_loop)
    {
        robjs_count = 0;
        robjs[robjs_count++] = g_term_event;
        robjs[robjs_count++] = g_sigchld_event;

        error = trans_get_wait_objs(g_ecp_trans, robjs, &robjs_count);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesexec_main_loop: "
                "trans_get_wait_objs(ECP) failed");
            sesexec_terminate_main_loop(error);
            continue;
        }

        if (g_obj_wait(robjs, robjs_count, NULL, 0, -1) != 0)
        {
            /* should not get here */
            LOG(LOG_LEVEL_WARNING, "sesexec_main_loop: "
                "Unexpected error from g_obj_wait()");
            g_sleep(100);
            continue;
        }

        if (g_is_wait_obj_set(g_term_event)) /* term */
        {
            g_reset_wait_obj(g_term_event);
            if (session_active(g_session_data))
            {
                // Ask the active session to terminate
                LOG(LOG_LEVEL_INFO, "sesexec_main_loop: "
                    "sesexec asked to terminate. "
                    "Terminating active session");
                session_send_term(g_session_data);
            }
            else
            {
                // Terminate immediately
                LOG(LOG_LEVEL_INFO, "sesexec_main_loop: "
                    "sesexec asked to terminate. "
                    "No session is active");
                sesexec_terminate_main_loop(0);
                continue;
            }
        }

        if (g_is_wait_obj_set(g_sigchld_event)) /* SIGCHLD */
        {
            g_reset_wait_obj(g_sigchld_event);

            // See whether the session goes from active to inactive
            // after processing SIGCHLD
            int session_was_active = session_active(g_session_data);
            process_sigchld_event();
            if (session_was_active && !session_active(g_session_data))
            {
                // We've finished the session. Tell sesman and
                // finish up.
                (void)ercp_send_session_finished_event(g_ecp_trans);

                session_data_free(g_session_data);
                g_session_data = NULL;
                sesexec_terminate_main_loop(0);
                continue;
            }
        }

        error = trans_check_wait_objs(g_ecp_trans);
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "sesexec_main_loop: "
                "trans_check_wait_objs failed for ECP transport");
            sesexec_terminate_main_loop(error);
            continue;
        }
    }

    login_info_free(g_login_info);

    return g_terminate_status;
}

/******************************************************************************/
static int start_logging(const char *sesman_ini)
{
    char text[256];
    int rv = 1;
    if (!g_file_exist(sesman_ini))
    {
        g_printf("Config file %s does not exist\n", sesman_ini);
    }
    else
    {
        enum logReturns log_error;
        log_error = log_start(sesman_ini, "xrdp-sesexec", 0);

        if (log_error != LOG_STARTUP_OK)
        {
            switch (log_error)
            {
                case LOG_ERROR_MALLOC:
                    g_writeln("error on malloc. cannot start logging. quitting.");
                    break;
                case LOG_ERROR_FILE_OPEN:
                    g_writeln("error opening log file [%s]. quitting.",
                              getLogFile(text, sizeof(text) - 1));
                    break;
                default:
                    // Assume sufficient messages have already been generated
                    break;
            }
        }
        else
        {
            rv = 0;
        }
    }

    return rv;
}

/******************************************************************************/
static int
get_eicp_fd(char errstr[], unsigned int errstr_size)
{
    const char *s =  g_getenv("EICP_FD");
    const char *p;
    int fd;

    errstr[0] = '\0';

    if (s == NULL || s[0] == '\0')
    {
        g_snprintf(errstr, errstr_size,
                   "Can't read EICP_FD environment variable");
        return -1;
    }

    for (p = s ; isdigit(*p) ; ++p)
    {
        ;
    }

    if (*p != '\0')
    {
        g_snprintf(errstr, errstr_size, "EICP_FD has non-digit char '%c'", *p);
        return -1;
    }

    if ((p - s) > 4)
    {
        g_snprintf(errstr, errstr_size, "EICP_FD has too many digits");
        return -1;
    }

    fd = g_atoi(s);
    if (!g_file_is_open(fd))
    {
        g_snprintf(errstr, errstr_size, "EICP_FD %d is not open", fd);
        return -1;
    }

    return fd;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int error = 1;
    struct startup_params startup_params = {0};
    int errored_argc;
    int eicp_fd;
    char eicp_errstr[128];
    /*
     * Check the EICP transport file descriptor is provided and open
     * before opening any log files, config files, etc. We then open
     * log files, and log errors at that point */
    eicp_fd = get_eicp_fd(eicp_errstr, sizeof(eicp_errstr));

    g_init("xrdp-sesexec");

    //g_sleep(15 * 1000);
    errored_argc = process_params(argc, argv, &startup_params);
    if (errored_argc > 0)
    {
        g_writeln("Unknown option: %s", argv[errored_argc]);
    }
    /* starting logging subsystem
     *
     * For historic reasons, we share a log file with sesman */
    else if (start_logging(startup_params.sesman_ini) == 0)
    {
        /* reading config
         *
         * For historic reasons, we share a config with sesman */
        if ((g_cfg = config_read(startup_params.sesman_ini)) == NULL)
        {
            LOG(LOG_LEVEL_ALWAYS, "error reading config %s: %s",
                startup_params.sesman_ini, g_get_strerror());
        }
        else if (eicp_fd < 0)
        {
            LOG(LOG_LEVEL_ERROR, "%s", eicp_errstr);
        }
        else
        {
            char text[128];

            g_pid = g_getpid();

            /* signal handling */
            g_snprintf(text, sizeof(text), "xrdp_sesexec_%8.8x_main_term",
                       g_pid);
            g_term_event = g_create_wait_obj(text);
            g_snprintf(text, sizeof(text), "xrdp_sesexec_%8.8x_sigchld",
                       g_pid);
            g_sigchld_event = g_create_wait_obj(text);

            // No need to terminate on SIGINT for sesexec. This can
            // also make it hard to debug sessions.
            //g_signal_user_interrupt(set_term_event);
            g_signal_terminate(set_term_event); /* SIGTERM */
            g_signal_pipe(sig_no_op);          /* SIGPIPE */
            g_signal_child_stop(set_sigchld_event);

            /* Set up an EICP process handler
             * Errors are logged by this call if necessary */
            g_ecp_trans = eicp_init_trans_from_fd(eicp_fd,
                                                  TRANS_TYPE_SERVER,
                                                  sesexec_is_term);
            if (g_ecp_trans != NULL)
            {
                g_ecp_trans->trans_data_in = sesexec_eicp_data_in;
                g_ecp_trans->callback_data = NULL;

                /* start program main loop */
                LOG(LOG_LEVEL_INFO, "starting xrdp-sesexec with pid %d", g_pid);
                error = sesexec_main_loop();
                trans_delete(g_ecp_trans);
            }

            g_delete_wait_obj(g_term_event);
        }
        config_free(g_cfg);
        log_end();
    }

    g_deinit();
    return error;
}
