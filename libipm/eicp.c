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
 * @file libipm/eicp.c
 * @brief EICP definitions
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "eicp.h"
#include "libipm.h"
#include "guid.h"
#include "os_calls.h"
#include "trans.h"

/*****************************************************************************/
static const char *
msgno_to_str(unsigned short n)
{
    return
        (n == E_EICP_SYS_LOGIN_REQUEST) ? "EICP_SYS_LOGIN_REQUEST" :
        (n == E_EICP_SYS_LOGIN_RESPONSE) ? "EICP_SYS_LOGIN_RESPONSE" :

        (n == E_EICP_LOGOUT_REQUEST) ? "EICP_LOGOUT_REQUEST" :

        (n == E_EICP_CREATE_SESSION_REQUEST) ? "EICP_CREATE_SESSION_REQUEST" :

        NULL;
}

/*****************************************************************************/
const char *
eicp_msgno_to_str(enum eicp_msg_code n, char *buff, unsigned int buff_size)
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
int
eicp_init_trans(struct trans *trans)
{
    return libipm_init_trans(trans, LIBIPM_FAC_EICP, msgno_to_str);
}

/*****************************************************************************/
struct trans *
eicp_init_trans_from_fd(int fd, int trans_type, int (*term_func)(void))
{
    struct trans *result;
    if ((result = trans_create(TRANS_MODE_UNIX, 128, 128)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't create ECP transport [%s]",
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

        if (eicp_init_trans(result) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "eicp_init_trans() call failed");
            trans_delete(result);
            result = NULL;
        }
    }

    return result;
}


/*****************************************************************************/

int
eicp_msg_in_check_available(struct trans *trans, int *available)
{
    return libipm_msg_in_check_available(trans, available);
}

/*****************************************************************************/

int
eicp_msg_in_wait_available(struct trans *trans)
{
    return libipm_msg_in_wait_available(trans);
}

/*****************************************************************************/

enum eicp_msg_code
eicp_msg_in_get_msgno(const struct trans *trans)
{
    return (enum eicp_msg_code)libipm_msg_in_get_msgno(trans);
}

/*****************************************************************************/

void
eicp_msg_in_reset(struct trans *trans)
{
    libipm_msg_in_reset(trans);
}

/*****************************************************************************/
int
eicp_send_sys_login_request(struct trans *trans,
                            const char *username,
                            const char *password,
                            const char *ip_addr,
                            int scp_fd)
{
    int rv;

    rv = libipm_msg_out_simple_send(
             trans,
             (int)E_EICP_SYS_LOGIN_REQUEST,
             "sssh",
             username,
             password,
             ip_addr,
             scp_fd);

    /* Wipe the output buffer to remove the password */
    libipm_msg_out_erase(trans);

    return rv;
}

/*****************************************************************************/

int
eicp_get_sys_login_request(struct trans *trans,
                           const char **username,
                           const char **password,
                           const char **ip_addr,
                           int *scp_fd)
{
    /* Make sure the buffer is cleared after processing this message */
    libipm_set_flags(trans, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    return libipm_msg_in_parse( trans, "sssh",
                                username, password, ip_addr, scp_fd);
}

/*****************************************************************************/

int
eicp_send_sys_login_response(struct trans *trans,
                             int is_logged_in,
                             uid_t uid,
                             int scp_fd)
{
    int rv;

    if (is_logged_in)
    {
        rv = libipm_msg_out_simple_send(
                 trans,
                 (int)E_EICP_SYS_LOGIN_RESPONSE,
                 "bih",
                 1,
                 uid,
                 scp_fd);
    }
    else
    {
        rv = libipm_msg_out_simple_send(
                 trans, (int)E_EICP_SYS_LOGIN_RESPONSE, "b", 0);
    }

    return rv;
}

/*****************************************************************************/

int
eicp_get_sys_login_response(struct trans *trans,
                            int *is_logged_in,
                            uid_t *uid,
                            int *scp_fd)
{
    int rv;

    if ((rv = libipm_msg_in_parse(trans, "b", is_logged_in)) == 0)
    {
        if (*is_logged_in)
        {
            int32_t i_uid;

            rv = libipm_msg_in_parse(
                     trans,
                     "ih",
                     &i_uid,
                     scp_fd);

            if (rv == 0)
            {
                *uid = (uid_t)i_uid;
            }
        }
        else
        {
            *uid = (uid_t) -1;
            *scp_fd = -1;
        }
    }

    return rv;
}

/*****************************************************************************/
int
eicp_send_logout_request(struct trans *trans)
{
    return libipm_msg_out_simple_send(trans, (int)E_EICP_LOGOUT_REQUEST, NULL);
}

/*****************************************************************************/

int
eicp_send_create_session_request(struct trans *trans,
                                 int scp_fd,
                                 unsigned int display,
                                 enum scp_session_type type,
                                 unsigned short width,
                                 unsigned short height,
                                 unsigned char bpp,
                                 const char *shell,
                                 const char *directory)
{
    return libipm_msg_out_simple_send(
               trans,
               (int)E_EICP_CREATE_SESSION_REQUEST,
               "huyqqyss",
               scp_fd,
               display,
               type,
               width,
               height,
               bpp,
               shell,
               directory);
}

/*****************************************************************************/

int
eicp_get_create_session_request(struct trans *trans,
                                int *scp_fd,
                                unsigned int *display,
                                enum scp_session_type *type,
                                unsigned short *width,
                                unsigned short *height,
                                unsigned char *bpp,
                                const char **shell,
                                const char **directory)
{
    /* Intermediate values */
    uint32_t i_display;
    uint8_t i_type;
    uint16_t i_width;
    uint16_t i_height;
    uint8_t i_bpp;

    int rv = libipm_msg_in_parse(
                 trans,
                 "huyqqyss",
                 scp_fd,
                 &i_display,
                 &i_type,
                 &i_width,
                 &i_height,
                 &i_bpp,
                 shell,
                 directory);

    if (rv == 0)
    {
        *display = i_display;
        *type = (enum scp_session_type)i_type;
        *width = i_width;
        *height = i_height;
        /* bpp is fixed for Xorg session types */
        *bpp = (*type == SCP_SESSION_TYPE_XORG) ? 24 : i_bpp;
    }

    return rv;
}
