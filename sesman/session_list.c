/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
 *
 * BSD process grouping by:
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland.
 * Copyright (c) 2000-2001 Markus Friedl.
 * Copyright (c) 2011-2015 Koichiro Iwao, Kyushu Institute of Technology.
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
 * @file session_list.c
 * @brief Session list management code
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "arch.h"
#include "session_list.h"
#include "trans.h"

#include "sesman_config.h"
#include "list.h"
#include "log.h"
#include "os_calls.h"
#include "sesman.h"
#include "set_int.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

static struct list *g_session_list = NULL;

#define SESSION_IN_USE(si) \
    ((si) != NULL && \
     (si)->sesexec_trans != NULL && \
     (si)->sesexec_trans->status == TRANS_STATUS_UP)

/******************************************************************************/
int
session_list_init(void)
{
    int rv = 0;
    if (g_session_list == NULL)
    {
        g_session_list = list_create_sized(g_cfg->sess.max_sessions);
        if (g_session_list == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Can't allocate session list");
            rv = 1;
        }
        else
        {
            g_session_list->auto_free = 0;
        }
    }

    return rv;
}

/******************************************************************************/
/**
 * Frees resources allocated to a session_item
 *
 * @param si Session item
 *
 * @note Any pointer to this item on g_session_list will be invalid
 *       after this call.
 */
static void
free_session(struct session_item *si)
{
    if (si != NULL)
    {
        if (si->sesexec_trans != NULL)
        {
            trans_delete(si->sesexec_trans);
        }
        g_free(si);
    }
}

/******************************************************************************/
void
session_list_cleanup(void)
{
    if (g_session_list != NULL)
    {
        int i;
        for (i = 0 ; i < g_session_list->count ; ++i)
        {
            struct session_item *si;
            si = (struct session_item *)list_get_item(g_session_list, i);
            free_session(si);
        }
        list_delete(g_session_list);
        g_session_list = NULL;
    }
}

/******************************************************************************/
unsigned int
session_list_get_count(void)
{
    return (g_session_list == NULL) ? 0 : g_session_list->count;
}

/******************************************************************************/
unsigned int
session_list_get_count_by_state(enum session_state state)
{
    unsigned int result = 0;
    int i;
    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        const struct session_item *si;
        si = (struct session_item *)list_get_item(g_session_list, i);
        if (si->state == state)
        {
            ++result;
        }
    }
    return result;
}

/******************************************************************************/
struct session_item *
session_list_new(void)
{
    struct session_item *result = g_new0(struct session_item, 1);
    if (result != NULL)
    {
        result->state = E_SESSION_STARTING;
        if (!list_add_item(g_session_list, (tintptr)result))
        {
            g_free(result);
            result = NULL;
        }
    }

    return result;
}

/******************************************************************************/
void
session_list_get_session_x11_displays(struct set_int *alloc_displays)
{
    int count = (g_session_list == NULL) ? 0 : g_session_list->count;

    int i = 0;
    for (i = 0 ; i < count ; ++i)
    {
        const struct session_item *si;
        si = (struct session_item *)list_get_item(g_session_list, i);

        if (SESSION_IN_USE(si) && SCP_SESSION_TYPE_IS_X11(si->type))
        {
            int display = g_get_x11_display_from_display_string(si->display);
            if (display >= 0)
            {
                set_int_add(alloc_displays, display);
            }
        }
    }
}

/******************************************************************************/
struct session_item *
session_list_get_bydata(uid_t uid,
                        enum scp_session_type type,
                        unsigned short width,
                        unsigned short height,
                        unsigned char  bpp,
                        const char *ip_addr,
                        const char *instance_name)
{
    char policy_str[64];
    int policy = g_cfg->sess.policy;
    int i;

    if (ip_addr == NULL)
    {
        ip_addr = "";
    }

    if (instance_name == NULL)
    {
        instance_name = "";
    }

    if ((policy & SESMAN_CFG_SESS_POLICY_DEFAULT) != 0)
    {
        /* Before xrdp v0.9.14, the default
         * session policy varied by type. If this is needed again
         * in the future, here is the place to add it */
        policy = SESMAN_CFG_SESS_POLICY_U | SESMAN_CFG_SESS_POLICY_B;
    }

    config_output_policy_string(policy, policy_str, sizeof(policy_str));

    LOG(LOG_LEVEL_DEBUG,
        "%s: search policy=%s type=%s U=%d B=%d D=(%dx%d) I=%s P=%s",
        __func__,
        policy_str, SCP_SESSION_TYPE_TO_STR(type),
        uid, bpp, width, height,
        ip_addr, instance_name);

    /* 'Separate' policy never matches */
    if (policy & SESMAN_CFG_SESS_POLICY_SEPARATE)
    {
        LOG(LOG_LEVEL_DEBUG, "%s: No matches possible", __func__);
        return NULL;
    }

    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        struct session_item *si;
        si = (struct session_item *)list_get_item(g_session_list, i);
        if (!SESSION_IN_USE(si))
        {
            continue;
        }

        LOG(LOG_LEVEL_DEBUG,
            "%s: try %p type=%s U=%d B=%d D=(%dx%d) I=%s N=%s",
            __func__,
            si,
            SCP_SESSION_TYPE_TO_STR(si->type),
            si->uid, si->bpp,
            si->start_width, si->start_height,
            si->start_ip_addr, si->xrdp_instance_name);

        if (si->type != type)
        {
            LOG(LOG_LEVEL_DEBUG, "%s: Type doesn't match", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_U) && uid != si->uid)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: UID doesn't match for 'U' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_B) && si->bpp != bpp)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: bpp doesn't match for 'B' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_D) &&
                (si->start_width != width ||
                 si->start_height != height))
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: Dimensions don't match for 'D' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_I) &&
                g_strcmp(si->start_ip_addr, ip_addr) != 0)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: IPs don't match for 'I' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_N) &&
                g_strcmp(si->xrdp_instance_name, instance_name) != 0)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: Instance names don't match for 'N' policy", __func__);
            continue;
        }

        LOG(LOG_LEVEL_DEBUG,
            "%s: Got match, display=%s", __func__, si->display);
        return si;
    }

    LOG(LOG_LEVEL_DEBUG, "%s: No matches found", __func__);
    return NULL;
}

