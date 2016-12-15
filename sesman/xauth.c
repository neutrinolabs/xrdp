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
 * @file xauth.c
 * @brief XAUTHORITY handling code
 * @author Emmaunel Blindauer
 *
 */

#include <stdio.h>
#include "sesman.h"
// #include "grp.h"
// #include "ssl_calls.h"
#include "os_calls.h"


/******************************************************************************/
int DEFAULT_CC
add_xauth_cookie(int display, const char *file)
{
    FILE *dp;
    char cookie[33];
    char char_cookie[16];
    char xauth_str[256];
    int ret;

    g_random(char_cookie, 16);
    g_bytes_to_hexstr(char_cookie, 16, cookie, 33);
    cookie[32] = '\0';

    if (file == NULL)
    {
        g_sprintf(xauth_str, "xauth -q add :%d . %s", display, cookie);
    }
    else
    {
        g_sprintf(xauth_str, "xauth -q -f %s add :%d . %s",
                file, display, cookie);
    }

    dp = popen(xauth_str, "r");
    if (dp == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "Unable to launch xauth");
        return 1;
    }

    ret = pclose(dp);
    if (ret < 0)
    {
        log_message(LOG_LEVEL_ERROR, "An error occured while running xauth");
        return 1;
    }

    return 0;
}
