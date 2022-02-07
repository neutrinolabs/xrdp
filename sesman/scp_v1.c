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

static void
parseCommonStates(enum SCP_SERVER_STATES_E e, const char *f);

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process1(struct trans *t, struct SCP_SESSION *s)
{
    int display = 0;
    int scount;
    long data;
    enum SCP_SERVER_STATES_E e;
    struct SCP_DISCONNECTED_SESSION *slist;
    bool_t do_auth_end = 1;

    if (s->retries == 0)
    {
        /* First time in */
        s->retries = g_cfg->sec.login_retry;
        s->current_try = s->retries;
    }
    data = auth_userpass(s->username, s->password, NULL);
    if (data == 0)
    {
        if ((s->retries == 0) || (s->current_try > 0))
        {
            e = scp_v1s_request_password(t, s, "Wrong username and/or "
                                         "password");
            switch (e)
            {
                case SCP_SERVER_STATE_OK:
                    /* one try less */
                    if (s->current_try > 0)
                    {
                        s->current_try--;
                    }
                    break;
                default:
                    /* we check the other errors */
                    parseCommonStates(e, "scp_v1s_list_sessions()");
                    break;
            }
        }
        else
        {
            scp_v1s_deny_connection(t, "Login failed");
            LOG(LOG_LEVEL_INFO, "Login failed for user %s. "
                "Connection terminated", s->username);
            return SCP_SERVER_STATE_END;
        }
        return SCP_SERVER_STATE_OK;
    }

    /* testing if login is allowed*/
    if (0 == access_login_allowed(s->username))
    {
        scp_v1s_deny_connection(t, "Access to Terminal Server not allowed.");
        LOG(LOG_LEVEL_INFO, "User %s not allowed on TS. "
            "Connection terminated", s->username);
        return SCP_SERVER_STATE_END;
    }

    //check if we need password change

    /* list disconnected sessions */
    slist = session_get_byuser(s->username, &scount,
                               SESMAN_SESSION_STATUS_DISCONNECTED);

    if (scount == 0)
    {
        /* no disconnected sessions - start a new one */
        LOG(LOG_LEVEL_DEBUG, "No disconnected sessions for this user "
            "- we create a new one");

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
        e = scp_v1s_connect_new_session(t, display);
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
        e = scp_v1s_list_sessions40(t);
    }

    /* cleanup */
    if (do_auth_end)
    {
        auth_end(data);
    }
    g_free(slist);
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process4(struct trans *t, struct SCP_SESSION *s)
{
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process41(struct trans *t, struct SCP_SESSION *s)
{
    int scount;
    enum SCP_SERVER_STATES_E e;
    struct SCP_DISCONNECTED_SESSION *slist;

    /* list disconnected sessions */
    slist = session_get_byuser(s->username, &scount,
                               SESMAN_SESSION_STATUS_DISCONNECTED);

    if (scount == 0)
    {
        /* */
        return SCP_SERVER_STATE_END;
    }

    e = scp_v1s_list_sessions42(t, scount, slist);
    if (SCP_SERVER_STATE_OK != e)
    {
        LOG(LOG_LEVEL_WARNING, "scp_v1s_list_sessions42 failed");
    }

    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process43(struct trans *t, struct SCP_SESSION *s)
{
    struct session_item *sitem;
    enum SCP_SERVER_STATES_E e;
    int display;

    sitem = session_get_bypid(s->return_sid);
    if (0 == sitem)
    {
        e = scp_v1s_connection_error(t, "Internal error");
        LOG(LOG_LEVEL_INFO, "No session exists with PID %d", s->return_sid);
    }
    else
    {
        display = sitem->display;
        e = scp_v1s_reconnect_session(t, display);
        if (SCP_SERVER_STATE_OK != e)
        {
            LOG(LOG_LEVEL_ERROR, "scp_v1s_reconnect_session failed");
        }
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
    return e;
}

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process44(struct trans *t, struct SCP_SESSION *s)
{
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
static enum SCP_SERVER_STATES_E
scp_v1_process45(struct trans *t, struct SCP_SESSION *s)
{
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v1_process(struct trans *t, struct SCP_SESSION *s)
{
    ; /* astyle 3.1 needs this, or the switch is badly formatted */
    switch (s->current_cmd)
    {
        case SCP_CMD_LOGIN:
            return scp_v1_process1(t, s);
        case SCP_CMD_RESEND_CREDS:
            return scp_v1_process4(t, s);
        case SCP_CMD_GET_SESSION_LIST:
            return scp_v1_process41(t, s);
        case SCP_CMD_SELECT_SESSION:
            return scp_v1_process43(t, s);
        case SCP_CMD_SELECT_SESSION_CANCEL:
            return scp_v1_process44(t, s);
        case SCP_CMD_FORCE_NEW_CONN:
            return scp_v1_process45(t, s);
    }
    return SCP_SERVER_STATE_END;
}

static void
parseCommonStates(enum SCP_SERVER_STATES_E e, const char *f)
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
