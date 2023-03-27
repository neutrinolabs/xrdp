/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022, all xrdp contributors
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
 * @file libipm/scp_application_types.c
 * @brief Support routines for types in scp_application_types.h
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "scp_application_types.h"
#include "os_calls.h"

/*****************************************************************************/
const char *
scp_login_status_to_str(enum scp_login_status n,
                        char *buff, unsigned int buff_size)
{
    const char *str =
        (n == E_SCP_LOGIN_OK) ?  "OK" :
        (n == E_SCP_LOGIN_ALREADY_LOGGED_IN) ? "A user is already logged in" :
        (n == E_SCP_LOGIN_NO_MEMORY) ? "No memory for login" :
        (n == E_SCP_LOGIN_NOT_AUTHENTICATED) ? "User does not exist, or could not be authenticated" :
        (n == E_SCP_LOGIN_NOT_AUTHORIZED) ? "User is not authorized" :
        (n == E_SCP_LOGIN_GENERAL_ERROR) ? "General login error" :
        /* Default */ NULL;

    if (str == NULL)
    {
        g_snprintf(buff, buff_size, "[login error code #%d]", (int)n);
    }
    else
    {
        g_snprintf(buff, buff_size, "%s", str);
    }

    return buff;
}

/*****************************************************************************/
const char *
scp_screate_status_to_str(enum scp_screate_status n,
                          char *buff, unsigned int buff_size)
{
    const char *str =
        (n == E_SCP_SCREATE_OK) ? "OK" :
        (n == E_SCP_SCREATE_NO_MEMORY) ? "No memory for session" :
        (n == E_SCP_SCREATE_NOT_LOGGED_IN) ? "Connection is not logged in" :
        (n == E_SCP_SCREATE_MAX_REACHED) ? "Max session limit reached" :
        (n == E_SCP_SCREATE_NO_DISPLAY) ? "No X displays are available" :
        (n == E_SCP_SCREATE_X_SERVER_FAIL) ? "X server could not be started" :
        (n == E_SCP_SCREATE_GENERAL_ERROR) ? "General session creation error" :
        /* Default */ NULL;

    if (str == NULL)
    {
        g_snprintf(buff, buff_size, "[session creation error code #%d]",
                   (int)n);
    }
    else
    {
        g_snprintf(buff, buff_size, "%s", str);
    }

    return buff;
}
