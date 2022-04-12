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
 * @file scp.c
 * @brief scp (sesman control protocol) handler function
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "trans.h"
#include "os_calls.h"
#include "scp.h"

#include "scp_process.h"
#include "access.h"
#include "auth.h"
#include "session.h"

/**************************************************************************//**
 * Logs an authentication failure message
 *
 * @param username Username
 * @param peer_details Details of connecting peer
 *
 * The message is intended for use by fail2ban. Make changes with care.
 */
static void
log_authfail_message(const char *username, const struct peer *peer_details)
{
    const char *ipp;
    if (peer_details->ip[0] != '\0')
    {
        ipp = peer_details->ip;
    }
    else
    {
        ipp = "unknown";
    }
    LOG(LOG_LEVEL_INFO, "AUTHFAIL: user=%s ip=%s time=%d",
        username, ipp, g_time1());
}

/******************************************************************************/

static int
process_gateway_request(struct trans *trans)
{
    int rv;
    const char *username;
    const char *password;
    struct peer peer_details;

    rv = scp_get_gateway_request(trans, &username, &password,
                                 &peer_details);
    if (rv == 0)
    {
        int errorcode = 0;
        tbus data;

        LOG(LOG_LEVEL_INFO,
            "Received authentication request for user: %s ip: %s",
            username, peer_details.ip);

        data = auth_userpass(username, password, &peer_details, &errorcode);
        if (data)
        {
            if (1 == access_login_allowed(username))
            {
                /* the user is member of the correct groups. */
                LOG(LOG_LEVEL_INFO, "Access permitted for user: %s",
                    username);
            }
            else
            {
                /* all first 32 are reserved for PAM errors */
                errorcode = 32 + 3;
                LOG(LOG_LEVEL_INFO, "Username okay but group problem for "
                    "user: %s", username);
            }
        }
        else
        {
            log_authfail_message(username, &peer_details);
        }
        rv = scp_send_gateway_response(trans, errorcode);
        auth_end(data);
    }
    return rv;
}

/******************************************************************************/

static int
process_create_session_request(struct trans *trans)
{
    int rv;
    struct session_parameters sp;
    const char *password;
    struct guid guid;

    guid_clear(&guid);
    int display = 0;

    rv = scp_get_create_session_request(
             trans,
             &sp.username, &password,
             &sp.type, &sp.width, &sp.height, &sp.bpp,
             &sp.shell, &sp.directory, &sp.peer_details);

    if (rv == 0)
    {
        tbus data;
        struct session_item *s_item;
        int errorcode = 0;
        bool_t do_auth_end = 1;

        LOG(LOG_LEVEL_INFO,
            "Received request to create %s session for user: %s ip: %s",
            SCP_SESSION_TYPE_TO_STR(sp.type),
            sp.username, sp.peer_details.ip);

        data = auth_userpass(sp.username, password, &sp.peer_details, &errorcode);
        if (data)
        {
            s_item = session_get_bydata(&sp);
            if (s_item != 0)
            {
                display = s_item->display;
                guid = s_item->guid;
                if (sp.peer_details.ip[0] != '\0')
                {
                    LOG( LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                         "display :%d.0, session_pid %d, ip %s",
                         sp.username, display, s_item->pid,
                         sp.peer_details.ip);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                        "display :%d.0, session_pid %d", sp.username, display,
                        s_item->pid);
                }

                /*
                 * Update the last connected peer
                 * TODO: Clear this field on disconnection */
                s_item->peer_details = sp.peer_details;

                session_reconnect(display, sp.username, data);
            }
            else
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "pre auth");

                if (1 == access_login_allowed(sp.username))
                {
                    if (sp.peer_details.ip[0] != '\0')
                    {
                        LOG(LOG_LEVEL_INFO,
                            "++ created session (access granted): "
                            "username %s, ip %s", sp.username,
                            sp.peer_details.ip);
                    }
                    else
                    {
                        LOG(LOG_LEVEL_INFO,
                            "++ created session (access granted): "
                            "username %s", sp.username);
                    }

                    display = session_start(data, &sp, &guid);

                    /* if the session started up ok, auth_end will be called on
                       sig child */
                    do_auth_end = display == 0;
                }
            }
        }
        else
        {
            log_authfail_message(sp.username, &sp.peer_details);
        }

        if (do_auth_end)
        {
            auth_end(data);
        }

        rv = scp_send_create_session_response(trans, errorcode, display, &guid);
    }

    return rv;
}

/******************************************************************************/

static int
process_list_sessions_request(struct trans *trans)
{
    int rv;

    const char *username;
    const char *password;

    rv = scp_get_list_sessions_request(trans, &username, &password);
    if (rv == 0)
    {
        enum scp_list_sessions_status status;
        int errorcode = 0;
        tbus data;

        LOG(LOG_LEVEL_INFO,
            "Received request to list sessions for user %s", username);

        data = auth_userpass(username, password, NULL, &errorcode);
        if (data)
        {
            struct scp_session_info *info = NULL;
            unsigned int cnt = 0;
            unsigned int i;
            info = session_get_byuser(username, &cnt,
                                      SESMAN_SESSION_STATUS_ALL);

            for (i = 0; rv == 0 && i < cnt; ++i)
            {
                rv = scp_send_list_sessions_response(trans,
                                                     E_SCP_LS_SESSION_INFO,
                                                     &info[i]);
            }
            free_session_info_list(info, cnt);
            status = E_SCP_LS_END_OF_LIST;
        }
        else
        {
            status = E_SCP_LS_AUTHENTICATION_FAIL;
        }
        auth_end(data);

        if (rv == 0)
        {
            rv = scp_send_list_sessions_response(trans, status, NULL);
        }
    }

    return rv;
}

/******************************************************************************/
int
scp_process(struct trans *t)
{
    enum scp_msg_code msgno;
    int rv = 0;

    switch ((msgno = scp_msg_in_start(t)))
    {
        case E_SCP_GATEWAY_REQUEST:
            rv = process_gateway_request(t);
            break;

        case E_SCP_CREATE_SESSION_REQUEST:
            rv = process_create_session_request(t);
            break;

        case E_SCP_LIST_SESSIONS_REQUEST:
            rv = process_list_sessions_request(t);
            break;

        default:
        {
            char buff[64];
            scp_msgno_to_str(msgno, buff, sizeof(buff));
            LOG(LOG_LEVEL_ERROR, "Ignored SCP message %s", buff);
        }
    }
    return rv;
}

