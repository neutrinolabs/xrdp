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

#include "auth.h"
#include "config.h"
#include "log.h"
#include "os_calls.h"
#include "sesman.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

static struct session_chain *g_sessions;
static int g_session_count;

/******************************************************************************/
unsigned int
session_list_get_count(void)
{
    return g_session_count;
}

/******************************************************************************/
void
session_list_add(struct session_chain *element)
{
    element->next = g_sessions;
    g_sessions = element;
    g_session_count++;
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
    struct session_chain *tmp;
    int policy = g_cfg->sess.policy;

    if ((policy & SESMAN_CFG_SESS_POLICY_DEFAULT) != 0)
    {
        /* In the past (i.e. xrdp before v0.9.14), the default
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

    for (tmp = g_sessions ; tmp != 0 ; tmp = tmp->next)
    {
        struct session_item *item = tmp->item;

        LOG(LOG_LEVEL_DEBUG,
            "%s: try %p type=%s U=%d B=%d D=(%dx%d) I=%s",
            __func__,
            item,
            SCP_SESSION_TYPE_TO_STR(item->type),
            item->uid,
            item->bpp,
            item->width, item->height,
            item->start_ip_addr);

        if (item->type != type)
        {
            LOG(LOG_LEVEL_DEBUG, "%s: Type doesn't match", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_U) && (int)uid != item->uid)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: UID doesn't match for 'U' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_B) && item->bpp != bpp)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: bpp doesn't match for 'B' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_D) &&
                (item->width != width || item->height != height))
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: Dimensions don't match for 'D' policy", __func__);
            continue;
        }

        if ((policy & SESMAN_CFG_SESS_POLICY_I) &&
                g_strcmp(item->start_ip_addr, ip_addr) != 0)
        {
            LOG(LOG_LEVEL_DEBUG,
                "%s: IPs don't match for 'I' policy", __func__);
            continue;
        }

        LOG(LOG_LEVEL_DEBUG,
            "%s: Got match, display=%d", __func__, item->display);
        return item;
    }

    LOG(LOG_LEVEL_DEBUG, "%s: No matches found", __func__);
    return 0;
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

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, XRDP_CHANSRV_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_PORT_OUT_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_PORT_IN_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, CHANSRV_API_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        g_sprintf(text, XRDP_X11RDP_STR, display);
        x_running = g_file_exist(text);
    }

    if (x_running)
    {
        LOG(LOG_LEVEL_INFO, "Found X server running at %s", text);
    }

    return x_running;
}

/******************************************************************************/
/* called with the main thread
   returns boolean */
static int
is_display_in_chain(int display)
{
    struct session_chain *chain;
    struct session_item *item;

    chain = g_sessions;

    while (chain != 0)
    {
        item = chain->item;

        if (item->display == display)
        {
            return 1;
        }

        chain = chain->next;
    }

    return 0;
}

/******************************************************************************/
int
session_list_get_available_display(void)
{
    int display;

    display = g_cfg->sess.x11_display_offset;

    while ((display - g_cfg->sess.x11_display_offset) <= g_cfg->sess.max_sessions)
    {
        if (!is_display_in_chain(display))
        {
            if (!x_server_running_check_ports(display))
            {
                return display;
            }
        }

        display++;
    }

    LOG(LOG_LEVEL_ERROR, "X server -- no display in range (%d to %d) is available",
        g_cfg->sess.x11_display_offset,
        g_cfg->sess.x11_display_offset + g_cfg->sess.max_sessions);
    return 0;
}

/******************************************************************************/
/**
 * Convert a UID to a username
 *
 * @param uid UID
 * @param uname pointer to output buffer
 * @param uname_len Length of output buffer
 * @return 0 for success.
 */
static int
username_from_uid(int uid, char *uname, int uname_len)
{
    char *ustr;
    int rv = g_getuser_info_by_uid(uid, &ustr, NULL, NULL, NULL, NULL);

    if (rv == 0)
    {
        g_snprintf(uname, uname_len, "%s", ustr);
        g_free(ustr);
    }
    else
    {
        g_snprintf(uname, uname_len, "<unknown>");
    }
    return rv;
}

/******************************************************************************/
enum session_kill_status
session_list_kill(int pid)
{
    struct session_chain *tmp;
    struct session_chain *prev;

    tmp = g_sessions;
    prev = 0;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "session descriptor for "
                "pid %d is null!", pid);

            if (prev == 0)
            {
                /* prev does no exist, so it's the first element - so we set
                   g_sessions */
                g_sessions = tmp->next;
            }
            else
            {
                prev->next = tmp->next;
            }

            return SESMAN_SESSION_KILL_NULLITEM;
        }

        if (tmp->item->pid == pid)
        {
            char username[256];
            username_from_uid(tmp->item->uid, username, sizeof(username));

            /* deleting the session */
            if (tmp->item->auth_info != NULL)
            {
                LOG(LOG_LEVEL_INFO,
                    "Calling auth_end for pid %d from pid %d",
                    pid, g_getpid());
                auth_end(tmp->item->auth_info);
                tmp->item->auth_info = NULL;
            }
            LOG(LOG_LEVEL_INFO,
                "++ terminated session: UID %d (%s), display :%d.0, "
                "session_pid %d, ip %s",
                tmp->item->uid, username, tmp->item->display,
                tmp->item->pid, tmp->item->start_ip_addr);
            g_free(tmp->item);

            if (prev == 0)
            {
                /* prev does no exist, so it's the first element - so we set
                   g_sessions */
                g_sessions = tmp->next;
            }
            else
            {
                prev->next = tmp->next;
            }

            g_free(tmp);
            g_session_count--;
            return SESMAN_SESSION_KILL_OK;
        }

        /* go on */
        prev = tmp;
        tmp = tmp->next;
    }

    return SESMAN_SESSION_KILL_NOTFOUND;
}

