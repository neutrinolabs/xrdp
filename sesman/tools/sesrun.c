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
 * @file sesrun.c
 * @brief An utility to start a session
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <unistd.h>
#include <limits.h>
#include <ctype.h>

#include "parse.h"
#include "trans.h"
#include "os_calls.h"
#include "sesman_config.h"
#include "log.h"
#include "string_calls.h"
#include "guid.h"

#include "scp.h"
#include "scp_sync.h"

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

#ifndef DEFAULT_WIDTH
#   define DEFAULT_WIDTH 1280
#endif

#ifndef DEFAULT_HEIGHT
#   define DEFAULT_HEIGHT 1024
#endif

/* Default setting used by Windows 10 mstsc.exe */
#ifndef DEFAULT_BPP
#   define DEFAULT_BPP 32
#endif

#ifndef DEFAULT_SESSION_TYPE
#   define DEFAULT_SESSION_TYPE "Xorg"
#endif

/**
 * Maps session type strings session type codes
 */
static struct
{
    const char *name;
    enum scp_session_type type;
} type_map[] =
{
    { "Xvnc", SCP_SESSION_TYPE_XVNC},
    { "Xorg", SCP_SESSION_TYPE_XORG},
    { NULL, (enum scp_session_type) - 1}
};

/**
 * Parameters needed for a session
 */
struct session_params
{
    int width;
    int height;
    int bpp;
    enum scp_session_type session_type;

    const char *directory;
    const char *shell;
    const char *ip_addr;

    const char *username;
    char password[MAX_PASSWORD_LEN + 1];
};

/**************************************************************************//**
 * Maps a string to a session type value
 *
 * @param string session type string
 * @param[out] value session type value
 * @return 0 for success or != 0 if not found
 */
static
int string_to_session_type(const char *t, enum scp_session_type *value)
{
    unsigned int i;
    for (i = 0 ; type_map[i].name != NULL; ++i)
    {
        if (g_strcasecmp(type_map[i].name, t) == 0)
        {
            *value = type_map[i].type;
            return 0;
        }
    }

    return 1;
}

/**************************************************************************//**
 * Returns a list of supported session types
 *
 * Caller supplies a buffer. Buffer handling and buffer overflow detection are
 * the same as snprint()
 *
 * @param buff area for result
 * @param bufflen Size of result
 * @return number of characters for the output string
 */
static
unsigned int get_session_type_list(char *buff, unsigned int bufflen)
{
    unsigned int i;
    unsigned int ret = 0;
    const char *sep = "";

    for (i = 0 ; type_map[i].name != NULL; ++i)
    {
        if (ret < bufflen)
        {
            ret += g_snprintf(buff + ret, bufflen - ret,
                              "%s%s", sep, type_map[i].name);
            sep = ", ";
        }
    }

    return ret;
}

/**************************************************************************//**
 * Prints a brief summary of options and defaults
 */
static void
usage(void)
{
    char sesstype_list[64];

    (void)get_session_type_list(sesstype_list, sizeof(sesstype_list));

    g_printf("xrdp session starter v" PACKAGE_VERSION "\n");
    g_printf("\nusage:\n");
    g_printf("sesrun --help\n"
             "\nor\n"
             "sesrun [options] [username]\n\n");
    g_printf("options:\n");
    g_printf("    -g <geometry>         Default:%dx%d\n",
             DEFAULT_WIDTH, DEFAULT_HEIGHT);
    g_printf("    -b <bits-per-pixel>   Default:%d\n", DEFAULT_BPP);
    g_printf("    -t <type>             Default:%s\n", DEFAULT_SESSION_TYPE);
    g_printf("    -D <directory>        Default: $HOME\n"
             "    -S <shell>            Default: Defined window manager\n"
             "    -p <password>         TESTING ONLY - DO NOT USE IN PRODUCTION\n"
             "    -F <file-descriptor>  Read password from this file descriptor\n"
             "    -c <sesman_ini>       Alternative sesman.ini file\n");
    g_printf("\nSupported types are %s\n",
             sesstype_list);
    g_printf("\nIf username is omitted, the current user is used.\n"
             "If username is provided, password is needed.\n"
             "    Password is prompted for if -p or -F are not specified\n");
}


/**************************************************************************//**
 * Parses a string <width>x<height>
 *
 * @param geom_str Input string
 * @param sp Session parameter structure for resulting width and height
 * @return !=0 for success
 */
