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
 * @file login_info.c
 * @brief Define functionality associated with user logins for sesexec
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "login_info.h"

#include "trans.h"

#include "sesman_auth.h"
#include "sesman_access.h"
#include "sesman_config.h"
#include "login_info.h"
#include "os_calls.h"
#include "scp.h"
#include "sesexec.h"
#include "string_calls.h"

/******************************************************************************/
/**
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
 * Authenticate and authorize the connection
 *
 * @param username Name for user
 * @param password Password
 * @param ip_addr Remote IP address
 * @param login_info Structure to fill in for a successful login
 * @return Status for the operation
 *
 * @post If E_SCP_LOGIN_OK is returned, g_login_info is filled in
 *
 */
static enum scp_login_status
authenticate_and_authorize_connection(const char *supplied_username,
                                      const char *password,
                                      const char *ip_addr,
                                      struct login_info *login_info)
{
    int uid;
    char *username; // From reverse-looking up the UID
    enum scp_login_status status;
    struct auth_info *auth_info;

    if (g_getuser_info_by_name(supplied_username,
                               &uid, NULL, NULL, NULL, NULL) != 0)
    {
        /* we can't get a UID for the user */
        LOG(LOG_LEVEL_ERROR, "Can't get UID for user %s",
            supplied_username);
        log_authfail_message(supplied_username, ip_addr);
        status = E_SCP_LOGIN_NOT_AUTHENTICATED;
    }
    else if (g_getuser_info_by_uid(uid,
                                   &username,
                                   NULL, NULL, NULL, NULL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't reverse lookup UID %d", uid);
        status = E_SCP_LOGIN_NOT_AUTHENTICATED;
    }
    else
    {
        if (g_strcmp(username, supplied_username) != 0)
        {
            /*
             * If using a federated naming service (e.g. AD), the username
             * supplied may not match that name mapped to by the UID. We
             * will generate a warning in this instance so the user can see
             * what is being used
             */
            LOG(LOG_LEVEL_WARNING,
                "Using username %s for the session (from UID %d)",
                username, uid);
        }

        auth_info = auth_userpass(username, password, ip_addr, &status);

        /* Sanity check on result of call */
        if ((auth_info != NULL && status != E_SCP_LOGIN_OK) ||
                (auth_info == NULL && status == E_SCP_LOGIN_OK))
        {
            LOG(LOG_LEVEL_ERROR, "Bugcheck; inconsistent auth result. "
                "info = %p, status = %d", (void *)auth_info, (int)status);
            status = E_SCP_LOGIN_GENERAL_ERROR;
            auth_end(auth_info);
            auth_info = NULL;
        }

        /* Group access allowed? */
        if (status == E_SCP_LOGIN_OK &&
                !access_login_allowed(&g_cfg->sec, username))
        {
            LOG(LOG_LEVEL_INFO, "Username okay but group problem for "
                "user: %s", username);
            status = E_SCP_LOGIN_NOT_AUTHORIZED;
            auth_end(auth_info);
            auth_info = NULL;
        }

        switch (status)
        {
            case E_SCP_LOGIN_OK:
            {
                char *dup_username = g_strdup(username);
                char *dup_ip_addr = g_strdup(ip_addr);

                if (dup_username == NULL || dup_ip_addr == NULL)
                {
                    LOG(LOG_LEVEL_ERROR, "%s : Memory allocation failed",
                        __func__);
                    g_free(dup_username);
                    g_free(dup_ip_addr);
                    status = E_SCP_LOGIN_NO_MEMORY;
                    auth_end(auth_info);
                    auth_info = NULL;
                }
                else
                {
                    LOG(LOG_LEVEL_INFO, "Access permitted for user: %s",
                        username);
                    login_info->uid = uid;
                    login_info->username = dup_username;
                    login_info->ip_addr = dup_ip_addr;
                    login_info->auth_info = auth_info;
                }
            }
            break;

            case E_SCP_LOGIN_NOT_AUTHENTICATED:
                log_authfail_message(username, ip_addr);
                break;

            default:
                break;
        }

        g_free(username);
    }
    return status;
}

/******************************************************************************/
static int
get_scp_client_retry(struct trans *scp_trans,
                     const char **username, const char **password,
                     const char **ip_addr)
{
    int got_message = 0;

    // Wait for an SCP message
    enum scp_msg_code msgno;

    scp_msg_in_reset(scp_trans);

    if (scp_msg_in_wait_available(scp_trans) == 0)
    {
        msgno = scp_msg_in_get_msgno(scp_trans);
        switch (msgno)
        {
            case E_SCP_SYS_LOGIN_REQUEST:
                if (scp_get_sys_login_request(scp_trans, username,
                                              password, ip_addr) == 0)
                {
                    got_message = 1;
                }
                break;

            case E_SCP_CLOSE_CONNECTION_REQUEST:
                break;

            default:
            {
                char buff[64];
                scp_msgno_to_str(msgno, buff, sizeof(buff));
                LOG(LOG_LEVEL_ERROR, "unexpected message %s from SCP client",
                    buff);
            }
            break;
        }
    }

    return got_message;
}

/******************************************************************************/
struct login_info *
login_info_sys_login_user(struct trans *scp_trans,
                          const char *username,
                          const char *password,
                          const char *ip_addr)
{
    struct login_info *result;
    enum scp_login_status status = E_SCP_LOGIN_GENERAL_ERROR;
    int server_closed = 0;

    if ((result = g_new0(struct login_info, 1)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Allocation failure logging in user");
    }
    else
    {
        int first_time = 1;
        unsigned int retry_count = g_cfg->sec.login_retry;

        result->uid = (uid_t) -1;

        while (status != E_SCP_LOGIN_OK && !server_closed)
        {
            // First time round, we have credentials supplied by the
            // caller. On subsequent trips, we have to wait for the
            // SCP client to send us more.
            if (first_time)
            {
                first_time = 0;
            }
            else if (!get_scp_client_retry(scp_trans, &username,
                                           &password, &ip_addr))
            {
                status = E_SCP_LOGIN_GENERAL_ERROR;
                break;
            }

            status = authenticate_and_authorize_connection(username,
                     password,
                     ip_addr,
                     result);

            if (status != E_SCP_LOGIN_OK)
            {
                if (retry_count > 0)
                {
                    --retry_count;
                }
                else
                {
                    server_closed = 1;
                }
            }

            if (scp_send_login_response(scp_trans, status,
                                        server_closed, result->uid) != 0)
            {
                status = E_SCP_LOGIN_GENERAL_ERROR;
                break;
            }
        }
    }

    if (status != E_SCP_LOGIN_OK)
    {
        login_info_free(result);
        result = NULL;
    }

    return result;
}

/******************************************************************************/
struct login_info *
login_info_uds_login_user(struct trans *scp_trans)
{
    struct login_info *result;
    int uid; // Needed as g_sck_get_peer_cred() doesn't use uid_t

    // Allocate a struct for the result, with the IP address set to ""
    if ((result = g_new0(struct login_info, 1)) == NULL ||
            (result->ip_addr = g_new0(char, 1)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Allocation failure logging in user");
    }
    else if (g_sck_get_peer_cred(scp_trans->sck, NULL, &uid, NULL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Unable to get peer credentials for SCP socket");
    }
    else if (g_getuser_info_by_uid(uid, &result->username,
                                   NULL, NULL, NULL, NULL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't reverse lookup UID %d", result->uid);
    }
    else if ((result->auth_info = auth_uds(result->username, NULL)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't authorize user %s over UDS",
            result->username);
    }
    else if (!access_login_allowed(&g_cfg->sec, result->username))
    {
        LOG(LOG_LEVEL_ERROR, "Access denied for user %s by your system admin",
            result->username);
    }
    else
    {
        result->uid = (uid_t)uid;
        return result;
    }

    login_info_free(result);
    return NULL;
}


/******************************************************************************/
void
login_info_free(struct login_info *self)
{
    if (self != NULL)
    {
        g_free(self->username);
        g_free(self->ip_addr);
        if (self->auth_info != NULL)
        {
            auth_end(self->auth_info);
        }
        g_free(self);
    }
}
