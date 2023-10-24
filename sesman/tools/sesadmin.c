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
    struct log_config *logging;
    int rv = 1;

    cmnd[0] = '\0';
    port[0] = '\0';

    logging = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(logging);
    log_config_free(logging);

    for (idx = 0; idx < argc; idx++)
    {
        if (0 == g_strncmp(argv[idx], "-u=", 3))
        {
            g_printf("** Ignoring unused argument '-u'");
        }
        else if (0 == g_strncmp(argv[idx], "-p=", 3))
        {
            g_printf("** Ignoring unused argument '-p'");
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

    if (0 == g_strncmp(cmnd, "", 1))
    {
        cmndHelp();
        return 0;
    }

    t = scp_connect(port, "xrdp-sesadmin", NULL);

    if (t == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "scp_connect() error");
    }
    else
    {
        enum scp_login_status  login_result;

        /* Log in as the current user */
        if ((rv = scp_send_uds_login_request(t)) == 0 &&
                (rv = wait_for_sesman_reply(t, E_SCP_LOGIN_RESPONSE)) == 0)
        {
            rv = scp_get_login_response(t, &login_result, NULL, NULL);
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
    }

    if (rv == 0)
    {
        if (0 == g_strncmp(cmnd, "list", 5))
        {
            rv = cmndList(t);
        }
        else if (0 == g_strncmp(cmnd, "kill:", 5))
        {
            rv = cmndKill(t);
        }
    }

    if (rv == 0)
    {
        rv = scp_send_close_connection_request(t);
    }
    trans_delete(t);
    log_end();

    return rv;
}

static void
cmndHelp(void)
{
    fprintf(stderr, "sesadmin - a console sesman administration tool\n");
    fprintf(stderr, "syntax: sesadmin [] COMMAND [OPTIONS]\n\n");
    fprintf(stderr, "-i=<port>    : sesman port (can be defaulted)\n");
    fprintf(stderr, "-c=<command> : command to execute on the server [MANDATORY]\n");
    fprintf(stderr, "               it can be one of those:\n");
    fprintf(stderr, "               list\n");
    fprintf(stderr, "               kill:<sid>\n");
}

static void
print_session(const struct scp_session_info *s)
{
    char *username;
    const char *uptr;
    g_getuser_info_by_uid(s->uid, &username, NULL, NULL, NULL, NULL);
    uptr = (username == NULL) ? "<unknown>" : username;

    printf("Session ID: %d\n", s->sid);
    printf("\tDisplay: :%u\n", s->display);
    printf("\tUser: %s\n", uptr);
    printf("\tSession type: %s\n", SCP_SESSION_TYPE_TO_STR(s->type));
    printf("\tScreen size: %dx%d, color depth %d\n",
           s->width, s->height, s->bpp);
    printf("\tStarted: %s", ctime(&s->start_time));
    if (s->start_ip_addr[0] != '\0')
    {
        printf("\tStart IP address: %s\n", s->start_ip_addr);
    }
    g_free(username);
}

static int
cmndList(struct trans *t)
{
    struct list *sessions = list_create();
    int end_of_list = 0;

    enum scp_list_sessions_status status;
    struct scp_session_info *p;

    int rv = scp_send_list_sessions_request(t);

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
