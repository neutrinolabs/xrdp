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
 * main program
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdarg.h>

#include "xrdp.h"
#include "log.h"
#include "xrdp_configure_options.h"
#include "string_calls.h"

#if !defined(PACKAGE_VERSION)
#define PACKAGE_VERSION "???"
#endif

#define THREAD_WAITING 100

static struct xrdp_listen *g_listen = 0;
static long g_threadid = 0; /* main threadid */

static long g_sync_mutex = 0;
static long g_sync1_mutex = 0;
static tbus g_term_event = 0;
static tbus g_sync_event = 0;
/* synchronize stuff */
static int g_sync_command = 0;
static long g_sync_result = 0;
static long g_sync_param1 = 0;
static long g_sync_param2 = 0;
static long (*g_sync_func)(long param1, long param2);

/*****************************************************************************/
static void
print_version(void)
{
    g_writeln("xrdp %s", PACKAGE_VERSION);
    g_writeln("  A Remote Desktop Protocol Server.");
    g_writeln("  Copyright (C) 2004-2020 Jay Sorg, "
              "Neutrino Labs, and all contributors.");
    g_writeln("  See https://github.com/neutrinolabs/xrdp for more information.");
    g_writeln("%s", "");

#if defined(XRDP_CONFIGURE_OPTIONS)
    g_writeln("  Configure options:");
    g_writeln("%s", XRDP_CONFIGURE_OPTIONS);
#endif

    g_writeln("  Compiled with %s", get_openssl_version());

}

/*****************************************************************************/
static void
print_help(void)
{
    g_writeln("Usage: xrdp [options]");
    g_writeln("   -k, --kill        shut down xrdp");
    g_writeln("   -h, --help        show help");
    g_writeln("   -v, --version     show version");
    g_writeln("   -n, --nodaemon    don't fork into background");
    g_writeln("   -p, --port        tcp listen port");
    g_writeln("   -f, --fork        fork on new connection");
    g_writeln("   -c, --config      specify new path to xrdp.ini");
    g_writeln("       --dump-config display config on stdout on startup");
}

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
/* Signal handler for SIGINT and SIGTERM
 * Note: only signal safe code (eg. setting wait event) should be executed in
 * this funciton. For more details see `man signal-safety`
 */
static void
xrdp_shutdown(int sig)
{
    if (!g_is_wait_obj_set(g_term_event))
    {
        g_set_wait_obj(g_term_event);
    }
}

/*****************************************************************************/
/* Signal handler for SIGCHLD
 * Note: only signal safe code (eg. setting wait event) should be executed in
 * this funciton. For more details see `man signal-safety`
 */
static void
xrdp_child(int sig)
{
    int safety;

    for (safety = 0; (g_waitchild() >= 0) && (safety <= 10); safety++)
    {
    }
}

/*****************************************************************************/
/* No-op signal handler.
 * Note: only signal safe code (eg. setting wait event) should be executed in
 * this funciton. For more details see `man signal-safety`
 */
static void
xrdp_sig_no_op(int sig)
{
    /* no-op */
}

