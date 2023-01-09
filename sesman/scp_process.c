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
#include "os_calls.h"
#include "session.h"
#include "sesman.h"
#include "string_calls.h"

/**************************************************************************//**
 * Logs an authentication failure message
 *
 * @param username Username
 * @param ip_addr IP address, if known
 *
 * The message is intended for use by fail2ban. Make changes with care.
 */
static void
log_authfail_message(const char *username, const char *ip_addr)
{
    if (ip_addr == NULL || ip_addr[0] == '\0')
    {
        ip_addr = "unknown";
    }
    LOG(LOG_LEVEL_INFO, "AUTHFAIL: user=%s ip=%s time=%d",
        username, ip_addr, g_time1());
}

/******************************************************************************/

/**
 * Mode parameter for authenticate_and_authorize_connection()
 */
enum login_mode
{
    AM_SYSTEM,
    AM_UDS
};

/**
 * Authenticate and authorize the connection
 *
 * @param sc  Connection to sesman
 * @param login_mode Describes the type of login in use
 * @param uid UID for user
 * @param username Name for user
 * @param password Password (AM_SYSTEM) or NULL.
 * @param ip_addr Remote IP address (AM_SYSTEM) or NULL.
 * @return Status for the operation
 *
 * @pre sc->auth_info, sc->username and sc->ip_addr must be NULL
 *
 * @post If E_SCP_LOGIN_OK is returned, sc->auth_info is non-NULL
 * @post If E_SCP_LOGIN_OK is returned, sc->username is non-NULL
 * @post If E_SCP_LOGIN_OK is returned, sc->ip_addr is non-NULL
 *
 */
static enum scp_login_status
authenticate_and_authorize_connection(struct sesman_con *sc,
                                      enum login_mode login_mode,
                                      int uid,
                                      const char *username,
                                      const char *password,
                                      const char *ip_addr)
{
    enum scp_login_status status = E_SCP_LOGIN_GENERAL_ERROR;
    struct auth_info *auth_info = NULL;

    /* Check preconditions */
    if (sc->auth_info != NULL || sc->username != NULL || sc->ip_addr != NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Internal error - connection already logged in");
    }
    else
    {
        switch (login_mode)
        {
            case AM_SYSTEM:
                auth_info = auth_userpass(username, password, ip_addr, &status);
                break;
            case AM_UDS:
                auth_info = auth_uds(username, &status);
                break;
            default:
                LOG(LOG_LEVEL_ERROR, "%s called with invalid mode %d",
                    __func__, (int)login_mode);
        }

        if (auth_info != NULL)
        {
            if (status != E_SCP_LOGIN_OK)
            {
                /* This shouldn't happen */
                LOG(LOG_LEVEL_ERROR,
                    "Unexpected status return %d from auth call",
                    (int)status);
            }
            else if (!access_login_allowed(username))
            {
                status = E_SCP_LOGIN_NOT_AUTHORIZED;
                LOG(LOG_LEVEL_INFO, "Username okay but group problem for "
                    "user: %s", username);
            }

            /* If all is well, put the auth_info in the sesman connection
             * for later use. If not, remove the auth_info */
            if (status == E_SCP_LOGIN_OK)
            {
                char *dup_username = g_strdup(username);
                char *dup_ip_addr =
                    (ip_addr == NULL) ? g_strdup("") : g_strdup(ip_addr);

                if (dup_username == NULL || dup_ip_addr == NULL)
                {
                    LOG(LOG_LEVEL_ERROR, "%s : Memory allocation failed",
                        __func__);
                    g_free(dup_username);
                    g_free(dup_ip_addr);
                    status = E_SCP_LOGIN_NO_MEMORY;
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "Access permitted for user: %s",
                        username);
                    sc->auth_info = auth_info;
                    sc->uid = uid;
                    sc->username = dup_username;
                    sc->ip_addr = dup_ip_addr;
                }
            }

            if (status != E_SCP_LOGIN_OK)
            {
                auth_end(auth_info);
            }
        }
    }

    return status;
}

/******************************************************************************/

