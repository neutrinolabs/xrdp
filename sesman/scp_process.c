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
#include "eicp.h"
#include "ercp.h"
#include "scp.h"

#include "scp_process.h"
#include "sesman.h"
#include "sesman_access.h"
#include "sesman_auth.h"
#include "sesman_config.h"
#include "os_calls.h"
#include "pre_session_list.h"
#include "session_list.h"
#include "sesexec_control.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

/******************************************************************************/

static int
process_set_peername_request(struct pre_session_item *psi)
{
    int rv;
    const char *peername;

    rv = scp_get_set_peername_request(psi->client_trans, &peername);
    if (rv == 0)
    {
        if (pre_session_list_set_peername(psi, peername) != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "Failed to set connection peername from %s to %s",
                psi->peername, peername);
        }
    }

    return rv;
}

/******************************************************************************/
static int
process_sys_login_request(struct pre_session_item *psi)
{
    int rv;
    const char *username;
    const char *password;
    const char *ip_addr;
    int send_client_reply = 1;

    rv = scp_get_sys_login_request(psi->client_trans, &username,
                                   &password, &ip_addr);
    if (rv == 0)
    {
        enum scp_login_status errorcode;

        LOG(LOG_LEVEL_INFO,
            "Received system login request from %s for user: %s IP: %s",
            psi->peername, username, ip_addr);

        if (psi->login_state != E_PS_LOGIN_NOT_LOGGED_IN)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                psi->username);
        }
        else if ((psi->username = g_strdup(username)) == NULL)
        {
            errorcode = E_SCP_LOGIN_NO_MEMORY;
            LOG(LOG_LEVEL_ERROR, "Memory allocation failure logging in %s",
                username);
        }
        else
        {
            /*
             * Copy the IP address of the requesting user, anticipating a
             * successful login. We need this so we can search for a session
             * with a matching IP address if required.
             */
            g_snprintf(psi->start_ip_addr, sizeof(psi->start_ip_addr),
                       "%s", ip_addr);

            /* Create a sesexec process to handle the login
             *
             * We won't check for the user being valid here, as this might
             * lead to information leakage */
            if (sesexec_start(psi) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't start sesexec to authenticate user");
                errorcode = E_SCP_LOGIN_GENERAL_ERROR;
            }
            else
            {
                int eicp_stat;
                eicp_stat = eicp_send_sys_login_request(psi->sesexec_trans,
                                                        username,
                                                        password,
                                                        ip_addr,
                                                        psi->client_trans->sck);
                if (eicp_stat != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Can't ask sesexec to authenticate user");
                    errorcode = E_SCP_LOGIN_GENERAL_ERROR;
                }
                else
                {
                    /* We've handed over responsibility for the
                     * SCP communication */
                    send_client_reply = 0;
                    psi->dispatcher_action = E_PSD_REMOVE_CLIENT_TRANS;
                }
            }
        }

        if (send_client_reply)
        {
            /* We only get here if something has gone
             * wrong with the handover to sesexec */
            rv = scp_send_login_response(psi->client_trans, errorcode, 1, -1);
            psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
        }
    }

    return rv;
}

/******************************************************************************/

/**
 * Authenticate and authorize a UDS connection
 *
 * @param psi  Connection to sesman
 * @param uid UID for user
 * @param username Name for user
 * @return Status for the operation
 *
 * @post If E_SCP_LOGIN_OK is returned, psi->username is non-NULL
 */
static enum scp_login_status
authenticate_and_authorize_uds_connection(struct pre_session_item *psi,
        int uid,
        const char *username)
{
    enum scp_login_status status = E_SCP_LOGIN_GENERAL_ERROR;
    struct auth_info *auth_info = auth_uds(username, &status);
    if (auth_info != NULL)
    {
        if (status != E_SCP_LOGIN_OK)
        {
            /* This shouldn't happen */
            LOG(LOG_LEVEL_ERROR,
                "Unexpected status return %d from auth_uds call",
                (int)status);
        }
        else if (!access_login_allowed(&g_cfg->sec, username))
        {
            status = E_SCP_LOGIN_NOT_AUTHORIZED;
            LOG(LOG_LEVEL_INFO, "Username okay but group problem for "
                "user: %s", username);
        }

        /* If all is well, add info to the sesman connection for later use */
        if (status == E_SCP_LOGIN_OK)
        {
            if ((psi->username = g_strdup(username)) == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "%s : Memory allocation failed",
                    __func__);
                g_free(psi->username);
                psi->username = NULL;
                status = E_SCP_LOGIN_NO_MEMORY;
            }
            else
            {
                LOG(LOG_LEVEL_INFO, "Access permitted for user: %s",
                    username);
                psi->login_state = E_PS_LOGIN_UDS;
                psi->uid = uid;
                psi->start_ip_addr[0] = '\0';
            }
        }

        auth_end(auth_info);
    }

    return status;
}