/*****************************************************************************/
/* called in child just after fork */
int
xrdp_child_fork(void)
{
    int pid;
    char text[256];

    /* close, don't delete these */
    g_close_wait_obj(g_term_event);
    g_close_wait_obj(g_sync_event);
    pid = g_getpid();
    g_snprintf(text, 255, "xrdp_%8.8x_main_term", pid);
    g_term_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_%8.8x_main_sync", pid);
    g_sync_event = g_create_wait_obj(text);
    return 0;
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
tbus
g_get_term_event(void)
{
    return g_term_event;
}

/*****************************************************************************/
tbus
g_get_sync_event(void)
{
    return g_sync_event;
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

/*****************************************************************************/
/**
 * @brief looks for a case-insensitive match of a string in a list
 * @param candidate  String to match
 * @param ... NULL-terminated list of strings to compare the candidate with
 * @return !=0 if the candidate is found in the list
 */
static int nocase_matches(const char *candidate, ...)
{
    va_list vl;
    const char *member;
    int result = 0;

    va_start(vl, candidate);
    while ((member = va_arg(vl, const char *)) != NULL)
    {
        if (g_strcasecmp(candidate, member) == 0)
        {
            result = 1;
            break;
        }
    }

    va_end(vl);
    return result;
}


/*****************************************************************************/
/**
 *
 * @brief  Command line argument parser
 * @param  number of command line arguments
 * @param  pointer array of commandline arguments
 * @param  [out] Returned startup parameters
 * @return 0 on success, n on nth argument is unknown
 *
 */
static int
xrdp_process_params(int argc, char **argv,
                    struct xrdp_startup_params *startup_params)
{
    int index;
    const char *option;
    const char *value;

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

        if (nocase_matches(option, "-help", "--help", "-h", NULL))
        {
            startup_params->help = 1;
        }
        else if (nocase_matches(option, "-kill", "--kill", "-k", NULL))
        {
            startup_params->kill = 1;
        }
        else if (nocase_matches(option, "-nodaemon", "--nodaemon", "-n",
                                "-nd", "--nd", "-ns", "--ns", NULL))
        {
            startup_params->no_daemon = 1;
        }
        else if (nocase_matches(option, "-v", "--version", NULL))
        {
            startup_params->version = 1;
        }
        else if (nocase_matches(option, "-p", "--port", NULL))
        {
            index++;
            g_strncpy(startup_params->port, value,
                      sizeof(startup_params->port) - 1);

            if (g_strlen(startup_params->port) < 1)
            {
                g_writeln("error processing params, port [%s]", startup_params->port);
                return 1;
            }
            else
            {
                g_writeln("--port parameter found, ini override [%s]",
                          startup_params->port);
            }
        }
        else if (nocase_matches(option, "-f", "--fork", NULL))
        {
            startup_params->fork = 1;
            g_writeln("--fork parameter found, ini override");
        }
        else if (nocase_matches(option, "--dump-config", NULL))
        {
            startup_params->dump_config = 1;
        }
        else if (nocase_matches(option, "-c", "--config", NULL))
        {
            index++;
            startup_params->xrdp_ini = value;
        }
        else /* unknown option */
        {
            return index;
        }

        index++;
    }

    return 0;
}

/*****************************************************************************/
/* Basic sanity checks before any forking */
static int
xrdp_sanity_check(void)
{
    int intval = 1;
    int host_be;
    const char *key_file = XRDP_CFG_PATH "/rsakeys.ini";

    /* check compiled endian with actual endian */
    host_be = !((int)(*(unsigned char *)(&intval)));

#if defined(B_ENDIAN)
    if (!host_be)
    {
        g_writeln("Not a big endian machine, edit arch.h");
        return 1;
    }
#endif
#if defined(L_ENDIAN)
    if (host_be)
    {
        g_writeln("Not a little endian machine, edit arch.h");
        return 1;
    }
#endif

    /* check long, int and void* sizes */
    if (sizeof(int) != 4)
    {
        g_writeln("unusable int size, must be 4");
        return 1;
    }

    if (sizeof(long) != sizeof(void *))
    {
        g_writeln("long size must match void* size");
        return 1;
    }

    if (sizeof(long) != 4 && sizeof(long) != 8)
    {
        g_writeln("unusable long size, must be 4 or 8");
        return 1;
    }

    if (sizeof(tui64) != 8)
    {
        g_writeln("unusable tui64 size, must be 8");
        return 1;
    }

    if (!g_file_exist(key_file))
    {
        g_writeln("File %s is missing, create it using xrdp-keygen", key_file);
        return 1;
    }

    return 0;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    int exit_status = 0;
    enum logReturns error;
    struct xrdp_startup_params startup_params = {0};
    int pid;
    int fd;
    int daemon;
    char text[256];
    const char *pid_file = XRDP_PID_PATH "/xrdp.pid";
    int errored_argc;

#ifdef USE_DEVEL_LOGGING
    int test;
    for (test = 0; test < argc; test++)
    {
        g_writeln("Argument %i - %s", test, argv[test]);
    }
#endif

    g_init("xrdp");
    ssl_init();

    startup_params.xrdp_ini = XRDP_CFG_PATH "/xrdp.ini";

    errored_argc = xrdp_process_params(argc, argv, &startup_params);
    if (errored_argc > 0)
    {
        print_version();
        g_writeln("%s", "");
        print_help();
        g_writeln("%s", "");

        g_writeln("Unknown option: %s", argv[errored_argc]);
        g_deinit();
        g_exit(1);
    }

    if (startup_params.help)
    {
        print_version();
        g_writeln("%s", "");
        print_help();

        g_deinit();
        g_exit(0);
    }

    if (startup_params.version)
    {
        print_version();
        g_deinit();
        g_exit(0);
    }

    if (xrdp_sanity_check() != 0)
    {
        g_writeln("Fatal error occurred, exiting");
        g_deinit();
        g_exit(1);
    }

    if (startup_params.kill)
    {
        g_writeln("stopping xrdp");
        /* read the xrdp.pid file */
        fd = -1;

        if (g_file_exist(pid_file)) /* xrdp.pid */
        {
            fd = g_file_open(pid_file); /* xrdp.pid */
        }

        if (fd == -1)
        {
            g_writeln("cannot open %s, maybe xrdp is not running", pid_file);
        }
        else
        {
            g_memset(text, 0, 32);
            g_file_read(fd, text, 31);
            pid = g_atoi(text);
            g_writeln("stopping process id %d", pid);

            if (pid > 0)
            {
                g_sigterm(pid);
            }

            g_file_close(fd);
        }

        g_deinit();
        g_exit(0);
    }

    /* starting logging subsystem */
    error = log_start(startup_params.xrdp_ini, "xrdp",
                      startup_params.dump_config);

    if (error != LOG_STARTUP_OK)
    {
        switch (error)
        {
            case LOG_ERROR_MALLOC:
                g_writeln("error on malloc. cannot start logging. quitting.");
                break;
            case LOG_ERROR_FILE_OPEN:
                g_writeln("error opening log file [%s]. quitting.",
                          getLogFile(text, 255));
                break;
            case LOG_ERROR_NO_CFG:
                g_writeln("config file %s unreadable or missing",
                          startup_params.xrdp_ini);
                break;
            default:
                g_writeln("log_start error");
                break;
        }

        g_deinit();
        g_exit(1);
    }



    if (g_file_exist(pid_file)) /* xrdp.pid */
    {
        g_writeln("It looks like xrdp is already running.");
        g_writeln("If not, delete %s and try again.", pid_file);
        g_deinit();
        g_exit(0);
    }

    daemon = !startup_params.no_daemon;


    if (daemon)
    {

        /* make sure containing directory exists */
        g_create_path(pid_file);

        /* make sure we can write to pid file */
        fd = g_file_open(pid_file); /* xrdp.pid */

        if (fd == -1)
        {
            g_writeln("running in daemon mode with no access to pid files, quitting");
            g_deinit();
            g_exit(0);
        }

        if (g_file_write(fd, "0", 1) == -1)
        {
            g_writeln("running in daemon mode with no access to pid files, quitting");
            g_deinit();
            g_exit(0);
        }

        g_file_close(fd);
        g_file_delete(pid_file);
    }

    if (daemon)
    {
        /* if can't listen, exit with failure status */
        if (xrdp_listen_test(&startup_params) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Failed to start xrdp daemon, "
                "possibly address already in use.");
            g_deinit();
            /* must exit with failure status,
               or systemd cannot detect xrdp daemon couldn't start properly */
            g_exit(1);
        }
        /* start of daemonizing code */
        pid = g_fork();

        if (pid == -1)
        {
            g_writeln("problem forking");
            g_deinit();
            g_exit(1);
        }

        if (0 != pid)
        {
            g_writeln("daemon process %d started ok", pid);
            /* exit, this is the main process */
            g_deinit();
            g_exit(0);
        }

        g_sleep(1000);
        /* write the pid to file */
        pid = g_getpid();
        fd = g_file_open(pid_file); /* xrdp.pid */

        if (fd == -1)
        {
            g_writeln("trying to write process id to xrdp.pid");
            g_writeln("problem opening xrdp.pid");
            g_writeln("maybe no rights");
        }
        else
        {
            g_sprintf(text, "%d", pid);
            g_file_write(fd, text, g_strlen(text));
            g_file_close(fd);
        }

        g_sleep(1000);
        g_file_close(0);
        g_file_close(1);
        g_file_close(2);

        if (g_file_open("/dev/null") < 0)
        {
        }

        if (g_file_open("/dev/null") < 0)
        {
        }

        if (g_file_open("/dev/null") < 0)
        {
        }

        /* end of daemonizing code */
    }

    g_threadid = tc_get_threadid();
    g_listen = xrdp_listen_create();
    g_signal_user_interrupt(xrdp_shutdown); /* SIGINT */
    g_signal_pipe(xrdp_sig_no_op);          /* SIGPIPE */
    g_signal_terminate(xrdp_shutdown);      /* SIGTERM */
    g_signal_child_stop(xrdp_child);        /* SIGCHLD */
    g_signal_hang_up(xrdp_sig_no_op);       /* SIGHUP */
    g_sync_mutex = tc_mutex_create();
    g_sync1_mutex = tc_mutex_create();
    pid = g_getpid();
    LOG(LOG_LEVEL_INFO, "starting xrdp with pid %d", pid);
    g_snprintf(text, 255, "xrdp_%8.8x_main_term", pid);
    g_term_event = g_create_wait_obj(text);

    if (g_term_event == 0)
    {
        LOG(LOG_LEVEL_WARNING, "error creating g_term_event");
    }

    g_snprintf(text, 255, "xrdp_%8.8x_main_sync", pid);
    g_sync_event = g_create_wait_obj(text);

    if (g_sync_event == 0)
    {
        LOG(LOG_LEVEL_WARNING, "error creating g_sync_event");
    }

    g_listen->startup_params = &startup_params;
    exit_status = xrdp_listen_main_loop(g_listen);
    xrdp_listen_delete(g_listen);
    tc_mutex_delete(g_sync_mutex);
    tc_mutex_delete(g_sync1_mutex);
    g_delete_wait_obj(g_term_event);
    g_delete_wait_obj(g_sync_event);

    /* only main process should delete pid file */
    if (daemon && (pid == g_getpid()))
    {
        /* delete the xrdp.pid file */
        g_file_delete(pid_file);
    }

    log_end();
    g_deinit();

    if (exit_status == 0)
    {
        g_exit(0);
    }
    else
    {
        g_exit(1);
    }

    return 0;
}
