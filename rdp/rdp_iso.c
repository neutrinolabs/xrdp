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
 * librdp iso layer
 */

#include "rdp.h"

/*****************************************************************************/
struct rdp_iso *APP_CC
rdp_iso_create(struct rdp_mcs *owner)
{
    struct rdp_iso *self;

    self = (struct rdp_iso *)g_malloc(sizeof(struct rdp_iso), 1);
    self->mcs_layer = owner;
    self->tcp_layer = rdp_tcp_create(self);
    return self;
}

/*****************************************************************************/
void APP_CC
rdp_iso_delete(struct rdp_iso *self)
{
    if (self == 0)
    {
        return;
    }

    rdp_tcp_delete(self->tcp_layer);
    g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
rdp_iso_recv_msg(struct rdp_iso *self, struct stream *s, int *code)
{
    int ver;
    int len;

    *code = 0;

    if (rdp_tcp_recv(self->tcp_layer, s, 4) != 0)
    {
        DEBUG(("   out rdp_iso_recv_msg error rdp_tcp_recv 1 failed"));
        return 1;
    }

    in_uint8(s, ver);

    if (ver != 3)
    {
        DEBUG(("   out rdp_iso_recv_msg error ver != 3"));
        return 1;
    }

    in_uint8s(s, 1);
    in_uint16_be(s, len);

    if (rdp_tcp_recv(self->tcp_layer, s, len - 4) != 0)
    {
        DEBUG(("   out rdp_iso_recv_msg error rdp_tcp_recv 2 failed"));
        return 1;
    }

    in_uint8s(s, 1);
    in_uint8(s, *code);

    if (*code == ISO_PDU_DT)
    {
        in_uint8s(s, 1);
    }
    else
    {
        in_uint8s(s, 5);
    }

    return 0;
}

/*****************************************************************************/
static int APP_CC
rdp_iso_send_msg(struct rdp_iso *self, struct stream *s, int code)
{
    if (rdp_tcp_init(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    out_uint8(s, 3);
    out_uint8(s, 0);
    out_uint16_be(s, 11); /* length */
    out_uint8(s, 6);
    out_uint8(s, code);
    out_uint16_le(s, 0);
    out_uint16_le(s, 0);
    out_uint8(s, 0);
    s_mark_end(s);

    if (rdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_iso_recv(struct rdp_iso *self, struct stream *s)
{
    int code;

    if (rdp_iso_recv_msg(self, s, &code) != 0)
    {
        return 1;
    }

    if (code != ISO_PDU_DT)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_iso_init(struct rdp_iso *self, struct stream *s)
{
    rdp_tcp_init(self->tcp_layer, s);
    s_push_layer(s, iso_hdr, 7);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_iso_send(struct rdp_iso *self, struct stream *s)
{
    int len;

    s_pop_layer(s, iso_hdr);
    len = s->end - s->p;
    out_uint8(s, 3);
    out_uint8(s, 0);
    out_uint16_be(s, len);
    out_uint8(s, 2);
    out_uint8(s, ISO_PDU_DT);
    out_uint8(s, 0x80);

    if (rdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_iso_connect(struct rdp_iso *self, char *ip, char *port)
{
    int code;
    struct stream *s;

    DEBUG(("   in rdp_iso_connect"));
    make_stream(s);
    init_stream(s, 8192);

    if (rdp_tcp_connect(self->tcp_layer, ip, port) != 0)
    {
        free_stream(s);
        DEBUG(("   out rdp_iso_connect error rdp_tcp_connect failed"));
        return 1;
    }

    if (rdp_iso_send_msg(self, s, ISO_PDU_CR) != 0)
    {
        free_stream(s);
        rdp_tcp_disconnect(self->tcp_layer);
        DEBUG(("   out rdp_iso_connect error rdp_iso_send_msg failed"));
        return 1;
    }

    init_stream(s, 8192);

    if (rdp_iso_recv_msg(self, s, &code) != 0)
    {
        free_stream(s);
        rdp_tcp_disconnect(self->tcp_layer);
        DEBUG(("   out rdp_iso_connect error rdp_iso_recv_msg failed"));
        return 1;
    }

    if (code != ISO_PDU_CC)
    {
        free_stream(s);
        rdp_tcp_disconnect(self->tcp_layer);
        DEBUG(("   out rdp_iso_connect error code != ISO_PDU_CC"));
        return 1;
    }

    free_stream(s);
    DEBUG(("   out rdp_iso_connect"));
    return 0;
}

/*****************************************************************************/
int APP_CC
rdp_iso_disconnect(struct rdp_iso *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    rdp_iso_send_msg(self, s, ISO_PDU_DR);
    rdp_tcp_disconnect(self->tcp_layer);
    free_stream(s);
    return 0;
}
