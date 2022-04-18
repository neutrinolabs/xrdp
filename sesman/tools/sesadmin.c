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
#include "trans.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "tools_common.h"

#include <stdio.h>
#include <unistd.h>

char user[257];
char pass[257];
char cmnd[257];
char port[257];

static int cmndList(struct trans *t);
static int cmndKill(struct trans *t);
static void cmndHelp(void);


int main(int argc, char **argv)
{
    struct trans *t;
    //int end;
    int idx;
    //int sel;
    char *pwd;
    struct log_config *logging;
    int rv = 1;

    user[0] = '\0';
    pass[0] = '\0';
    cmnd[0] = '\0';
    port[0] = '\0';

    logging = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(logging);
    log_config_free(logging);

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
        else if (0 == g_strncmp(argv[idx], "-i=", 3))
        {
            g_strncpy(port, (argv[idx]) + 3, 256);
        }
        else if (0 == g_strncmp(argv[idx], "-c=", 3))
        {
            g_strncpy(cmnd, (argv[idx]) + 3, 256);
        }
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

    }

    t = scp_connect(port, NULL);


    if (t == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "scp_connect() error");
    }
    else if (0 == g_strncmp(cmnd, "list", 5))
    {
        rv = cmndList(t);
    }
    else if (0 == g_strncmp(cmnd, "kill:", 5))
    {
        rv = cmndKill(t);
    }

    g_memset(pass, '\0', sizeof(pass));

    trans_delete(t);
    log_end();

    return rv;
}

static void
cmndHelp(void)
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
print_session(const struct scp_session_info *s)
{
    printf("Session ID: %d\n", s->sid);
    printf("\tDisplay: :%u\n", s->display);
    printf("\tUser: %s\n", s->username);
    printf("\tSession type: %s\n", SCP_SESSION_TYPE_TO_STR(s->type));
    printf("\tScreen size: %dx%d, color depth %d\n",
           s->width, s->height, s->bpp);
    printf("\tStarted: %s", ctime(&s->start_time));
    if (s->connection_description[0] != '\0')
    {
        printf("\tConnection Description: %s\n", s->connection_description);
    }
}

static int
cmndList(struct trans *t)
{
    struct list *sessions = list_create();
    int end_of_list = 0;

    enum scp_list_sessions_status status;
    struct scp_session_info *p;

    int rv = scp_send_list_sessions_request(t, user, pass);

    sessions->auto_free = 1;

    while (rv == 0 && !end_of_list)
    {
        rv = wait_for_sesman_reply(t, E_SCP_LIST_SESSIONS_RESPONSE);
        if (rv != 0)
        {
            break;
        }

        rv = scp_get_list_sessions_response(t, &status, &p);
        if (rv != 0)
        {
            break;
        }

        switch (status)
        {
            case E_SCP_LS_AUTHENTICATION_FAIL:
                printf("Connection denied (authentication error)\n");
                rv = 1;
                break;

            case E_SCP_LS_SESSION_INFO:
                list_add_item(sessions, (tintptr)p);
                break;

            case E_SCP_LS_END_OF_LIST:
                end_of_list = 1;
                break;

            default:
                printf("Unexpected return code %d\n", status);
                rv = 1;
        }
        scp_msg_in_reset(t);
    }

    if (rv == 0)
    {
        if (sessions->count == 0)
        {
            printf("No sessions.\n");
        }
        else
        {
            int i;
            for (i = 0 ; i < sessions->count; ++i)
            {
                print_session((struct scp_session_info *)sessions->items[i]);
            }
        }
    }

    list_delete(sessions);
    return rv;
}

static int
cmndKill(struct trans *t)
{
    fprintf(stderr, "not yet implemented\n");
    return 1;
}
