/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2023
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
 * @file eicp_process.c
 * @brief eicp (executive initialisation control protocol) handler function
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "trans.h"

#include "eicp.h"
#include "eicp_process.h"
#include "ercp.h"
#include "os_calls.h"
#include "scp_list.h"
#include "session_list.h"
#include "scp.h"
#include "sesman.h"
#include "sesman_access.h"
#include "sesman_config.h"
#include "guid.h"

/******************************************************************************/

static int
process_sys_login_response(struct scp_list_item *sli)
{
    int rv;
    int is_logged_in;
    uid_t uid;
    int scp_fd;

    rv = eicp_get_sys_login_response(sli->sesexec_trans, &is_logged_in,
                                     &uid, &scp_fd);
    if (rv == 0)
    {
        LOG(LOG_LEVEL_INFO, "Received sys login status for %s : %s",
            sli->username,
            (is_logged_in) ? "logged in" : "not logged in");

        if (!is_logged_in)
        {
            // This shouldn't happen. Close the connection to the
            // client immediately.
            sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
        }
        else
        {
            /* We've been handed back the client connection */
            sli->client_trans = scp_init_trans_from_fd(scp_fd,
                                TRANS_TYPE_SERVER,
                                sesman_is_term);
            if (sli->client_trans == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Can't re-create client connection");
                g_file_close(scp_fd);
                sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
            }
            else
            {
                sli->client_trans->trans_data_in = sesman_scp_data_in;
                sli->client_trans->callback_data = (void *)sli;
                sli->login_state = E_SLI_LOGIN_SYS;
                sli->uid = uid;
                // For system logins, don't allow admin access
                //sli->is_admin = access_login_mng_allowed(&g_cfg->sec,
                //                sli->username);
                sli->is_admin = 0;
            }
        }
    }

    return rv;
}

/******************************************************************************/
static int
process_create_session_response(struct scp_list_item *sli)
{
    struct session_item *s_item;
    const char *display;
    struct guid guid;
    enum scp_screate_status status;

    int rv = eicp_get_create_session_response(sli->sesexec_trans,
             &status, &display, &guid);
    if (rv == 0)
    {
        // Create an entry on the session list for the new session
        if (status == E_SCP_SCREATE_OK &&
                (s_item = session_list_new()) == NULL)
        {
            status = E_SCP_SCREATE_NO_MEMORY;
        }

        if (status == E_SCP_SCREATE_OK)
        {
            // Further comms from sesexec comes over the ERCP
            // protocol
            ercp_trans_from_eicp_trans(sli->sesexec_trans,
                                       sesman_ercp_data_in,
                                       (void *)s_item);

            // Move the new ERCP transport over to the session list item,
            // and initialise enough data so that a connection request
            // can be serviced.
            s_item->sesexec_trans = sli->sesexec_trans;
            s_item->sesexec_pid = sli->sesexec_pid;
            s_item->guid = guid;
            s_item->uid = sli->uid;
            strlcpy(s_item->display, display, sizeof(s_item->display));

            // We don't use the sesexec process again
            sli->sesexec_trans = NULL;
            sli->sesexec_pid = 0;
        }
        else
        {
            guid_clear(&guid);
            display = "";
        }
        rv = scp_send_create_session_response(sli->client_trans, status,
                                              display, &guid);
        sli->create_session_in_progress = 0;
        sli->session_x11_display = -1;
    }

    return rv;
}
/******************************************************************************/
int
eicp_process(struct scp_list_item *sli)
{
    enum eicp_msg_code msgno;
    int rv = 0;

    switch ((msgno = eicp_msg_in_get_msgno(sli->sesexec_trans)))
    {
        case E_EICP_SYS_LOGIN_RESPONSE:
            rv = process_sys_login_response(sli);
            break;

        case E_EICP_CREATE_SESSION_RESPONSE:
            rv = process_create_session_response(sli);
            break;

        default:
        {
            char buff[64];
            eicp_msgno_to_str(msgno, buff, sizeof(buff));
            LOG(LOG_LEVEL_ERROR, "Ignored EICP message %s", buff);
        }
    }
    return rv;
}

