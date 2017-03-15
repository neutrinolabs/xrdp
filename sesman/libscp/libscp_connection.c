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

//extern struct log_config* s_log;

struct SCP_CONNECTION *
scp_connection_create(int sck)
{
    struct SCP_CONNECTION *conn;

    conn = g_new(struct SCP_CONNECTION, 1);

    if (0 == conn)
    {
        log_message(LOG_LEVEL_ERROR, "[connection:%d] connection create: malloc error", __LINE__);
        return 0;
    }

    conn->in_sck = sck;
    make_stream(conn->in_s);
    init_stream(conn->in_s, 8196);
    make_stream(conn->out_s);
    init_stream(conn->out_s, 8196);

    return conn;
}

void
scp_connection_destroy(struct SCP_CONNECTION *c)
{
    free_stream(c->in_s);
    free_stream(c->out_s);
    g_free(c);
}
