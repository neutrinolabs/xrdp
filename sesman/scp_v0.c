/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * @file scp_v0.c
 * @brief scp version 0 implementation
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "sesman.h"

extern struct config_sesman *g_cfg; /* in sesman.c */

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0_process(struct trans *t, struct SCP_SESSION *s)
{
    int display = 0;
    tbus data;
    struct session_item *s_item;
    int errorcode = 0;
    bool_t do_auth_end = 1;

    data = auth_userpass(s->username, s->password, &errorcode);

    if (s->type == SCP_GW_AUTHENTICATION)
    {
        /* this is just authentication in a gateway situation */
        /* g_writeln("SCP_GW_AUTHENTICATION message received"); */
        if (data)
        {
            if (1 == access_login_allowed(s->username))
            {
                /* the user is member of the correct groups. */
                scp_v0s_replyauthentication(t, errorcode);
                LOG(LOG_LEVEL_INFO, "Access permitted for user: %s",
                    s->username);
                /* g_writeln("Connection allowed"); */
            }
            else
            {
                scp_v0s_replyauthentication(t, 32 + 3); /* all first 32 are reserved for PAM errors */
                LOG(LOG_LEVEL_INFO, "Username okay but group problem for "
                    "user: %s", s->username);
                /* g_writeln("user password ok, but group problem"); */
            }
        }
        else
        {
            /* g_writeln("username or password error"); */
            LOG(LOG_LEVEL_INFO, "Username or password error for user: %s",
                s->username);
            scp_v0s_replyauthentication(t, errorcode);
        }
    }
    else if (data)
    {
        s_item = session_get_bydata(s->username, s->width, s->height,
                                    s->bpp, s->type, s->client_ip);

        if (s_item != 0)
        {
            display = s_item->display;
            g_memcpy(s->guid, s_item->guid, 16);
            if (0 != s->client_ip)
            {
                LOG( LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                     "display :%d.0, session_pid %d, ip %s",
                     s->username, display, s_item->pid, s->client_ip);
            }
            else
            {
                LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                    "display :%d.0, session_pid %d", s->username, display,
                    s_item->pid);
            }

            session_reconnect(display, s->username, data);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "pre auth");

            if (1 == access_login_allowed(s->username))
            {
                tui8 guid[16];

                g_random((char *)guid, 16);
                scp_session_set_guid(s, guid);

                if (0 != s->client_ip)
                {
                    LOG(LOG_LEVEL_INFO, "++ created session (access granted): "
                        "username %s, ip %s", s->username, s->client_ip);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "++ created session (access granted): "
                        "username %s", s->username);
                }

                if (SCP_SESSION_TYPE_XVNC == s->type)
                {
                    LOG( LOG_LEVEL_INFO, "starting Xvnc session...");
                    display = session_start(data, SESMAN_SESSION_TYPE_XVNC, s);
                }
                else if (SCP_SESSION_TYPE_XRDP == s->type)
                {
                    LOG(LOG_LEVEL_INFO, "starting X11rdp session...");
                    display = session_start(data, SESMAN_SESSION_TYPE_XRDP, s);
                }
                else if (SCP_SESSION_TYPE_XORG == s->type)
                {
                    /* type is SCP_SESSION_TYPE_XORG */
                    LOG(LOG_LEVEL_INFO, "starting Xorg session...");
                    display = session_start(data, SESMAN_SESSION_TYPE_XORG, s);
                }
                /* if the session started up ok, auth_end will be called on
                   sig child */
                do_auth_end = display == 0;
            }
            else
            {
                display = 0;
            }
        }

        if (display == 0)
        {
            scp_v0s_deny_connection(t);
        }
        else
        {
            scp_v0s_allow_connection(t, display, s->guid);
        }
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "Username or password error for user: %s",
            s->username);
        scp_v0s_deny_connection(t);
    }
    if (do_auth_end)
    {
        auth_end(data);
    }
    return SCP_SERVER_STATE_END;
}
