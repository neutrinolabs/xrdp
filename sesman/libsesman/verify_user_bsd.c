/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2005-2014
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
 * @file verify_user_bsd.c
 * @brief Authenticate user using BSD password system
 * @author Renaud Allard
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "sesman_auth.h"

#define _XOPEN_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/param.h>
#if defined(OpenBSD)
#include <login_cap.h>
#include <bsd_auth.h>
#else
/*
 * If OpenBSD isn't defined, add static definitions of OpenBSD-specific
 * functions. This won't work, but will let the compiler on other
 * systems check that all is correctly defined */
static int
auth_userokay(char *name, char *style, char *type, char *password)
{
    fprintf(stderr, "auth_userokay() not implmented on this platform!\n");
    abort();
    return 0;
}
#endif


/*
 * Need a complete type for struct auth_info, even though we're
 * not really using it if this module (BSD authentication) is selected */
struct auth_info
{
    char dummy;
};

/******************************************************************************/
/* returns non-NULL for success */
struct auth_info *
auth_userpass(const char *const_user, const char *const_pass,
              const char *client_ip, enum scp_login_status *errorcode)
{
    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};
    enum scp_login_status status;

    // auth_userokay is not const-correct. See usr.sbin/smtpd/smtpd.c in
    // the OpenBSD source tree for this workaround
    char user[LOGIN_NAME_MAX];
    char pass[LINE_MAX];
    char type[] = "auth-xrdp";

    snprintf(user, sizeof(user), "%s", const_user);
    snprintf(pass, sizeof(pass), "%s", const_pass);

    if (auth_userokay(user, NULL, type, pass))
    {
        status = E_SCP_LOGIN_OK;
    }
    else
    {
        status = E_SCP_LOGIN_NOT_AUTHENTICATED;
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return (status == E_SCP_LOGIN_OK) ? &success : NULL;
}

/******************************************************************************/
/* returns non-NULL for success */
struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode)
{
    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};

    if (errorcode != NULL)
    {
        *errorcode = E_SCP_LOGIN_OK;
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

/******************************************************************************/
int
auth_check_pwd_chg(const char *user)
{
    return 0;
}

int
auth_change_pwd(const char *user, const char *newpwd)
{
    return 0;
}

int
auth_stop_session(struct auth_info *auth_info)
{
    return 0;
}
