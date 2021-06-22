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
 * @file scp.c
 * @brief scp (sesman control protocol) common code
 *        scp (sesman control protocol) common code
 *        This code controls which version is being used and starts the
 *        appropriate process
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "sesman.h"

extern struct config_sesman *g_cfg; /* in sesman.c */

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_process(struct trans *t, struct SCP_SESSION *sdata)
{
    enum SCP_SERVER_STATES_E result = scp_vXs_accept(t, sdata);
    switch (result)
    {
        case SCP_SERVER_STATE_OK:
            if (sdata->version == 0)
            {
                /* starts processing an scp v0 connection */
                LOG_DEVEL(LOG_LEVEL_DEBUG, "accept ok, go on with scp v0");
                result = scp_v0_process(t, sdata);
            }
            else
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "accept ok, go on with scp v1");
                result = scp_v1_process(t, sdata);
            }
            break;
        case SCP_SERVER_STATE_START_MANAGE:
            /* starting a management session */
            LOG(LOG_LEVEL_INFO,
                "starting a sesman management session...");
            result = scp_v1_mng_process_msg(t, sdata);
            break;
        case SCP_SERVER_STATE_VERSION_ERR:
        case SCP_SERVER_STATE_SIZE_ERR:
            /* an unknown scp version was requested, or the message sizes
               are inconsistent. Shut down the connection and log the
               fact */
            LOG(LOG_LEVEL_WARNING,
                "protocol violation. connection refused.");
            break;
        case SCP_SERVER_STATE_NETWORK_ERR:
            LOG(LOG_LEVEL_WARNING, "libscp network error.");
            break;
        case SCP_SERVER_STATE_SEQUENCE_ERR:
            LOG(LOG_LEVEL_WARNING, "libscp sequence error.");
            break;
        case SCP_SERVER_STATE_INTERNAL_ERR:
            /* internal error occurred (eg. malloc() error, ecc.) */
            LOG(LOG_LEVEL_ERROR, "libscp internal error occurred.");
            break;
        default:
            LOG(LOG_LEVEL_ALWAYS, "unknown return from scp_vXs_accept()");
            result = SCP_SERVER_STATE_INTERNAL_ERR;
            break;
    }

    return result;
}

