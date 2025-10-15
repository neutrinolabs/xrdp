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
 * @file ercp_server.c
 * @brief ercp (executive run-time control protocol) server function
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"

#include "login_info.h"
#include "scp.h"
#include "sesexec.h"
#include "os_calls.h"
#include "session.h"
#include "sesman_config.h"
#include "string_calls.h"
#include "trans.h"

#include "ercp.h"
#include "ercp_server.h"

/******************************************************************************/
static enum scp_sconnect_status
get_session_fds(struct session_data *sd, unsigned int scp_flags,
                int *display_fd, int *chan_fd)
{
    enum scp_sconnect_status result = E_SCP_SCONNECT_OK;

    if ((*display_fd = session_get_display_server_fd(sd, g_login_info)) < 0)
    {
        result = E_SCP_SCONNECT_SERVER_FAIL;
    }
    else if ((scp_flags & E_SCP_SCONNECT_FLAG_NEED_CHANSRV) == 0)
    {
        // Don't need to try to connect to chansrv
        *chan_fd = -1;
    }
    else
    {
        // If this fails, it's inconvenient, but not a show-stopper
        *chan_fd = session_get_chansrv_fd(sd, g_login_info);
    }

    return result;
}

/******************************************************************************/
static int
handle_connect_session_request(struct trans *self)
{
    int scp_fd = -1;
    unsigned int scp_flags;
    const char *client_ip;
    const char *client_name;
    int rv = ercp_get_connect_session_request(self, &scp_fd, &client_ip,
             &client_name, &scp_flags);
    if (rv == 0)
    {
        struct trans *scp_trans;

        if ((scp_trans = scp_init_trans_from_fd(scp_fd,
                                                TRANS_TYPE_SERVER,
                                                sesexec_is_term)) == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Can't create SCP trans");
            rv = 1;
        }
        else
        {
            // Ownership of the file descriptor is passed to scp_trans;
            // don't delete it separately.
            scp_fd = -1;

            // Now we've got a transport we can send data back to
            // the SCP client
            enum scp_sconnect_status scp_status;
            int display_fd = -1;
            int chan_fd = -1;

            // Terminate any existing xrdp process to sesexec
            if (g_ccp_trans != NULL)
            {
                sesexec_terminate_connected_xrdp_process(
                    CCP_CLOSE_DISCONNECTED_BY_OTHERCONNECTION);
                g_sleep(500);
            }
            scp_status = get_session_fds(g_session_data, scp_flags,
                                         &display_fd, &chan_fd);

            if (scp_status == E_SCP_SCONNECT_OK)
            {
                // Tell sesman about the new client connection
                strlcpy(g_client_ip, client_ip, sizeof(g_client_ip));
                strlcpy(g_client_name, client_name, sizeof(g_client_name));
                g_last_connect_disconnect = time(NULL);

                if (g_ecp_trans != NULL)
                {
                    (void)ercp_send_client_connect_event(
                        g_ecp_trans, g_client_ip, g_client_name,
                        g_last_connect_disconnect);

                }
            }

            // Pass the session file descriptors to the client
            rv = scp_send_connect_session_response(scp_trans, scp_status,
                                                   display_fd, chan_fd);

            if (rv == 0 && scp_status == E_SCP_SCONNECT_OK)
            {
                // Variables to pass to the reconnect script
                const char *vars[] =
                {
                    "XRDP_CLIENT_IP", g_client_ip,
                    "XRDP_CLIENT_NAME", g_client_name,
                    NULL // Terminator
                };
                // Don't run the reconnect script on the first connect,
                // unless we're configured to do so.
                if (session_increment_connect_count(g_session_data) == 0)
                {
                    LOG(LOG_LEVEL_INFO, "User %s has connected to a session",
                        g_login_info->username);
                    if (g_cfg->always_run_reconnect)
                    {
                        session_run_reconnect_script(g_session_data,
                                                     g_login_info, vars);
                    }
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "User %s has reconnected to a session",
                        g_login_info->username);
                    session_run_reconnect_script(g_session_data,
                                                 g_login_info, vars);
                }

                // Convert the SCP transport to a CCP transport, and
                // record it
                if (sesexec_set_ccp_trans(scp_trans) == 0)
                {
                    scp_trans = NULL; // Prevent transport being deleted.
                }
            }

            // Close all our copies of file descriptors, including the
            // SCP transport if we failed to convert it to a CCP transport
            if (display_fd >= 0)
            {
                g_file_close(display_fd);
            }
            if (chan_fd >= 0)
            {
                g_file_close(chan_fd);
            }
            if (scp_trans != NULL)
            {
                trans_delete(scp_trans);
            }
        }
    }

    if (scp_fd >= 0)
    {
        g_file_close(scp_fd);
    }

    return rv;
}

/******************************************************************************/
int
ercp_server(struct trans *self)
{
    int rv = 0;
    enum ercp_msg_code msgno;

    switch ((msgno = ercp_msg_in_get_msgno(self)))
    {
        case E_ERCP_CONNECT_SESSION_REQUEST:
            rv = handle_connect_session_request(self);
            break;

        default:
        {
            char buff[64];
            ercp_msgno_to_str(msgno, buff, sizeof(buff));
            LOG(LOG_LEVEL_ERROR, "Ignored ERCP message %s", buff);
        }
    }
    return rv;
}
