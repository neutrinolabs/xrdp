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
    int rv = 1;
    if (g_session_list == NULL)
    {
        g_session_list = list_create_sized(g_cfg->sess.max_sessions);
    }

    if (g_session_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't allocate session list");
    }
    else
    {
        g_session_list->auto_free = 0;
        rv = 0;
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
    return g_session_list->count;
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
/**
 *
 * @brief checks if there's a server running on a display
 * @param display the display to check
 * @return 0 if there isn't a display running, nonzero otherwise
 *
 */
static int
x_server_running_check_ports(int display)
{
    char text[256];
    int x_running;
    int sck;

    g_sprintf(text, "/tmp/.X11-unix/X%d", display);
    x_running = g_file_exist(text);

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, "/tmp/.X%d-lock", display);
        x_running = g_file_exist(text);
    }

    if (!x_running) /* check 59xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "59%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 60xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "60%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 62xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            g_sprintf(text, "62%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (x_running)
    {
        LOG(LOG_LEVEL_INFO, "Found X server running at %s", text);
    }

    return x_running;
}

/******************************************************************************/
/* Helper function for get_sorted_display_list():qsort() */
static int
icmp(const void *i1, const void *i2)
{
    return *(const unsigned int *)i2 - *(const unsigned int *)i1;
}

/******************************************************************************/
/**
 * Get a sorted array of all the displays allocated to sessions
 * @param[out] cnt Count of displays in list
 * @return Allocated array of displays or NULL for no memory
 *
 * Result must always be freed, even if cnt == 0
 */

static unsigned int *
get_sorted_session_displays(unsigned int *cnt)
{
    unsigned int *displays;

    *cnt = 0;
    displays = g_new(unsigned int, session_list_get_count() + 1);
    if (displays == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't allocate memory for display list");
    }
    else if (g_session_list != NULL)
    {
        int i;

        for (i = 0 ; i < g_session_list->count ; ++i)
        {
            const struct session_item *si;
            si = (const struct session_item *)list_get_item(g_session_list, i);
            if (SESSION_IN_USE(si) && si->display >= 0)
            {
                displays[(*cnt)++] = si->display;
            }
        }
        qsort(displays, *cnt, sizeof(displays[0]), icmp);
    }

    return displays;
}

/******************************************************************************/
int
session_list_get_available_display(void)
{
    int rv = -1;
    unsigned int max_alloc = 0;

    // Find all displays already allocated. We do this to prevent
    // unnecessary file system accesses, and also to prevent us allocating
    // the same display number to two callers who call in quick
    // succession  i.e. if the first caller has not created its X server
    // by the time we service the second request
    unsigned int *allocated_displays = get_sorted_session_displays(&max_alloc);
    if (allocated_displays != NULL)
    {
        unsigned int i = 0;
        unsigned int display;

        for (display = g_cfg->sess.x11_display_offset;
                display <= g_cfg->sess.max_display_number;
                ++display)
        {
            // Have we already allocated this one?
            while (i < max_alloc && display > allocated_displays[i])
            {
                ++i;
            }
            if (i < max_alloc && display == allocated_displays[i])
            {
                continue; // Already allocated
            }

            if (!x_server_running_check_ports(display))
            {
                break;
            }
        }

        g_free(allocated_displays);

        if (display > g_cfg->sess.max_display_number)
        {
            LOG(LOG_LEVEL_ERROR,
                "X server -- no display in range (%d to %d) is available",
                g_cfg->sess.x11_display_offset,
                g_cfg->sess.max_display_number);
        }
        else
        {
            rv = display;
        }
    }

    return rv;
}

/******************************************************************************/
struct session_item *
session_list_get_bydata(uid_t uid,
                        enum scp_session_type type,
                        unsigned short width,
                        unsigned short height,
                        unsigned char  bpp,
                        const char *ip_addr)
{
    char policy_str[64];
    int policy = g_cfg->sess.policy;
    int i;

    if (ip_addr == NULL)
    {
        ip_addr = "";
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
        "%s: search policy=%s type=%s U=%d B=%d D=(%dx%d) I=%s",
        __func__,
        policy_str, SCP_SESSION_TYPE_TO_STR(type),
        uid, bpp, width, height,
        ip_addr);

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
            "%s: try %p type=%s U=%d B=%d D=(%dx%d) I=%s",
            __func__,
            si,
            SCP_SESSION_TYPE_TO_STR(si->type),
            si->uid, si->bpp,
            si->start_width, si->start_height,
            si->start_ip_addr);

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

        LOG(LOG_LEVEL_DEBUG,
            "%s: Got match, display=%d", __func__, si->display);
        return si;
    }

    LOG(LOG_LEVEL_DEBUG, "%s: No matches found", __func__);
    return NULL;
}

/******************************************************************************/
struct scp_session_info *
session_list_get_byuid(uid_t uid, unsigned int *cnt, unsigned int flags)
{
    int i;
    struct scp_session_info *sess;
    int count;
    int index;

    count = 0;

    LOG(LOG_LEVEL_DEBUG, "searching for session by UID: %d", uid);

    for (i = 0 ; i < g_session_list->count ; ++i)
    {
        const struct session_item *si;
        si = (const struct session_item *)list_get_item(g_session_list, i);
        if (SESSION_IN_USE(si) && uid == si->uid)
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

        if (SESSION_IN_USE(si) && uid == si->uid)
        {
            (sess[index]).sid = si->sesexec_pid;
            (sess[index]).display = si->display;
            (sess[index]).type = si->type;
            (sess[index]).height = si->start_height;
            (sess[index]).width = si->start_width;
            (sess[index]).bpp = si->bpp;
            (sess[index]).start_time = si->start_time;
            (sess[index]).uid = si->uid;
            (sess[index]).start_ip_addr = g_strdup(si->start_ip_addr);

            /* Check for string allocation failures */
            if ((sess[index]).start_ip_addr == NULL)
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
void
free_session_info_list(struct scp_session_info *sesslist, unsigned int cnt)
{
    if (sesslist != NULL && cnt > 0)
    {
        unsigned int i;
        for (i = 0 ; i < cnt ; ++i)
        {
            g_free(sesslist[i].start_ip_addr);
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
