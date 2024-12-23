/*
 *
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg and contributors 2004-2024
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
 *
 * Shell around the g_drop_privileges() call
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "os_calls.h"
#include "log.h"

int main(int argc, char *argv[])
{
    struct log_config *logging;
    int status = 1;
    logging = log_config_init_for_console(LOG_LEVEL_WARNING,
                                          g_getenv("DROPPRIV_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    if (argc < 4)
    {
        LOG(LOG_LEVEL_ERROR, "Usage : %s [user] [group] [cmd...]\n", argv[0]);
    }
    else if (g_drop_privileges(argv[1], argv[2]) == 0)
    {
        status = g_execvp(argv[3], &argv[3]);
    }

    log_end();
    return status;
}
