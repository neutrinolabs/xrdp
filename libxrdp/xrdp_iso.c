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
#include "string_calls.h"
#include "log.h"


/*****************************************************************************/
/**
 * Converts a protocol mask ([MS-RDPBCGR] 2.2.1.1.1 to a string)
 *
 * @param protocol Protocol mask
 * @param buff Output buffer
 * @param bufflen total length of buff
 * @return As for snprintf()
 *
 * The string "RDP" is always added to the output, even if other bits
 * are set
 */
static int
protocol_mask_to_str(int protocol, char *buff, int bufflen)
{
    char delim = '|';

    static const struct bitmask_string bits[] =
    {
        { PROTOCOL_SSL, "SSL" },
        { PROTOCOL_HYBRID, "HYBRID" },
        { PROTOCOL_RDSTLS, "RDSTLS" },
        { PROTOCOL_HYBRID_EX, "HYBRID_EX"},
        BITMASK_STRING_END_OF_LIST
    };

    return g_bitmask_to_str(protocol, bits, delim, buff, bufflen);
}

/*****************************************************************************/
struct xrdp_iso *
xrdp_iso_create(struct xrdp_mcs *owner, struct trans *trans)
{
    struct xrdp_iso *self;

    self = (struct xrdp_iso *) g_malloc(sizeof(struct xrdp_iso), 1);
    self->mcs_layer = owner;
    self->trans = trans;

    // See if we're running in vmconnect mode on this connection
    struct xrdp_client_info *client_info = &(self->mcs_layer->sec_layer->rdp_layer->client_info);
    if (client_info->vmconnect && trans->mode != TRANS_MODE_VSOCK)
    {
        char desc[MAX_PEER_DESCSTRLEN];
        g_sck_get_peer_description(trans->sck, desc, sizeof(desc));
        LOG(LOG_LEVEL_INFO, "Disabling vmconnect mode for connection from %s",
            desc);
        client_info->vmconnect = 0;
    }
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
    char protostr[64];
    int got_protocol = 0;
    int security_type_mask;

    /* Map the configuration from xrdp.ini to a mask of allowed
     * security types ([MS-RDPBCGR] 2.2.1.2.1)
     *
     * There's some oddness around PROTOCOL_RDP. This value is 0,
     * for compatibility reasons, and it's OK for the server to
     * suggest RDP as the fallback protocol if nothing else is
     * agreed on. Nowadays, classic RDP security should
     * not be used, if at all avoidable */

    /* At present we only support SSL and RDP security */
    if (client_info->security_layer == SECURITY_LAYER_RDP)
    {
        security_type_mask = PROTOCOL_RDP;
    }
    else
    {
        security_type_mask = PROTOCOL_SSL;
    }
    /* But VMConnect mode supports everything. */
    if (client_info->vmconnect)
    {
        security_type_mask |= PROTOCOL_HYBRID | PROTOCOL_HYBRID_EX;
    }

    /* Logically 'and' this value with the mask requested by the client, and
     * see what's left */
    protocol_mask_to_str(self->requestedProtocol, protostr, sizeof(protostr));
    LOG(LOG_LEVEL_INFO, "Client requested security types (RDP assumed) : %s",
        protostr);
    security_type_mask &= self->requestedProtocol;

    if (security_type_mask & PROTOCOL_HYBRID_EX)
    {
        /* Currently supported by VMConnect mode only */
        LOG(LOG_LEVEL_INFO, "Selected HYBRID_EX security");
        self->selectedProtocol = PROTOCOL_HYBRID_EX;
        got_protocol = 1;
    }
    else if (security_type_mask & PROTOCOL_HYBRID)
    {
        /* Currently supported by VMConnect mode only */
        LOG(LOG_LEVEL_INFO, "Selected HYBRID security");
        self->selectedProtocol = PROTOCOL_HYBRID;
        got_protocol = 1;
    }
    else if ((security_type_mask & PROTOCOL_SSL) != 0)
    {
        /* Can we do TLS? (basic check). VMConnect is exempt. */
        if ((g_file_readable(client_info->certificate) &&
                g_file_readable(client_info->key_file)) || client_info->vmconnect)
        {
            LOG(LOG_LEVEL_INFO, "Selected TLS security");
            self->selectedProtocol = PROTOCOL_SSL;
            got_protocol = 1;
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "Cannot accept TLS connections because "
                "certificate or private key file is not readable. "
                "certificate file: [%s], private key file: [%s]",
                client_info->certificate, client_info->key_file);

            /* If we're configured to ONLY use TLS, this is a problem.
             * If not, we can fall back to RDP */
            if (client_info->security_layer == SECURITY_LAYER_TLS)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Server requires TLS (security_layer=tls)");
                self->failureCode = SSL_CERT_NOT_ON_SERVER;
                rv = 1;
            }
        }
    }
    else if (client_info->security_layer == SECURITY_LAYER_TLS)
    {
        /* We don't have a match on TLS, but we'll accept nothing less */
        LOG(LOG_LEVEL_ERROR, "Server requires TLS (security_layer=tls)");
        self->failureCode = SSL_REQUIRED_BY_SERVER;
        rv = 1;
    }

    /* If we haven't got a match so far, and we haven't got a fail,
     * try RDP */
    if (!got_protocol && !rv)
    {
        self->selectedProtocol = PROTOCOL_RDP;
        LOG(LOG_LEVEL_INFO, "Selected classic RDP security");
    }

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
    if ((flags & 0x0000000b) != flags)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unsupported [MS-RDPBCGR] RDP_NEG_REQ flags: 0x%2.2x", flags);
        return 1;
    }

    /* If both flags are set, it means 'OR', so fail only if only the one is set. */
    if ((flags & REDIRECTED_AUTHENTICATION_MODE_REQUIRED) && !(flags & RESTRICTED_ADMIN_MODE_REQUIRED))
    {
        LOG(LOG_LEVEL_ERROR, "[MS-RDPBCGR] RDP_NEG_REQ: RemoteGuard isn't supported !");
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
    char flags;
    int len;
    int len_indicator;

    struct xrdp_client_info *client_info = &(self->mcs_layer->sec_layer->rdp_layer->client_info);

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
            flags = EXTENDED_CLIENT_DATA_SUPPORTED;

            if (client_info->vmconnect)
            {
                /* NLA is handled by the host and not us. */
                flags |= RESTRICTED_ADMIN_MODE_SUPPORTED;
            }

            /* [MS-RDPBCGR] RDP_NEG_RSP */
            out_uint8(s, RDP_NEG_RSP);                    /* type*/
            out_uint8(s, flags);                          /* flags */
            out_uint16_le(s, 8);                          /* length (must be 8) */
            out_uint32_le(s, self->selectedProtocol);     /* selectedProtocol */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding structure [MS-RDPBCGR] RDP_NEG_RSP "
                      "flags 0x%02x, length 8, selectedProtocol 0x%8.8x",
                      EXTENDED_CLIENT_DATA_SUPPORTED,
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
