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

#include "display_utils.h"
#include "scp_process.h"
#include "sesman.h"
#include "sesman_access.h"
#include "sesman_auth.h"
#include "sesman_config.h"
#include "os_calls.h"
#include "set_int.h"
#include "scp_list.h"
#include "session_list.h"
#include "sesexec_control.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

/******************************************************************************/

static int
process_set_peername_request(struct scp_list_item *sli)
{
    int rv;
    const char *peername;

    rv = scp_get_set_peername_request(sli->client_trans, &peername);
    if (rv == 0)
    {
        if (scp_list_set_peername(sli, peername) != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "Failed to set connection peername from %s to %s",
                sli->peername, peername);
        }
    }

    return rv;
}

/******************************************************************************/
static int
process_sys_login_request(struct scp_list_item *sli)
{
    int rv;
    const char *username;
    const char *password;
    const char *ip_addr;
    int send_client_reply = 1;

    rv = scp_get_sys_login_request(sli->client_trans, &username,
                                   &password, &ip_addr);
    if (rv == 0)
    {
        enum scp_login_status errorcode;

        LOG(LOG_LEVEL_INFO,
            "Received system login request from %s for user: %s IP: %s",
            sli->peername, username, ip_addr);

        if (sli->login_state != E_SLI_LOGIN_NOT_LOGGED_IN)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                sli->username);
        }
        else if ((sli->username = g_strdup(username)) == NULL)
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
            g_snprintf(sli->start_ip_addr, sizeof(sli->start_ip_addr),
                       "%s", ip_addr);

            /* Create a sesexec process to handle the login
             *
             * We won't check for the user being valid here, as this might
             * lead to information leakage */
            if (sesexec_start(sli) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't start sesexec to authenticate user");
                errorcode = E_SCP_LOGIN_GENERAL_ERROR;
            }
            else
            {
                int eicp_stat;
                eicp_stat = eicp_send_sys_login_request(sli->sesexec_trans,
                                                        username,
                                                        password,
                                                        ip_addr,
                                                        sli->client_trans->sck);
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
                    sli->dispatcher_action = E_SLD_REMOVE_CLIENT_TRANS;
                }
            }
        }

        if (send_client_reply)
        {
            /* We only get here if something has gone
             * wrong with the handover to sesexec */
            rv = scp_send_login_response(sli->client_trans, errorcode, 1, -1);
            sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
        }
    }

    return rv;
}

/******************************************************************************/

/**
 * Authenticate and authorize a UDS connection
 *
 * @param sli  Connection to sesman
 * @param uid UID for user
 * @param username Name for user
 * @return Status for the operation
 *
 * @post If E_SCP_LOGIN_OK is returned, sli->username is non-NULL
 */
static enum scp_login_status
authenticate_and_authorize_uds_connection(struct scp_list_item *sli,
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
            if ((sli->username = g_strdup(username)) == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "%s : Memory allocation failed",
                    __func__);
                g_free(sli->username);
                sli->username = NULL;
                status = E_SCP_LOGIN_NO_MEMORY;
            }
            else
            {
                sli->login_state = E_SLI_LOGIN_UDS;
                sli->uid = uid;
                sli->start_ip_addr[0] = '\0';
                sli->is_admin = access_login_is_admin(&g_cfg->sec,
                                                      sli->username);
                if (sli->is_admin)
                {
                    LOG(LOG_LEVEL_INFO, "Admin access permitted for user: %s",
                        username);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "Normal access permitted for user: %s",
                        username);
                }
            }
        }

        auth_end(auth_info);
    }

    return status;
}

/******************************************************************************/

static int
process_uds_login_request(struct scp_list_item *sli)
{
    enum scp_login_status errorcode;
    int rv;
    int uid;
    int pid;
    char *username = NULL;
    int server_closed;

    rv = g_sck_get_peer_cred(sli->client_trans->sck, &pid, &uid, NULL);
    if (rv != 0)
    {
        errorcode = E_SCP_LOGIN_GENERAL_ERROR;
        LOG(LOG_LEVEL_INFO,
            "Unable to get peer credentials for socket %d",
            (int)sli->client_trans->sck);
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received UDS login request from %s for UID: %d from PID: %d",
            sli->peername, uid, pid);

        if (sli->login_state != E_SLI_LOGIN_NOT_LOGGED_IN)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                sli->username);
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
                            sli, uid, username);
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
        sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;

        /* Never return the UID if the server is closing */
        uid = -1;
    }

    return scp_send_login_response(sli->client_trans, errorcode,
                                   server_closed, uid);
}

