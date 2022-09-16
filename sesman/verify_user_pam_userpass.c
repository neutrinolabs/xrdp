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
 * @file verify_user_pam_userpass.c
 * @brief Authenticate user using pam_userpass module
 * @author Jay Sorg
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "auth.h"
#include "os_calls.h"
#include "string_calls.h"

#include <security/pam_userpass.h>

#define SERVICE "xrdp"

/*
 * Need a complete type for struct auth_info, even though we're
 * not really using it if this module (PAM userpass) is selected */
struct auth_info
{
    char dummy;
};

/******************************************************************************/
/* returns non-NULL for success */
struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, int *errorcode)
{
    pam_handle_t *pamh;
    pam_userpass_t userpass;
    struct pam_conv conv = {pam_userpass_conv, &userpass};
    const void *template1;
    int status;
    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};

    userpass.user = user;
    userpass.pass = pass;

    if (pam_start(SERVICE, user, &conv, &pamh) != PAM_SUCCESS)
    {
        return NULL;
    }

    status = pam_authenticate(pamh, 0);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return NULL;
    }

    status = pam_acct_mgmt(pamh, 0);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return NULL;
    }

    status = pam_get_item(pamh, PAM_USER, &template1);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return NULL;
    }

    if (pam_end(pamh, PAM_SUCCESS) != PAM_SUCCESS)
    {
        return NULL;
    }

    return &success;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(struct auth_info *auth_info, int display_num)
{
    return 0;
}

/******************************************************************************/
/* returns error */
int
auth_stop_session(struct auth_info *auth_info)
{
    return 0;
}

/******************************************************************************/
int
auth_end(struct auth_info *auth_info)
{
    return 0;
}

/******************************************************************************/
int
auth_set_env(struct auth_info *auth_info)
{
    return 0;
}