static int
process_set_peername_request(struct sesman_con *sc)
{
    int rv;
    const char *peername;

    rv = scp_get_set_peername_request(sc->t, &peername);
    if (rv == 0)
    {
        if (sesman_set_connection_peername(sc, peername) != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "Failed to set connection peername from %s to %s",
                sc->peername, peername);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_sys_login_request(struct sesman_con *sc)
{
    int rv;
    const char *supplied_username;
    const char *password;
    const char *ip_addr;

    rv = scp_get_sys_login_request(sc->t, &supplied_username,
                                   &password, &ip_addr);
    if (rv == 0)
    {
        enum scp_login_status errorcode;
        int server_closed = 1;
        int uid;
        char *username = NULL;

        LOG(LOG_LEVEL_INFO,
            "Received system login request from %s for user: %s IP: %s",
            sc->peername, supplied_username, ip_addr);

        if (sc->auth_info != NULL)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                sc->username);
        }
        else if (g_getuser_info_by_name(supplied_username,
                                        &uid, NULL, NULL, NULL, NULL) != 0)
        {
            /* we can't get a UID for the user */
            errorcode = E_SCP_LOGIN_NOT_AUTHENTICATED;
            LOG(LOG_LEVEL_ERROR, "Can't get UID for user %s",
                supplied_username);
            log_authfail_message(username, ip_addr);
        }
        else if (g_getuser_info_by_uid(uid,
                                       &username, NULL, NULL, NULL, NULL) != 0)
        {
            errorcode = E_SCP_LOGIN_GENERAL_ERROR;
            LOG(LOG_LEVEL_ERROR, "Can't reverse lookup UID %d", uid);
        }
        else
        {
            if (g_strcmp(supplied_username, username) != 0)
            {
                /*
                 * If using a federated naming service (e.g. AD), the
                 * username supplied may not match that name mapped to by
                 * the UID. We will generate a warning in this instance so
                 * the user can see what is being used within sesman
                 */
                LOG(LOG_LEVEL_WARNING,
                    "Using username %s for the session (from UID %d)",
                    username, uid);
            }

            errorcode = authenticate_and_authorize_connection(
                            sc, AM_SYSTEM,
                            uid, username,
                            password, ip_addr);
            if (errorcode == E_SCP_LOGIN_OK)
            {
                server_closed = 0;
            }
            else if (errorcode == E_SCP_LOGIN_NOT_AUTHENTICATED)
            {
                log_authfail_message(username, ip_addr);
                if (sc->auth_retry_count > 0)
                {
                    /* Password problem? Invite the user to retry */
                    server_closed = 0;
                    --sc->auth_retry_count;
                }
            }

            g_free(username);
        }

        if (server_closed)
        {
            /* Expecting no more client messages. Close the connection
             * after returning from this callback */
            sc->close_requested = 1;
        }

        rv = scp_send_login_response(sc->t, errorcode, server_closed);
    }

    return rv;
}

/******************************************************************************/

static int
process_uds_login_request(struct sesman_con *sc)
{
    enum scp_login_status errorcode;
    int rv;
    int uid;
    int pid;
    char *username = NULL;
    int server_closed = 1;

    rv = g_sck_get_peer_cred(sc->t->sck, &pid, &uid, NULL);
    if (rv != 0)
    {
        LOG(LOG_LEVEL_INFO,
            "Unable to get peer credentials for socket %d",
            (int)sc->t->sck);
        errorcode = E_SCP_LOGIN_GENERAL_ERROR;
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received UDS login request from %s for UID: %d from PID: %d",
            sc->peername, uid, pid);

        if (sc->auth_info != NULL)
        {
            errorcode = E_SCP_LOGIN_ALREADY_LOGGED_IN;
            LOG(LOG_LEVEL_ERROR, "Connection is already logged in for %s",
                sc->username);
        }
        else if (g_getuser_info_by_uid(uid, &username,
                                       NULL, NULL, NULL, NULL) != 0)
        {
            errorcode = E_SCP_LOGIN_GENERAL_ERROR;
            LOG(LOG_LEVEL_ERROR, "Can't reverse lookup UID %d", uid);
        }
        else
        {
            errorcode = authenticate_and_authorize_connection(
                            sc, AM_UDS,
                            uid, username,
                            NULL, NULL);
            g_free(username);

            if (errorcode == E_SCP_LOGIN_OK)
            {
                server_closed = 0;
            }
        }
    }

    if (server_closed)
    {
        /* Close the connection after returning from this callback */
        sc->close_requested = 1;
    }

    return scp_send_login_response(sc->t, errorcode, server_closed);
}

/******************************************************************************/

static int
process_logout_request(struct sesman_con *sc)
{
    if (sc->auth_info != NULL)
    {
        LOG(LOG_LEVEL_INFO, "Logging out %s from sesman", sc->username);
        auth_end(sc->auth_info);
        sc->auth_info = NULL;
        sc->uid = -1;
        g_free(sc->username);
        sc->username = NULL;
        g_free(sc->ip_addr);
        sc->ip_addr = NULL;
    }

    return 0;
}

/******************************************************************************/

