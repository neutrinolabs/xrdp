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
 * @file sig.c
 * @brief signal handling functions
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "sig.h"

#include "sesman_config.h"
#include "log.h"
#include "os_calls.h"
#include "sesman.h"
#include "session_list.h"
#include "string_calls.h"

/******************************************************************************/
void
sig_sesman_reload_cfg(void)
{
    int error;
    struct config_sesman *cfg;

    LOG(LOG_LEVEL_INFO, "receiving SIGHUP");

    if ((cfg = config_read(g_cfg->sesman_ini)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "error reading config - keeping old cfg");
        return;
    }

    /* Deal with significant config changes */
    if (g_strcmp(g_cfg->listen_port, cfg->listen_port) != 0)
    {
        LOG(LOG_LEVEL_INFO, "sesman listen port changed to %s",
            cfg->listen_port);

        /* We have to delete the old port before listening to the new one
         * in case they overlap in scope */
        sesman_delete_listening_transport();
        if (sesman_create_listening_transport(cfg) == 0)
        {
            LOG(LOG_LEVEL_INFO, "Sesman now listening on %s",
                g_cfg->listen_port);
        }
    }

    /* free old config data */
    config_free(g_cfg);

    /* replace old config with newly read one */
    g_cfg = cfg;

    /* Restart logging subsystem */
    error = log_start(g_cfg->sesman_ini, "xrdp-sesman", LOG_START_RESTART);

    if (error != LOG_STARTUP_OK)
    {
        char buf[256];

        switch (error)
        {
            case LOG_ERROR_MALLOC:
                g_printf("error on malloc. cannot restart logging. log stops here, sorry.\n");
                break;
            case LOG_ERROR_FILE_OPEN:
                g_printf("error reopening log file [%s]. log stops here, sorry.\n", getLogFile(buf, 255));
                break;
        }
    }

    LOG(LOG_LEVEL_INFO, "configuration reloaded, log subsystem restarted");
}