/******************************************************************************/
void
session_list_sigkill_all(void)
{
    struct session_chain *tmp;

    tmp = g_sessions;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "found null session descriptor!");
        }
        else
        {
            g_sigterm(tmp->item->pid);
        }

        /* go on */
        tmp = tmp->next;
    }
}

/******************************************************************************/
struct session_item *
session_list_get_bypid(int pid)
{
    struct session_chain *tmp;
    struct session_item *dummy;

    dummy = g_new0(struct session_item, 1);

    if (0 == dummy)
    {
        LOG(LOG_LEVEL_ERROR, "session_get_bypid: out of memory");
        return 0;
    }

    tmp = g_sessions;

    while (tmp != 0)
    {
        if (tmp->item == 0)
        {
            LOG(LOG_LEVEL_ERROR, "session descriptor for pid %d is null!", pid);
            g_free(dummy);
            return 0;
        }

        if (tmp->item->pid == pid)
        {
            g_memcpy(dummy, tmp->item, sizeof(struct session_item));
            return dummy;
        }

        /* go on */
        tmp = tmp->next;
    }

    g_free(dummy);
    return 0;
}

/******************************************************************************/
struct scp_session_info *
session_list_get_byuid(int uid, unsigned int *cnt, unsigned char flags)
{
    struct session_chain *tmp;
    struct scp_session_info *sess;
    int count;
    int index;

    count = 0;

    tmp = g_sessions;

    LOG(LOG_LEVEL_DEBUG, "searching for session by UID: %d", uid);
    while (tmp != 0)
    {
        if (uid == tmp->item->uid)
        {
            LOG(LOG_LEVEL_DEBUG, "session_list_get_byuid: status=%d, flags=%d, "
                "result=%d", (tmp->item->status), flags,
                ((tmp->item->status) & flags));

            if ((tmp->item->status) & flags)
            {
                count++;
            }
        }

        /* go on */
        tmp = tmp->next;
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

    tmp = g_sessions;
    index = 0;

    while (tmp != 0 && index < count)
    {
        if (uid == tmp->item->uid)
        {
            if ((tmp->item->status) & flags)
            {
                (sess[index]).sid = tmp->item->pid;
                (sess[index]).display = tmp->item->display;
                (sess[index]).type = tmp->item->type;
                (sess[index]).height = tmp->item->height;
                (sess[index]).width = tmp->item->width;
                (sess[index]).bpp = tmp->item->bpp;
                (sess[index]).start_time = tmp->item->start_time;
                (sess[index]).uid = tmp->item->uid;
                (sess[index]).start_ip_addr = g_strdup(tmp->item->start_ip_addr);

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

        /* go on */
        tmp = tmp->next;
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
