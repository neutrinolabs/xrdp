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

#include <ctype.h>

#include "scp.h"
#include "libipm.h"
#include "guid.h"
#include "trans.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xrdp_sockets.h"

/*****************************************************************************/
static const char *
msgno_to_str(unsigned short n)
{
    return
        (n == E_SCP_SET_PEERNAME_REQUEST) ? "SCP_SET_PEERNAME_REQUEST" :

        (n == E_SCP_SYS_LOGIN_REQUEST) ? "SCP_SYS_LOGIN_REQUEST" :
        (n == E_SCP_UDS_LOGIN_REQUEST) ? "SCP_UDS_LOGIN_REQUEST" :
        (n == E_SCP_LOGIN_RESPONSE) ? "SCP_LOGIN_RESPONSE" :

        (n == E_SCP_LOGOUT_REQUEST) ? "SCP_LOGOUT_REQUEST" :

        (n == E_SCP_CREATE_SESSION_REQUEST) ? "SCP_CREATE_SESSION_REQUEST" :
        (n == E_SCP_CREATE_SESSION_RESPONSE) ? "SCP_CREATE_SESSION_RESPONSE" :

        (n == E_SCP_LIST_SESSIONS_REQUEST) ? "SCP_LIST_SESSIONS_REQUEST" :
        (n == E_SCP_LIST_SESSIONS_RESPONSE) ? "SCP_LIST_SESSIONS_RESPONSE" :

        (n == E_SCP_CLOSE_CONNECTION_REQUEST) ? "SCP_CLOSE_CONNECTION_REQUEST" :
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
/**
 * Helper function returning 1 if the passed-in string is an integer >= 0
 */
static int is_positive_int(const char *s)
{
    for ( ; *s != '\0' ; ++s)
    {
        if (!isdigit(*s))
        {
            return 0;
        }
    }

    return 1;
}

/*****************************************************************************/
int
scp_port_to_unix_domain_path(const char *port, char *buff,
                             unsigned int bufflen)
{
    /* GOTCHA: Changes to this logic should be mirrored in
     * scp_port_to_display_string() */

    int result;

    /* Make sure we can safely de-reference 'port' */
    if (port == NULL)
    {
        port = "";
    }

    if (port[0] == '/')
    {
        result = g_snprintf(buff, bufflen, "%s", port);
    }
    else
    {
        const char *sep;
        if ((sep = g_strrchr(port, '/')) != NULL && sep != port)
        {
            /* We allow the user to specify an absolute path, but not
             * a relative one with embedded '/' characters */
            LOG(LOG_LEVEL_WARNING, "Ignoring path elements of '%s'", port);
            port = sep + 1;
        }

        if (port[0] == '\0')
        {
            port = SCP_LISTEN_PORT_BASE_STR;
        }
        else if (is_positive_int(port))
        {
            /* Version v0.9.x and earlier of xrdp used a TCP port
             * number. If we come across this, we'll ignore it for
             * compatibility with old config files */
            LOG(LOG_LEVEL_WARNING,
                "Ignoring obsolete SCP port value '%s'", port);
            port = SCP_LISTEN_PORT_BASE_STR;
        }

        result = g_snprintf(buff, bufflen, XRDP_SOCKET_ROOT_PATH "/%s", port);
    }

    return result;
}

/*****************************************************************************/
int
scp_port_to_display_string(const char *port, char *buff, unsigned int bufflen)
{
    /* Make sure we can safely de-reference 'port' */
    if (port == NULL)
    {
        port = "";
    }

    /* Ignore any directories for the display */
    const char *sep;
    if ((sep = g_strrchr(port, '/')) != NULL)
    {
        port = sep + 1;
    }

    /* Check for a default */
    if (port[0] == '\0' || g_strcmp(port, "3350") == 0)
    {
        port = SCP_LISTEN_PORT_BASE_STR;
    }

    return g_snprintf(buff, bufflen, "%s", port);
}

/*****************************************************************************/
struct trans *
scp_connect(const  char *port,
            const char *peername,
            int (*term_func)(void))
{
    char sock_path[256];
    struct trans *t;

    (void)scp_port_to_unix_domain_path(port, sock_path, sizeof(sock_path));
    if ((t = trans_create(TRANS_MODE_UNIX, 128, 128)) != NULL)
    {
        t->is_term = term_func;

        if (trans_connect(t, NULL, sock_path, 3000) != 0)
        {
            trans_delete(t);
            t = NULL;
        }
        else if (scp_init_trans(t) != 0)
        {
            trans_delete(t);
            t = NULL;
        }
        else if (scp_send_set_peername_request(t, peername) != 0)
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
struct trans *
scp_init_trans_from_fd(int fd, int trans_type, int (*term_func)(void))
{
    struct trans *result;
    if ((result = trans_create(TRANS_MODE_UNIX, 128, 128)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't create SCP transport [%s]",
            g_get_strerror());
    }
    else
    {
        result->sck = fd;
        result->type1 = trans_type;
        result->status = TRANS_STATUS_UP;
        result->is_term = term_func;

        // Make sure child processes don't inherit our FD
        (void)g_file_set_cloexec(result->sck, 1);

        if (scp_init_trans(result) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "scp_init_trans() call failed");
            trans_delete(result);
            result = NULL;
        }
    }

    return result;
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

enum scp_msg_code
scp_msg_in_get_msgno(const struct trans *trans)
{
    return (enum scp_msg_code)libipm_msg_in_get_msgno(trans);
}

/*****************************************************************************/

void
scp_msg_in_reset(struct trans *trans)
{
    libipm_msg_in_reset(trans);
}

/*****************************************************************************/
int
scp_send_set_peername_request(struct trans *trans,
                              const char *peername)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_SET_PEERNAME_REQUEST,
               "s",
               peername);
}

/*****************************************************************************/

int
scp_get_set_peername_request(struct trans *trans,
                             const char **peername)
{
    return libipm_msg_in_parse( trans, "s", peername);
}


/*****************************************************************************/
int
scp_send_uds_login_request(struct trans *trans)
{
    return libipm_msg_out_simple_send(trans,
                                      (int)E_SCP_UDS_LOGIN_REQUEST,
                                      NULL);
}


/*****************************************************************************/
int
scp_send_sys_login_request(struct trans *trans,
                           const char *username,
                           const char *password,
                           const char *ip_addr)
{
    int rv;

    rv = libipm_msg_out_simple_send(
             trans,
             (int)E_SCP_SYS_LOGIN_REQUEST,
             "sss",
             username,
             password,
             ip_addr);

    /* Wipe the output buffer to remove the password */
    libipm_msg_out_erase(trans);

    return rv;
}

/*****************************************************************************/

int
scp_get_sys_login_request(struct trans *trans,
                          const char **username,
                          const char **password,
                          const char **ip_addr)
{
    /* Make sure the buffer is cleared after processing this message */
    libipm_set_flags(trans, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    return libipm_msg_in_parse( trans, "sss",
                                username, password, ip_addr);
}

/*****************************************************************************/

int
scp_send_login_response(struct trans *trans,
                        enum scp_login_status login_result,
                        int server_closed,
                        int uid)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_LOGIN_RESPONSE,
               "ibi",
               login_result,
               (server_closed != 0), /* Convert to 0/1 */
               uid);
}