/******************************************************************************/

static int
process_uds_login_request(struct pre_session_item *psi)
{
    enum scp_login_status errorcode;
    int rv;
    int uid;
    int pid;
    char *username = NULL;
    int server_closed;

    rv = g_sck_get_peer_cred(psi->client_trans->sck, &pid, &uid, NULL);
    if (rv != 0)
    {
        errorcode = E_SCP_LOGIN_GENERAL_ERROR;
        LOG(LOG_LEVEL_INFO,
            "Unable to get peer credentials for socket %d",
            (int)psi->client_trans->sck);
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received UDS login request from %s for UID: %d from PID: %d",
            psi->peername, uid, pid);

        if (psi->login_state != E_PS_LOGIN_NOT_LOGGED_IN)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                psi->username);
        }
        else if (g_getuser_info_by_uid(uid, &username,
                                       NULL, NULL, NULL, NULL) != 0)
        {
            errorcode = E_SCP_LOGIN_GENERAL_ERROR;
            LOG(LOG_LEVEL_ERROR, "Can't reverse lookup UID %d", uid);
        }
        else
        {
            errorcode = authenticate_and_authorize_uds_connection(
                            psi, uid, username);
            g_free(username);
        }
    }

    if (errorcode == E_SCP_LOGIN_OK)
    {
        server_closed = 0;
    }
    else
    {
        server_closed = 1;

        /* Close the connection after returning from this callback */
        psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;

        /* Never return the UID if the server is closing */
        uid = -1;
    }

    return scp_send_login_response(psi->client_trans, errorcode,
                                   server_closed, uid);
}

/******************************************************************************/

static void
logout_pre_session(struct pre_session_item *psi)
{
    if (psi->login_state != E_PS_LOGIN_NOT_LOGGED_IN)
    {
        (void)eicp_send_logout_request(psi->sesexec_trans);
        trans_delete(psi->sesexec_trans);
        psi->sesexec_trans = NULL;
        psi->uid = (uid_t) -1;
        g_free(psi->username);
        psi->username = NULL;
        psi->start_ip_addr[0] = '\0';

        psi->login_state = E_PS_LOGIN_NOT_LOGGED_IN;
    }
}

/******************************************************************************/

static int
process_logout_request(struct pre_session_item *psi)
{
    if (psi->login_state != E_PS_LOGIN_NOT_LOGGED_IN)
    {
        LOG(LOG_LEVEL_INFO, "Logging out %s from sesman", psi->username);
        logout_pre_session(psi);
    }

    return 0;
}

/******************************************************************************/
/**
 * Create xrdp socket path for the user
 *
 * We do this here rather than in sesexec as we're single-threaded here
 * and so don't have to worry about race conditions
 *
 * Directory is owned by UID of session, but can be accessed by
 * the group specified in the config.
 *
 * Errors are logged so the caller doesn't have to
 */
