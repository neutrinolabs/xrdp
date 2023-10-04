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
 * @file sesman_access.c
 * @brief User access control code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>

#include "arch.h"

#include "sesman_access.h"
#include "sesman_config.h"
#include "log.h"
#include "os_calls.h"

/******************************************************************************/
/**
 * Root user login check
 *
 * @param cfg_sec Sesman security config
 * @param user user name
 * @return 0 if user is root and root accesses are not allowed
 */
static int
root_login_check(const struct config_security *cfg_sec,
                 const char *user)
{
    int rv = 0;
    if (cfg_sec->allow_root)
    {
        rv = 1;
    }
    else
    {
        // Check the UID of the user isn't 0
        int uid;
        if (g_getuser_info_by_name(user, &uid, NULL, NULL, NULL, NULL) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't get UID for user %s", user);
        }
        else if (0 == uid)
        {
            LOG(LOG_LEVEL_ERROR,
                "ROOT login attempted, but root login is disabled");
        }
        else
        {
            rv = 1;
        }
    }
    return rv;
}

/******************************************************************************/
/**
 * Common access control for group checks
 * @param group Group name
 * @param param Where group comes from (e.g. "TerminalServerUsers")
 * @param always_check_group 0 if a missing group allows a login
 * @param user Username
 * @return != 0 if the access is allowed */

static int
access_login_allowed_common(const char *group, const char *param,
                            int always_check_group,
                            const char *user)
{
    int rv = 0;
    int gid;
    int ok;

    if (group == NULL || group[0] == '\0')
    {
        /* Group is not defined. Default access depends on whether
         * we must have the group or not */
        if (always_check_group)
        {
            LOG(LOG_LEVEL_ERROR,
                "%s group is not defined. Access denied for %s",
                param, user);
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "%s group is not defined. Access granted for %s",
                param, user);
            rv = 1;
        }
    }
    else if (g_getgroup_info(group, &gid) != 0)
    {
        /* Group is defined but doesn't exist. Default access depends
         * on whether we must have the group or not */
        if (always_check_group)
        {
            LOG(LOG_LEVEL_ERROR,
                "%s group %s doesn't exist. Access denied for %s",
                param, group, user);
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "%s group %s doesn't exist. Access granted for %s",
                param, group, user);
            rv = 1;
        }
    }
    else if (0 != g_check_user_in_group(user, gid, &ok))
    {
        LOG(LOG_LEVEL_ERROR, "Error checking %s group %s. "
            "Access denied for %s", param, group, user);
    }
    else if (!ok)
    {
        LOG(LOG_LEVEL_ERROR, "User %s is not in %s group %s. Access denied",
            user, param, group);
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "User %s is in %s group %s. Access granted",
            user, param, group);
        rv = 1;
    }

    return rv;
}

/******************************************************************************/
int
access_login_allowed(const struct config_security *cfg_sec, const char *user)
{
    int rv = 0;

    if (root_login_check(cfg_sec, user))
    {
        rv = access_login_allowed_common(cfg_sec->ts_users,
                                         "TerminalServerUsers",
                                         cfg_sec->ts_always_group_check,
                                         user);
    }

    return rv;
}

/******************************************************************************/
int
access_login_mng_allowed(const struct config_security *cfg_sec,
                         const char *user)
{
    int rv = 0;

    if (root_login_check(cfg_sec, user))
    {
        rv = access_login_allowed_common(cfg_sec->ts_admins,
                                         "TerminalServerAdmins",
                                         1,
                                         user);
    }

    return rv;
}
