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
 *
 * Note: [ITU-T X.224] and [ISO/IEC 8073] are essentially two specifications
 * of the same protocol (see [ITU-T X.224] Appendix I – Differences between
 * ITU-T Rec. X.224 (1993) and ISO/IEC 8073:1992). The RDP protocol
 * specification [MS-RDPBCGR] makes reference to the [ITU-T X.224] specificaiton.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"




/*****************************************************************************/
struct xrdp_iso *
xrdp_iso_create(struct xrdp_mcs *owner, struct trans *trans)
{
    struct xrdp_iso *self;

    self = (struct xrdp_iso *) g_malloc(sizeof(struct xrdp_iso), 1);
    self->mcs_layer = owner;
    self->trans = trans;
    return self;
}

/*****************************************************************************/
void
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
static int
xrdp_iso_negotiate_security(struct xrdp_iso *self)
{
    int rv = 0;
    struct xrdp_client_info *client_info = &(self->mcs_layer->sec_layer->rdp_layer->client_info);

    self->selectedProtocol = client_info->security_layer;

    switch (client_info->security_layer)
    {
        case PROTOCOL_RDP:
            break;
        case PROTOCOL_SSL:
            if (self->requestedProtocol & PROTOCOL_SSL)
            {
                if (!g_file_readable(client_info->certificate) ||
                        !g_file_readable(client_info->key_file))
                {
                    /* certificate or privkey is not readable */
                    LOG(LOG_LEVEL_ERROR, "Cannot accept TLS connections because "
                        "certificate or private key file is not readable. "
                        "certificate file: [%s], private key file: [%s]",
                        client_info->certificate, client_info->key_file);
                    self->failureCode = SSL_CERT_NOT_ON_SERVER;
                    rv = 1; /* error */
                }
                else
                {
                    self->selectedProtocol = PROTOCOL_SSL;
                }
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "Server requires TLS for security, "
                    "but the client did not request TLS.");
                self->failureCode = SSL_REQUIRED_BY_SERVER;
                rv = 1; /* error */
            }
            break;
        case PROTOCOL_HYBRID:
        case PROTOCOL_HYBRID_EX:
        default:
            if ((self->requestedProtocol & PROTOCOL_SSL) &&
                    g_file_readable(client_info->certificate) &&
                    g_file_readable(client_info->key_file))
            {
                /* that's a patch since we don't support CredSSP for now */
                self->selectedProtocol = PROTOCOL_SSL;
            }
            else
            {
                self->selectedProtocol = PROTOCOL_RDP;
            }
            break;
    }

    LOG(LOG_LEVEL_DEBUG, "Security layer: requested %d, selected %d",
        self->requestedProtocol, self->selectedProtocol);
    return rv;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] RDP_NEG_REQ message.
 * returns error
 */
static int
xrdp_iso_process_rdp_neg_req(struct xrdp_iso *self, struct stream *s)
{
    int flags;
    int len;

    if (!s_check_rem_and_log(s, 7, "Parsing [MS-RDPBCGR] RDP_NEG_REQ"))
    {
        return 1;
    }

    /* The type field has already been read to determine that this function
       should be called */
    in_uint8(s, flags); /* flags */
    if (flags != 0x0 && flags != 0x8 && flags != 0x1)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unsupported [MS-RDPBCGR] RDP_NEG_REQ flags: 0x%2.2x", flags);
        return 1;
    }

    in_uint16_le(s, len); /* length */
    if (len != 8)
    {
        LOG(LOG_LEVEL_ERROR,
            "Protocol error: [MS-RDPBCGR] RDP_NEG_REQ length must be 8, "
            "received %d", len);
        return 1;
    }

    in_uint32_le(s, self->requestedProtocol); /* requestedProtocols */

    /* TODO: why is requestedProtocols flag value bigger than 0xb invalid? */
    if (self->requestedProtocol > 0xb)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unknown requested protocol flag [MS-RDPBCGR] RDP_NEG_REQ, "
            "requestedProtocol 0x%8.8x", self->requestedProtocol);
        return 1;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received struct [MS-RDPBCGR] RDP_NEG_REQ "
              "flags 0x%2.2x, length 8, requestedProtocol 0x%8.8x",
              flags, self->requestedProtocol);

    return 0;
}
/*****************************************************************************
 * Reads an X.224 PDU (X.224 section 13) preceded by a T.123 TPKT
 * header (T.123 section 8)
 *
 * On entry, the TPKT header length field will have been inspected and used to
 * set up the input stream.
 *
 * On exit, the TPKT header and the fixed part of the PDU header will have been
 * removed from the stream.
 *
 * @param self
 * @param s [in]
 * @param code [out]
 * @param len [out]
 * Returns error
 *****************************************************************************/
