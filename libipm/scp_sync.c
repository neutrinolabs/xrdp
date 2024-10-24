/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022
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
 * @file scp_sync.c
 * @brief scp definitions (synchronous calls)
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

//nclude "tools_common.h"
//nclude "trans.h"
#include "os_calls.h"
#include "log.h"
#include "scp.h"
#include "scp_sync.h"

/*****************************************************************************/
int
scp_sync_wait_specific(struct trans *t, enum scp_msg_code wait_msgno)
{

    int rv = 0;
    int available = 0;

    while (rv == 0 && !available)
    {
        if ((rv = scp_msg_in_wait_available(t)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Error waiting on sesman transport");
        }
        else
        {
            enum scp_msg_code reply_msgno = scp_msg_in_get_msgno(t);

            available = 1;
            if (reply_msgno != wait_msgno)
            {
                char buff[64];
                scp_msgno_to_str(reply_msgno, buff, sizeof(buff));

                LOG(LOG_LEVEL_WARNING,
                    "Ignoring unexpected message %s", buff);
                scp_msg_in_reset(t);
                available = 0;
            }
        }
    }

    return rv;
}

/*****************************************************************************/
int
scp_sync_uds_login_request(struct trans *t)
{
    int rv = scp_send_uds_login_request(t);
    if (rv == 0)
    {
        if ((rv = scp_sync_wait_specific(t, E_SCP_LOGIN_RESPONSE)) == 0)
        {
            enum scp_login_status login_result;
            int server_closed;
            rv = scp_get_login_response(t, &login_result, &server_closed, NULL);
            if (rv == 0 && login_result != E_SCP_LOGIN_OK)
            {
                char msg[256];
                scp_login_status_to_str(login_result, msg, sizeof(msg));
                g_printf("Login failed; %s\n", msg);
                rv = 1;
                if (!server_closed)
                {
                    (void)scp_send_close_connection_request(t);
                }
            }
            scp_msg_in_reset(t); // Done with this message
        }

    }
    return rv;
}

/*****************************************************************************/
struct list *
scp_sync_list_sessions_request(struct trans *t)
{
    struct list *sessions = list_create();
    if (sessions == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory for sessions list");
    }
    else
    {
        int end_of_list = 0;

        enum scp_list_sessions_status status;
        struct scp_session_info *p;

        int rv = scp_send_list_sessions_request(t);

        sessions->auto_free = 1;

        while (rv == 0 && !end_of_list)
        {
            rv = scp_sync_wait_specific(t, E_SCP_LIST_SESSIONS_RESPONSE);
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
                    if (!list_add_item(sessions, (tintptr)p))
                    {
                        g_free(p);
                        LOG(LOG_LEVEL_ERROR, "Out of memory for session item");
                        rv = 1;
                    }
                    break;

                case E_SCP_LS_END_OF_LIST:
                    end_of_list = 1;
                    break;

                default:
                    LOG(LOG_LEVEL_ERROR,
                        "Unexpected return code %d for session item", status);
                    rv = 1;
            }
            scp_msg_in_reset(t);
        }

        if (rv != 0)
        {
            list_delete(sessions);
            sessions = NULL;
        }
    }

    return sessions;
}

/*****************************************************************************/
int
scp_sync_create_sockdir_request(struct trans *t)
{
    int rv = scp_send_create_sockdir_request(t);
    if (rv == 0)
    {
        rv = scp_sync_wait_specific(t, E_SCP_CREATE_SOCKDIR_RESPONSE);
        if (rv == 0)
        {
            enum scp_create_sockdir_status status;
            rv = scp_get_create_sockdir_response(t, &status);
            if (rv == 0)
            {
                switch (status)
                {
                    case E_SCP_CS_OK:
                        break;

                    case E_SCP_CS_NOT_LOGGED_IN:
                        LOG(LOG_LEVEL_ERROR, "sesman reported not-logged-in");
                        rv = 1;
                        break;

                    case E_SCP_CS_OTHER_ERROR:
                        LOG(LOG_LEVEL_ERROR,
                            "sesman reported fail on create directory");
                        rv = 1;
                        break;
                }
            }
            scp_msg_in_reset(t); // Done with this message
            if (!rv)
            {
                (void)scp_send_close_connection_request(t);
            }
        }

    }
    return rv;
}
