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
 * @file eicp_server.c
 * @brief eicp (executive initialisation control protocol) server function
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "trans.h"

#include "eicp.h"
#include "eicp_server.h"
#include "login_info.h"
#include "os_calls.h"
#include "ercp.h"
#include "scp.h"
#include "sesexec.h"
#include "session.h"

/******************************************************************************/
static int
handle_sys_login_request(struct trans *self)
{
    const char *username;
    const char *password;
    const char *ip_addr;
    int scp_fd;

    int rv = eicp_get_sys_login_request(self, &username,
                                        &password, &ip_addr, &scp_fd);
    if (rv == 0)
    {
        struct trans *scp_trans;
        scp_trans = scp_init_trans_from_fd(scp_fd, TRANS_TYPE_SERVER,
                                           sesexec_is_term);
        if (scp_trans == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Can't create SCP trans");
            g_file_close(scp_fd);
            rv = 1;
        }
        else
        {
            g_login_info = login_info_sys_login_user(scp_trans, username,
                           password, ip_addr);

            if (g_login_info != NULL)
            {
                rv = eicp_send_sys_login_response(self, 1,
                                                  g_login_info->uid, scp_fd);
            }
            else
            {
                rv = eicp_send_sys_login_response(self, 0, (uid_t) -1, 0);
            }

            trans_delete(scp_trans); // Closes scp_fd as well
        }
    }

    return rv;
}

/******************************************************************************/
static int
handle_logout_request(struct trans *self)
{
    LOG(LOG_LEVEL_INFO, "xrdp-sesexec pid %d is now logging out", g_pid);
    sesexec_terminate_main_loop(0);
    return 0;
}

/******************************************************************************/
static int
handle_create_session_request(struct trans *self)
{
    int scp_fd;
    struct session_parameters sp = {0};
    int rv;

    rv = eicp_get_create_session_request(self, &scp_fd, &sp.display,
                                         &sp.type, &sp.width, &sp.height,
                                         &sp.bpp, &sp.shell, &sp.directory);
    if (rv == 0)
    {
        // Need to talk to the SCP client
        struct trans *scp_trans;
        scp_trans = scp_init_trans_from_fd(scp_fd, TRANS_TYPE_SERVER,
                                           sesexec_is_term);
        if (scp_trans == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Can't create SCP trans");
            g_file_close(scp_fd);
            rv = 1;
        }
        else
        {
            enum scp_screate_status scp_status = E_SCP_SCREATE_OK;

            // Use the UID from the SCP connection if the user hasn't
            // explicitly logged in
            if (g_login_info == NULL &&
                    (g_login_info = login_info_uds_login_user(scp_trans)) == NULL)
            {
                scp_status = E_SCP_SCREATE_GENERAL_ERROR;
            }

            if (scp_status == E_SCP_SCREATE_OK)
            {
                // Try to create the session
                sp.guid = guid_new();
                scp_status = session_start(g_login_info, &sp, &g_session_data);
            }

            // Return the status to the SCP client
            rv = scp_send_create_session_response(scp_trans, scp_status,
                                                  sp.display, &sp.guid);
            trans_delete(scp_trans);

            // Further comms from sesexec is sent over the ERCP protocol
            ercp_trans_from_eicp_trans(self,
                                       sesexec_ercp_data_in,
                                       (void *)self);

            if (scp_status == E_SCP_SCREATE_OK)
            {
                rv = ercp_send_session_announce_event(
                         self,
                         sp.display,
                         g_login_info->uid,
                         sp.type,
                         sp.width,
                         sp.height,
                         sp.bpp,
                         &sp.guid,
                         g_login_info->ip_addr,
                         session_get_start_time(g_session_data));
            }
            else
            {
                rv = ercp_send_session_finished_event(self);
                sesexec_terminate_main_loop(1);
            }
        }

    }
    return rv;
}

/******************************************************************************/
int
eicp_server(struct trans *self)
{
    int rv = 0;
    enum eicp_msg_code msgno;

    switch ((msgno = eicp_msg_in_get_msgno(self)))
    {
        case E_EICP_SYS_LOGIN_REQUEST:
            rv = handle_sys_login_request(self);
            break;

        case E_EICP_LOGOUT_REQUEST:
            rv = handle_logout_request(self);
            break;

        case E_EICP_CREATE_SESSION_REQUEST:
            rv = handle_create_session_request(self);
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