static int
parse_geometry_string(const char *geom_str, struct session_params *sp)
{
    int result = 0;
    unsigned int sep_count = 0; /* Count of 'x' separators */
    unsigned int other_count = 0; /* Count of non-digits and non separators */
    const char *sepp = NULL; /* Pointer to the 'x' */
    const char *p = geom_str;

    while (*p != '\0')
    {
        if (!isdigit(*p))
        {
            if (*p == 'x' || *p == 'X')
            {
                ++sep_count;
                sepp = p;
            }
            else
            {
                ++other_count;
            }
        }
        ++p;
    }

    if (sep_count != 1 || other_count > 0 ||
            sepp == geom_str ||  /* Separator at start of string */
            sepp == (p - 1) )    /* Separator at end of string */
    {
        LOG(LOG_LEVEL_ERROR, "Invalid geometry string '%s'", geom_str);
    }
    else
    {
        sp->width = atoi(geom_str);
        sp->height = atoi(sepp + 1);
        result = 1;
    }

    return result;
}


/**************************************************************************//**
 * Read a password from a file descriptor
 *
 * @param fd_str string representing file descriptor
 * @param sp Session parameter structure for resulting password
 * @return !=0 for success
 */
static int
read_password_from_fd(const char *fd_str, struct session_params *sp)
{
    int result = 0;
    int s = g_file_read(atoi(fd_str), sp->password, sizeof (sp->password) - 1);
    if (s < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't read password from fd %s - %s",
            fd_str, g_get_strerror());
        sp->password[0] = '\0';
    }
    else
    {
        sp->password[s] = '\0';
        if (s > 0 && sp->password[s - 1] == '\n')
        {
            sp->password[s - 1] = '\0';
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
 * @param sp Session parameter structure for resulting values
 * @param sesman_ini Pointer to an alternative config file if one is specified
 * @return !=0 for success
 */
static int
parse_program_args(int argc, char *argv[], struct session_params *sp,
                   const char **sesman_ini)
{
    int params_ok = 1;
    int opt;
    bool_t password_set = 0;

    sp->width = DEFAULT_WIDTH;
    sp->height = DEFAULT_HEIGHT;
    sp->bpp = DEFAULT_BPP;
    (void)string_to_session_type(DEFAULT_SESSION_TYPE, &sp->session_type);

    sp->directory = "";
    sp->shell = "";
    sp->ip_addr = "";

    sp->username = NULL;
    sp->password[0] = '\0';

    while ((opt = getopt(argc, argv, "g:b:s:t:D:S:p:F:c:")) != -1)
    {
        switch (opt)
        {
            case 'g':
                if (!parse_geometry_string(optarg, sp))
                {
                    params_ok = 0;
                }
                break;

            case 'b':
                sp->bpp = atoi(optarg);
                break;

            case 't':
                if (string_to_session_type(optarg, &sp->session_type) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "Unrecognised session type '%s'",
                        optarg);
                    params_ok = 0;
                }
                break;

            case 'D':
                sp->directory = optarg;
                break;

            case 'S':
                sp->shell = optarg;
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
                    g_strncpy(sp->password, optarg, sizeof(sp->password) - 1);
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
                    if (read_password_from_fd(optarg, sp))
                    {
                        password_set = 1;
                    }
                    else
                    {
                        params_ok = 0;
                    }
                }
                break;

            case 'c':
                *sesman_ini = optarg;
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
            sp->password[0] = '\0';
        }
        sp->username = NULL;
    }
    else if ((argc - optind) > 1)
    {
        LOG(LOG_LEVEL_ERROR, "Unexpected arguments after username");
        params_ok = 0;
    }
    else if (params_ok)
    {
        // A username is specified
        sp->username = argv[optind];
        if (!password_set)
        {
            const char *p = getpass("Password: ");
            if (p == NULL)
            {
                params_ok = 0;
            }
            else
            {
                g_snprintf(sp->password, sizeof(sp->password), "%s", p);
            }
        }
    }

    return params_ok;
}

/**************************************************************************//**
 * Sends an SCP login request
 *
 * A sys login request (i.e. username / password) is used if a username
 * is specified. Otherwise we use a uds login request for the current user.
 *
 * @param t SCP connection
 * @param sp Data for request
 */
static int
send_login_request(struct trans *t, const struct session_params *sp)
{
    int rv;
    LOG(LOG_LEVEL_DEBUG, "ip_addr:\"%s\"", sp->ip_addr);
    if (sp->username != NULL)
    {
        /* Only log the password in development builds */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "password:\"%s\"", sp->password);

        rv = scp_send_sys_login_request(t, sp->username,
                                        sp->password, sp->ip_addr);
    }
    else
    {
        rv = scp_send_uds_login_request(t);
    }

    return rv;
}