static int
create_xrdp_socket_path(uid_t uid)
{
    // Owner all permissions, group read+execute
#define RWX_PERMS 0x750

    int rv = 1;
    const char *sockdir_group = g_cfg->sec.session_sockdir_group;
    int gid = 0; // Default if no group specified

    char sockdir[XRDP_SOCKETS_MAXPATH];
    g_snprintf(sockdir, sizeof(sockdir), XRDP_SOCKET_PATH, (int)uid);

    // Create directory permissions RWX_PERMS, if it doesn't exist already
    // (our os_calls layer doesn't allow us to set the SGID bit here)
    int old_umask = g_umask_hex(RWX_PERMS ^ 0x777);
    if (!g_directory_exist(sockdir) && !g_create_dir(sockdir))
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_path: Can't create %s [%s]",
            sockdir, g_get_strerror());
    }
    else if (g_chmod_hex(sockdir, RWX_PERMS | 0x2000) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_path: Can't set SGID bit on %s [%s]",
            sockdir, g_get_strerror());
    }
    else if (sockdir_group != NULL && sockdir_group[0] != '\0' &&
             g_getgroup_info(sockdir_group, &gid) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_path: Can't get GID of group %s [%s]",
            sockdir_group, g_get_strerror());
    }
    else if (g_chown(sockdir, uid, gid) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "create_xrdp_socket_path: Can't set owner of %s to %d:%d [%s]",
            sockdir, uid, gid, g_get_strerror());
    }
    else
    {
        rv = 0;
    }
    (void)g_umask_hex(old_umask);

    return rv;
#undef RWX_PERMS
}

/******************************************************************************/