static int
xrdp_iso_recv_msg(struct xrdp_iso *self, struct stream *s, int *code, int *len)
{
    int ver;

    *code = 0;
    *len = 0;

    if (s != self->trans->in_s)
    {
        LOG(LOG_LEVEL_WARNING,
            "Bug: the input stream is not the same stream as the "
            "transport input stream");
    }

    /* [ITU-T T.123] TPKT header is 4 bytes, then first 2 bytes of the X.224 CR-TPDU */
    if (!s_check_rem_and_log(s, 6,
                             "Parsing [ITU-T T.123] TPKT header and [ITU-T X.224] TPDU header"))
    {
        return 1;
    }

    /* [ITU-T T.123] TPKT header */
    in_uint8(s, ver); /* version */
    in_uint8s(s, 3); /* Skip reserved field (1 byte), plus length (2 bytes) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [ITU-T T.123] TPKT "
              "version %d, length (ignored)", ver);

    /* [ITU-T X.224] TPDU header */
    in_uint8(s, *len);  /* LI (length indicator) */
    in_uint8(s, *code); /* TPDU code */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [ITU-T X.224] TPDU "
              "length indicator %d, TDPU code 0x%2.2x", *len, *code);

    if (ver != 3)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unsupported [ITU-T T.123] TPKT header version: %d", ver);
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_ERROR, "[ITU-T T.123] TPKT header", s->data, 4);
        return 1;
    }

    if (*len == 255)
    {
        /* X.224 13.2.1 - reserved value */
        LOG(LOG_LEVEL_ERROR,
            "[ITU-T X.224] TPDU header: unsupported use of reserved length value");
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_ERROR, "[ITU-T X.224] TPDU header", s->data + 4, 4);
        return 1;
    }

    if (*code == ISO_PDU_DT)
    {
        /* Data PDU : X.224 13.7 class 0 */
        if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T X.224] DT-TPDU (Data) header"))
        {
            return 1;
        }
        in_uint8s(s, 1); /* EOT (End of TSDU Mark) (upper 1 bit) and
                            TPDU-NR (Data TPDU Number) (lower 7 bits) */
    }
    else
    {
        /* Other supported X.224 class 0 PDUs all have 5 bytes remaining
           in the fixed header :
            CR Connection request (13.3)
            CC Connection confirm (13.4)
            DR Disconnect request (13.5) */
        if (!s_check_rem_and_log(s, 5, "Parsing [ITU-T X.224] Other PDU header"))
        {
            return 1;
        }
        in_uint8s(s, 5); /* DST-REF (2 bytes)
                            SRC-REF (2 bytes)
                            [CR, CC] CLASS OPTION (1 byte) or [DR] REASON (1 byte) */
    }

    return 0;
}

/*****************************************************************************/
/* Process the header of a [ITU-T X.224] DT-TPDU (Data) message.
 *
 * returns error
 */
int
xrdp_iso_recv(struct xrdp_iso *self, struct stream *s)
{
    int code;
    int len;

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_iso_recv: xrdp_iso_recv_msg failed");
        return 1;
    }

    if (code != ISO_PDU_DT || len != 2)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_iso_recv only supports processing "
            "[ITU-T X.224] DT-TPDU (Data) headers. Received TPDU header: "
            "length indicator %d, TDPU code 0x%2.2x", len, code);
        return 1;
    }

    return 0;
}
/*****************************************************************************/
/*
 * Send a [ITU-T X.224] CC-TPDU (Connection Confirm) message with
 * [ITU-T T.123] TPKT header.
 *
 * returns error
 */