static int
process_create_session_request(struct sesman_con *sc)
{
    int rv;
    struct session_parameters sp;
    struct guid guid;
    int display = 0;
    enum scp_screate_status status = E_SCP_SCREATE_OK;

    guid_clear(&guid);

    rv = scp_get_create_session_request(sc->t,
                                        &sp.type, &sp.width, &sp.height,
                                        &sp.bpp, &sp.shell, &sp.directory);

    if (rv == 0)
    {
        if (sc->auth_info == NULL)
        {
            status = E_SCP_SCREATE_NOT_LOGGED_IN;
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "Received request from %s to create a session for user %s",
                sc->peername, sc->username);

            // Copy over the items we got from the login request,
            // but which we need to describe the session
            sp.uid = sc->uid;
            sp.ip_addr = sc->ip_addr; //  Guaranteed to be non-NULL

            struct session_item *s_item = session_get_bydata(&sp);
            if (s_item != 0)
            {
                // Found an existing session
                display = s_item->display;
                guid = s_item->guid;
                if (sp.ip_addr[0] != '\0')
                {
                    LOG( LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                         "display :%d.0, session_pid %d, ip %s",
                         sc->username, display, s_item->pid, sp.ip_addr);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "++ reconnected session: username %s, "
                        "display :%d.0, session_pid %d",
                        sc->username, display, s_item->pid);
                }

                session_reconnect(display, sc->uid, sc->auth_info);
            }
            else
            {
                // Need to create a new session
                if (sp.ip_addr[0] != '\0')
                {
                    LOG(LOG_LEVEL_INFO,
                        "++ created session: username %s, ip %s",
                        sc->username, sp.ip_addr);
                }
                else
                {
                    LOG(LOG_LEVEL_INFO,
                        "++ created session: username %s", sc->username);
                }

                // The new session will have a lifetime longer than
                // the sesman connection, and so needs to own
                // the auth_info struct.
                //
                // Copy the auth_info struct out of the connection and pass
                // it to the session
                struct auth_info *auth_info = sc->auth_info;
                sc->auth_info = NULL;
                status = session_start(auth_info, &sp, &display, &guid);
                if (status != E_SCP_SCREATE_OK)
                {
                    // Close the auth session down as it can't be re-used.
                    auth_end(auth_info);
                }
            }
        }

        /* Currently a create session request is the last thing on a
         * connection, and results in automatic closure */
        sc->close_requested = 1;

        rv = scp_send_create_session_response(sc->t, status, display, &guid);
    }

    return rv;
}

/******************************************************************************/

static int
process_list_sessions_request(struct sesman_con *sc)
{
    int rv = 0;

    struct scp_session_info *info = NULL;
    unsigned int cnt = 0;
    unsigned int i;

    if (sc->auth_info == NULL)
    {
        rv = scp_send_list_sessions_response(sc->t,
                                             E_SCP_LS_NOT_LOGGED_IN,
                                             NULL);
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "Received request from %s to list sessions for user %s",
            sc->peername, sc->username);

        info = session_get_byuid(sc->uid, &cnt,
                                 SESMAN_SESSION_STATUS_ALL);

        for (i = 0; rv == 0 && i < cnt; ++i)
        {
            rv = scp_send_list_sessions_response(sc->t,
                                                 E_SCP_LS_SESSION_INFO,
                                                 &info[i]);
        }
        free_session_info_list(info, cnt);

        if (rv == 0)
        {
            rv = scp_send_list_sessions_response(sc->t,
                                                 E_SCP_LS_END_OF_LIST,
                                                 NULL);
        }
    }

    return rv;
}

/******************************************************************************/

static int
process_close_connection_request(struct sesman_con *sc)
{
    int rv = 0;

    LOG(LOG_LEVEL_INFO, "Received request to close connection from %s",
        sc->peername);

    /* Expecting no more client messages. Close the connection
     * after returning from this callback */
    sc->close_requested = 1;
    return rv;
}

/******************************************************************************/
int
scp_process(struct sesman_con *sc)
{
    enum scp_msg_code msgno;
    int rv = 0;

    switch ((msgno = scp_msg_in_get_msgno(sc->t)))
    {
        case E_SCP_SET_PEERNAME_REQUEST:
            rv = process_set_peername_request(sc);
            break;

        case E_SCP_SYS_LOGIN_REQUEST:
            rv = process_sys_login_request(sc);
            break;

        case E_SCP_UDS_LOGIN_REQUEST:
            rv = process_uds_login_request(sc);
            break;

        case E_SCP_LOGOUT_REQUEST:
            rv = process_logout_request(sc);
            break;

        case E_SCP_CREATE_SESSION_REQUEST:
            rv = process_create_session_request(sc);
            break;

        case E_SCP_LIST_SESSIONS_REQUEST:
            rv = process_list_sessions_request(sc);
            break;

        case E_SCP_CLOSE_CONNECTION_REQUEST:
            rv = process_close_connection_request(sc);
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

