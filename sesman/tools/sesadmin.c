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

#include "scp.h"
#include "scp_sync.h"

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
        /* Log in as the current user */
        rv = scp_sync_uds_login_request(t);
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
    // Don't need to check error return explicitly - can check username
    // against NULL
    (void)g_getuser_info_by_uid(s->uid, &username, NULL, NULL, NULL, NULL);
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
    int rv = 1;
    struct list *sessions = scp_sync_list_sessions_request(t);
    if (sessions != NULL)
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
        (void)scp_send_close_connection_request(t);
        list_delete(sessions);
    }

    return rv;
}

static int
cmndKill(struct trans *t)
{
    fprintf(stderr, "not yet implemented\n");
    return 1;
}
