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
 * @file libscp_vX.c
 * @brief libscp version neutral code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_vX.h"

/* server API */
enum SCP_SERVER_STATES_E scp_vXs_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s)
{
    tui32 version;

    /* reading version and packet size */
    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version == 0)
    {
        return scp_v0s_accept(c, s, 1);
    }
    else if (version == 1)
    {
        return scp_v1s_accept(c, s, 1);
    }

    return SCP_SERVER_STATE_VERSION_ERR;
}
