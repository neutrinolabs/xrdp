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
 * @file authtest.c
 * @brief An utility to test the compiled-in authentication module
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <unistd.h>

#include "log.h"
#include "sesman_auth.h"
#include "os_calls.h"
#include "string_calls.h"

// cppcheck doesn't always set this macro to something in double-quotes
#if defined(__cppcheck__)
#undef PACKAGE_VERSION
#endif

#if !defined(PACKAGE_VERSION)
#define PACKAGE_VERSION "???"
#endif

#ifndef MAX_PASSWORD_LEN
#   define MAX_PASSWORD_LEN 512
#endif

/**
 * Parameters needed to call the auth module
 */
struct authmod_params
{
    const char *username;
    char password[MAX_PASSWORD_LEN + 1];
    const char *command;
    int start_session;
};


/**************************************************************************//**
 * Prints a brief summary of options and defaults
 */
static void
usage(void)
{
    g_printf("xrdp auth module tester v" PACKAGE_VERSION "\n");
    g_printf("\n"
             "Calls functions in the compiled-in auth module, so that the\n"
             "module can be checked simply for functionality, memory leaks,\n"
             "etc.\n\n"
             "This is a DEVELOPER-ONLY tool\n");
    g_printf("\nusage:\n");
    g_printf("authtest [options] [username]\n\n");
    g_printf("options:\n");
    g_printf("    -p <password>\n"
             "    -F <file-descriptor>  Read password from this file descriptor\n"
             "    -c <command>          Start a session and run the\n"
             "                          specified non-interactive command\n"
             "                          in it\n");
    g_printf("\nIf username is omitted, the current user is used, and.\n"
             "a UDS login is attempted\n"
             "If username is provided, password is needed.\n"
             "    Password is prompted for if -p or -F are not specified\n");
}


/**************************************************************************//**
 * Read a password from a file descriptor
 *
 * @param fd_str string representing file descriptor
 * @param sp Authmod parameter structure for resulting password
 * @return !=0 for success
 */
static int
read_password_from_fd(const char *fd_str, struct authmod_params *amp)
{
    int result = 0;
    int s = g_file_read(atoi(fd_str), amp->password,
                        sizeof (amp->password) - 1);
    if (s < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't read password from fd %s - %s",
            fd_str, g_get_strerror());
        amp->password[0] = '\0';
    }
    else
    {
        amp->password[s] = '\0';
        if (s > 0 && amp->password[s - 1] == '\n')
        {
            amp->password[s - 1] = '\0';
        }
        result = 1;
    }
    return result;
}

/**************************************************************************//**
 * Parses the program args
 *
 * @param argc Passed to main
 * @param @argv Passed to main
 * @param amp Authmod parameter structure for resulting values
 * @return !=0 for success
 */
static int
parse_program_args(int argc, char *argv[], struct authmod_params *amp)
{
    int params_ok = 1;
    int opt;
    bool_t password_set = 0;

    amp->username = NULL;
    amp->password[0] = '\0';
    amp->start_session = 0;
    amp->command = NULL;

    while ((opt = getopt(argc, argv, "c:p:F:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                if (amp->command)
                {
                    LOG(LOG_LEVEL_WARNING, "Ignoring multiple '%c' options",
                        (char)opt);
                }
                else
                {
                    amp->command = optarg;
                }
                break;

            case 'p':
                if (password_set)
                {
                    LOG(LOG_LEVEL_WARNING,
                        "Ignoring option '%c' - password already set ",
                        (char)opt);
                }
                else
                {
                    g_strncpy(amp->password, optarg, sizeof(amp->password) - 1);
                    password_set = 1;
                }
                break;

            case 'F':
                if (password_set)
                {
                    LOG(LOG_LEVEL_WARNING,
                        "Ignoring option '%c' - password already set ",
                        (char)opt);
                }
                else
                {
                    if (read_password_from_fd(optarg, amp))
                    {
                        password_set = 1;
                    }
                    else
                    {
                        params_ok = 0;
                    }
                }
                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Unrecognised switch '%c'", (char)opt);
                params_ok = 0;
        }
    }

    if (argc == optind)
    {
        // No username was specified
        if (password_set)
        {
            LOG(LOG_LEVEL_WARNING, "No username - ignoring specified password");
            amp->password[0] = '\0';
        }
        amp->username = NULL;
    }
    else if ((argc - optind) > 1)
    {
        LOG(LOG_LEVEL_ERROR, "Unexpected arguments after username");
        params_ok = 0;
    }
    else
    {
        amp->username = argv[optind];
        if (!password_set)
        {
            const char *p = getpass("Password: ");
            if (p == NULL)
            {
                params_ok = 0;
            }
            else
            {
                g_snprintf(amp->password, sizeof(amp->password), "%s", p);
            }
        }
    }

    return params_ok;
}

/******************************************************************************/
/**
 * Gets the current username for a UDS login
 *
 * Result must be freed after use.
 */
static char *
get_username()
{
    int uid = g_getuid();
    char *this_user;
    int status;

    status = g_getuser_info_by_uid(uid, &this_user, NULL, NULL, NULL, NULL);
    if (status != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't map UID %d to a username", uid);
        this_user = NULL;
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "Mapped current UID %d to \"%s\"",
            uid, this_user);
    }

    return this_user;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    struct log_config *logging;
    int rv = 0;
    char *this_user = NULL;
    struct authmod_params amp;

    logging = log_config_init_for_console(LOG_LEVEL_DEBUG,
                                          g_getenv("AUTHTEST_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    if (!parse_program_args(argc, argv, &amp))
    {
        usage();
        rv = 1;
    }
    else if (amp.username == NULL && (this_user = get_username()) == NULL)
    {
        rv = 1;
    }
    else
    {
        struct auth_info *auth_info;
        enum scp_login_status errorcode;
        char errstr[64];

        if (amp.username == NULL)
        {
            auth_info = auth_uds(this_user, &errorcode);
        }
        else
        {
            auth_info = auth_userpass(amp.username, amp.password,
                                      NULL, &errorcode);
        }
        scp_login_status_to_str(errorcode, errstr, sizeof(errstr));
        LOG(LOG_LEVEL_INFO,
            "auth_userpass() returned %s, errorcode=%d [%s]",
            (auth_info == NULL) ? "NULL" : "non-NULL",
            (int)errorcode, errstr);

        rv = (int)errorcode;
        if (auth_info && rv == 0 && amp.command != NULL)
        {
            int display = 10;
            rv = auth_start_session(auth_info, display);
            LOG(LOG_LEVEL_INFO, "auth_start_session(,%d) returned %d",
                display, rv);
            if (rv == 0)
            {
                rv = g_system(amp.command);
                LOG(LOG_LEVEL_INFO, "command \"%s\" returned %d",
                    amp.command, rv);
            }
        }
        if (auth_info != NULL)
        {
            int rv2 = auth_end(auth_info);
            LOG(LOG_LEVEL_INFO, "auth_end() returned %d", rv2);
            rv = (rv == 0) ? rv2 : rv;
        }
    }

    g_free(this_user);
    log_end();

    return rv;
}
