/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
 * Copyright (C) Idan Freiberg 2013
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
struct xrdp_iso *APP_CC
xrdp_iso_create(struct xrdp_mcs *owner, struct trans *trans)
{
    struct xrdp_iso *self;

    DEBUG(("   in xrdp_iso_create"));
    self = (struct xrdp_iso *)g_malloc(sizeof(struct xrdp_iso), 1);
    self->mcs_layer = owner;
    self->tcp_layer = xrdp_tcp_create(self, trans);
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

    xrdp_tcp_delete(self->tcp_layer);
    g_free(self);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_iso_recv_rdpnegreq(struct xrdp_iso *self, struct stream *s)
{
    int flags;
    int len;

    DEBUG(("     in xrdp_iso_recv_rdpnegreq"));

    in_uint8(s, flags);
    if (flags != 0x0)
    {
        DEBUG(("       xrdp_iso_recv_rdpnegreq: flags: %x",flags));
        return 1;
    }

    in_uint16_le(s, len);
    if (len != 8) // fixed length
    {
        DEBUG(("       xrdp_iso_recv_rdpnegreq: length: %x",len));
        return 1;
    }

    in_uint32_le(s, self->requestedProtocol);

    //TODO: think of protocol verification logic
//    if (requestedProtocol != PROTOCOL_RDP || PROTOCOL_SSL || PROTOCOL_HYBRID || PROTOCOL_HYBRID_EX)
//    {
//        DEBUG(("       xrdp_iso_recv_rdpnegreq: wrong requestedProtocol: %x",requestedProtocol));
//        return 1;
//    }

    DEBUG(("     out xrdp_iso_recv_rdpnegreq"));
    return 0;
}
/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_iso_recv_msg(struct xrdp_iso *self, struct stream *s, int *code, int *len)
{
    int plen; // PacketLength

    *code = 0; // X.224 Packet Type
    *len = 0;  // X.224 Length Indicator

    plen = xrdp_iso_recv_tpkt_header(self, s);
    if (plen == 1)
    {
        DEBUG(("     xrdp_iso_recv_msg: error in tpkt header reading"));
        return 1;
    }

    // receive the left bytes
    if (xrdp_tcp_recv(self->tcp_layer, s, plen - 4) != 0)
    {
        return 1;
    }

    xrdp_iso_read_x224_header(s, code, len);

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
/* returns error */
int APP_CC
xrdp_iso_detect_tpkt(struct xrdp_iso *self, struct stream *s)
{
    int ver;

    DEBUG(("    in xrdp_iso_detect_tpkt"));
    if (xrdp_tcp_recv(self->tcp_layer, s, 1) != 0)
    {
       return 1;
    }

    in_uint8_peek(s, ver);
    g_writeln("tpkt version: %x", ver);

    if (ver != 3)
    {
      return 1;
    }

    DEBUG(("    out xrdp_iso_detect_tpkt"));
    return 0;
}
/*****************************************************************************/
/* returns packet length or error (1) */
int APP_CC
xrdp_iso_recv_tpkt_header(struct xrdp_iso *self, struct stream *s)
{
    int plen;

    DEBUG(("      in xrdp_iso_recv_tpkt_header"));

    if (xrdp_tcp_recv(self->tcp_layer, s, 3) != 0)
    {
       return 1;
    }

    in_uint8s(s, 1);
    in_uint16_be(s, plen);

    if (plen < 4)
    {
        return 1; // tpkt must be >= 4 bytes length
    }

    DEBUG(("      out xrdp_iso_recv_tpkt_header"));

    return plen;
}
/*****************************************************************************/
void APP_CC
xrdp_iso_write_tpkt_header(struct stream *s, int len)
{
    /* TPKT HEADER - 4 bytes */
    out_uint8(s, 3);    /* version */
    out_uint8(s, 0);    /* RESERVED */
    out_uint16_be(s, len); /* length */
}
/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_read_x224_header(struct stream *s, int *code, int *len)
{
    DEBUG(("      in xrdp_iso_read_x224_header"));
    if (!s_check_rem(s, 2))
    {
        return 1;
    }

    in_uint8(s, *len); /* length */
    in_uint8(s, *code); /* code */

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
    DEBUG(("      out xrdp_iso_read_x224_header"));

    return 0;
}
/*****************************************************************************/
void APP_CC
xrdp_iso_write_x224_header(struct stream *s, int len, int code)
{
    /* ISO LAYER - X.224  - 7 bytes*/
    out_uint8(s, len); /* length */
    out_uint8(s, code); /* code */

    if (code == ISO_PDU_DT)
    {
            out_uint8(s, 0x80);
    } else {
            out_uint16_be(s, 0);
            out_uint16_be(s, 0x1234);
            out_uint8(s, 0);
    }
}
/*****************************************************************************/
static int APP_CC
xrdp_iso_send_rdpnegrsp(struct xrdp_iso *self, struct stream *s)
{
    if (xrdp_tcp_init(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    // Write TPKT Header
    if (self->selectedProtocol != -1)
    {
        //rdp negotiation happens.
        xrdp_iso_write_tpkt_header(s, 19);
    }
    else
    {
       //rdp negotiation doesn't happen.
        xrdp_iso_write_tpkt_header(s, 11);
    }

    // Write x224 header
    if (self->selectedProtocol != -1)
    {
        xrdp_iso_write_x224_header(s, 14, ISO_PDU_CC);
    }
    else
    {
        xrdp_iso_write_x224_header(s, 6, ISO_PDU_CC);
    }

    /* RDP_NEG */
    if (self->selectedProtocol != -1)
    {
        /* RDP_NEG_RSP - 8 bytes*/
        out_uint8(s, RDP_NEG_RSP);
        out_uint8(s, EXTENDED_CLIENT_DATA_SUPPORTED); /* flags */
        out_uint16_le(s, 8); /* fixed length */
        out_uint32_le(s, self->selectedProtocol); /* selected protocol */
    }

    s_mark_end(s);

    if (xrdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    return 0;
}
/*****************************************************************************/
static int APP_CC
xrdp_iso_send_rdpnegfailure(struct xrdp_iso *self, struct stream *s, int failureCode)
{
    if (xrdp_tcp_init(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    xrdp_iso_write_tpkt_header(s, 19);

    xrdp_iso_write_x224_header(s, 14, ISO_PDU_CC);

    /* RDP_NEG_FAILURE - 8 bytes*/
    out_uint8(s, RDP_NEG_FAILURE);
    out_uint8(s, 0); /* no flags available */
    out_uint16_le(s, 8); /* fixed length */
    out_uint32_le(s, failureCode); /* failure code */
    s_mark_end(s);

    if (xrdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_iso_send_nego(struct xrdp_iso *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    //TODO: negotiation logic here.
    if (self->requestedProtocol != PROTOCOL_RDP)
    {
        // Send RDP_NEG_FAILURE back to client
        if (xrdp_iso_send_rdpnegfailure(self, s, SSL_NOT_ALLOWED_BY_SERVER) != 0)
        {
            free_stream(s);
            return 1;
        }
    }
    else
    {
        self->selectedProtocol = PROTOCOL_RDP;
        // Send RDP_NEG_RSP back to client
        if (xrdp_iso_send_rdpnegrsp(self, s) != 0)
        {
            free_stream(s);
            return 1;
        }
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

    make_stream(s);
    init_stream(s, 8192);
    DEBUG(("   in xrdp_iso_incoming"));

    if (xrdp_iso_detect_tpkt(self, s) != 0)
    {
        g_writeln("xrdp_iso_incoming: TPKT not detected");
        free_stream(s);
        return 1;
    }

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        DEBUG(("   in xrdp_iso_recv_msg error!!"));
        free_stream(s);
        return 1;
    }

    if ((code != ISO_PDU_CR) || (len < 6))
    {
        DEBUG(("   in xrdp_iso_recv_msg error: non iso_pdu_cr msg"));
        free_stream(s);
        return 1;
    }

    self->selectedProtocol = -1;
    self->requestedProtocol = PROTOCOL_RDP;

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
                if (xrdp_iso_recv_rdpnegreq(self, s) != 0)
                {
                    free_stream(s);
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

    if (xrdp_iso_send_nego(self) != 0)
    {
        free_stream(s);
        return 1;
    }

    DEBUG(("   out xrdp_iso_incoming"));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_init(struct xrdp_iso *self, struct stream *s)
{
    xrdp_tcp_init(self->tcp_layer, s);
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
    len = (int)(s->end - s->p);

    xrdp_iso_write_tpkt_header(s, len);
    xrdp_iso_write_x224_header(s, 2, ISO_PDU_DT);

    if (xrdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    DEBUG(("   out xrdp_iso_send"));
    return 0;
}
