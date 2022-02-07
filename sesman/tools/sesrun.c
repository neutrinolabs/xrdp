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
#include "os_calls.h"
#include "config.h"
#include "log.h"
#include "tcp.h"
#include "string_calls.h"

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

#ifndef DEFAULT_SERVER
#   define DEFAULT_SERVER "localhost"
#endif

#ifndef DEFAULT_TYPE
#   define DEFAULT_TYPE "Xorg"
#endif

/**
 * Maps session type strings to internal code numbers
 */
static struct
{
    const char *name;
    int code;
} type_map[] =
{
    { "Xvnc", 0},
    { "X11rdp", 10},
    { "Xorg", 20},
    { NULL, -1}
};

/**
 * Parameters needed for a session
 */
struct session_params
{
    int width;
    int height;
    int bpp;
    int session_code;
    const char *server;

    const char *domain;  /* Currently unused by sesman */
    const char *directory;
    const char *shell;
    const char *client_ip;

    const char *username;
    char password[MAX_PASSWORD_LEN + 1];
};

/**************************************************************************//**
 * Maps a string to a session code
 *
 * @param t session type
 * @return session code, or -1 if not found
 */
static
int get_session_type_code(const char *t)
{
    unsigned int i;
    for (i = 0 ; type_map[i].name != NULL; ++i)
    {
        if (g_strcasecmp(type_map[i].name, t) == 0)
        {
            return type_map[i].code;
        }
    }

    return -1;
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
    g_printf("sesrun [options] username\n\n");
    g_printf("options:\n");
    g_printf("    -g <geometry>         Default:%dx%d\n",
             DEFAULT_WIDTH, DEFAULT_HEIGHT);
    g_printf("    -b <bits-per-pixel>   Default:%d\n", DEFAULT_BPP);
    /* Don't encourage use of this one - we need to move to local sockets */
    g_printf("    -s <server>           Default:%s (Deprecated)\n",
             DEFAULT_SERVER);
    g_printf("    -t <type>             Default:%s\n", DEFAULT_TYPE);
    g_printf("    -D <directory>        Default: $HOME\n"
             "    -S <shell>            Default: Defined window manager\n"
             "    -p <password>         TESTING ONLY - DO NOT USE IN PRODUCTION\n"
             "    -F <file-descriptor>  Read password from this file descriptor\n"
             "    -c <sesman_ini>       Alternative sesman.ini file\n");
    g_printf("Supported types are %s or use int for internal code\n",
             sesstype_list);
    g_printf("Password is prompted if -p or -F are not specified\n");
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
    sp->session_code = get_session_type_code(DEFAULT_TYPE);
    sp->server = DEFAULT_SERVER;

    sp->domain = "";
    sp->directory = "";
    sp->shell = "";
    sp->client_ip = "";

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

            case 's':
                LOG(LOG_LEVEL_WARNING, "Using deprecated option '-s'");
                sp->server = optarg;
                break;

            case 't':
                if (isdigit(optarg[0]))
                {
                    sp->session_code = atoi(optarg);
                }
                else
                {
                    sp->session_code = get_session_type_code(optarg);
                    if (sp->session_code < 0)
                    {
                        LOG(LOG_LEVEL_ERROR, "Unrecognised session type '%s'",
                            optarg);
                        params_ok = 0;
                    }
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

    if (argc <= optind)
    {
        LOG(LOG_LEVEL_ERROR, "No user name speciified");
        params_ok = 0;
    }
    else if ((argc - optind) > 1)
    {
        LOG(LOG_LEVEL_ERROR, "Unexpected arguments after username");
        params_ok = 0;
    }
    else
    {
        sp->username = argv[optind];
    }

    if (params_ok && !password_set)
    {
        const char *p = getpass("Password: ");
        if (p != NULL)
        {
            g_strcpy(sp->password, p);
        }
    }

    return params_ok;
}

/**************************************************************************//**
 * Helper function for send_scpv0_auth_request()
 *
 * @param s Output string
 * @param str String to write to s
 */
static void
out_string16(struct stream *s, const char *str)
{
    int i = g_strlen(str);
    out_uint16_be(s, i);
    out_uint8a(s, str, i);
}

/**************************************************************************//**
 * Sends an SCP V0 authorization request
 *
 * @param sck file descriptor to send request on
 * @param sp Data for request
 *
 * @todo This code duplicates functionality in the XRDP function
 *       xrdp_mm_send_login(). When SCP is reworked, a common library
 *       function should be used
 */
static void
send_scpv0_auth_request(int sck, const struct session_params *sp)
{
    struct stream *out_s;

    LOG(LOG_LEVEL_DEBUG,
        "width:%d  height:%d  bpp:%d  code:%d\n"
        "server:\"%s\"    domain:\"%s\"    directory:\"%s\"\n"
        "shell:\"%s\"    client_ip:\"%s\"",
        sp->width, sp->height, sp->bpp, sp->session_code,
        sp->server, sp->domain, sp->directory,
        sp->shell, sp->client_ip);
    /* Only log the password in development builds */
    LOG_DEVEL(LOG_LEVEL_DEBUG, "password:\"%s\"", sp->password);

    make_stream(out_s);
    init_stream(out_s, 8192);

    s_push_layer(out_s, channel_hdr, 8);
    out_uint16_be(out_s, sp->session_code);
    out_string16(out_s, sp->username);
    out_string16(out_s, sp->password);
    out_uint16_be(out_s, sp->width);
    out_uint16_be(out_s, sp->height);
    out_uint16_be(out_s, sp->bpp);
    out_string16(out_s, sp->domain);
    out_string16(out_s, sp->shell);
    out_string16(out_s, sp->directory);
    out_string16(out_s, sp->client_ip);
    s_mark_end(out_s);

    s_pop_layer(out_s, channel_hdr);
    out_uint32_be(out_s, 0); /* version */
    out_uint32_be(out_s, out_s->end - out_s->data); /* size */
    tcp_force_send(sck, out_s->data, out_s->end - out_s->data);

    free_stream(out_s);
}

/**************************************************************************//**
 * Receives an SCP V0 authorization reply
 *
 * @param sck file descriptor to receive reply on
 *
 * @todo This code duplicates functionality in the XRDP function
 *       xrdp_mm_process_login_response(). When SCP is reworked, a
 *       common library function should be used
 */
static int
handle_scpv0_auth_reply(int sck)
{
    int result = 1;
    int packet_ok = 0;

    struct stream *in_s;

    make_stream(in_s);
    init_stream(in_s, 8192);

    if (tcp_force_recv(sck, in_s->data, 8) == 0)
    {
        int version;
        int size;
        int code;
        int data;
        int display;

        in_s->end = in_s->data + 8;
        in_uint32_be(in_s, version);
        in_uint32_be(in_s, size);
        if (version == 0 && size >= 14)
        {
            init_stream(in_s, 8192);
            if (tcp_force_recv(sck, in_s->data, size - 8) == 0)
            {
                in_s->end = in_s->data + (size - 8);

                in_uint16_be(in_s, code);
                in_uint16_be(in_s, data);
                in_uint16_be(in_s, display);

                if (code == 3)
                {
                    packet_ok = 1;

                    if (data == 0)
                    {
                        g_printf("Connection denied (authentication error)\n");
                    }
                    else
                    {
                        char guid[16];
                        char guid_str[64];
                        if (s_check_rem(in_s, 16) != 0)
                        {
                            in_uint8a(in_s, guid, 16);
                            g_bytes_to_hexstr(guid, 16, guid_str, 64);
                        }
                        else
                        {
                            g_strcpy(guid_str, "<none>");
                        }

                        g_printf("ok data=%d display=:%d GUID=%s\n",
                                 (int)data, display, guid_str);
                        result = 0;
                    }
                }
            }
        }
    }

    if (!packet_ok)
    {
        LOG(LOG_LEVEL_ERROR, "Corrupt reply packet");
    }
    free_stream(in_s);

    return result;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    const char *sesman_ini = XRDP_CFG_PATH "/sesman.ini";
    struct config_sesman *cfg = NULL;

    int sck = -1;
    struct session_params sp;

    struct log_config *logging;
    int status = 1;

    logging = log_config_init_for_console(LOG_LEVEL_WARNING,
                                          g_getenv("SESRUN_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    if (!parse_program_args(argc, argv, &sp, &sesman_ini))
    {
        usage();
    }
    else if ((cfg = config_read(sesman_ini)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "error reading config file %s : %s",
            sesman_ini, g_get_strerror());
    }
    else
    {
        sck = g_tcp_socket();
        if (sck < 0)
        {
            LOG(LOG_LEVEL_ERROR, "socket error - %s", g_get_strerror());
        }
        else if (g_tcp_connect(sck, sp.server, cfg->listen_port) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "connect error - %s", g_get_strerror());
        }
        else
        {
            send_scpv0_auth_request(sck, &sp);
            status = handle_scpv0_auth_reply(sck);
        }
    }

    if (sck >= 0)
    {
        g_tcp_close(sck);
    }

    g_memset(sp.password, '\0', sizeof(sp.password));
    config_free(cfg);
    log_end();

    return status;
}
