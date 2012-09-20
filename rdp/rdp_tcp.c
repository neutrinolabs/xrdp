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
 *
 * librdp tcp layer
 */

#include "rdp.h"

/*****************************************************************************/
struct rdp_tcp *APP_CC
rdp_tcp_create(struct rdp_iso *owner)
{
    struct rdp_tcp *self;

    self = (struct rdp_tcp *)g_malloc(sizeof(struct rdp_tcp), 1);
    self->iso_layer = owner;
    return self;
}

/*****************************************************************************/
void APP_CC
rdp_tcp_delete(struct rdp_tcp *self)
{
    g_free(self);
}

/*****************************************************************************/
/* get out stream ready for data */
/* returns error */
int APP_CC
rdp_tcp_init(struct rdp_tcp *self, struct stream *s)
{
    init_stream(s, 8192);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_tcp_recv(struct rdp_tcp *self, struct stream *s, int len)
{
    int rcvd;

    DEBUG(("    in rdp_tcp_recv gota get %d bytes on sck %d",
           len, self->sck));

    if (self->sck_closed)
    {
        DEBUG(("    out rdp_tcp_recv error sck closed"));
        return 1;
    }

    init_stream(s, len);

    while (len > 0)
    {
        rcvd = g_tcp_recv(self->sck, s->end, len, 0);

        if (rcvd == -1)
        {
            if (g_tcp_last_error_would_block(self->sck))
            {
                g_tcp_can_recv(self->sck, 10);
            }
            else
            {
                self->sck_closed = 1;
                DEBUG(("    out rdp_tcp_recv error unknown"));
                return 1;
            }
        }
        else if (rcvd == 0)
        {
            self->sck_closed = 1;
            DEBUG(("    out rdp_tcp_recv error connection dropped"));
            return 1;
        }
        else
        {
            s->end += rcvd;
            len -= rcvd;
        }
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_tcp_send(struct rdp_tcp *self, struct stream *s)
{
    int len;
    int total;
    int sent;

    if (self->sck_closed)
    {
        DEBUG(("    out rdp_tcp_send error sck closed"));
        return 1;
    }

    len = s->end - s->data;
    DEBUG(("    in rdp_tcp_send gota send %d bytes on sck %d", len,
           self->sck));
    total = 0;

    while (total < len)
    {
        sent = g_tcp_send(self->sck, s->data + total, len - total, 0);

        if (sent == -1)
        {
            if (g_tcp_last_error_would_block(self->sck))
            {
                g_tcp_can_send(self->sck, 10);
            }
            else
            {
                self->sck_closed = 1;
                DEBUG(("    out rdp_tcp_send error unknown"));
                return 1;
            }
        }
        else if (sent == 0)
        {
            self->sck_closed = 1;
            DEBUG(("    out rdp_tcp_send error connection dropped"));
            return 1;
        }
        else
        {
            total = total + sent;
        }
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_tcp_connect(struct rdp_tcp *self, char *ip, char *port)
{
    DEBUG(("    in rdp_tcp_connect ip %s port %s", ip, port));
    self->sck = g_tcp_socket();

    if (g_tcp_connect(self->sck, ip, port) == 0)
    {
        g_tcp_set_non_blocking(self->sck);
    }
    else
    {
        DEBUG(("    out rdp_tcp_connect error g_tcp_connect failed"));
        return 1;
    }

    DEBUG(("    out rdp_tcp_connect"));
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_tcp_disconnect(struct rdp_tcp *self)
{
    if (self->sck != 0)
    {
        g_tcp_close(self->sck);
    }

    self->sck = 0;
    return 0;
}
