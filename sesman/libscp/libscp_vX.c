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

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_vXs_accept(struct trans *atrans, struct SCP_SESSION *s)
{
    struct stream *in_s;
    int version;

    in_s = atrans->in_s;
    if (!s_check_rem(in_s, 4))
    {
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    in_uint32_be(in_s, version);
    if (version == 0)
    {
        return scp_v0s_accept(atrans, s);
    }
    else if (version == 1)
    {
        return scp_v1s_accept(atrans, s);
    }
    return SCP_SERVER_STATE_VERSION_ERR;
}