static int
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
    /* [ITU-T T.123] TPKT header */
    out_uint8(s, 3); /* version */
    out_uint8(s, 0); /* reserved (padding) */
    len_ptr = s->p;
    out_uint16_be(s, 0); /* length, set later */

    /* [ITU-T X.224] CC-TPDU */
    len_indicator_ptr = s->p;
    out_uint8(s, 0);          /* length indicator, set later */
    out_uint8(s, ISO_PDU_CC); /* Connection Confirm PDU */
    out_uint16_be(s, 0);      /* DST-REF */
    out_uint16_be(s, 0x1234); /* SRC-REF */
    out_uint8(s, 0);          /* CLASS OPTION */

    /* [MS-RDPBCGR] 2.2.1.2 rdpNegData */
    if (self->rdpNegData)
    {
        if (self->failureCode)
        {
            /* [MS-RDPBCGR] RDP_NEG_FAILURE */
            out_uint8(s, RDP_NEG_FAILURE);       /* type*/
            out_uint8(s, 0);                     /* flags (none) */
            out_uint16_le(s, 8);                 /* length (must be 8) */
            out_uint32_le(s, self->failureCode); /* failureCode */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding structure [MS-RDPBCGR] RDP_NEG_FAILURE "
                      "flags 0, length 8, failureCode 0x%8.8x", self->failureCode);
        }
        else
        {
            /* [MS-RDPBCGR] RDP_NEG_RSP */
            out_uint8(s, RDP_NEG_RSP);                    /* type*/
            //TODO: hardcoded flags
            out_uint8(s, EXTENDED_CLIENT_DATA_SUPPORTED); /* flags */
            out_uint16_le(s, 8);                          /* length (must be 8) */
            out_uint32_le(s, self->selectedProtocol);     /* selectedProtocol */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding structure [MS-RDPBCGR] RDP_NEG_RSP "
                      "flags 0, length 8, selectedProtocol 0x%8.8x",
                      self->selectedProtocol);
        }
    }
    s_mark_end(s);

    len = (int) (s->end - holdp);
    len_indicator = (int) (s->end - len_indicator_ptr) - 1;
    len_ptr[0] = len >> 8;
    len_ptr[1] = len;
    len_indicator_ptr[0] = len_indicator;

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [ITU-T T.123] TPKT "
              "version 3, length %d", len);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [ITU-T X.224] CC-TPDU (Connection Confirm) "
              "length indicator %d, DST-REF 0, SRC-REF 0, CLASS OPTION 0",
              len_indicator);

    if (trans_write_copy_s(self->trans, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [ITU-T X.224] CC-TPDU (Connection Confirm) failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}
/*****************************************************************************
 * Process an X.224 connection request PDU
 *
 * See MS-RDPBCGR v20190923 sections 2.2.1.1 and 3.3.5.3.1.
 *
 * From the latter, in particular:-
 * - The length embedded in the TPKT header MUST be examined for
 *   consistency with the received data. If there is a discrepancy, the
 *   connection SHOULD be dropped
 * - If the optional routingToken field exists it MUST be ignored.
 * - If the optional cookie field is present it MUST be ignored.
 * - If both the routingToken and cookie fields are present, the server
 *   SHOULD continue with the connection.
 *****************************************************************************/
/* returns error */
int
xrdp_iso_incoming(struct xrdp_iso *self)
{
    int rv = 0;
    int code;
    int len;
    int cc_type;
    struct stream *s;
    int expected_pdu_len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "[ITU-T X.224] Connection Sequence: receive connection request");
    s = libxrdp_force_read(self->trans);
    if (s == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "[ITU-T X.224] Connection Sequence: CR-TPDU (Connection Request) failed");
        return 1;
    }

    if (xrdp_iso_recv_msg(self, s, &code, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[ITU-T X.224] Connection Sequence: CR-TPDU (Connection Request) failed");
        return 1;
    }

    if (code != ISO_PDU_CR)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_iso_incoming only supports processing "
            "[ITU-T X.224] CR-TPDU (Connection Request) headers. "
            "Received TPDU header: length indicator %d, TDPU code 0x%2.2x",
            len, code);
        return 1;
    }

    /*
     * Make sure the length indicator field extracted from the X.224
     * connection request TPDU corresponds to the length in the TPKT header.
     *
     * We do this by seeing how the indicator field minus the counted
     * octets in the TPDU header (6) compares with the space left in
     * the stream.
     */
    expected_pdu_len = (s->end - s->p) + 6;
    if (len != expected_pdu_len)
    {
        LOG(LOG_LEVEL_ERROR,
            "Invalid length indicator in [ITU-T X.224] CR-TPDU (Connection Request). "
            "expected %d, received %d",
            expected_pdu_len, len);
        return 1;
    }

    /* process connection request [MS-RDPBCGR] 2.2.1.1 */
    while (s_check_rem(s, 1))
    {
        in_uint8(s, cc_type); /* type or 'C' */
        switch (cc_type)
        {
            default:
                LOG_DEVEL(LOG_LEVEL_WARNING,
                          "Ignoring unknown structure type in [ITU-T X.224] CR-TPDU (Connection Request). "
                          "type 0x%2.2x", cc_type);
                break;
            case RDP_NEG_REQ: /* rdpNegReq 1 */
                self->rdpNegData = 1;
                if (xrdp_iso_process_rdp_neg_req(self, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "[ITU-T X.224] Connection Sequence: failed");
                    return 1;
                }
                break;
            case RDP_CORRELATION_INFO: /* rdpCorrelationInfo 6 */
                // TODO
                if (!s_check_rem_and_log(s, 1 + 2 + 16 + 16,
                                         "Parsing [MS-RDPBCGR] RDP_NEG_CORRELATION_INFO"))
                {
                    return 1;
                }

                in_uint8s(s, 1 + 2 + 16 + 16);
                LOG_DEVEL(LOG_LEVEL_TRACE,
                          "Received struct [MS-RDPBCGR] RDP_NEG_CORRELATION_INFO "
                          "(all fields ignored)");
                break;
            case 'C': /* Cookie or routingToken */
                /* The routingToken and cookie fields are both ASCII
                 * strings starting with the word 'Cookie: ' and
                 * ending with CR+LF. We ignore both, so we do
                 * not need to distinguish them  */
                while (s_check_rem(s, 1))
                {
                    in_uint8(s, cc_type);
                    if (cc_type == 0x0D && s_check_rem(s, 1))
                    {
                        in_uint8(s, cc_type);
                        if (cc_type == 0x0A)
                        {
                            break;
                        }
                    }
                }
                LOG_DEVEL(LOG_LEVEL_TRACE,
                          "Received struct [MS-RDPBCGR] routingToken or cookie "
                          "(ignored)");
                break;
        }
    }

    /* negotiate client-server security layer */
    rv = xrdp_iso_negotiate_security(self);

    /* send connection confirm back to client */
    LOG_DEVEL(LOG_LEVEL_DEBUG, "[ITU-T X.224] Connection Sequence: send connection confirmation");
    if (xrdp_iso_send_cc(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[ITU-T X.224] Connection Sequence: send connection confirmation failed");
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "[ITU-T X.224] Connection Sequence: completed");
    return rv;
}