/******************************************************************************/
struct scp_session_info *
session_list_get_byuid(const uid_t *uid, unsigned int *cnt, unsigned int flags)
{
    int i;
    struct scp_session_info *sess;
    int count;
    int index;

    count = 0;

    if (uid != NULL)
    {
        LOG(LOG_LEVEL_DEBUG, "searching for session by UID: %d", (int)*uid);
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "searching for all sessions");
    }

    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        const struct session_item *si;
        si = (const struct session_item *)list_get_item(g_session_list, i);
        if (SESSION_IN_USE(si) && (uid == NULL || *uid == si->uid))
        {
            count++;
        }
    }

    if (count == 0)
    {
        (*cnt) = 0;
        return 0;
    }

    /* malloc() an array of disconnected sessions */
    sess = g_new0(struct scp_session_info, count);

    if (sess == 0)
    {
        (*cnt) = 0;
        return 0;
    }

    index = 0;
    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        const struct session_item *si;
        si = (const struct session_item *)list_get_item(g_session_list, i);

        if (SESSION_IN_USE(si) && (uid == NULL || *uid == si->uid))
        {
            sess[index].sid = si->sesexec_pid;
            sess[index].display = g_strdup(si->display);
            sess[index].type = si->type;
            sess[index].height = si->start_height;
            sess[index].width = si->start_width;
            sess[index].bpp = si->bpp;
            sess[index].start_time = si->start_time;
            sess[index].uid = si->uid;
            sess[index].start_ip_addr = g_strdup(si->start_ip_addr);
            sess[index].client_ip = g_strdup(si->client_ip);
            sess[index].client_name = g_strdup(si->client_name);
            sess[index].last_connect_disconnect = si->last_connect_disconnect;
            sess[index].xrdp_instance_name = g_strdup(si->xrdp_instance_name);

            /* Check for string allocation failures */
            if (sess[index].display == NULL ||
                    sess[index].start_ip_addr == NULL ||
                    sess[index].client_ip == NULL ||
                    sess[index].client_name == NULL ||
                    sess[index].xrdp_instance_name == NULL)
            {
                free_session_info_list(sess, *cnt);
                (*cnt) = 0;
                return 0;
            }
            index++;
        }
    }

    (*cnt) = count;
    return sess;
}

/******************************************************************************/
struct session_item *
session_list_get_byguid(const struct guid *guid)
{
    int i;

    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        struct session_item *si;
        si = (struct session_item *)list_get_item(g_session_list, i);
        if (SESSION_IN_USE(si) && GUID_ARE_EQUAL(guid, &si->guid))
        {
            return si;
        }
    }

    return NULL;
}

/******************************************************************************/
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt)
{
    if (sesslist != NULL && cnt > 0)
    {
        unsigned int i;
        for (i = 0 ; i < cnt ; ++i)
        {
            g_free(sesslist[i].display);
            g_free(sesslist[i].start_ip_addr);
            g_free(sesslist[i].client_ip);
            g_free(sesslist[i].client_name);
            g_free(sesslist[i].xrdp_instance_name);
        }
    }

    g_free(sesslist);
}

/******************************************************************************/
int
session_list_get_wait_objs(tbus robjs[], int *robjs_count)
{
    int i;

    for (i = 0 ; i < g_session_list->count; ++i)
    {
        const struct session_item *si;
        si = (const struct session_item *)list_get_item(g_session_list, i);
        if (SESSION_IN_USE(si))
        {
            robjs[(*robjs_count)++] = si->sesexec_trans->sck;
        }
    }

    return 0;
}

/******************************************************************************/
int
session_list_check_wait_objs(void)
{
    int i = 0;

    while (i < g_session_list->count)
    {
        struct session_item *si;
        si = (struct session_item *)list_get_item(g_session_list, i);
        if (SESSION_IN_USE(si))
        {
            if (trans_check_wait_objs(si->sesexec_trans) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "sesman_check_wait_objs: "
                    "trans_check_wait_objs failed, removing trans");
                si->sesexec_trans->status = TRANS_STATUS_DOWN;
            }
        }

        if (SESSION_IN_USE(si))
        {
            ++i;
        }
        else
        {
            free_session(si);
            list_remove_item(g_session_list, i);
        }
    }

    return 0;
}
