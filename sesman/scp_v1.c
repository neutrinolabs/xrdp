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
 * @file scp_v1.c
 * @brief scp version 1 implementation
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "sesman.h"

//#include "libscp_types.h"
#include "libscp.h"

extern struct config_sesman *g_cfg; /* in sesman.c */

static void parseCommonStates(enum SCP_SERVER_STATES_E e, const char *f);

/******************************************************************************/
void
scp_v1_process(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    long data;
    int display = 0;
    int retries;
    int current_try;
    enum SCP_SERVER_STATES_E e;
    struct SCP_DISCONNECTED_SESSION *slist;
    struct session_item *sitem;
    int scount;
    SCP_SID sid;
    bool_t do_auth_end = 1;

    retries = g_cfg->sec.login_retry;
    current_try = retries;

    data = auth_userpass(s->username, s->password, NULL);
    /*LOG_DEVEL(LOG_LEVEL_DEBUG, "user: %s\npass: %s", s->username, s->password);*/

    while ((!data) && ((retries == 0) || (current_try > 0)))
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "data %ld - retry %d - currenttry %d - expr %d",
                  data, retries, current_try,
                  ((!data) && ((retries == 0) || (current_try > 0))));

        e = scp_v1s_request_password(c, s, "Wrong username and/or password");

        switch (e)
        {
            case SCP_SERVER_STATE_OK:
                /* all ok, we got new username and password */
                data = auth_userpass(s->username, s->password, NULL);

                /* one try less */
                if (current_try > 0)
                {
                    current_try--;
                }

                break;
            default:
                /* we check the other errors */
                parseCommonStates(e, "scp_v1s_list_sessions()");
                return;
                //break;
        }
    }

    if (!data)
    {
        scp_v1s_deny_connection(c, "Login failed");
        LOG( LOG_LEVEL_INFO,
             "Login failed for user %s. Connection terminated", s->username);
        return;
    }

    /* testing if login is allowed*/
    if (0 == access_login_allowed(s->username))
    {
        scp_v1s_deny_connection(c, "Access to Terminal Server not allowed.");
        LOG(LOG_LEVEL_INFO,
            "User %s not allowed on TS. Connection terminated", s->username);
        return;
    }

    //check if we need password change

    /* list disconnected sessions */
    slist = session_get_byuser(s->username, &scount, SESMAN_SESSION_STATUS_DISCONNECTED);

    if (scount == 0)
    {
        /* no disconnected sessions - start a new one */
        LOG(LOG_LEVEL_DEBUG, "No disconnected sessions for this user "
            "- we create a new one");

        if (0 != s->client_ip)
        {
            LOG(LOG_LEVEL_INFO, "++ created session (access granted): username %s, ip %s", s->username, s->client_ip);
        }
        else
        {
            LOG(LOG_LEVEL_INFO, "++ created session (access granted): username %s", s->username);
        }

        if (SCP_SESSION_TYPE_XVNC == s->type)
        {
            LOG(LOG_LEVEL_INFO, "starting Xvnc session...");
            display = session_start(data, SESMAN_SESSION_TYPE_XVNC, s);
        }
        else if (SCP_SESSION_TYPE_XRDP == s->type)
        {
            LOG(LOG_LEVEL_INFO, "starting X11rdp session...");
            display = session_start(data, SESMAN_SESSION_TYPE_XRDP, s);
        }
        else if (SCP_SESSION_TYPE_XORG == s->type)
        {
            LOG(LOG_LEVEL_INFO, "starting Xorg session...");
            display = session_start(data, SESMAN_SESSION_TYPE_XORG, s);
        }
        /* if the session started up ok, auth_end will be called on
           sig child */
        do_auth_end = display == 0;
        e = scp_v1s_connect_new_session(c, display);
        switch (e)
        {
            case SCP_SERVER_STATE_OK:
                /* all ok, we got new username and password */
                break;
            default:
                /* we check the other errors */
                parseCommonStates(e, "scp_v1s_connect_new_session()");
                break;
        }
    }
    else
    {
        /* one or more disconnected sessions - listing */
        e = scp_v1s_list_sessions(c, scount, slist, &sid);

        switch (e)
        {
            /*case SCP_SERVER_STATE_FORCE_NEW:*/
            /* we should check for MaxSessions */
            case SCP_SERVER_STATE_SELECTION_CANCEL:
                LOG( LOG_LEVEL_INFO, "Connection cancelled after session listing");
                break;
            case SCP_SERVER_STATE_OK:
                /* ok, reconnecting... */
                sitem = session_get_bypid(sid);

                if (0 == sitem)
                {
                    e = scp_v1s_connection_error(c, "Internal error");
                    LOG(LOG_LEVEL_INFO, "Cannot find session item on the chain");
                }
                else
                {
                    display = sitem->display;
                    /*e=scp_v1s_reconnect_session(c, sitem, display);*/
                    e = scp_v1s_reconnect_session(c, display);

                    if (0 != s->client_ip)
                    {
                        LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, display :%d.0, session_pid %d, ip %s", s->username, display, sitem->pid, s->client_ip);
                    }
                    else
                    {
                        LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, display :%d.0, session_pid %d", s->username, display, sitem->pid);
                    }

                    g_free(sitem);
                }

                break;
            default:
                /* we check the other errors */
                parseCommonStates(e, "scp_v1s_list_sessions()");
                break;
        }
    }

    /* resource management */
    if ((e == SCP_SERVER_STATE_OK) && (s->rsr))
    {
        /* here goes scp resource sharing code */
    }

    /* cleanup */
    if (do_auth_end)
    {
        auth_end(data);
    }
    g_free(slist);
}

/******************************************************************************/
void
scp_v1_process_msg(struct trans *atrans, struct SCP_SESSION *s)
{
    // JAY TODO
}

static void parseCommonStates(enum SCP_SERVER_STATES_E e, const char *f)
{
    switch (e)
    {
        case SCP_SERVER_STATE_VERSION_ERR:
            LOG(LOG_LEVEL_WARNING, "version error");
        case SCP_SERVER_STATE_SIZE_ERR:
            /* an unknown scp version was requested, so we shut down the */
            /* connection (and log the fact)                             */
            LOG(LOG_LEVEL_WARNING,
                "protocol violation. connection closed.");
            break;
        case SCP_SERVER_STATE_NETWORK_ERR:
            LOG(LOG_LEVEL_WARNING, "libscp network error.");
            break;
        case SCP_SERVER_STATE_SEQUENCE_ERR:
            LOG(LOG_LEVEL_WARNING, "libscp sequence error.");
            break;
        case SCP_SERVER_STATE_INTERNAL_ERR:
            /* internal error occurred (eg. malloc() error, ecc.) */
            LOG(LOG_LEVEL_ERROR, "libscp internal error occurred.");
            break;
        default:
            /* dummy: scp_v1s_request_password won't generate any other */
            /* error other than the ones before                         */
            LOG(LOG_LEVEL_ALWAYS, "unknown return from %s", f);
            break;
    }
}
