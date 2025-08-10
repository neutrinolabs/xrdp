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
 * @file scp_list.h
 * @brief List of SCP connections to sesman (definitions)
 *
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif


#include "arch.h"
#include "list.h"
#include "os_calls.h"
#include "scp_list.h"
#include "set_int.h"
#include "trans.h"

#define SCP_LIST_ITEM_IN_USE(sli) \
    ( \
      (sli) != NULL && \
      ( \
        ((sli)->client_trans != NULL && (sli)->client_trans->status == TRANS_STATUS_UP) || \
        ((sli)->sesexec_trans != NULL && (sli)->sesexec_trans->status == TRANS_STATUS_UP) \
      ) \
    )

static struct list *g_scp_list = NULL;

/**
 * Deletes a scp_list_item, freeing resources
 *
 * After this call, the passed-in pointer is invalid and must not be
 * referenced.
 *
 * Any auth_info struct found in the sesman_con is also deallocated.
 *
 * @param sli struct to de-allocate
 */
static void
free_scp_list_item(struct scp_list_item *sli)
{
    if (sli != NULL)
    {
        trans_delete(sli->client_trans);
        trans_delete(sli->sesexec_trans);
        g_free(sli->username);
        g_free(sli);
    }
}

/******************************************************************************/
int
scp_list_init(unsigned int list_size)
{
    int rv = 1;
    if (g_scp_list == NULL)
    {
        g_scp_list = list_create_sized(list_size);
    }

    if (g_scp_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't allocate SCP list");
    }
    else
    {
        g_scp_list->auto_free = 0;
        rv = 0;
    }

    return rv;
}

/******************************************************************************/
void
scp_list_cleanup(void)
{
    if (g_scp_list != NULL)
    {
        int i;
        for (i = 0 ; i < g_scp_list->count ; ++i)
        {
            struct scp_list_item *p;
            p = (struct scp_list_item *)list_get_item(g_scp_list, i);
            free_scp_list_item(p);
        }
        list_delete(g_scp_list);
        g_scp_list = NULL;
    }
}

/******************************************************************************/
unsigned int
scp_list_get_count(void)
{
    return g_scp_list->count;
}

/******************************************************************************/
struct scp_list_item *
scp_list_item_new(void)
{
    struct scp_list_item *result = g_new0(struct scp_list_item, 1);
    if (result != NULL)
    {
        g_snprintf(result->peername, sizeof(result->peername), "unknown");
        result->uid = (uid_t) -1;
        result->session_x11_display = -1;
        if (!list_add_item(g_scp_list, (tintptr)result))
        {
            g_free(result);
            result = NULL;
        }
    }

    return result;
}

/*****************************************************************************/
int
scp_list_set_peername(struct scp_list_item *sli, const char *name)
{
    int rv = 1;

    if (sli != NULL && name != NULL)
    {
        g_snprintf(sli->peername, sizeof(sli->peername), "%s", name);
        rv = 0;
    }

    return rv;
}

/******************************************************************************/
int
scp_list_get_wait_objs(tbus robjs[], int *robjs_count)
{
    int i = 0;

    while (i < g_scp_list->count)
    {
        struct scp_list_item *sli;
        sli = (struct scp_list_item *)list_get_item(g_scp_list, i);
        int sli_in_use = 0;

        if (sli != NULL)
        {
            if (sli->client_trans != NULL &&
                    sli->client_trans->status == TRANS_STATUS_UP)
            {
                robjs[(*robjs_count)++] = sli->client_trans->sck;
                sli_in_use = 1;
            }

            if (sli->sesexec_trans != NULL &&
                    sli->sesexec_trans->status == TRANS_STATUS_UP)
            {
                robjs[(*robjs_count)++] = sli->sesexec_trans->sck;
                sli_in_use = 1;
            }
        }

        if (sli_in_use)
        {
            ++i;
        }
        else
        {
            free_scp_list_item(sli);
            list_remove_item(g_scp_list, i);
        }
    }

    return 0;
}

/******************************************************************************/
int
scp_list_check_wait_objs(void)
{
    int i = 0;

    while (i < g_scp_list->count)
    {
        struct scp_list_item *sli;
        enum scp_list_dispatcher_action action;

        sli = (struct scp_list_item *)list_get_item(g_scp_list, i);
        action = E_SLD_TERMINATE_SCP_CONN;

        if (SCP_LIST_ITEM_IN_USE(sli))
        {
            if (sli->client_trans != NULL &&
                    sli->client_trans->status == TRANS_STATUS_UP)
            {
                if (trans_check_wait_objs(sli->client_trans) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "scp_list_check_wait_objs: "
                        "trans_check_wait_objs(1) failed, removing trans");
                    sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
                }
            }

            if (sli->sesexec_trans != NULL &&
                    sli->sesexec_trans->status == TRANS_STATUS_UP)
            {
                if (trans_check_wait_objs(sli->sesexec_trans) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "scp_list_check_wait_objs: "
                        "trans_check_wait_objs(2) failed, removing trans");
                    sli->dispatcher_action = E_SLD_TERMINATE_SCP_CONN;
                }
            }

            /* Get any action, and reset the requested one */
            action = sli->dispatcher_action;
            sli->dispatcher_action = E_SLD_NONE;
        }

        switch (action)
        {
            case E_SLD_NONE:
                /* On to the next item on the list */
                ++i;
                break;

            case E_SLD_REMOVE_CLIENT_TRANS:
                trans_delete(sli->client_trans);
                sli->client_trans = NULL;
                /* On to the next item on the list */
                ++i;
                break;
            case E_SLD_TERMINATE_SCP_CONN:
                free_scp_list_item(sli);
                list_remove_item(g_scp_list, i);
                break;
        }
    }

    return 0;
}

/******************************************************************************/
void
scp_list_get_create_session_x11_displays(struct set_int *alloc_displays)
{
    int i = 0;
    for (i = 0; i < g_scp_list->count; ++i)
    {
        struct scp_list_item *sli;

        sli = (struct scp_list_item *)list_get_item(g_scp_list, i);

        // Only add X11 displays
        if (SCP_LIST_ITEM_IN_USE(sli) &&
                sli->create_session_in_progress &&
                sli->session_x11_display >= 0)
        {
            set_int_add(alloc_displays, sli->session_x11_display);
        }
    }
}
