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
void *
scp_process_start(void *sck)
{
    struct SCP_CONNECTION scon;
    struct SCP_SESSION *sdata = NULL;

    scon.in_sck = (int)(tintptr)sck;
    LOG_DBG("started scp thread on socket %d", scon.in_sck);

    make_stream(scon.in_s);
    make_stream(scon.out_s);

    init_stream(scon.in_s, 8192);
    init_stream(scon.out_s, 8192);

    switch (scp_vXs_accept(&scon, &(sdata)))
    {
        case SCP_SERVER_STATE_OK:

            if (sdata->version == 0)
            {
                /* starts processing an scp v0 connection */
                LOG_DBG("accept ok, go on with scp v0");
                scp_v0_process(&scon, sdata);
            }
            else
            {
                LOG_DBG("accept ok, go on with scp v1");
                /*LOG_DBG("user: %s\npass: %s",sdata->username, sdata->password);*/
                scp_v1_process(&scon, sdata);
            }

            break;
        case SCP_SERVER_STATE_START_MANAGE:
            /* starting a management session */
            log_message(LOG_LEVEL_WARNING,
                        "starting a sesman management session...");
            scp_v1_mng_process(&scon, sdata);
            break;
        case SCP_SERVER_STATE_VERSION_ERR:
            /* an unknown scp version was requested, so we shut down the */
            /* connection (and log the fact)                             */
            log_message(LOG_LEVEL_WARNING,
                        "unknown protocol version specified. connection refused.");
            break;
        case SCP_SERVER_STATE_NETWORK_ERR:
            log_message(LOG_LEVEL_WARNING, "libscp network error.");
            break;
        case SCP_SERVER_STATE_SEQUENCE_ERR:
            log_message(LOG_LEVEL_WARNING, "libscp sequence error.");
            break;
        case SCP_SERVER_STATE_INTERNAL_ERR:
            /* internal error occurred (eg. malloc() error, ecc.) */
            log_message(LOG_LEVEL_ERROR, "libscp internal error occurred.");
            break;
        default:
            log_message(LOG_LEVEL_ALWAYS, "unknown return from scp_vXs_accept()");
            break;
    }

    free_stream(scon.in_s);
    free_stream(scon.out_s);

    if (sdata)
    {
        scp_session_destroy(sdata);
    }

    return 0;
}