/*****************************************************************************/
/* returns error */
int
xrdp_iso_init(struct xrdp_iso *self, struct stream *s)
{
    init_stream(s, 8192 * 4); /* 32 KB */
    s_push_layer(s, iso_hdr, 7);
    return 0;
}

/*****************************************************************************/
/* Sends a message with the [ITU-T T.123] TPKT header (T.123 section 8) and
 * [ITU-T X.224] DT-TPDU (Data) header (X.224 section 13)
 * returns error
 */
int
xrdp_iso_send(struct xrdp_iso *self, struct stream *s)
{
    int len;

    s_pop_layer(s, iso_hdr);
    len = (int) (s->end - s->p);
    /* [ITU-T T.123] TPKT header */
    out_uint8(s, 3);       /* version */
    out_uint8(s, 0);       /* reserved (padding) */
    out_uint16_be(s, len); /* length */

    /* [ITU-T X.224] DT-TPDU (Data) header */
    out_uint8(s, 2);          /* LI (length indicator) */
    out_uint8(s, ISO_PDU_DT); /* TPDU code */
    out_uint8(s, 0x80);       /* EOT (End of TSDU Mark) (upper 1 bit) and
                                 TPDU-NR (Data TPDU Number) (lower 7 bits) */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [ITU-T T.123] TPKT "
              "version 3, length %d", len);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [ITU-T X.224] DT-TPDU (Data) "
              "length indicator 2, TPDU code 0x%2.2x, EOT 1, TPDU-NR 0x00",
              ISO_PDU_DT);

    if (trans_write_copy_s(self->trans, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_iso_send: trans_write_copy_s failed");
        return 1;
    }

    return 0;
}