/******************************************************************************/

static void
logout_scp_list_item(struct scp_list_item *sli)
{
    if (sli->login_state != E_SLI_LOGIN_NOT_LOGGED_IN)
    {
        if (sli->sesexec_trans != NULL)
        {
            (void)eicp_send_logout_request(sli->sesexec_trans);
            trans_delete(sli->sesexec_trans);
            sli->sesexec_trans = NULL;
        }
        sli->uid = (uid_t) -1;
        g_free(sli->username);
        sli->username = NULL;
        sli->start_ip_addr[0] = '\0';

        sli->login_state = E_SLI_LOGIN_NOT_LOGGED_IN;
    }
}

/******************************************************************************/

static int
process_logout_request(struct scp_list_item *sli)
{
    if (sli->login_state != E_SLI_LOGIN_NOT_LOGGED_IN)
    {
        LOG(LOG_LEVEL_INFO, "Logging out %s from sesman", sli->username);
        logout_scp_list_item(sli);
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
/*
 * Gets a free display number
 *
 * We can't use displays either allocated to sessions, or being used to
 * create sessions
 */
static int
get_free_display(void)
{
    int result = -1;
    struct set_int *alloc_displays;

    alloc_displays = set_int_init(g_cfg->sess.x11_display_offset,
                                  g_cfg->sess.max_display_number);

    if (alloc_displays != NULL)
    {
        // Get all the displays either allocated to sessions, or
        // potentially assigned to sessions on the SCP list
        session_list_get_session_x11_displays(alloc_displays);
        scp_list_get_create_session_x11_displays(alloc_displays);

        // Find a free display, taking the allocated ones into account
        result = display_utils_get_free_display(alloc_displays);

        set_int_delete(alloc_displays);
    }

    return result;
}

/******************************************************************************/

static int
process_create_session_request(struct scp_list_item *sli)
{
    int rv;
    /* Client parameters describing new session */
    enum scp_session_type type;
    unsigned short width;
    unsigned short height;
    unsigned char bpp;
    const char *shell;
    const char *directory;
    const char *instance_name;
    const char *client_name;

    struct guid guid;
    const char *display;
    int x11_display = -1;
    struct session_item *s_item = NULL;
    int start_sesexec = (sli->sesexec_trans == NULL);
    int send_client_reply = 1;

    enum scp_screate_status status = E_SCP_SCREATE_OK;

    rv = scp_get_create_session_request(sli->client_trans,
                                        &type, &width, &height,
                                        &bpp, &shell, &directory,
                                        &instance_name, &client_name);

    if (rv == 0)
    {
        if (sli->login_state == E_SLI_LOGIN_NOT_LOGGED_IN)
        {
            status = E_SCP_SCREATE_NOT_LOGGED_IN;
        }
        else if (sli->create_session_in_progress)
        {
            status = E_SCP_SCREATE_IN_PROGRESS;
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "Received request from %s to create a session for user %s",
                sli->peername, sli->username);

            s_item = session_list_get_bydata(sli->uid, type, width, height,
                                             bpp, sli->start_ip_addr,
                                             instance_name, client_name);
            if (s_item != NULL)
            {
                // Found an existing session
                LOG(LOG_LEVEL_INFO,
                    "A suitable session on display %s is already active",
                    s_item->display);
                display = s_item->display;
                guid = s_item->guid;
            }
            // Need to create a new session
            else if (g_cfg->sess.max_sessions > 0 &&
                     session_list_get_count() >= g_cfg->sess.max_sessions)
            {
                LOG(LOG_LEVEL_ERROR,
                    "The maximum number of sessions has been reached");
                status = E_SCP_SCREATE_MAX_REACHED;
            }
            else if (SCP_SESSION_TYPE_IS_X11(type) &&
                     (x11_display = get_free_display()) < 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "No free X11 display can be found for a new session");
                status = E_SCP_SCREATE_NO_DISPLAY;
            }
            // Create a socket dir for this user
            else if (create_xrdp_socket_path(sli->uid) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't create a socket directory for UID %d",
                    (int)sli->uid);
                status = E_SCP_SCREATE_GENERAL_ERROR;
            }
            // Create a sesexec process if we don't have one (UDS login)
            else if (start_sesexec && sesexec_start(sli) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't start sesexec to manage session");
                status = E_SCP_SCREATE_GENERAL_ERROR;
            }
            else if (start_sesexec &&
                     eicp_send_uds_login_request(
                         sli->sesexec_trans, sli->client_trans->sck) != 0)
            {
                // Because we started sesexec late, we needed to log it in.
                // That hasn't gone too well.
                LOG(LOG_LEVEL_ERROR,
                    "Can't set UID for sesexec process");
                status = E_SCP_SCREATE_GENERAL_ERROR;

                // Looks like sesexec is broken...
                trans_delete(sli->sesexec_trans);
                sli->sesexec_trans = NULL;
            }
            else
            {
                // Pass the session create request to sesexec
                LOG(LOG_LEVEL_INFO,
                    "Passing session creation request to sesexec");
                int eicp_stat;
                eicp_stat = eicp_send_create_session_request(
                                sli->sesexec_trans,
                                x11_display,
                                type, width, height,
                                bpp, shell, directory,
                                instance_name, client_name);

                if (eicp_stat != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Can't ask sesexec to create a session");
                    status = E_SCP_SCREATE_GENERAL_ERROR;
                    // Looks like sesexec is broken...
                    trans_delete(sli->sesexec_trans);
                    sli->sesexec_trans = NULL;
                }
                else
                {
                    // We're not sending a reply yet
                    send_client_reply = 0;
                    sli->create_session_in_progress = 1;
                    sli->session_x11_display = x11_display; // Reserve display
                }
            }
        }

        if (send_client_reply)
        {
            if (status != E_SCP_SCREATE_OK)
            {
                display = "";
                guid_clear(&guid);
            }
            rv = scp_send_create_session_response(sli->client_trans,
                                                  status, display, &guid);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_connect_session_request(struct scp_list_item *sli)
{
    int rv;
    /* Client parameters describing new session */
    struct guid guid;
    const char *client_ip;
    const char *client_name;
    unsigned int flags;
    enum scp_sconnect_status status;

    rv = scp_get_connect_session_request(sli->client_trans, &guid,
                                         &client_ip, &client_name, &flags);

    if (rv == 0)
    {
        if (sli->login_state == E_SLI_LOGIN_NOT_LOGGED_IN)
        {
            status = E_SCP_SCONNECT_NOT_LOGGED_IN;
        }
        else
        {
            struct session_item *s_item = session_list_get_byguid(&guid);
            if (s_item == NULL)
            {
                LOG(LOG_LEVEL_ERROR,
                    "User %s tried to connect to non-existent session",
                    sli->username);
                status = E_SCP_SCONNECT_NO_SUCH_GUID;
            }
            else if (s_item->uid != sli->uid)
            {
                LOG(LOG_LEVEL_ERROR,
                    "User %s (UID %d) denied access to session for UID %d",
                    sli->username, sli->uid, s_item->uid);
                status = E_SCP_SCONNECT_NO_SUCH_GUID;
            }
            else
            {
                // Don't log the GUID
                LOG(LOG_LEVEL_INFO,
                    "Forwarding request from %s to connect to a session",
                    sli->username);
                // Pass the session create request to sesexec
                int ercp_stat;
                ercp_stat = ercp_send_connect_session_request(
                                s_item->sesexec_trans,
                                sli->client_trans->sck,
                                client_ip,
                                client_name,
                                flags);

                if (ercp_stat != 0)
                {
                    // The sesexec transport is broken. That's dealt with
                    // elsewhere.
                    LOG(LOG_LEVEL_ERROR,
                        "Can't ask sesexec to connect to a session");
                    status = E_SCP_SCONNECT_GENERAL_ERROR;
                }
                else
                {
                    status = E_SCP_SCONNECT_OK;
                }
            }
        }

        // Send anything other than a successful connection request
        // back to the client
        if (status != E_SCP_SCONNECT_OK)
        {
            rv = scp_send_connect_session_response(sli->client_trans,
                                                   status, -1, -1);
        }
    }

    // This call is always the last thing on the SCP connection.
    logout_scp_list_item(sli); // Remove any sesexec process used for auth
    sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;

    return rv;
}

/******************************************************************************/

static int
process_list_sessions_request(struct scp_list_item *sli)
{
    int rv = 0;

    struct scp_session_info *info = NULL;
    unsigned int cnt = 0;
    unsigned int i;

    if (sli->login_state == E_SLI_LOGIN_NOT_LOGGED_IN)
    {
        rv = scp_send_list_sessions_response(sli->client_trans,
                                             E_SCP_LS_NOT_LOGGED_IN,
                                             NULL);
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received request from %s to list sessions for user %s",
            sli->peername, sli->username);

        if (sli->is_admin)
        {
            info = session_list_get_byuid(NULL, &cnt, 0);
        }
        else
        {
            info = session_list_get_byuid(&sli->uid, &cnt, 0);
        }

        for (i = 0; rv == 0 && i < cnt; ++i)
        {
            rv = scp_send_list_sessions_response(sli->client_trans,
                                                 E_SCP_LS_SESSION_INFO,
                                                 &info[i]);
        }
        free_session_info_list(info, cnt);

        if (rv == 0)
        {
            rv = scp_send_list_sessions_response(sli->client_trans,
                                                 E_SCP_LS_END_OF_LIST,
                                                 NULL);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_create_sockdir_request(struct scp_list_item *sli)
{
    enum scp_create_sockdir_status status = E_SCP_CS_OTHER_ERROR;

    if (sli->login_state == E_SLI_LOGIN_NOT_LOGGED_IN)
    {
        status = E_SCP_CS_NOT_LOGGED_IN;
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received request from %s to create sockdir for user %s",
            sli->peername, sli->username);

        if (create_xrdp_socket_path(sli->uid) == 0)
        {
            status = E_SCP_CS_OK;
        }
    }

    return scp_send_create_sockdir_response(sli->client_trans, status);
}

/******************************************************************************/

static int
process_close_connection_request(struct scp_list_item *sli)
{
    int rv = 0;

    LOG(LOG_LEVEL_INFO, "Received request to close connection from %s",
        sli->peername);

    /* Make sure we're logged out */
    if (sli->login_state != E_SLI_LOGIN_NOT_LOGGED_IN)
    {
        logout_scp_list_item(sli);
    }

    /* Expecting no more client messages. Close the connection
     * after returning from this callback */
    sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
    return rv;
}

/******************************************************************************/
int
scp_process(struct scp_list_item *sli)
{
    enum scp_msg_code msgno;
    int rv = 0;

    switch ((msgno = scp_msg_in_get_msgno(sli->client_trans)))
    {
        case E_SCP_SET_PEERNAME_REQUEST:
            rv = process_set_peername_request(sli);
            break;

        case E_SCP_SYS_LOGIN_REQUEST:
            rv = process_sys_login_request(sli);
            break;

        case E_SCP_UDS_LOGIN_REQUEST:
            rv = process_uds_login_request(sli);
            break;

        case E_SCP_LOGOUT_REQUEST:
            rv = process_logout_request(sli);
            break;

        case E_SCP_CREATE_SESSION_REQUEST:
            rv = process_create_session_request(sli);
            break;

        case E_SCP_CONNECT_SESSION_REQUEST:
            rv = process_connect_session_request(sli);
            break;

        case E_SCP_LIST_SESSIONS_REQUEST:
            rv = process_list_sessions_request(sli);
            break;

        case E_SCP_CREATE_SOCKDIR_REQUEST:
            rv = process_create_sockdir_request(sli);
            break;

        case E_SCP_CLOSE_CONNECTION_REQUEST:
            rv = process_close_connection_request(sli);
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

