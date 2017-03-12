/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
 * @file libscp_init.c
 * @brief libscp initialization code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_init.h"

//struct log_config* s_log;

/* server API */
int
scp_init(void)
{
    /*
      if (0 == log)
      {
        return 1;
      }
    */

    //s_log = log;

    scp_lock_init();

    log_message(LOG_LEVEL_DEBUG, "libscp initialized");

    return 0;
}