/**************************************************************************//**
 * Receives an SCP login response
 *
 * @param t SCP transport to receive reply on
 * @param[out] server_closed != 0 if server has gone away
 * @return 0 for successful authentication
 */
static int
handle_login_response(struct trans *t, int *server_closed)
{
    enum scp_login_status login_result;

    int rv = scp_sync_wait_specific(t, E_SCP_LOGIN_RESPONSE);
    if (rv != 0)
    {
        *server_closed = 1;
    }
    else
    {
        rv = scp_get_login_response(t, &login_result, server_closed, NULL);
        if (rv == 0)
        {
            if (login_result != E_SCP_LOGIN_OK)
            {
                char msg[256];
                scp_login_status_to_str(login_result, msg, sizeof(msg));
                g_printf("Login failed; %s\n", msg);
                rv = 1;
            }
        }
        scp_msg_in_reset(t); // Done with this message
    }

    return rv;
}


/**************************************************************************//**
 * Sends an SCP create session request
 *
 * @param t SCP connection
 * @param sp Data for request
 */
static int
send_create_session_request(struct trans *t, const struct session_params *sp)
{
    LOG(LOG_LEVEL_DEBUG,
        "width:%d  height:%d  bpp:%d  code:%d\n"
        "directory:\"%s\" shell:\"%s\"",
        sp->width, sp->height, sp->bpp, sp->session_type,
        sp->directory, sp->shell);

    return scp_send_create_session_request(
               t, sp->session_type,
               sp->width, sp->height, sp->bpp, sp->shell, sp->directory);
}

/**************************************************************************//**
 * Receives an SCP create session response
 *
 * @param t SCP transport to receive reply on
 * @return 0 for success
 */
static int
handle_create_session_response(struct trans *t)
{
    enum scp_screate_status status;
    int display;
    struct guid guid;

    int rv = scp_sync_wait_specific(t, E_SCP_CREATE_SESSION_RESPONSE);
    if (rv == 0)
    {
        rv = scp_get_create_session_response(t, &status,
                                             &display, &guid);

        if (rv == 0)
        {
            if (status != E_SCP_SCREATE_OK)
            {
                char msg[256];
                scp_screate_status_to_str(status, msg, sizeof(msg));
                g_printf("Connection failed; %s\n", msg);
                rv = 1;
            }
            else
            {
                char guid_str[GUID_STR_SIZE];
                g_printf("ok display=:%d GUID=%s\n",
                         display,
                         guid_to_str(&guid, guid_str));
            }
        }
        scp_msg_in_reset(t); // Done with this message
    }

    return rv;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    const char *sesman_ini = XRDP_CFG_PATH "/sesman.ini";
    struct config_sesman *cfg = NULL;

    struct trans *t = NULL;
    struct session_params sp;

    struct log_config *logging;
    int rv = 1;

    logging = log_config_init_for_console(LOG_LEVEL_WARNING,
                                          g_getenv("SESRUN_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    if (argc == 2 && g_strcmp(argv[1], "--help") == 0)
    {
        usage();
        rv = 0;
    }
    else if (!parse_program_args(argc, argv, &sp, &sesman_ini))
    {
        usage();
    }
    else if ((cfg = config_read(sesman_ini)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "error reading config file %s : %s",
            sesman_ini, g_get_strerror());
    }
    else if (!(t = scp_connect(cfg->listen_port, "xrdp-sesrun", NULL)))
    {
        LOG(LOG_LEVEL_ERROR, "connect error - %s", g_get_strerror());
    }
    else
    {
        int server_closed = 0;
        while (!server_closed)
        {
            rv = send_login_request(t, &sp);
            if (rv != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Error sending login request to sesman");
                break;
            }

            rv = handle_login_response(t, &server_closed);
            if (rv == 0)
            {
                break; /* Successful authentication */
            }
            if (!server_closed)
            {
                const char *p = getpass("Password: ");
                if (p == NULL)
                {
                    break;
                }
                g_snprintf(sp.password, sizeof(sp.password), "%s", p);
            }
        }

        if (rv == 0)
        {
            if ((rv = send_create_session_request(t, &sp)) == 0)
            {
                rv = handle_create_session_response(t);
            }
        }
        trans_delete(t);
    }

    g_memset(sp.password, '\0', sizeof(sp.password));
    config_free(cfg);
    log_end();

    return rv;
}
