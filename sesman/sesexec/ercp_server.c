/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2023
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
 * @file ercp_server.c
 * @brief ercp (executive run-time control protocol) server function
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"

#include "sesexec.h"
#include "session.h"
#include "trans.h"

#include "ercp.h"
#include "ercp_server.h"

/******************************************************************************/
static int
handle_session_reconnect_event(struct trans *self)
{
    session_reconnect(g_login_info, g_session_data);
    return 0;
}

/******************************************************************************/
int
ercp_server(struct trans *self)
{
    int rv = 0;
    enum ercp_msg_code msgno;

    switch ((msgno = ercp_msg_in_get_msgno(self)))
    {
        case E_ERCP_SESSION_RECONNECT_EVENT:
            rv = handle_session_reconnect_event(self);
            break;

        default:
        {
            char buff[64];
            ercp_msgno_to_str(msgno, buff, sizeof(buff));
            LOG(LOG_LEVEL_ERROR, "Ignored ERCP message %s", buff);
        }
    }
    return rv;
}
