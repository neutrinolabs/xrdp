/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022, all xrdp contributors
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
 * @file libipm/scp.c
 * @brief scp definitions
 * @author Simone Fedele/ Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "scp.h"
#include "libipm.h"
#include "guid.h"
#include "trans.h"
#include "os_calls.h"
#include "string_calls.h"

/*****************************************************************************/
static const char *
msgno_to_str(unsigned short n)
{
    return
        (n == E_SCP_GATEWAY_REQUEST) ? "SCP_GATEWAY_REQUEST" :
        (n == E_SCP_GATEWAY_RESPONSE) ? "SCP_GATEWAY_RESPONSE" :
        (n == E_SCP_CREATE_SESSION_REQUEST) ? "SCP_CREATE_SESSION_REQUEST" :
        (n == E_SCP_CREATE_SESSION_RESPONSE) ? "SCP_CREATE_SESSION_RESPONSE" :
        (n == E_SCP_LIST_SESSIONS_REQUEST) ? "SCP_LIST_SESSIONS_REQUEST" :
        (n == E_SCP_LIST_SESSIONS_RESPONSE) ? "SCP_LIST_SESSIONS_RESPONSE" :
        NULL;
}

/*****************************************************************************/
const char *
scp_msgno_to_str(enum scp_msg_code n, char *buff, unsigned int buff_size)
{
    const char *str = msgno_to_str((unsigned short)n);

    if (str == NULL)
    {
        g_snprintf(buff, buff_size, "[code #%d]", (int)n);
    }
    else
    {
        g_snprintf(buff, buff_size, "%s", str);
    }

    return buff;
}

/*****************************************************************************/
struct trans *
scp_connect(const char *host, const  char *port,
            int (*term_func)(void))
{
    struct trans *t;
    if ((t = trans_create(TRANS_MODE_TCP, 128, 128)) != NULL)
    {
        if (host == NULL)
        {
            host = "localhost";
        }

        if (port == NULL)
        {
            port = "3350";
        }

        t->is_term = term_func;

        trans_connect(t, host, port, 3000);
        if (t->status != TRANS_STATUS_UP)
        {
            trans_delete(t);
            t = NULL;
        }
        else if (libipm_init_trans(t, LIBIPM_FAC_SCP, msgno_to_str) !=
                 E_LI_SUCCESS)
        {
            trans_delete(t);
            t = NULL;
        }
    }

    return t;
}

/*****************************************************************************/
int
scp_init_trans(struct trans *trans)
{
    return libipm_init_trans(trans, LIBIPM_FAC_SCP, msgno_to_str);
}

/*****************************************************************************/

int
scp_msg_in_check_available(struct trans *trans, int *available)
{
    return libipm_msg_in_check_available(trans, available);
}

/*****************************************************************************/

int
scp_msg_in_wait_available(struct trans *trans)
{
    return libipm_msg_in_wait_available(trans);
}

/*****************************************************************************/

void
scp_msg_in_reset(struct trans *trans)
{
    libipm_msg_in_reset(trans);
}

/*****************************************************************************/

enum scp_msg_code
scp_msg_in_start(struct trans *trans)
{
    return (enum scp_msg_code)libipm_msg_in_start(trans);
}

/*****************************************************************************/
int
scp_send_gateway_request(struct trans *trans,
                         const char *username,
                         const char *password,
                         const char *connection_description)
{
    int rv;

    rv = libipm_msg_out_simple_send(
             trans,
             (int)E_SCP_GATEWAY_REQUEST,
             "sss",
             username,
             password,
             connection_description);

    /* Wipe the output buffer to remove the password */
    libipm_msg_out_erase(trans);

    return rv;
}

/*****************************************************************************/

int
scp_get_gateway_request(struct trans *trans,
                        const char **username,
                        const char **password,
                        const char **connection_description)
{
    /* Make sure the buffer is cleared after processing this message */
    libipm_set_flags(trans, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    return libipm_msg_in_parse(trans, "sss", username, password,
                               connection_description);
}

/*****************************************************************************/

int
scp_send_gateway_response(struct trans *trans,
                          int auth_result)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_GATEWAY_RESPONSE,
               "i",
               auth_result);
}

/*****************************************************************************/

int
scp_get_gateway_response(struct trans *trans,
                         int *auth_result)
{
    int32_t i_auth_result = 0;
    int rv = libipm_msg_in_parse(trans, "i", &i_auth_result);
    if (rv == 0)
    {
        *auth_result = i_auth_result;
    }
    return rv;
}

/*****************************************************************************/

int
scp_send_create_session_request(struct trans *trans,
                                const char *username,
                                const char *password,
                                enum scp_session_type type,
                                unsigned short width,
                                unsigned short height,
                                unsigned char bpp,
                                const char *shell,
                                const char *directory,
                                const char *connection_description)
{
    int rv = libipm_msg_out_simple_send(
                 trans,
                 (int)E_SCP_CREATE_SESSION_REQUEST,
                 "ssyqqysss",
                 username,
                 password,
                 type,
                 width,
                 height,
                 bpp,
                 shell,
                 directory,
                 connection_description);

    /* Wipe the output buffer to remove the password */
    libipm_msg_out_erase(trans);

    return rv;
}

/*****************************************************************************/

