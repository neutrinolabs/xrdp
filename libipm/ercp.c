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
 * @file libipm/ercp.c
 * @brief ERCP definitions
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "ercp.h"
#include "libipm.h"
#include "guid.h"
#include "os_calls.h"
#include "trans.h"

/*****************************************************************************/
static const char *
msgno_to_str(unsigned short n)
{
    return
        (n == E_ERCP_SESSION_ANNOUNCE_EVENT) ? "ERCP_SESSION_ANNOUNCE_EVENT" :
        (n == E_ERCP_SESSION_FINISHED_EVENT) ? "ERCP_SESSION_FINISHED_EVENT" :
        NULL;
}

/*****************************************************************************/
const char *
ercp_msgno_to_str(enum ercp_msg_code n, char *buff, unsigned int buff_size)
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
ercp_init_trans(struct trans *trans)
{
    return libipm_init_trans(trans, LIBIPM_FAC_ERCP, msgno_to_str);
}

/*****************************************************************************/
void
ercp_trans_from_eicp_trans(struct trans *trans,
                           ttrans_data_in callback_func,
                           void *callback_data)
{
    libipm_change_facility(trans, LIBIPM_FAC_EICP, LIBIPM_FAC_ERCP);
    trans->trans_data_in = callback_func;
    trans->callback_data = callback_data;
}

/*****************************************************************************/

int
ercp_msg_in_check_available(struct trans *trans, int *available)
{
    return libipm_msg_in_check_available(trans, available);
}

/*****************************************************************************/

int
ercp_msg_in_wait_available(struct trans *trans)
{
    return libipm_msg_in_wait_available(trans);
}

/*****************************************************************************/

enum ercp_msg_code
ercp_msg_in_get_msgno(const struct trans *trans)
{
    return (enum ercp_msg_code)libipm_msg_in_get_msgno(trans);
}

/*****************************************************************************/

void
ercp_msg_in_reset(struct trans *trans)
{
    libipm_msg_in_reset(trans);
}

/*****************************************************************************/

int
ercp_send_session_announce_event(struct trans *trans,
                                 unsigned int display,
                                 uid_t uid,
                                 enum scp_session_type type,
                                 unsigned short start_width,
                                 unsigned short start_height,
                                 unsigned char bpp,
                                 const struct guid *guid,
                                 const char *start_ip_addr,
                                 time_t start_time)
{
    struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    return libipm_msg_out_simple_send(
               trans,
               (int)E_ERCP_SESSION_ANNOUNCE_EVENT,
               "uiyqqyBsx",
               display,
               uid,
               type,
               start_width,
               start_height,
               bpp,
               &guid_descriptor,
               start_ip_addr,
               start_time);
}

/*****************************************************************************/

int
ercp_get_session_announce_event(struct trans *trans,
                                unsigned int *display,
                                uid_t *uid,
                                enum scp_session_type *type,
                                unsigned short *start_width,
                                unsigned short *start_height,
                                unsigned char *bpp,
                                struct guid *guid,
                                const char **start_ip_addr,
                                time_t *start_time)
{
    /* Intermediate values */
    uint32_t i_display;
    int32_t i_uid;
    uint8_t i_type;
    uint16_t i_width;
    uint16_t i_height;
    uint8_t i_bpp;
    int64_t i_start_time;

    const struct libipm_fsb guid_descriptor = { (void *)guid, sizeof(*guid) };

    int rv = libipm_msg_in_parse(
                 trans,
                 "uiyqqyBsx",
                 &i_display,
                 &i_uid,
                 &i_type,
                 &i_width,
                 &i_height,
                 &i_bpp,
                 &guid_descriptor,
                 start_ip_addr,
                 &i_start_time);

    if (rv == 0)
    {
        if (display != NULL)
        {
            *display = i_display;
        }
        *uid = (uid_t)i_uid;
        *type = (enum scp_session_type)i_type;
        *start_width = i_width;
        *start_height = i_height;
        *bpp = i_bpp;
        *start_time = (time_t)i_start_time;
    }

    return rv;
}

/*****************************************************************************/

int
ercp_send_session_finished_event(struct trans *trans)
{
    return libipm_msg_out_simple_send(
               trans, (int)E_ERCP_SESSION_FINISHED_EVENT, NULL);
}

/*****************************************************************************/

int
ercp_send_session_reconnect_event(struct trans *trans)
{
    return libipm_msg_out_simple_send(
               trans, (int)E_ERCP_SESSION_RECONNECT_EVENT, NULL);
}
