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
#include "auth.h"

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
auth_userpass(const char *user, const char *pass,
              const char *client_ip, int *errorcode)
{
    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};
    struct auth_info *ret = NULL;

    if (auth_userokay(user, NULL, "auth-xrdp", pass))
    {
        ret = &success;
    }
    return ret;
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
