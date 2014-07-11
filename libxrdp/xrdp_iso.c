/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 * Copyright (C) Idan Freiberg 2013-2014
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
 * iso layer
 */

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_iso *
APP_CC
xrdp_iso_create(struct xrdp_mcs *owner, struct trans *trans)
{
    struct xrdp_iso *self;

    DEBUG(("   in xrdp_iso_create"));
    self = (struct xrdp_iso *) g_malloc(sizeof(struct xrdp_iso), 1);
    self->mcs_layer = owner;
    self->trans = trans;
    DEBUG(("   out xrdp_iso_create"));
    return self;
}

/*****************************************************************************/
void APP_CC
xrdp_iso_delete(struct xrdp_iso *self)
{
    if (self == 0)
    {
        return;
    }

    g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_iso_process_rdpNegReq(struct xrdp_iso *self, struct stream *s)
{
    int flags;
    int len;

    DEBUG(("     in xrdp_iso_process_neg_req"));

    in_uint8(s, flags);
    if (flags != 0x0 && flags != 0x8 && flags != 0x1)
    {
        DEBUG(("xrdp_iso_process_rdpNegReq: error, flags: %x",flags));
        return 1;
    }

    in_uint16_le(s, len);
    if (len != 8)
    {
        DEBUG(("xrdp_iso_process_rdpNegReq: error, length: %x",len));
        return 1;
    }

    in_uint32_le(s, self->requestedProtocol);
    if (self->requestedProtocol > 0xb)
    {
        DEBUG(("xrdp_iso_process_rdpNegReq: error, requestedProtocol: %x",
                        self->requestedProtocol));
        return 1;
    }

    DEBUG(("     out xrdp_iso_process_rdpNegReq"));
    return 0;
}
/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_iso_recv_msg(struct xrdp_iso *self, struct stream *s, int *code, int *len)
{
    int ver; // tpkt ver
    int plen; // tpkt len

    *code = 0; // x.244 type
    *len = 0; // X.224 len indicator

    if (s != self->trans->in_s)
    {
        g_writeln("xrdp_iso_recv_msg error logic");
    }

    in_uint8(s, ver);

    if (ver != 3)
    {
        g_writeln("xrdp_iso_recv_msg: bad ver");
        g_hexdump(s->data, 4);
        return 1;
    }

    in_uint8s(s, 1);
    in_uint16_be(s, plen);

    if (plen < 4)
    {
        return 1;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }

    in_uint8(s, *len);
    in_uint8(s, *code);

    if (*code == ISO_PDU_DT)
    {
        if (!s_check_rem(s, 1))
        {
            return 1;
        }
        in_uint8s(s, 1);
    }
    else
    {
        if (!s_check_rem(s, 5))
        {
            return 1;
        }
        in_uint8s(s, 5);
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_recv(struct xrdp_iso *self, struct stream *s)
{
    int code;
    int len;

    DEBUG(("   in xrdp_iso_recv"));

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        DEBUG(("   out xrdp_iso_recv xrdp_iso_recv_msg return non zero"));
        return 1;
    }

    if (code != ISO_PDU_DT || len != 2)
    {
        DEBUG(("   out xrdp_iso_recv code != ISO_PDU_DT or length != 2"));
        return 1;
    }

    DEBUG(("   out xrdp_iso_recv"));
    return 0;
}
/*****************************************************************************/
static int APP_CC
xrdp_iso_send_cc(struct xrdp_iso *self)
{
    struct stream *s;
    char *holdp;
    char *len_ptr;
    char *len_indicator_ptr;
    int len;
    int len_indicator;

    make_stream(s);
    init_stream(s, 8192);

    holdp = s->p;
    /* tpkt */
    out_uint8(s, 3); /* version */
    out_uint8(s, 0); /* pad */
    len_ptr = s->p;
    out_uint16_be(s, 0); /* length, set later */
    /* iso */
    len_indicator_ptr = s->p;
    out_uint8(s, 0); /* length indicator, set later */
    out_uint8(s, ISO_PDU_CC); /* Connection Confirm PDU */
    out_uint16_be(s, 0);
    out_uint16_be(s, 0x1234);
    out_uint8(s, 0);
    /* rdpNegData */
    if (self->rdpNegData)
    {
        if (self->failureCode)
        {
            out_uint8(s, RDP_NEG_FAILURE);
            out_uint8(s, 0); /* no flags */
            out_uint16_le(s, 8); /* must be 8 */
            out_uint32_le(s, self->failureCode); /* failure code */
        }
        else
        {
            out_uint8(s, RDP_NEG_RSP);
            //TODO: hardcoded flags
            out_uint8(s, EXTENDED_CLIENT_DATA_SUPPORTED); /* flags */
            out_uint16_le(s, 8); /* must be 8 */
            out_uint32_le(s, self->selectedProtocol); /* selected protocol */
        }
    }

    s_mark_end(s);

    len = (int) (s->end - holdp);
    len_indicator = (int) (s->end - len_indicator_ptr) - 1;
    len_ptr[0] = len << 8;
    len_ptr[1] = len;
    len_indicator_ptr[0] = len_indicator;

    if (trans_force_write_s(self->trans, s) != 0)
    {
        return 1;
    }

    free_stream(s);
    return 0;
}
/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_incoming(struct xrdp_iso *self)
{
    int code;
    int len;
    int cookie_index;
    int cc_type;
    char text[256];
    char *pend;
    struct stream *s;

    DEBUG(("   in xrdp_iso_incoming"));

    s = libxrdp_force_read(self->trans);
    if (s == 0)
    {
        return 1;
    }

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        g_writeln("xrdp_iso_incoming: xrdp_iso_recv_msg returned non zero");
        return 1;
    }

    if ((code != ISO_PDU_CR) || (len < 6))
    {
        return 1;
    }

    /* process connection request */
    pend = s->p + (len - 6);
    cookie_index = 0;
    while (s->p < pend)
    {
        in_uint8(s, cc_type);
        switch (cc_type)
        {
            default:
                break;
            case RDP_NEG_REQ: /* rdpNegReq 1 */
                self->rdpNegData = 1;
                if (xrdp_iso_process_rdpNegReq(self, s) != 0)
                {
                    g_writeln(
                            "xrdp_iso_incoming: xrdp_iso_process_rdpNegReq returned non zero");
                    return 1;
                }
                break;
            case RDP_CORRELATION_INFO: /* rdpCorrelationInfo 6 */
                // TODO
                in_uint8s(s, 1 + 2 + 16 + 16);
                break;
            case 'C': /* Cookie routingToken */
                while (s->p < pend)
                {
                    text[cookie_index] = cc_type;
                    cookie_index++;
                    if ((s->p[0] == 0x0D) && (s->p[1] == 0x0A))
                    {
                        in_uint8s(s, 2);
                        text[cookie_index] = 0;
                        cookie_index = 0;
                        break;
                    }
                    in_uint8(s, cc_type);
                }
                break;
        }
    }

    /* security layer negotiation */
    if (self->rdpNegData)
    {
        int
                serverSecurityLayer =
                        self->mcs_layer->sec_layer->rdp_layer->client_info.security_layer;
        self->selectedProtocol = PROTOCOL_RDP; /* set default security layer */

        switch (serverSecurityLayer)
        {
            case (PROTOCOL_SSL | PROTOCOL_HYBRID | PROTOCOL_HYBRID_EX):
                /* server supports tls+hybrid+hybrid_ex */
                if (self->requestedProtocol == (PROTOCOL_SSL | PROTOCOL_HYBRID
                        | PROTOCOL_HYBRID_EX))
                {
                    /* client supports tls+hybrid+hybrid_ex */
                    self->selectedProtocol = PROTOCOL_SSL; //TODO: change
                }
                else
                {
                    self->failureCode = SSL_WITH_USER_AUTH_REQUIRED_BY_SERVER;
                }
                break;
            case (PROTOCOL_SSL | PROTOCOL_HYBRID):
                /* server supports tls+hybrid */
                if (self->requestedProtocol == (PROTOCOL_SSL | PROTOCOL_HYBRID))
                {
                    /* client supports tls+hybrid */
                    self->selectedProtocol = PROTOCOL_SSL; //TODO: change
                }
                else
                {
                    self->failureCode = HYBRID_REQUIRED_BY_SERVER;
                }
                break;
            case PROTOCOL_SSL:
                /* server supports tls */
                if (self->requestedProtocol & PROTOCOL_SSL) //TODO
                {
                    /* client supports tls */
                    self->selectedProtocol = PROTOCOL_SSL;
                }
                else
                {
                    self->failureCode = SSL_REQUIRED_BY_SERVER;
                }
                break;
            case PROTOCOL_RDP:
                /* server supports rdp */
                if (self->requestedProtocol == PROTOCOL_RDP)
                {
                    /* client supports rdp */
                    self->selectedProtocol = PROTOCOL_RDP;
                }
                else
                {
                    self->failureCode = SSL_NOT_ALLOWED_BY_SERVER;
                }
                break;
            default:
                /* unsupported protocol */
                g_writeln("xrdp_iso_incoming: unsupported protocol %d",
                        self->requestedProtocol);
                self->failureCode = INCONSISTENT_FLAGS; //TODO: ?
        }
    }

    /* send connection confirm back to client */
    if (xrdp_iso_send_cc(self) != 0)
    {
        g_writeln("xrdp_iso_incoming: xrdp_iso_send_cc returned non zero");
        return 1;
    }

    DEBUG(("   out xrdp_iso_incoming"));
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_init(struct xrdp_iso *self, struct stream *s)
{
    init_stream(s, 8192 * 4); /* 32 KB */
    s_push_layer(s, iso_hdr, 7);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_send(struct xrdp_iso *self, struct stream *s)
{
    int len;

    DEBUG(("   in xrdp_iso_send"));
    s_pop_layer(s, iso_hdr);
    len = (int) (s->end - s->p);
    out_uint8(s, 3);
    out_uint8(s, 0);
    out_uint16_be(s, len);
    out_uint8(s, 2);
    out_uint8(s, ISO_PDU_DT);
    out_uint8(s, 0x80);

    if (trans_force_write_s(self->trans, s) != 0)
    {
        return 1;
    }

    DEBUG(("   out xrdp_iso_send"));
    return 0;
}
