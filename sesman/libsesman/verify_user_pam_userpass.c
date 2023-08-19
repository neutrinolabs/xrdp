/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file verify_user_pam.c
 * @brief Authenticate user using pam
 * @author Jay Sorg
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "log.h"
#include "string_calls.h"
#include "sesman_auth.h"

#include <security/pam_userpass.h>

#include <stdio.h>
#include <security/pam_appl.h>

#define SERVICE "xrdp"

struct auth_info
{
    pam_userpass_t userpass;
    int session_opened;
    int did_setcred;
    struct pam_conv pamc;
    pam_handle_t *ph;
};

/******************************************************************************/

/** Performs PAM operations common to login methods
 *
 * @param auth_info Module auth_info structure
 * @param client_ip Client IP if known, or NULL
 * @param need_pam_authenticate True if user must be authenticated as
 *                              well as authorized
 * @return Code describing the success of the operation
 *
 * The username is assumed to be supplied by the caller in
 * auth_info->userpass.user
 */
static enum scp_login_status
common_pam_login(struct auth_info *auth_info,
                 const char *client_ip,
                 int need_pam_authenticate)
{
    int perror;
    char service_name[256];

    perror = pam_start(SERVICE, auth_info->userpass.user,
                       &(auth_info->pamc), &(auth_info->ph));

    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_start failed: %s",
            pam_strerror(auth_info->ph, perror));
        pam_end(auth_info->ph, perror);
        return E_SCP_LOGIN_GENERAL_ERROR;
    }

    if (client_ip != NULL && client_ip[0] != '\0')
    {
        perror = pam_set_item(auth_info->ph, PAM_RHOST, client_ip);
        if (perror != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_set_item(PAM_RHOST) failed: %s",
                pam_strerror(auth_info->ph, perror));
        }
    }

    perror = pam_set_item(auth_info->ph, PAM_TTY, service_name);
    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item(PAM_TTY) failed: %s",
            pam_strerror(auth_info->ph, perror));
    }

    if (need_pam_authenticate)
    {
        perror = pam_authenticate(auth_info->ph, 0);

        if (perror != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_authenticate failed: %s",
                pam_strerror(auth_info->ph, perror));
            pam_end(auth_info->ph, perror);
            return E_SCP_LOGIN_NOT_AUTHENTICATED;
        }
    }
    /* From man page:
       The pam_acct_mgmt function is used to determine if the users account is
       valid. It checks for authentication token and account expiration and
       verifies access restrictions. It is typically called after the user has
       been authenticated.
     */
    perror = pam_acct_mgmt(auth_info->ph, 0);

    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_acct_mgmt failed: %s",
            pam_strerror(auth_info->ph, perror));
        pam_end(auth_info->ph, perror);
        return E_SCP_LOGIN_NOT_AUTHORIZED;
    }

    return E_SCP_LOGIN_OK;
}


/******************************************************************************/
/* returns non-NULL for success
 * Detailed error code is in the errorcode variable */

struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, enum scp_login_status *errorcode)
{
    struct auth_info *auth_info;
    enum scp_login_status status;

    auth_info = g_new0(struct auth_info, 1);
    if (auth_info == NULL)
    {
        status = E_SCP_LOGIN_NO_MEMORY;
    }
    else
    {
        auth_info->userpass.user = user;
        auth_info->userpass.pass = pass;

        auth_info->pamc.conv = &pam_userpass_conv;
        auth_info->pamc.appdata_ptr = &(auth_info->userpass);
        status = common_pam_login(auth_info, client_ip, 1);

        if (status != E_SCP_LOGIN_OK)
        {
            g_free(auth_info);
            auth_info = NULL;
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return auth_info;
}

/******************************************************************************/

struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode)
{
    struct auth_info *auth_info;
    enum scp_login_status status;

    auth_info = g_new0(struct auth_info, 1);
    if (auth_info == NULL)
    {
        status = E_SCP_LOGIN_NO_MEMORY;
    }
    else
    {
        auth_info->userpass.user = user;
        status = common_pam_login(auth_info, NULL, 0);

        if (status != E_SCP_LOGIN_OK)
        {
            g_free(auth_info);
            auth_info = NULL;
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return auth_info;
}

/******************************************************************************/

/* returns error */
static int
auth_start_session_private(struct auth_info *auth_info, int display_num)
{
    int error;
    char display[256];

    g_sprintf(display, ":%d", display_num);
    error = pam_set_item(auth_info->ph, PAM_TTY, display);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    error = pam_setcred(auth_info->ph, PAM_ESTABLISH_CRED);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_setcred failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    auth_info->did_setcred = 1;
    error = pam_open_session(auth_info->ph, 0);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_open_session failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    auth_info->session_opened = 1;
    return 0;
}

/******************************************************************************/
/**
 * Main routine to start a session
 *
 * Calls the private routine and logs an additional error if the private
 * routine fails
 */
int
auth_start_session(struct auth_info *auth_info, int display_num)
{
    int result = auth_start_session_private(auth_info, display_num);
    if (result != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't start PAM session. See PAM logging for more info");
    }

    return result;
}

/******************************************************************************/
/* returns error */
static int
auth_stop_session(struct auth_info *auth_info)
{
    int rv = 0;
    int error;

    if (auth_info->session_opened)
    {
        error = pam_close_session(auth_info->ph, 0);
        if (error != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_close_session failed: %s",
                pam_strerror(auth_info->ph, error));
            rv = 1;
        }
        else
        {
            auth_info->session_opened = 0;
        }
    }

    if (auth_info->did_setcred)
    {
        pam_setcred(auth_info->ph, PAM_DELETE_CRED);
        auth_info->did_setcred = 0;
    }

    return rv;
}

/******************************************************************************/
/* returns error */
/* cleanup */
int
auth_end(struct auth_info *auth_info)
{
    if (auth_info != NULL)
    {
        if (auth_info->ph != 0)
        {
            auth_stop_session(auth_info);

            pam_end(auth_info->ph, PAM_SUCCESS);
            auth_info->ph = 0;
        }
    }

    g_free(auth_info);
    return 0;
}

/******************************************************************************/
/* returns error */
/* set any pam env vars */
int
auth_set_env(struct auth_info *auth_info)
{
    char **pam_envlist;
    char **pam_env;

    if (auth_info != NULL)
    {
        /* export PAM environment */
        pam_envlist = pam_getenvlist(auth_info->ph);

        if (pam_envlist != NULL)
        {
            for (pam_env = pam_envlist; *pam_env != NULL; ++pam_env)
            {
                char *str = *pam_env;
                int eq_pos = g_pos(str, "=");

                if (eq_pos > 0)
                {
                    str[eq_pos] = '\0';
                    g_setenv(str, str + eq_pos + 1, 1);
                }

                g_free(str);
            }

            g_free(pam_envlist);
        }
    }

    return 0;
}