static int
process_create_session_request(struct pre_session_item *psi)
{
    int rv;
    /* Client parameters describing new session*/
    enum scp_session_type type;
    unsigned short width;
    unsigned short height;
    unsigned char bpp;
    const char *shell;
    const char *directory;

    struct guid guid;
    int display = 0;
    struct session_item *s_item = NULL;
    int send_client_reply = 1;

    enum scp_screate_status status = E_SCP_SCREATE_OK;

    rv = scp_get_create_session_request(psi->client_trans,
                                        &type, &width, &height,
                                        &bpp, &shell, &directory);

    if (rv == 0)
    {
        if (psi->login_state == E_PS_LOGIN_NOT_LOGGED_IN)
        {
            status = E_SCP_SCREATE_NOT_LOGGED_IN;
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "Received request from %s to create a session for user %s",
                psi->peername, psi->username);

            s_item = session_list_get_bydata(psi->uid, type, width, height,
                                             bpp, psi->start_ip_addr);
            if (s_item != NULL)
            {
                // Found an existing session
                display = s_item->display;
                guid = s_item->guid;

                // Tell the existing session to run the reconnect script.
                // We ignore errors at this level, as any comms errors
                // will be picked up in the main loop
                (void)ercp_send_session_reconnect_event(s_item->sesexec_trans);

                if (psi->start_ip_addr[0] != '\0')
                {
                    LOG( LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                         "display :%d.0, session_pid %d, ip %s",
                         psi->username, display,
                         s_item->sesexec_pid, psi->start_ip_addr);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                        "display :%d.0, session_pid %d",
                        psi->username, display, s_item->sesexec_pid);
                }

                // If we created an authentication process for this SCP
                // connection, close it gracefully
                logout_pre_session(psi);
            }
            // Need to create a new session
            else if (g_cfg->sess.max_sessions > 0 &&
                     session_list_get_count() >= g_cfg->sess.max_sessions)
            {
                status = E_SCP_SCREATE_MAX_REACHED;
            }
            else if ((display = session_list_get_available_display()) < 0)
            {
                status = E_SCP_SCREATE_NO_DISPLAY;
            }
            // Create an entry on the session list for the new session
            else if ((s_item = session_list_new()) == NULL)
            {
                status = E_SCP_SCREATE_NO_MEMORY;
            }
            // Create a socket dir for this user
            else if (create_xrdp_socket_path(psi->uid) != 0)
            {
                status = E_SCP_SCREATE_GENERAL_ERROR;
            }
            // Create a sesexec process if we don't have one (UDS login)
            else if (psi->sesexec_trans == NULL && sesexec_start(psi) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't start sesexec to manage session");
                status = E_SCP_SCREATE_GENERAL_ERROR;
            }
            else
            {
                // Pass the session create request to sesexec
                int eicp_stat;
                eicp_stat = eicp_send_create_session_request(
                                psi->sesexec_trans,
                                psi->client_trans->sck,
                                display,
                                type, width, height,
                                bpp, shell, directory);

                if (eicp_stat != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Can't ask sesexec to authenticate user");
                    status = E_SCP_SCREATE_GENERAL_ERROR;
                }
                else
                {
                    // We've handed over responsibility for the
                    // SCP communication
                    send_client_reply = 0;

                    // Further comms from sesexec comes over the ERCP
                    // protocol
                    ercp_trans_from_eicp_trans(psi->sesexec_trans,
                                               sesman_ercp_data_in,
                                               (void *)s_item);

                    // Move the transport over to the session list item
                    s_item->sesexec_trans = psi->sesexec_trans;
                    s_item->sesexec_pid = psi->sesexec_pid;
                    psi->sesexec_trans = NULL;
                    psi->sesexec_pid = 0;

                    // Add the display to the session item so we don't try
                    // to allocate it to another session
                    s_item->display = display;
                }
            }
        }

        // Currently a create session request is the last thing on a
        // connection, and results in automatic closure
        //
        // We may have passed the client_trans over to sesexec. If so,
        // we can't send a reply here.
        psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
        if (send_client_reply)
        {
            rv = scp_send_create_session_response(psi->client_trans,
                                                  status, display, &guid);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_list_sessions_request(struct pre_session_item *psi)
{
    int rv = 0;

    struct scp_session_info *info = NULL;
    unsigned int cnt = 0;
    unsigned int i;

    if (psi->login_state == E_PS_LOGIN_NOT_LOGGED_IN)
    {
        rv = scp_send_list_sessions_response(psi->client_trans,
                                             E_SCP_LS_NOT_LOGGED_IN,
                                             NULL);
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received request from %s to list sessions for user %s",
            psi->peername, psi->username);

        info = session_list_get_byuid(psi->uid, &cnt, 0);

        for (i = 0; rv == 0 && i < cnt; ++i)
        {
            rv = scp_send_list_sessions_response(psi->client_trans,
                                                 E_SCP_LS_SESSION_INFO,
                                                 &info[i]);
        }
        free_session_info_list(info, cnt);

        if (rv == 0)
        {
            rv = scp_send_list_sessions_response(psi->client_trans,
                                                 E_SCP_LS_END_OF_LIST,
                                                 NULL);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_create_sockdir_request(struct pre_session_item *psi)
{
    enum scp_create_sockdir_status status = E_SCP_CS_OTHER_ERROR;

    if (psi->login_state == E_PS_LOGIN_NOT_LOGGED_IN)
    {
        status = E_SCP_CS_NOT_LOGGED_IN;
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received request from %s to create sockdir for user %s",
            psi->peername, psi->username);

        if (create_xrdp_socket_path(psi->uid) == 0)
        {
            status = E_SCP_CS_OK;
        }
    }

    return scp_send_create_sockdir_response(psi->client_trans, status);
}

/******************************************************************************/

static int
process_close_connection_request(struct pre_session_item *psi)
{
    int rv = 0;

    LOG(LOG_LEVEL_INFO, "Received request to close connection from %s",
        psi->peername);

    /* Expecting no more client messages. Close the connection
     * after returning from this callback */
    psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
    return rv;
}

/******************************************************************************/
int
scp_process(struct pre_session_item *psi)
{
    enum scp_msg_code msgno;
    int rv = 0;

    switch ((msgno = scp_msg_in_get_msgno(psi->client_trans)))
    {
        case E_SCP_SET_PEERNAME_REQUEST:
            rv = process_set_peername_request(psi);
            break;

        case E_SCP_SYS_LOGIN_REQUEST:
            rv = process_sys_login_request(psi);
            break;

        case E_SCP_UDS_LOGIN_REQUEST:
            rv = process_uds_login_request(psi);
            break;

        case E_SCP_LOGOUT_REQUEST:
            rv = process_logout_request(psi);
            break;

        case E_SCP_CREATE_SESSION_REQUEST:
            rv = process_create_session_request(psi);
            break;

        case E_SCP_LIST_SESSIONS_REQUEST:
            rv = process_list_sessions_request(psi);
            break;

        case E_SCP_CREATE_SOCKDIR_REQUEST:
            rv = process_create_sockdir_request(psi);
            break;

        case E_SCP_CLOSE_CONNECTION_REQUEST:
            rv = process_close_connection_request(psi);
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