/*****************************************************************************/

int
scp_get_login_response(struct trans *trans,
                       enum scp_login_status *login_result,
                       int *server_closed,
                       int *uid)
{
    int32_t i_login_result = 0;
    int32_t i_uid = 0;
    int dummy;

    /* User can pass in NULL for server_closed if they're trying an
     * login method like UDS for which all fails are fatal. Likewise
     * they may be uninterested in the uid */
    if (server_closed == NULL)
    {
        server_closed = &dummy;
    }
    if (uid == NULL)
    {
        uid = &dummy;
    }

    int rv = libipm_msg_in_parse(trans, "ibi",
                                 &i_login_result, server_closed, &i_uid);
    if (rv == 0)
    {
        *login_result = (enum scp_login_status)i_login_result;
        *uid = i_uid;
    }

    return rv;
}

/*****************************************************************************/

int
scp_send_logout_request(struct trans *trans)
{
    return libipm_msg_out_simple_send( trans, (int)E_SCP_LOGOUT_REQUEST, NULL);
}


/*****************************************************************************/

int
scp_send_create_session_request(struct trans *trans,
                                enum scp_session_type type,
                                unsigned short width,
                                unsigned short height,
                                unsigned char bpp,
                                const char *shell,
                                const char *directory)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_CREATE_SESSION_REQUEST,
               "yqqyss",
               type,
               width,
               height,
               bpp,
               shell,
               directory);
}

