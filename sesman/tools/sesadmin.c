/*
 * sesadmin.c - an sesman administration tool
 * (c) 2008 Simone Fedele
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "tcp.h"
#include "libscp.h"
#include "parse.h"
#include "log.h"
#include "libscp.h"

#include <stdio.h>
#include <unistd.h>

char user[257];
char pass[257];
char cmnd[257];
char serv[257];
char port[257];

struct log_config logging;

void cmndList(struct SCP_CONNECTION *c);
void cmndKill(struct SCP_CONNECTION *c, struct SCP_SESSION *s);
void cmndHelp(void);

int inputSession(struct SCP_SESSION *s);
unsigned int menuSelect(unsigned int choices);

int main(int argc, char **argv)
{
    struct SCP_SESSION *s;
    struct SCP_CONNECTION *c;
    enum SCP_CLIENT_STATES_E e;
    //int end;
    int idx;
    //int sel;
    int sock;
    char *pwd;

    user[0] = '\0';
    pass[0] = '\0';
    cmnd[0] = '\0';
    serv[0] = '\0';
    port[0] = '\0';

    logging.program_name = "sesadmin";
    logging.log_file = g_strdup("xrdp-sesadmin.log");
    logging.log_level = LOG_LEVEL_DEBUG;
    logging.enable_syslog = 0;
    log_start_from_param(&logging);

    for (idx = 0; idx < argc; idx++)
    {
        if (0 == g_strncmp(argv[idx], "-u=", 3))
        {
            g_strncpy(user, (argv[idx]) + 3, 256);
        }
        else if (0 == g_strncmp(argv[idx], "-p=", 3))
        {
            g_strncpy(pass, (argv[idx]) + 3, 256);
        }
        else if (0 == g_strncmp(argv[idx], "-s=", 3))
        {
            g_strncpy(serv, (argv[idx]) + 3, 256);
        }
        else if (0 == g_strncmp(argv[idx], "-i=", 3))
        {
            g_strncpy(port, (argv[idx]) + 3, 256);
        }
        else if (0 == g_strncmp(argv[idx], "-c=", 3))
        {
            g_strncpy(cmnd, (argv[idx]) + 3, 256);
        }
    }

    if (0 == g_strncmp(serv, "", 1))
    {
        g_strncpy(serv, "localhost", 256);
    }

    if (0 == g_strncmp(port, "", 1))
    {
        g_strncpy(port, "3350", 256);
    }

    if (0 == g_strncmp(user, "", 1))
    {
        cmndHelp();
        return 0;
    }

    if (0 == g_strncmp(cmnd, "", 1))
    {
        cmndHelp();
        return 0;
    }

    if (0 == g_strncmp(pass, "", 1))
    {
        pwd = getpass("password:");
        g_strncpy(pass, pwd, 256);

        /* zeroing the password */
        while ((*pwd) != '\0')
        {
            (*pwd) = 0x00;
            pwd++;
        }
    }

    scp_init();

    sock = g_tcp_socket();
    if (sock < 0)
    {
        LOG_DBG("Socket open error, g_tcp_socket() failed");
        return 1;
    }

    s = scp_session_create();
    c = scp_connection_create(sock);

    LOG_DBG("Connecting to %s:%s with user %s (%s)", serv, port, user, pass);

    if (0 != g_tcp_connect(sock, serv, port))
    {
        LOG_DBG("g_tcp_connect() error");
        return 1;
    }

    scp_session_set_type(s, SCP_SESSION_TYPE_MANAGE);
    scp_session_set_version(s, 1);
    scp_session_set_username(s, user);
    scp_session_set_password(s, pass);

    e = scp_v1c_mng_connect(c, s);

    if (SCP_CLIENT_STATE_OK != e)
    {
        LOG_DBG("libscp error connecting: %s %d", s->errstr, (int)e);
    }

    if (0 == g_strncmp(cmnd, "list", 5))
    {
        cmndList(c);
    }
    else if (0 == g_strncmp(cmnd, "kill:", 5))
    {
        cmndKill(c, s);
    }

    g_tcp_close(sock);
    scp_session_destroy(s);
    scp_connection_destroy(c);
    log_end();

    return 0;
}

void cmndHelp(void)
{
    fprintf(stderr, "sesadmin - a console sesman administration tool\n");
    fprintf(stderr, "syntax: sesadmin [] COMMAND [OPTIONS]\n\n");
    fprintf(stderr, "-u=<username>: username to connect to sesman [MANDATORY]\n");
    fprintf(stderr, "-p=<password>: password to connect to sesman (asked if not given)\n");
    fprintf(stderr, "-s=<hostname>: sesman host (default is localhost)\n");
    fprintf(stderr, "-i=<port>    : sesman port (default 3350)\n");
    fprintf(stderr, "-c=<command> : command to execute on the server [MANDATORY]\n");
    fprintf(stderr, "               it can be one of those:\n");
    fprintf(stderr, "               list\n");
    fprintf(stderr, "               kill:<sid>\n");
}

static void
print_session(const struct SCP_DISCONNECTED_SESSION *s)
{
    printf("Session ID: %d\n", s->SID);
    printf("\tSession type: %d\n", s->type);
    printf("\tScreen size: %dx%d, color depth %d\n",
           s->width, s->height, s->bpp);
    printf("\tIdle time: %d day(s) %d hour(s) %d minute(s)\n",
           s->idle_days, s->idle_hours, s->idle_minutes);
    printf("\tConnected: %04d/%02d/%02d %02d:%02d\n",
           s->conn_year, s->conn_month, s->conn_day, s->conn_hour,
           s->conn_minute);
}

void cmndList(struct SCP_CONNECTION *c)
{
    struct SCP_DISCONNECTED_SESSION *dsl;
    enum SCP_CLIENT_STATES_E e;
    int scnt;
    int idx;

    e = scp_v1c_mng_get_session_list(c, &scnt, &dsl);

    if (e != SCP_CLIENT_STATE_LIST_OK)
    {
        printf("Error getting session list.\n");
        return;
    }

    if (scnt > 0)
    {
        for (idx = 0; idx < scnt; idx++)
        {
            print_session(&dsl[idx]);
        }
    }
    else
    {
        printf("No sessions.\n");
    }

    g_free(dsl);
}

void cmndKill(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{

}