int
scp_get_create_session_request(struct trans *trans,
                               const char **username,
                               const char **password,
                               enum scp_session_type *type,
                               unsigned short *width,
                               unsigned short *height,
                               unsigned char *bpp,
                               const char **shell,
                               const char **directory,
                               const char **connection_description)
{
    /* Intermediate values */
    uint8_t i_type;
    uint16_t i_width;
    uint16_t i_height;
    uint8_t i_bpp;

    /* Make sure the buffer is cleared after processing this message */
    libipm_set_flags(trans, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    int rv = libipm_msg_in_parse(
                 trans,
                 "ssyqqysss",
                 username,
                 password,
                 &i_type,
                 &i_width,
                 &i_height,
                 &i_bpp,
                 shell,
                 directory,
                 connection_description);

    if (rv == 0)
    {
        *type = (enum scp_session_type)i_type;
        *width = i_width;
        *height = i_height;
        /* bpp is fixed for Xorg session types */
        *bpp = (*type == SCP_SESSION_TYPE_XORG) ? 24 : i_bpp;
    }

    return rv;
}

/*****************************************************************************/

int
scp_send_create_session_response(struct trans *trans,
                                 int auth_result,
                                 int display,
                                 const struct guid *guid)
{
    struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_CREATE_SESSION_RESPONSE,
               "iiB",
               auth_result,
               display,
               &guid_descriptor);
}

/*****************************************************************************/

int
scp_get_create_session_response(struct trans *trans,
                                int *auth_result,
                                int *display,
                                struct guid *guid)
{
    /* Intermediate values */
    int32_t i_auth_result;
    int32_t i_display;

    const struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    int rv = libipm_msg_in_parse(
                 trans,
                 "iiB",
                 &i_auth_result,
                 &i_display,
                 &guid_descriptor);
    if (rv == 0)
    {
        *auth_result = i_auth_result;
        *display = i_display;
    }

    return rv;
}

/*****************************************************************************/

int
scp_send_list_sessions_request(struct trans *trans,
                               const char *username,
                               const char *password)
{
    int rv;

    rv = libipm_msg_out_simple_send(
             trans,
             (int)E_SCP_LIST_SESSIONS_REQUEST,
             "ss",
             username,
             password);

    /* Wipe the output buffer to remove the password */
    libipm_msg_out_erase(trans);

    return rv;
}

/*****************************************************************************/

int
scp_get_list_sessions_request(struct trans *trans,
                              const char **username,
                              const char **password)
{
    /* Make sure the buffer is cleared after processing this message */
    libipm_set_flags(trans, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    return libipm_msg_in_parse(trans, "ss", username, password);
}

/*****************************************************************************/

int
scp_send_list_sessions_response(
    struct trans *trans,
    enum scp_list_sessions_status status,
    const struct scp_session_info *info)
{
    int rv;

    if (status != E_SCP_LS_SESSION_INFO)
    {
        rv = libipm_msg_out_simple_send(
                 trans,
                 (int)E_SCP_LIST_SESSIONS_RESPONSE,
                 "i", status);
    }
    else
    {
        rv = libipm_msg_out_simple_send(
                 trans,
                 (int)E_SCP_LIST_SESSIONS_RESPONSE,
                 "iiuyqqyxss",
                 status,
                 info->sid,
                 info->display,
                 info->type,
                 info->width,
                 info->height,
                 info->bpp,
                 info->start_time,
                 info->username,
                 info->connection_description);
    }

    return rv;
}

/*****************************************************************************/

int
scp_get_list_sessions_response(
    struct trans *trans,
    enum scp_list_sessions_status *status,
    struct scp_session_info **info)
{
    int32_t i_status;
    int rv;

    if (info == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Bad pointer in %s", __func__);
        rv = 1;
    }
    else if ((rv = libipm_msg_in_parse(trans, "i", &i_status)) == 0)
    {
        *status = (enum scp_list_sessions_status)i_status;
        struct scp_session_info *p = NULL;

        if (*status == E_SCP_LS_SESSION_INFO)
        {
            int32_t i_sid;
            uint32_t i_display;
            uint8_t i_type;
            uint16_t i_width;
            uint16_t i_height;
            uint8_t i_bpp;
            int64_t i_start_time;
            char *i_username;
            char *i_connection_description;

            rv = libipm_msg_in_parse(
                     trans,
                     "iuyqqyxss",
                     &i_sid,
                     &i_display,
                     &i_type,
                     &i_width,
                     &i_height,
                     &i_bpp,
                     &i_start_time,
                     &i_username,
                     &i_connection_description);

            if (rv == 0)
            {
                /* Allocate a block of memory large enough for the
                 * structure result, and the strings it contains */
                unsigned int len = sizeof(struct scp_session_info) +
                                   g_strlen(i_username) + 1 +
                                   g_strlen(i_connection_description) + 1;
                if ((p = (struct scp_session_info *)g_malloc(len, 1)) == NULL)
                {
                    *status = E_SCP_LS_NO_MEMORY;
                }
                else
                {
                    /* Set up the string pointers in the block to point
                     * into the memory allocated after the block */
                    p->username = (char *)p + sizeof(struct scp_session_info);
                    p->connection_description =
                        p->username + g_strlen(i_username) + 1;

                    /* Copy the data over */
                    p->sid = i_sid;
                    p->display = i_display;
                    p->type = (enum scp_session_type)i_type;
                    p->width = i_width;
                    p->height = i_height;
                    p->bpp = i_bpp;
                    p->start_time = i_start_time;
                    g_strcpy(p->username, i_username);
                    g_strcpy(p->connection_description,
                             i_connection_description);
                }
            }
        }
        *info = p;
    }
    return rv;
}
