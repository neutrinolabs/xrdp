/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004 - 2012
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
 *
 * tcp layer
 */

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_tcp *APP_CC
xrdp_tcp_create(struct xrdp_iso *owner, struct trans *trans)
{
    struct xrdp_tcp *self;

    DEBUG(("    in xrdp_tcp_create"));
    self = (struct xrdp_tcp *)g_malloc(sizeof(struct xrdp_tcp), 1);
    self->iso_layer = owner;
    self->trans = trans;
    DEBUG(("    out xrdp_tcp_create"));
    return self;
}

/*****************************************************************************/
void APP_CC
xrdp_tcp_delete(struct xrdp_tcp *self)
{
    g_free(self);
}

/*****************************************************************************/
/* get out stream ready for data */
/* returns error */
int APP_CC
xrdp_tcp_init(struct xrdp_tcp *self, struct stream *s)
{
    init_stream(s, 8192);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_tcp_recv(struct xrdp_tcp *self, struct stream *s, int len)
{
    DEBUG(("    in xrdp_tcp_recv, gota get %d bytes", len));
    init_stream(s, len);

    if (trans_force_read_s(self->trans, s, len) != 0)
    {
        DEBUG(("    error in trans_force_read_s"));
        return 1;
    }

    DEBUG(("    out xrdp_tcp_recv"));
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_tcp_send(struct xrdp_tcp *self, struct stream *s)
{
    int len;
    len = s->end - s->data;
    DEBUG(("    in xrdp_tcp_send, gota send %d bytes", len));

    if (trans_force_write_s(self->trans, s) != 0)
    {
        DEBUG(("    error in trans_force_write_s"));
        return 1;
    }

    DEBUG(("    out xrdp_tcp_send, sent %d bytes ok", len));
    return 0;
}