/*****************************************************************************/

int
scp_get_create_session_request(struct trans *trans,
                               enum scp_session_type *type,
                               unsigned short *width,
                               unsigned short *height,
                               unsigned char *bpp,
                               const char **shell,
                               const char **directory)
{
    /* Intermediate values */
    uint8_t i_type;
    uint16_t i_width;
    uint16_t i_height;
    uint8_t i_bpp;

    int rv = libipm_msg_in_parse(
                 trans,
                 "yqqyss",
                 &i_type,
                 &i_width,
                 &i_height,
                 &i_bpp,
                 shell,
                 directory);

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
                                 enum scp_screate_status status,
                                 int display,
                                 const struct guid *guid)
{
    struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_CREATE_SESSION_RESPONSE,
               "iiB",
               status,
               display,
               &guid_descriptor);
}

/*****************************************************************************/

int
scp_get_create_session_response(struct trans *trans,
                                enum scp_screate_status *status,
                                int *display,
                                struct guid *guid)
{
    /* Intermediate values */
    int32_t i_status;
    int32_t i_display;

    const struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    int rv = libipm_msg_in_parse(
                 trans,
                 "iiB",
                 &i_status,
                 &i_display,
                 &guid_descriptor);
    if (rv == 0)
    {
        *status = (enum scp_screate_status)i_status;
        *display = i_display;
    }

    return rv;
}

/*****************************************************************************/

int
scp_send_list_sessions_request(struct trans *trans)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_LIST_SESSIONS_REQUEST,
               NULL);
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
                 "iiuyqqyxis",
                 status,
                 info->sid,
                 info->display,
                 info->type,
                 info->width,
                 info->height,
                 info->bpp,
                 info->start_time,
                 info->uid,
                 info->start_ip_addr);
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
            int32_t i_uid;
            char *i_start_ip_addr;

            rv = libipm_msg_in_parse(
                     trans,
                     "iuyqqyxis",
                     &i_sid,
                     &i_display,
                     &i_type,
                     &i_width,
                     &i_height,
                     &i_bpp,
                     &i_start_time,
                     &i_uid,
                     &i_start_ip_addr);

            if (rv == 0)
            {
                /* Allocate a block of memory large enough for the
                 * structure result, and the strings it contains */
                unsigned int len = sizeof(struct scp_session_info) +
                                   g_strlen(i_start_ip_addr) + 1;
                if ((p = (struct scp_session_info *)g_malloc(len, 1)) == NULL)
                {
                    *status = E_SCP_LS_NO_MEMORY;
                }
                else
                {
                    /* Set up the string pointers in the block to point
                     * into the memory allocated after the block */
                    p->start_ip_addr =
                        (char *)p + sizeof(struct scp_session_info);

                    /* Copy the data over */
                    p->sid = i_sid;
                    p->display = i_display;
                    p->type = (enum scp_session_type)i_type;
                    p->width = i_width;
                    p->height = i_height;
                    p->bpp = i_bpp;
                    p->start_time = i_start_time;
                    p->uid = i_uid;
                    g_strcpy(p->start_ip_addr, i_start_ip_addr);
                }
            }
        }
        *info = p;
    }
    return rv;
}

/*****************************************************************************/

int
scp_send_close_connection_request(struct trans *trans)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_SCP_CLOSE_CONNECTION_REQUEST,
               NULL);
}
