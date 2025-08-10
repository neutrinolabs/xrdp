/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2024
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
 * @file sesexec_discover.c
 * @brief Declare functionality associated with sesman restart support
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>

#include "sesexec_discover.h"
#include "trans.h"
#include "sesexec.h"
#include "session.h"
#include "session_parameters.h"
#include "sesman_config.h"
#include "os_calls.h"
#include "ercp.h"
#include "login_info.h"

/*
 * Module-scope globals
 */
static struct trans *g_discover_trans = NULL;

/*****************************************************************************/
static int
discover_trans_conn_in(struct trans *trans, struct trans *new_trans)
{
    const struct session_parameters *sp;
    int rv = 0;

    if (trans == NULL || new_trans == NULL || trans != g_discover_trans)
    {
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "discover_trans_conn_in:");

    if (sesexec_is_ecp_active())
    {
        int pid = 0;
        (void)g_sck_get_peer_cred(new_trans->sck, &pid, 0, 0);

        LOG(LOG_LEVEL_WARNING,
            "Connection attempt to sesexec PID %d from PID %d"
            " while ECP is still active",
            g_pid, pid);

        trans_delete(new_trans);
    }
    else if ((sp = session_get_parameters(g_session_data)) == NULL)
    {
        // If we haven't got session parameters, we shouldn't be here
        LOG(LOG_LEVEL_ERROR, "Bugcheck: Can't get active session params");
        trans_delete(new_trans);
        rv = 1;
    }
    else
    {
        // Reconnect to sesman and tell it about our existing
        // session
        ercp_init_trans(new_trans);
        new_trans->trans_data_in = sesexec_ercp_data_in;
        new_trans->callback_data = NULL;

        // Note, this call makes further privilege checks that may still
        // fail.  If they do however, we wish to carry on running. These
        // failed checks will be logged.
        if (sesexec_set_ecp_transport(new_trans) == 0)
        {
            (void)ercp_send_session_announce_event(
                new_trans,
                session_get_display(g_session_data),
                g_login_info->uid,
                sp->type,
                sp->width,
                sp->height,
                sp->bpp,
                &sp->guid,
                g_login_info->ip_addr,
                session_get_start_time(g_session_data));

            // Tell semsan about the last client connect or disconnect
            if (g_ccp_trans != NULL)
            {
                (void)ercp_send_client_connect_event(new_trans,
                                                     g_client_ip,
                                                     g_client_name,
                                                     g_last_connect_disconnect);
            }
            else
            {
                (void)ercp_send_client_disconnect_event(new_trans,
                                                        g_last_connect_disconnect);
            }
        }
    }
    return rv;
}

/******************************************************************************/
int
sesexec_discover_enable(void)
{
    int rv = 1;
    if (g_session_data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Cant enable discovery without an active session");
    }
    else if (g_discover_trans != NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Logic error: discovery is already active");
    }
    else if ((g_discover_trans =
                  trans_create(TRANS_MODE_UNIX, 8192, 8192)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory enabling discovery");
    }
    else
    {
        char discover_port[XRDP_SOCKETS_MAXPATH];

        snprintf(discover_port, sizeof(discover_port), "%s.r/%d",
                 g_cfg->listen_port,
                 session_get_parameters(g_session_data)->x11_display);
        g_discover_trans->is_term = sesexec_is_term;
        g_discover_trans->trans_conn_in = discover_trans_conn_in;
        if ((rv = trans_listen(g_discover_trans, discover_port)) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Transport error enabling discovery [%s]",
                g_get_strerror());
            trans_delete(g_discover_trans);
            g_discover_trans = NULL;
        }
    }

    return rv;
}

/******************************************************************************/
int
sesexec_discover_disable(void)
{
    trans_delete(g_discover_trans);
    g_discover_trans = NULL;

    return 0;
}

/******************************************************************************/

int
sesexec_discover_get_wait_objs(intptr_t robjs[], int *robjs_count,
                               int max_count)
{
    int rv;
    if (g_discover_trans == NULL)
    {
        rv = 0;
    }
    else if (*robjs_count >= max_count)
    {
        rv = 1;
    }
    else
    {
        rv = trans_get_wait_objs(g_discover_trans, robjs, robjs_count);
    }

    return rv;
}

/******************************************************************************/
int
sesexec_discover_check_wait_objs(void)
{
    return (g_discover_trans == NULL)
           ? 0
           : trans_check_wait_objs(g_discover_trans);
}
