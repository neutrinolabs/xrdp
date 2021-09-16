/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
 * @file libscp_connection.c
 * @brief SCP_CONNECTION handling code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_connection.h"
#include "string_calls.h"

struct trans *
scp_trans_create(int sck)
{
    struct trans *result = trans_create(TRANS_MODE_TCP, 8192, 8192);
    if (result != NULL)
    {
        result->sck = sck;
    }

    return result;
}

/*****************************************************************************/

const char *
scp_client_state_to_str(enum SCP_CLIENT_STATES_E e)
{
    const char *result = "SCP_CLIENT_STATE_????";

    /* Some compilers will warn if this switch is missing states */
    switch (e)
    {
        case SCP_CLIENT_STATE_OK:
            result = "SCP_CLIENT_STATE_OK";
            break;
        case SCP_CLIENT_STATE_NETWORK_ERR:
            result = "SCP_CLIENT_STATE_NETWORK_ERR";
            break;
        case SCP_CLIENT_STATE_VERSION_ERR:
            result = "SCP_CLIENT_STATE_VERSION_ERR";
            break;
        case SCP_CLIENT_STATE_SEQUENCE_ERR:
            result = "SCP_CLIENT_STATE_SEQUENCE_ERR";
            break;
        case SCP_CLIENT_STATE_SIZE_ERR:
            result = "SCP_CLIENT_STATE_SIZE_ERR";
            break;
        case SCP_CLIENT_STATE_INTERNAL_ERR:
            result = "SCP_CLIENT_STATE_INTERNAL_ERR";
            break;
        case SCP_CLIENT_STATE_SESSION_LIST:
            result = "SCP_CLIENT_STATE_SESSION_LIST";
            break;
        case SCP_CLIENT_STATE_LIST_OK:
            result = "SCP_CLIENT_STATE_LIST_OK";
            break;
        case SCP_CLIENT_STATE_RESEND_CREDENTIALS:
            result = "SCP_CLIENT_STATE_RESEND_CREDENTIALS";
            break;
        case SCP_CLIENT_STATE_CONNECTION_DENIED:
            result = "SCP_CLIENT_STATE_CONNECTION_DENIED";
            break;
        case SCP_CLIENT_STATE_PWD_CHANGE_REQ:
            result = "SCP_CLIENT_STATE_PWD_CHANGE_REQ";
            break;
        case SCP_CLIENT_STATE_RECONNECT_SINGLE:
            result = "SCP_CLIENT_STATE_RECONNECT_SINGLE";
            break;
        case SCP_CLIENT_STATE_SELECTION_CANCEL:
            result = "SCP_CLIENT_STATE_SELECTION_CANCEL";
            break;
        case SCP_CLIENT_STATE_END:
            result = "SCP_CLIENT_STATE_END";
            break;
    }

    return result;
}
