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
#include "os_calls.h"
#include "pre_session_list.h"
#include "scp.h"
#include "sesman.h"

/******************************************************************************/

static int
process_sys_login_response(struct pre_session_item *psi)
{
    int rv;
    int is_logged_in;
    uid_t uid;
    int scp_fd;

    rv = eicp_get_sys_login_response(psi->sesexec_trans, &is_logged_in,
                                     &uid, &scp_fd);
    if (rv == 0)
    {
        LOG(LOG_LEVEL_INFO, "Received sys login status for %s : %s",
            psi->username,
            (is_logged_in) ? "logged in" : "not logged in");

        if (!is_logged_in)
        {
            // This shouldn't happen. Close the connection to the
            // client immediately.
            psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
        }
        else
        {
            /* We've been handed back the client connection */
            psi->client_trans = scp_init_trans_from_fd(scp_fd,
                                TRANS_TYPE_SERVER,
                                sesman_is_term);
            if (psi->client_trans == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Can't re-create client connection");
                g_file_close(scp_fd);
                psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
            }
            else
            {
                psi->client_trans->trans_data_in = sesman_scp_data_in;
                psi->client_trans->callback_data = (void *)psi;
                psi->login_state = E_PS_LOGIN_SYS;
                psi->uid = uid;
            }
        }
    }

    return rv;
}

/******************************************************************************/
int
eicp_process(struct pre_session_item *psi)
{
    enum eicp_msg_code msgno;
    int rv = 0;

    switch ((msgno = eicp_msg_in_get_msgno(psi->sesexec_trans)))
    {
        case E_EICP_SYS_LOGIN_RESPONSE:
            rv = process_sys_login_response(psi);
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

