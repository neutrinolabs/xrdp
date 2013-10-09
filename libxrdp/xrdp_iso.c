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
xrdp_iso_recv_rdpnegreq(struct xrdp_iso *self, struct stream *s, int *requestedProtocol)
{
    int type;
    int flags;
    int len;

    *requestedProtocol = 0;

    DEBUG(("     in xrdp_iso_recv_rdpnegreq"));

    in_uint8(s, type);
    if (type != RDP_NEG_REQ)
    {
        DEBUG(("       xrdp_iso_recv_rdpnegreq: type: %x",type));
        return 1;
    }

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

    in_uint32_le(s, *requestedProtocol);

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
    int ver;  // TPKT Version
    int plen; // TPKT PacketLength

    *code = 0; // X.224 Packet Type
    *len = 0;  // X.224 Length Indicator

    if (xrdp_tcp_recv(self->tcp_layer, s, 4) != 0)
    {
        return 1;
    }

    in_uint8(s, ver);

    if (ver != 3)
    {
        return 1;
    }

    in_uint8s(s, 1);
    in_uint16_be(s, plen);

    if (xrdp_tcp_recv(self->tcp_layer, s, plen - 4) != 0)
    {
        return 1;
    }

    in_uint8(s, *len);
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
xrdp_iso_send_rdpnegrsp(struct xrdp_iso *self, struct stream *s, int code, int selectedProtocol)
{
    int send_rdpnegdata;

	if (xrdp_tcp_init(self->tcp_layer, s) != 0)
    {
        return 1;
    }

	//check for RDPNEGDATA
	send_rdpnegdata = 1;
	if (selectedProtocol == -1) {
		send_rdpnegdata = 0;
	}

    /* TPKT HEADER - 4 bytes */
    out_uint8(s, 3);	/* version */
    out_uint8(s, 0);	/* RESERVED */
    if (send_rdpnegdata == 1) {
        out_uint16_be(s, 19); /* length */
    }
    else
    {
        out_uint16_be(s, 11); /* length */
    }
    /* ISO LAYER - X.224  - 7 bytes*/
    if (send_rdpnegdata == 1) {
        out_uint8(s, 14); /* length */
    }
    else
    {
        out_uint8(s, 6); /* length */
    }
    out_uint8(s, code); /* SHOULD BE 0xD for CC */
    out_uint16_be(s, 0);
    out_uint16_be(s, 0x1234);
    out_uint8(s, 0);
    if (send_rdpnegdata == 1) {
        /* RDP_NEG_RSP - 8 bytes*/
    	out_uint8(s, RDP_NEG_RSP);
    	out_uint8(s, EXTENDED_CLIENT_DATA_SUPPORTED); /* flags */
    	out_uint16_le(s, 8); /* fixed length */
    	out_uint32_le(s, selectedProtocol); /* selected protocol */
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
xrdp_iso_send_rdpnegfailure(struct xrdp_iso *self, struct stream *s, int code, int failureCode)
{
    if (xrdp_tcp_init(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    /* TPKT HEADER - 4 bytes */
    out_uint8(s, 3);	/* version */
    out_uint8(s, 0);	/* RESERVED */
    out_uint16_be(s, 19); /* length */
    /* ISO LAYER - X.224  - 7 bytes*/
    out_uint8(s, 14); /* length */
    out_uint8(s, code); /* SHOULD BE 0xD for CC */
    out_uint16_be(s, 0);
    out_uint16_be(s, 0x1234);
    out_uint8(s, 0);
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
xrdp_iso_proccess_nego(struct xrdp_iso *self, struct stream *s, int requstedProtocol)
{
	//TODO: negotiation logic here.
	if (requstedProtocol != PROTOCOL_RDP) {
	    // Send RDP_NEG_FAILURE back to client
	    if (xrdp_iso_send_rdpnegfailure(self, s, ISO_PDU_CC, SSL_NOT_ALLOWED_BY_SERVER) != 0)
	    {
	        free_stream(s);
	        return 1;
	    }
	} else {
	    // Send RDP_NEG_RSP back to client
	    if (xrdp_iso_send_rdpnegrsp(self, s, ISO_PDU_CC, PROTOCOL_RDP) != 0)
	    {
	        free_stream(s);
	        return 1;
	    }
	}

    return 0;
}
/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_iso_incoming(struct xrdp_iso *self)
{
    int code;
    int len;
    int requestedProtocol;
    int selectedProtocol;
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);
    DEBUG(("   in xrdp_iso_incoming"));

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        DEBUG(("   in xrdp_iso_recv_msg error!!"));
        free_stream(s);
        return 1;
    }

    if (code != ISO_PDU_CR)
    {
        free_stream(s);
        return 1;
    }

    if (len > 6) {
        // Receive RDP_NEG_REQ data
        if (xrdp_iso_recv_rdpnegreq(self, s, &requestedProtocol) != 0)
        {
            free_stream(s);
            return 1;
        }
        // Process negotiation request, should return protocol type.
        if (xrdp_iso_proccess_nego(self, s, requestedProtocol) != 0)
        {
            free_stream(s);
            return 1;
        }
    }
    else if (len == 6) {
    	xrdp_iso_send_rdpnegrsp(self, s, ISO_PDU_CC, -1);
    }
    else {
    	DEBUG(("   error in xrdp_iso_incoming: unknown length detected"));
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
    len = s->end - s->p;
    out_uint8(s, 3);
    out_uint8(s, 0);
    out_uint16_be(s, len);
    out_uint8(s, 2);
    out_uint8(s, ISO_PDU_DT);
    out_uint8(s, 0x80);

    if (xrdp_tcp_send(self->tcp_layer, s) != 0)
    {
        return 1;
    }

    DEBUG(("   out xrdp_iso_send"));
    return 0;
}
