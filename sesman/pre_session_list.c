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
 * @file pre_session_list.h
 * @brief List of pre-session connections to sesman (definitions)
 *
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif


#include "arch.h"
#include "list.h"
#include "os_calls.h"
#include "pre_session_list.h"
#include "trans.h"

#define PRE_SESSION_IN_USE(si) \
    ( \
      (si) != NULL && \
      ( \
        ((si)->client_trans != NULL && (si)->client_trans->status == TRANS_STATUS_UP) || \
        ((si)->sesexec_trans != NULL && (si)->sesexec_trans->status == TRANS_STATUS_UP) \
      ) \
    )

static struct list *g_pre_session_list = NULL;

/**
 * Deletes a pre_session_item, freeing resources
 *
 * After this call, the passed-in pointer is invalid and must not be
 * referenced.
 *
 * Any auth_info struct found in the sesman_con is also deallocated.
 *
 * @param sc struct to de-allocate
 */
static void
free_pre_session_item(struct pre_session_item *psi)
{
    if (psi != NULL)
    {
        trans_delete(psi->client_trans);
        trans_delete(psi->sesexec_trans);
        g_free(psi->username);
        g_free(psi);
    }
}

/******************************************************************************/
int
pre_session_list_init(unsigned int list_size)
{
    int rv = 1;
    if (g_pre_session_list == NULL)
    {
        g_pre_session_list = list_create_sized(list_size);
    }

    if (g_pre_session_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't allocate pre-session list");
    }
    else
    {
        g_pre_session_list->auto_free = 0;
        rv = 0;
    }

    return rv;
}

/******************************************************************************/
void
pre_session_list_cleanup(void)
{
    if (g_pre_session_list != NULL)
    {
        int i;
        for (i = 0 ; i < g_pre_session_list->count ; ++i)
        {
            struct pre_session_item *p;
            p = (struct pre_session_item *)list_get_item(g_pre_session_list, i);
            free_pre_session_item(p);
        }
        list_delete(g_pre_session_list);
        g_pre_session_list = NULL;
    }
}

/******************************************************************************/
unsigned int
pre_session_list_get_count(void)
{
    return g_pre_session_list->count;
}

/******************************************************************************/
struct pre_session_item *
pre_session_list_new(void)
{
    struct pre_session_item *result = g_new0(struct pre_session_item, 1);
    if (result != NULL)
    {
        g_snprintf(result->peername, sizeof(result->peername), "unknown");
        result->uid = (uid_t) -1;

        if (!list_add_item(g_pre_session_list, (tintptr)result))
        {
            g_free(result);
            result = NULL;
        }
    }

    return result;
}

/*****************************************************************************/
int
pre_session_list_set_peername(struct pre_session_item *psi, const char *name)
{
    int rv = 1;

    if (psi != NULL && name != NULL)
    {
        g_snprintf(psi->peername, sizeof(psi->peername), "%s", name);
        rv = 0;
    }

    return rv;
}

/******************************************************************************/
int
pre_session_list_get_wait_objs(tbus robjs[], int *robjs_count)
{
    int i = 0;

    while (i < g_pre_session_list->count)
    {
        struct pre_session_item *psi;
        psi = (struct pre_session_item *)list_get_item(g_pre_session_list, i);
        int psi_in_use = 0;

        if (psi != NULL)
        {
            if (psi->client_trans != NULL &&
                    psi->client_trans->status == TRANS_STATUS_UP)
            {
                robjs[(*robjs_count)++] = psi->client_trans->sck;
                psi_in_use = 1;
            }

            if (psi->sesexec_trans != NULL &&
                    psi->sesexec_trans->status == TRANS_STATUS_UP)
            {
                robjs[(*robjs_count)++] = psi->sesexec_trans->sck;
                psi_in_use = 1;
            }
        }

        if (psi_in_use)
        {
            ++i;
        }
        else
        {
            free_pre_session_item(psi);
            list_remove_item(g_pre_session_list, i);
        }
    }

    return 0;
}

/******************************************************************************/
int
pre_session_list_check_wait_objs(void)
{
    int i = 0;

    while (i < g_pre_session_list->count)
    {
        struct pre_session_item *psi;
        enum pre_session_dispatcher_action action;

        psi = (struct pre_session_item *)list_get_item(g_pre_session_list, i);
        action = E_PSD_TERMINATE_PRE_SESSION;

        if (PRE_SESSION_IN_USE(psi))
        {
            if (psi->client_trans != NULL &&
                    psi->client_trans->status == TRANS_STATUS_UP)
            {
                if (trans_check_wait_objs(psi->client_trans) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "pre_session_list_check_wait_objs: "
                        "trans_check_wait_objs(1) failed, removing trans");
                    psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
                }
            }

            if (psi->sesexec_trans != NULL &&
                    psi->sesexec_trans->status == TRANS_STATUS_UP)
            {
                if (trans_check_wait_objs(psi->sesexec_trans) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "pre_session_list_check_wait_objs: "
                        "trans_check_wait_objs(2) failed, removing trans");
                    psi->dispatcher_action = E_PSD_TERMINATE_PRE_SESSION;
                }
            }

            /* Get any action, and reset the requested one */
            action = psi->dispatcher_action;
            psi->dispatcher_action = E_PSD_NONE;
        }

        switch (action)
        {
            case E_PSD_NONE:
                /* On to the next item on the list */
                ++i;
                break;

            case E_PSD_REMOVE_CLIENT_TRANS:
                trans_delete(psi->client_trans);
                psi->client_trans = NULL;
                /* On to the next item on the list */
                ++i;
                break;
            case E_PSD_TERMINATE_PRE_SESSION:
                free_pre_session_item(psi);
                list_remove_item(g_pre_session_list, i);
                break;
        }
    }

    return 0;
}
