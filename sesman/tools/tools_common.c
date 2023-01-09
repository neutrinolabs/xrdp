/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022
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
 * @file tools_common.c
 * @brief Common definitions for the tools utilities
 * @author Matt Burt
 *
 */


#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "trans.h"
#include "os_calls.h"
#include "scp.h"


/*****************************************************************************/
int
wait_for_sesman_reply(struct trans *t, enum scp_msg_code wait_msgno)
{

    int rv = 0;
    int available = 0;

    while (rv == 0 && !available)
    {
        if ((rv = scp_msg_in_wait_available(t)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Error waiting on sesman transport");
        }
        else
        {
            enum scp_msg_code reply_msgno = scp_msg_in_get_msgno(t);

            available = 1;
            if (reply_msgno != wait_msgno)
            {
                char buff[64];
                scp_msgno_to_str(reply_msgno, buff, sizeof(buff));

                LOG(LOG_LEVEL_WARNING,
                    "Ignoring unexpected message %s", buff);
                scp_msg_in_reset(t);
                available = 0;
            }
        }
    }

    return rv;
}
