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
#include "os_calls.h"
#include "string_calls.h"

#include <security/pam_userpass.h>

#define SERVICE "xrdp"

/******************************************************************************/
/* returns boolean */
int
auth_userpass(const char *user, const char *pass, int *errorcode)
{
    pam_handle_t *pamh;
    pam_userpass_t userpass;
    struct pam_conv conv = {pam_userpass_conv, &userpass};
    const void *template1;
    int status;

    userpass.user = user;
    userpass.pass = pass;

    if (pam_start(SERVICE, user, &conv, &pamh) != PAM_SUCCESS)
    {
        return 0;
    }

    status = pam_authenticate(pamh, 0);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return 0;
    }

    status = pam_acct_mgmt(pamh, 0);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return 0;
    }

    status = pam_get_item(pamh, PAM_USER, &template1);

    if (status != PAM_SUCCESS)
    {
        pam_end(pamh, status);
        return 0;
    }

    if (pam_end(pamh, PAM_SUCCESS) != PAM_SUCCESS)
    {
        return 0;
    }

    return 1;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(long in_val, int in_display)
{
    return 0;
}

/******************************************************************************/
/* returns error */
int
auth_stop_session(long in_val)
{
    return 0;
}

/******************************************************************************/
int
auth_end(long in_val)
{
    return 0;
}

/******************************************************************************/
int
auth_set_env(long in_val)
{
    return 0;
}
