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

#include "sesman.h"

#define _XOPEN_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <login_cap.h>
#include <bsd_auth.h>

#ifndef SECS_PER_DAY
#define SECS_PER_DAY (24L*3600L)
#endif

extern struct config_sesman* g_cfg; /* in sesman.c */

/******************************************************************************/
/* returns boolean */
long
auth_userpass(const char *user, const char *pass, int *errorcode)
{
    int ret = auth_userokay(user, NULL, "auth-xrdp", pass);
    return ret;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(long in_val, int in_display)
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
auth_stop_session(long in_val)
{
    return 0;
}

/**
 *
 * @brief Password encryption
 * @param pwd Old password
 * @param pln Plaintext new password
 * @param crp Crypted new password
 *
 */

static int
auth_crypt_pwd(const char *pwd, const char *pln, char *crp)
{
    return 0;
}

/**
 *
 * @return 1 if the account is disabled, 0 otherwise
 *
 */
static int
auth_account_disabled(struct spwd* stp)
{
    return 0;
}
