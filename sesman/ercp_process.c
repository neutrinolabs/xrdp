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
 * @file ercp_process.c
 * @brief ERCP (executive run-time control protocol) handler function
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>

#include "trans.h"

#include "ercp.h"
#include "ercp_process.h"
#include "session_list.h"

/******************************************************************************/
static int
process_session_announce_event(struct session_item *si)
{
    int rv;
    const char *start_ip_addr;

    rv = ercp_get_session_announce_event(si->sesexec_trans,
                                         NULL,
                                         &si->uid,
                                         &si->type,
                                         &si->start_width,
                                         &si->start_height,
                                         &si->bpp,
                                         &si->guid,
                                         &start_ip_addr,
                                         &si->start_time);
    if (rv == 0)
    {
        snprintf(si->start_ip_addr, sizeof(si->start_ip_addr),
                 "%s", start_ip_addr);
        si->state = E_SESSION_RUNNING;
    }

    return rv;
}

/******************************************************************************/
static void
process_session_finished_event(struct session_item *si)
{
    LOG(LOG_LEVEL_INFO, "Session on display %d has finished.",
        si->display);
    // Setting the transport down will remove this connection from the list
    si->sesexec_trans->status = TRANS_STATUS_DOWN;
}

/******************************************************************************/
int
ercp_process(struct session_item *si)
{
    enum ercp_msg_code msgno;
    int rv = 0;

    switch ((msgno = ercp_msg_in_get_msgno(si->sesexec_trans)))
    {
        case E_ERCP_SESSION_ANNOUNCE_EVENT:
            rv = process_session_announce_event(si);
            break;

        case E_ERCP_SESSION_FINISHED_EVENT:
            process_session_finished_event(si);
            break;

        default:
        {
            char buff[64];
            ercp_msgno_to_str(msgno, buff, sizeof(buff));
            LOG(LOG_LEVEL_ERROR, "Ignored EICP message %s", buff);
        }
    }
    return rv;
}

