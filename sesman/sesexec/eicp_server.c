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
#include "sesexec_discover.h"
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
            if (g_login_info != NULL)
            {
                // Shouldn't get here. Prevent a memory leak.
                LOG(LOG_LEVEL_WARNING,
                    "Asked to sys login when a login has already been made");
                login_info_free(g_login_info);
            }
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
handle_uds_login_request(struct trans *self)
{
    int scp_fd;

    int rv = eicp_get_uds_login_request(self, &scp_fd);
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
            if (g_login_info != NULL)
            {
                // Shouldn't get here. Prevent a memory leak.
                LOG(LOG_LEVEL_WARNING,
                    "Asked to UDS login when a login has already been made");
                login_info_free(g_login_info);
            }
            // The following call logs errors, but these are not
            // returned to the caller, as the call is expected to succeed.
            if ((g_login_info = login_info_uds_login_user(scp_trans)) == NULL)
            {
                rv = 1;
            }
            trans_delete(scp_trans);
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
    struct session_parameters sp = {0};
    int status;

    status = eicp_get_create_session_request(
                 self, &sp.x11_display,
                 &sp.type, &sp.width, &sp.height,
                 &sp.bpp, &sp.shell, &sp.directory,
                 &sp.instance_name, &sp.client_name);
    if (status == 0)
    {
        enum scp_screate_status scp_status = E_SCP_SCREATE_OK;
        const char *display = "";

        // Must be logged in to start a session
        if (g_login_info == NULL)
        {
            scp_status = E_SCP_SCREATE_NOT_LOGGED_IN;
        }
        else
        {
            // Try to create the session
            sp.guid = guid_new();
            scp_status = session_start(g_login_info, &sp, &g_session_data);
            if (scp_status == E_SCP_SCREATE_OK)
            {
                display = session_get_display(g_session_data);
            }
        }

        // Return the creation status to sesman.
        status = eicp_send_create_session_response(self, scp_status, display,
                 &sp.guid);
        if (status == 0 && scp_status == E_SCP_SCREATE_OK)
        {
            // Further comms to sesman is sent over the ERCP protocol
            ercp_trans_from_eicp_trans(self, sesexec_ercp_data_in, NULL);

            // Announce the session to sesman
            if ((status = ercp_send_session_announce_event(
                              self,
                              display,
                              g_login_info->uid,
                              sp.type,
                              sp.width,
                              sp.height,
                              sp.bpp,
                              &sp.guid,
                              g_login_info->ip_addr,
                              session_get_start_time(g_session_data),
                              sp.instance_name,
                              sp.client_name)) != 0)
            {
                // We failed to tell sesman about the new session. This
                // probably means sesman has exited in the time between
                // asking us to start a session, and our reply. This
                // could be many seconds, and a new sesman may well
                // have started.
                // If we enable the restart functionality at
                // this point, we have a race condition that could
                // result in a session which sesman doesn't know
                // about. The simplest thing to do in this rare situation
                // is to abort the session - the user can create a
                // new one
                LOG(LOG_LEVEL_ERROR,
                    "sesman appears to have failed - stopping session");
            }
            else if ((status = sesexec_discover_enable()) != 0)
            {
                // Equally regrettable - we can't make the session
                // discoverable, so we'll stop it.
                LOG(LOG_LEVEL_ERROR,
                    "unable to make session discoverable"
                    " - stopping session");
            }
        }
    }

    if (status != 0)
    {
        // Kill sesexec, and any active session
        sesexec_terminate_main_loop(status);
    }
    return 0;
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

        case E_EICP_UDS_LOGIN_REQUEST:
            rv = handle_uds_login_request(self);
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
