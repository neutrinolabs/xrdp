/*
 * sestest.c - an scp_v1 testing tool
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
#include "string_calls.h"

#include <stdio.h>

int inputSession(struct SCP_SESSION *s);
unsigned int menuSelect(unsigned int choices);

int main(int argc, char **argv)
{
    char buf[256];
    struct SCP_SESSION *s;
    struct trans *t;
    /*struct SCP_DISCONNECTED_SESSION ds;*/
    struct SCP_DISCONNECTED_SESSION *dsl;
    enum SCP_CLIENT_STATES_E e;
    struct log_config *logging;
    int end;
    int scnt;
    int idx;
    int sel;
    int sock;

    logging = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(logging);
    log_config_free(logging);

    scp_init();

    sock = g_tcp_socket();
    if (sock < 0)
    {
        return 1;
    }

    s = scp_session_create();
    t = scp_trans_create(sock);

    if (0 != trans_connect(t, "localhost", "3350", 3000))
    {
        g_printf("error connecting");
        return 1;
    }

    g_printf("001 - send connect request\n");

    scp_session_set_type(s, SCP_SESSION_TYPE_XVNC);
    scp_session_set_version(s, 1);
    scp_session_set_height(s, 600);
    scp_session_set_width(s, 800);
    scp_session_set_bpp(s, 16);
    scp_session_set_rsr(s, 0);
    scp_session_set_locale(s, "it_IT");
    scp_session_set_username(s, "prog");
    scp_session_set_password(s, "prog");
    scp_session_set_hostname(s, "odin");
    //   scp_session_set_addr(s, SCP_ADDRESS_TYPE_IPV4, "127.0.0.1");
    //   scp_session_set_display(struct SCP_SESSION* s, SCP_DISPLAY display);
    //   scp_session_set_errstr(struct SCP_SESSION* s, char* str);

    /*s.type=SCP_SESSION_TYPE_XVNC;
    s.version=1;
    s.height=600;
    s.width=800;
    s.bpp=8;
    s.rsr=0;
    g_strncpy(s.locale,"it_IT  0123456789",18);
    s.username=g_malloc(256, 1);
    g_strncpy(s.username,"prog",255);
    s.password=g_malloc(256,1);
    g_strncpy(s.password, "prog", 255);
    g_printf("%s - %s\n", s.username, s.password);
    s.hostname=g_malloc(256,1);
    g_strncpy(s.hostname, "odin", 255);
    s.addr_type=SCP_ADDRESS_TYPE_IPV4;
    s.ipv4addr=0;
    s.errstr=0;*/

    end = 0;
    e = scp_v1c_connect(t, s);

    while (!end)
    {
        switch (e)
        {
            case SCP_CLIENT_STATE_OK:
                g_printf("OK : display is %d\n", (short int)s->display);
                end = 1;
                break;
            case SCP_CLIENT_STATE_SESSION_LIST:
                g_printf("OK : session list needed\n");
                e = scp_v1c_get_session_list(t, &scnt, &dsl);
                break;
            case SCP_CLIENT_STATE_LIST_OK:
                g_printf("OK : selecting a session:\n");

                for (idx = 0; idx < scnt; idx++)
                {
                    printf("Session \t%d - %d - %dx%dx%d - %d %d %d - %4d/%2d/%2d@%2d:%2d\n", \
                           (dsl[idx]).SID, (dsl[idx]).type, (dsl[idx]).width, (dsl[idx]).height, (dsl[idx]).bpp, \
                           (dsl[idx]).idle_days, (dsl[idx]).idle_hours, (dsl[idx]).idle_minutes, \
                           (dsl[idx]).conn_year, (dsl[idx]).conn_month, (dsl[idx]).conn_day, (dsl[idx]).conn_hour, (dsl[idx]).conn_minute);
                }

                sel = menuSelect(scnt);
                e = scp_v1c_select_session(t, s, dsl[sel - 1].SID);
                g_printf("\n return: %d \n", e);
                break;
            case SCP_CLIENT_STATE_RESEND_CREDENTIALS:
                g_printf("ERR: resend credentials - %s\n", s->errstr);
                g_printf("     username:");

                if (scanf("%255s", buf) < 0)
                {
                    g_writeln("error");
                }

                scp_session_set_username(s, buf);
                g_printf("     password:");

                if (scanf("%255s", buf) < 0)
                {
                    g_writeln("error");
                }

                scp_session_set_password(s, buf);
                e = scp_v1c_resend_credentials(t, s);
                break;
            case SCP_CLIENT_STATE_CONNECTION_DENIED:
                g_printf("ERR: connection denied: %s\n", s->errstr);
                end = 1;
                break;
            case SCP_CLIENT_STATE_PWD_CHANGE_REQ:
                g_printf("OK : password change required\n");
                break;
                /*case SCP_CLIENT_STATE_RECONNECT_SINGLE:
                  g_printf("OK : reconnect to 1 disconnected session\n");
                  e=scp_v1c_retrieve_session(&c, &s, &ds);
                  g_printf("Session Type: %d on %d\n", ds.type, s.display);
                  g_printf("Session Screen: %dx%dx%d\n", ds.height, ds.width, ds.bpp);*/
                break;
            default:
                g_printf("protocol error: %d\n", e);
                end = 1;
        }
    }

    scp_session_destroy(s);
    trans_delete(t);
    log_end();

    return 0;
}

int inputSession(struct SCP_SESSION *s)
{
    unsigned int integer;

    g_printf("username: ");

    if (scanf("%255s", s->username) < 0)
    {
        g_writeln("error");
    }

    g_printf("password:");

    if (scanf("%255s", s->password) < 0)
    {
        g_writeln("error");
    }

    g_printf("hostname:");

    if (scanf("%255s", s->hostname) < 0)
    {
        g_writeln("error");
    }

    g_printf("session type:\n");
    g_printf("%d: Xvnc\n", SCP_SESSION_TYPE_XVNC);
    g_printf("%d: x11rdp\n", SCP_SESSION_TYPE_XRDP);
    integer = menuSelect(1);

    if (integer == 1)
    {
        s->type = SCP_SESSION_TYPE_XRDP;
    }
    else
    {
        s->type = SCP_SESSION_TYPE_XVNC;
    }

    s->version = 1;
    s->height = 600;
    s->width = 800;
    s->bpp = 8;

    /* fixed for now */
    s->rsr = 0;
    g_strncpy(s->locale, "it_IT  0123456789", 18);

    return 0;
}

tui32
menuSelect(tui32 choices)
{
    tui32 sel;
    int ret;

    ret = scanf("%u", &sel);

    while ((ret == 0) || (sel > choices))
    {
        g_printf("invalid choice.");
        ret = scanf("%u", &sel);
    }

    return sel;
}
