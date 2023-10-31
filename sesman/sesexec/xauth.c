/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Emmanuel Blindauer 2016
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
 * @file xauth.c
 * @brief XAUTHORITY handling code
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include "xauth.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"


/******************************************************************************/
int
add_xauth_cookie(int display, const char *file)
{
    FILE *dp;
    char cookie_str[33];
    char cookie_bin[16];
    char xauth_str[256];
    int ret;

    g_random(cookie_bin, 16);
    g_bytes_to_hexstr(cookie_bin, 16, cookie_str, 33);

    g_sprintf(xauth_str, "xauth -q -f %s add :%d . %s",
              file, display, cookie_str);

    dp = popen(xauth_str, "r");
    if (dp == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Unable to launch xauth");
        return 1;
    }

    ret = pclose(dp);
    if (ret < 0)
    {
        LOG(LOG_LEVEL_ERROR, "An error occurred while running xauth");
        return 1;
    }

    return 0;
}
