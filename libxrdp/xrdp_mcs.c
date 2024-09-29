/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * mcs layer which implements the Multipoint Communication Service protocol as
 * specified in [ITU-T T.125]
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"

/* Forward references */
static int
handle_channel_join_requests(struct xrdp_mcs *self,
                             struct stream *s, int *appid);

/*****************************************************************************/
struct xrdp_mcs *
xrdp_mcs_create(struct xrdp_sec *owner, struct trans *trans,
                struct stream *client_mcs_data,
                struct stream *server_mcs_data)
{
    struct xrdp_mcs *self;

    self = (struct xrdp_mcs *)g_malloc(sizeof(struct xrdp_mcs), 1);
    self->sec_layer = owner;
    self->userid = 1;
    self->chanid = 1001;
    self->client_mcs_data = client_mcs_data;
    self->server_mcs_data = server_mcs_data;
    self->iso_layer = xrdp_iso_create(self, trans);
    self->channel_list = list_create();

    return self;
}

/*****************************************************************************/
void
xrdp_mcs_delete(struct xrdp_mcs *self)
{
    struct mcs_channel_item *channel_item;
    int index;
    int count;

    if (self == 0)
    {
        return;
    }

    /* here we have to free the channel items and anything in them */
    count = self->channel_list->count;

    for (index = count - 1; index >= 0; index--)
    {
        channel_item = (struct mcs_channel_item *)
                       list_get_item(self->channel_list, index);
        g_free(channel_item);
    }

    list_delete(self->channel_list);

    xrdp_iso_delete(self->iso_layer);
    /* make sure we get null pointer exception if struct is used again. */
    LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_mcs_delete processed");
    g_memset(self, 0, sizeof(struct xrdp_mcs)) ;
    g_free(self);
}

/*****************************************************************************/
/* Send an [ITU-T T.125] DomainMCSPDU message with type ChannelJoinConfirm */
/* returns error = 1 ok = 0 */
static int
xrdp_mcs_send_cjcf(struct xrdp_mcs *self, int userid, int chanid)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "xrdp_mcs_send_cjcf: xrdp_iso_init failed");
        return 1;
    }

    /* The DomainMCSPDU choice index is a 6-bit int with the next bit as the
       bit field of the two optional fields in the struct (channelId, nonStandard)
    */
    out_uint8(s, (MCS_CJCF << 2) | 0x02); /* DomainMCSPDU choice index,
                                             channelId field is present,
                                             nonStandard field is not present */
    out_uint8(s, 0); /* result choice index 0 = rt-successful */
    out_uint16_be(s, userid); /* initiator */
    out_uint16_be(s, chanid); /* requested */
    out_uint16_be(s, chanid); /* channelId (OPTIONAL) */
    s_mark_end(s);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [ITU-T T.125] ChannelJoinConfirm "
              "result SUCCESS, initiator %d, requested %d, "
              "channelId %d",  userid, chanid, chanid);
    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "Sening [ITU-T T.125] ChannelJoinConfirm failed");
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Reads the header of a DomainMCSPDU and gets the appid
 *
 * @return 0 for success */
static int
get_domain_mcs_pdu_header(struct xrdp_mcs *self, struct stream *s, int *appid)
{
    int opcode;
    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_mcs_recv: xrdp_iso_recv failed");
        return 1;
    }

    if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T T.125] DomainMCSPDU"))
    {
        return 1;
    }

    /* The DomainMCSPDU choice index is a 6-bit int with the 2 least
       significant bits of the byte as padding */
    in_uint8(s, opcode);
    *appid = opcode >> 2; /* 2-bit padding */
    LOG_DEVEL(LOG_LEVEL_TRACE,
              "Received [ITU-T T.125] DomainMCSPDU choice index %d", *appid);
    return 0;
}

/*****************************************************************************/
/*
 * Processes an [ITU-T T.125] DomainMCSPDU message.
 *
 * Note: DomainMCSPDU messages use the ALIGNED BASIC-PER (Packed Encoding Rules)
 * from [ITU-T X.691].
 *
 * returns error
 */
int
xrdp_mcs_recv(struct xrdp_mcs *self, struct stream *s, int *chan)
{
    int appid;
    int len;

    if (get_domain_mcs_pdu_header(self, s, &appid) != 0)
    {
        return 1;
    }

    if (self->expecting_channel_join_requests)
    {
        if (handle_channel_join_requests(self, s, &appid) != 0)
        {
            return 1;
        }
        LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] completed");
        self->expecting_channel_join_requests = 0;
    }

    if (appid == MCS_DPUM) /* Disconnect Provider Ultimatum */
    {
        LOG_DEVEL(LOG_LEVEL_TRACE,
                  "Received [ITU-T T.125] DisconnectProviderUltimatum");
        LOG(LOG_LEVEL_DEBUG, "Received disconnection request");
        return 1;
    }


    if (appid != MCS_SDRQ)
    {
        LOG(LOG_LEVEL_ERROR, "Received [ITU-T T.125] DomainMCSPDU "
            "choice index %d is unknown. Expected the DomainMCSPDU to "
            "contain the type SendDataRequest with index %d",
            appid, MCS_SDRQ);
        return 1;
    }

    if (!s_check_rem_and_log(s, 6, "Parsing [ITU-T T.125] SendDataRequest"))
    {
        return 1;
    }

    in_uint8s(s, 2); /* initiator */
    in_uint16_be(s, *chan); /* channelId */
    in_uint8s(s, 1); /* dataPriority (4-bits), segmentation (2-bits), padding (2-bits) */
    in_uint8(s, len); /* userData Length (byte 1) */

    if ((len & 0xC0) == 0x80)
    {
        /* From [ITU-T X.691] 11.9.3.7
           encoding a length determinant if "n" is greater than 127 and
           less than 16K, then n is encoded using 2 bytes.
           The first byte will have the two highest order bits set to 1 and 0
           (ie. len & 0xC0 == 0x80) and the length is encoded as remaining 14 bits of
           the two bytes (ie. len & 0x3fff). */
        if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T T.125] SendDataRequest userData Length"))
        {
            return 1;
        }
        in_uint8s(s, 1); /* userData Length (byte 2) */
    }
    else if ((len & 0xC0) == 0xC0)
    {
        /* From [ITU-T X.691] 11.9.3.8
           encoding a length determinant if "n" is greater than 16K,
           then the list of items is fragmented with the length of the first
           fragment encoded using 1 byte. The two highest order bits are set
           to 1 and 1 (ie. len & 0xC0 == 0xC0) and the remaining 6 bits contain
           a multiplyer for 16K (ie. n = (len & 0x3f) * 0x3f)
        */
        LOG(LOG_LEVEL_ERROR, "[ITU-T T.125] SendDataRequest with length greater "
            "than 16K is not supported. len 0x%2.2x", len);
        return 1;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] SendDataRequest "
              "initiator (ignored), channelId %d, dataPriority (ignored), "
              "segmentation (ignored), userData Length (ignored)", *chan);

    return 0;
}

/*****************************************************************************/
/**
 * Parse the identifier and length of a [ITU-T X.690] BER (Basic Encoding Rules)
 * structure header.
 *
 * @param self
 * @param s [in] - the stream to read from
 * @param tag_val [in] - the expected tag value
 * @param len [out] - the length of the structure
 * @returns error
 */
static int
xrdp_mcs_ber_parse_header(struct xrdp_mcs *self, struct stream *s,
                          int tag_val, int *len)
{
    int tag;
    int l;
    int i;

    if (tag_val > 0xff)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [ITU-T X.690] Identifier"))
        {
            return 1;
        }
        in_uint16_be(s, tag);
    }
    else
    {
        if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T X.690] Identifier"))
        {
            return 1;
        }
        in_uint8(s, tag);
    }

    if (tag != tag_val)
    {
        LOG(LOG_LEVEL_ERROR, "Parsed [ITU-T X.690] Identifier: "
            "expected 0x%4.4x, actual 0x%4.4x", tag_val, tag);
        return 1;
    }

    if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T X.690] Length"))
    {
        return 1;
    }

    in_uint8(s, l);

    if (l & 0x80)
    {
        l = l & ~0x80;
        *len = 0;

        while (l > 0)
        {
            if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T X.690] Length"))
            {
                return 1;
            }
            in_uint8(s, i);
            *len = (*len << 8) | i;
            l--;
        }
    }
    else
    {
        *len = l;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "Parsed BER header [ITU-T X.690] "
              "Identifier 0x%4.4x, Length %d", tag, *len);

    return 0;
}

/*****************************************************************************/
/* Parses a [ITU-T T.125] DomainParameters structure encoded using BER */
/* returns error */
static int
xrdp_mcs_parse_domain_params(struct xrdp_mcs *self, struct stream *s)
{
    int len;

    if (xrdp_mcs_ber_parse_header(self, s, MCS_TAG_DOMAIN_PARAMS, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] DomainParameters failed");
        return 1;
    }

    if (len < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] DomainParameters length field is "
            "invalid. Expected >= 0, actual %d", len);
        return 1;
    }
    if (!s_check_rem_and_log(s, len, "Parsing [ITU-T T.125] DomainParameters"))
    {
        return 1;
    }

    in_uint8s(s, len); /* skip all fields */

    return !s_check_rem_and_log(s, 0, "Parsing [ITU-T T.125] DomainParameters");
}

/*****************************************************************************/
/* Process a [ITU-T T.125] Connect-Initial message encoded using BER */
/* returns error */
static int
xrdp_mcs_recv_connect_initial(struct xrdp_mcs *self)
{
    int len;
    struct stream *s;

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] Connect-Initial failed");
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] Connect-Initial failed");
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, MCS_CONNECT_INITIAL, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial failed");
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial callingDomainSelector failed");
        return 1;
    }

    if (len < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial callingDomainSelector length field is "
            "invalid. Expected >= 0, actual %d", len);
        return 1;
    }
    if (!s_check_rem_and_log(s, len, "Parsing [ITU-T T.125] Connect-Initial callingDomainSelector"))
    {
        return 1;
    }

    in_uint8s(s, len); /* [ITU-T T.125] Connect-Initial callingDomainSelector */

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial calledDomainSelector failed");
        return 1;
    }
    if (len < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial calledDomainSelector length field is "
            "invalid. Expected >= 0, actual %d", len);
        return 1;
    }
    if (!s_check_rem_and_log(s, len, "Parsing [ITU-T T.125] Connect-Initial calledDomainSelector"))
    {
        return 1;
    }

    in_uint8s(s, len); /* [ITU-T T.125] Connect-Initial calledDomainSelector */

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_BOOLEAN, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial upwardFlag failed");
        return 1;
    }
    if (len < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial upwardFlag length field is "
            "invalid. Expected >= 0, actual %d", len);
        return 1;
    }
    if (!s_check_rem_and_log(s, len, "Parsing [ITU-T T.125] Connect-Initial upwardFlag"))
    {
        return 1;
    }

    in_uint8s(s, len);  /* [ITU-T T.125] Connect-Initial upwardFlag */

    /* [ITU-T T.125] Connect-Initial targetParameters */
    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial targetParameters failed");
        return 1;
    }

    /* [ITU-T T.125] Connect-Initial minimumParameters */
    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial minimumParameters failed");
        return 1;
    }

    /* [ITU-T T.125] Connect-Initial maximumParameters */
    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial maximumParameters failed");
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial userData failed");
        return 1;
    }
    /* mcs userData can not be zero length */
    if ((len <= 0) || (len > 16 * 1024))
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [ITU-T T.125] Connect-Initial userData: invalid length. "
            "Expected min 1, max %d; Actual %d", 16 * 1024, len);
        return 1;
    }
    if (!s_check_rem_and_log(s, len, "Parsing [ITU-T T.125] Connect-Initial userData"))
    {
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [ITU-T T.125] Connect-Initial "
              "callingDomainSelector (ignored), calledDomainSelector (ignored), "
              "upwardFlag (ignored), targetParameters (ignored), "
              "minimumParameters (ignored), maximumParameters (ignored), "
              "userData (copied to client_mcs_data)");
    /* make a copy of client mcs data */
    init_stream(self->client_mcs_data, len);
    out_uint8a(self->client_mcs_data, s->p, len); /* [ITU-T T.125] Connect-Initial userData */
    in_uint8s(s, len);
    s_mark_end(self->client_mcs_data);

    if (!s_check_end_and_log(s, "MCS protocol error [ITU-T T.125] Connect-Initial"))
    {
        return 1;
    }

    return 0;

}

/*****************************************************************************/
/* Processes a [ITU-T T.25] DomainMCSPDU with type ErectDomainRequest
 *
 * Note: a parsing example can be found in [MS-RDPBCGR] 4.1.5
 *
 * returns error */
static int
xrdp_mcs_recv_edrq(struct xrdp_mcs *self)
{
    int opcode;
    struct stream *s;

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] ErectDomainRequest failed");
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] ErectDomainRequest failed");
        return 1;
    }

    if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T T.125] DomainMCSPDU"))
    {
        return 1;
    }

    /* The DomainMCSPDU choice index is a 6-bit int with the next bit as the
       bit field of the optional field in the struct
    */
    in_uint8(s, opcode);

    if ((opcode >> 2) != MCS_EDRQ)
    {
        LOG(LOG_LEVEL_ERROR, "Parsed [ITU-T T.125] DomainMCSPDU choice index "
            "expected %d, received %d", MCS_EDRQ, (opcode >> 2));
        return 1;
    }

    if (!s_check_rem_and_log(s, 4, "Parsing [ITU-T T.125] ErectDomainRequest"))
    {
        return 1;
    }

    in_uint8s(s, 2); /* subHeight */
    in_uint8s(s, 2); /* subInterval */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] DomainMCSPDU "
              "choice index %d (ErectDomainRequest)", (opcode >> 2));
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] ErectDomainRequest "
              "subHeight (ignored), subInterval (ignored), "
              "nonStandard (%s)",
              (opcode & 2) ? "present" : "not present");

    /*
     * [MS-RDPBCGR] 2.2.1.5 says that the mcsEDrq field is 5 bytes (which have
     * already been read into the opcode and previous fields), so the
     * nonStandard field should never be present.
     */
    if (opcode & 2) /* ErectDomainRequest v3 nonStandard optional field is present? */
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [ITU-T T.125] ErectDomainRequest nonStandard"))
        {
            return 1;
        }
        in_uint16_be(s, self->userid); /* NonStandardParameter.key
                                          NonStandardParameter.data */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] DomainMCSPDU "
                  "choice index %d (ErectDomainRequest)", (opcode >> 2));
    }

    if (!s_check_end_and_log(s, "MCS protocol error [ITU-T T.125] ErectDomainRequest"))
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* Processes a [ITU-T T.25] DomainMCSPDU with type AttachUserRequest
 *
 * Note: a parsing example can be found in [MS-RDPBCGR] 4.1.6
 *
 * returns error */
static int
xrdp_mcs_recv_aurq(struct xrdp_mcs *self)
{
    int opcode;
    struct stream *s;

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] AttachUserRequest failed");
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Processing [ITU-T T.125] AttachUserRequest failed");
        return 1;
    }

    if (!s_check_rem_and_log(s, 1, "Parsing [ITU-T T.125] DomainMCSPDU"))
    {
        return 1;
    }

    /* The DomainMCSPDU choice index is a 6-bit int with the next bit as the
       bit field of the optional field in the struct
    */
    in_uint8(s, opcode);

    if ((opcode >> 2) != MCS_AURQ)
    {
        LOG(LOG_LEVEL_ERROR, "Parsed [ITU-T T.125] DomainMCSPDU choice index "
            "expected %d, received %d", MCS_AURQ, (opcode >> 2));
        return 1;
    }

    /*
     * [MS-RDPBCGR] 2.2.1.6 says that the mcsAUrq field is 1 bytes (which have
     * already been read into the opcode), so the nonStandard field should
     * never be present.
     */
    if (opcode & 2)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [ITU-T T.125] AttachUserRequest nonStandard"))
        {
            return 1;
        }
        in_uint16_be(s, self->userid); /* NonStandardParameter.key
                                          NonStandardParameter.data */
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] DomainMCSPDU "
              "choice index %d (AttachUserRequest)", (opcode >> 2));
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [ITU-T T.125] AttachUserRequest "
              "nonStandard (%s)",
              (opcode & 2) ? "present" : "not present");

    if (!s_check_end_and_log(s, "MCS protocol error [ITU-T T.125] AttachUserRequest"))
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* Send a [ITU-T T.125] DomainMCSPDU with type AttachUserConfirm.
 *
 * Note: a parsing example can be found in [MS-RDPBCGR] 4.1.7
 *
 * returns error */
static int
xrdp_mcs_send_aucf(struct xrdp_mcs *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "xrdp_mcs_send_aucf: xrdp_iso_init failed");
        return 1;
    }

    out_uint8(s, ((MCS_AUCF << 2) | 2)); /* AttachUserConfirm
                                            optional field initiator is present */
    out_uint8s(s, 1); /* result = 0 rt-successful */
    out_uint16_be(s, self->userid); /* initiator */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [ITU-T T.125] DomainMCSPDU "
              "of type AttachUserConfirm: result SUCCESS, initiator %d",
              self->userid);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "Sending [ITU-T T.125] AttachUserConfirm failed");
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Write the identifier and length of a [ITU-T X.690] BER (Basic Encoding Rules)
 * structure header.
 * returns error */
static int
xrdp_mcs_ber_out_header(struct xrdp_mcs *self, struct stream *s,
                        int tag_val, int len)
{
    if (tag_val > 0xff)
    {
        out_uint16_be(s, tag_val);
    }
    else
    {
        out_uint8(s, tag_val);
    }

    if (len >= 0x80)
    {
        out_uint8(s, 0x82);
        out_uint16_be(s, len);
    }
    else
    {
        out_uint8(s, len);
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [ITU-T X.690] Identifier %d, Length %d",
              tag_val, len);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_ber_out_int8(struct xrdp_mcs *self, struct stream *s, int value)
{
    xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
    out_uint8(s, value);
    return 0;
}

#if 0 /* not used */
/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_ber_out_int16(struct xrdp_mcs *self, struct stream *s, int value)
{
    xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 2);
    out_uint8(s, (value >> 8));
    out_uint8(s, value);
    return 0;
}
#endif

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_ber_out_int24(struct xrdp_mcs *self, struct stream *s, int value)
{
    xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 3);
    out_uint8(s, (value >> 16));
    out_uint8(s, (value >> 8));
    out_uint8(s, value);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_out_domain_params(struct xrdp_mcs *self, struct stream *s,
                           int max_channels,
                           int max_users, int max_tokens,
                           int max_pdu_size)
{
    xrdp_mcs_ber_out_header(self, s, MCS_TAG_DOMAIN_PARAMS, 26);
    xrdp_mcs_ber_out_int8(self, s, max_channels);
    xrdp_mcs_ber_out_int8(self, s, max_users);
    xrdp_mcs_ber_out_int8(self, s, max_tokens);
    xrdp_mcs_ber_out_int8(self, s, 1); /* numPriorities */
    xrdp_mcs_ber_out_int8(self, s, 0); /* minThroughput */
    xrdp_mcs_ber_out_int8(self, s, 1); /* maxHeight */
    xrdp_mcs_ber_out_int24(self, s, max_pdu_size);
    xrdp_mcs_ber_out_int8(self, s, 2); /* protocolVersion */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [ITU-T T.125] DomainParameters "
              "maxChannelIds %d, maxUserIds %d, maxTokenIds %d, numPriorities 1, "
              "minThroughput 0 B/s, maxHeight 1, maxMCSPDUsize %d, "
              "protocolVersion 2",
              max_channels, max_users, max_tokens, max_pdu_size);
    return 0;
}

/*****************************************************************************/
/*
 * Output a TS_UD_SC_CORE struct ([MS-RDPBCGR] 2.2.1.4.2)
 */
static void
out_server_core_data(struct xrdp_sec *self, struct stream *s)
{
    unsigned int version = 0x00080004;
    unsigned int client_requested_protocols = 0;
    unsigned int early_capability_flags = RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED;

    if (self->mcs_layer->iso_layer->rdpNegData)
    {
        client_requested_protocols =
            self->mcs_layer->iso_layer->requestedProtocol;
    }

    /* [MS-RDPBCGR] TS_UD_HEADER */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct header [MS-RDPBCGR] TS_UD_HEADER "
              "type 0x%4.4x, length %d", SEC_TAG_SRV_INFO, 16);
    out_uint16_le(s, SEC_TAG_SRV_INFO); /* type */
    out_uint16_le(s, 16); /* length */

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] TS_UD_SC_CORE "
              "version 0x%8.8x clientRequestedProtocols 0x%8.8x "
              "earlyCapabilityFlags 0x%8.8x",
              version, client_requested_protocols,
              early_capability_flags);
    out_uint32_le(s, version);
    out_uint32_le(s, client_requested_protocols);
    out_uint32_le(s, early_capability_flags);
}

/*****************************************************************************/
/* Write an [ITU-T T.124] ConnectData (ALIGNED variant of BASIC-PER) message
 * with ConnectGCCPDU, ConferenceCreateResponse,
 * and [MS-RDPBCGR] Server Data Blocks as user data.
 */
static int
xrdp_mcs_out_gcc_data(struct xrdp_sec *self)
{
    struct stream *s;
    int num_channels_even;
    int num_channels;
    int index;
    int channel;
    int gcc_size;
    char *gcc_size_ptr;
    char *ud_ptr;
    int header_length = 0;
    int server_cert_len = 0;
    int public_key_blob_len = 0;
    int key_len = 0;
    int bit_len = 0;
    int data_len = 0;
    int modulus_len = 0;

    num_channels = self->mcs_layer->channel_list->count;
    num_channels_even = num_channels + (num_channels & 1);
    s = &(self->server_mcs_data);
    init_stream(s, 8192);

    /* [ITU-T T.124] ConnectData (ALIGNED variant of BASIC-PER) */
    out_uint16_be(s, 5); /* = 0x00 0x05 */
    /* t124Identifier choice index = 0 (object) */
    /* object length = 5 */
    out_uint16_be(s, 0x14);  /* t124Identifier.object = ??? (0x00 0x14 0x7c 0x00 0x01) */
    out_uint8(s, 0x7c);
    out_uint16_be(s, 1); /* -- */
    out_uint8(s, 0x2a);  /* connectPDU length = 42 */
    /* connectPDU octet string of type ConnectGCCPDU
       (unknown where this octet string is defined to be
       of type ConnectGCCPDU) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [ITU-T T.124] ConnectData "
              "t124Identifier.object 0x00 0x14 0x7c 0x00 0x01, connectPDU length %d",
              0x2a);

    /* [ITU-T T.124] ConnectGCCPDU (ALIGNED variant of BASIC-PER) */
    out_uint8(s, 0x14);  /* ConnectGCCPDU choice index 1 = ConferenceCreateResponse with userData present */

    /* [ITU-T T.124] ConferenceCreateResponse (ALIGNED variant of BASIC-PER) */
    out_uint8(s, 0x76); /* nodeID = 31219 - 1001 (PER offset for min value)
                                  = 30218 (big-endian 0x760a) */
    out_uint8(s, 0x0a);
    out_uint8(s, 1);    /* tag length */
    out_uint8(s, 1);    /* tag */
    out_uint8(s, 0);    /* result = 0 (success) */
    out_uint16_le(s, 0xc001); /* userData set count = 1 (0x01),
                                 userData.isPresent = 0x80 (yes) | userData.key choice index = 0x40 (1 = h221NonStandard) */
    out_uint8(s, 0);    /* userData.key.h221NonStandard length
                           = 4 - 4 (PER offset for min value) (H221NonStandardIdentifier is an octet string SIZE (4..255))
                           = 0
                           */

    /* [ITU-T H.221] H221NonStandardIdentifier uses country codes and
       manufactuer codes from [ITU-T T.35]. Unknown why these values are used,
       maybe this is just copied from the [MS-RDPBCGR] 4.1.4 example which uses
       the value "McDn" */
    out_uint8(s, 0x4d); /* M */
    out_uint8(s, 0x63); /* c */
    out_uint8(s, 0x44); /* D */
    out_uint8(s, 0x6e); /* n */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [ITU-T T.124] ConferenceCreateResponse "
              "nodeID %d, result SUCCESS", 0x760a + 1001);

    /* ConferenceCreateResponse.userData.key.value (octet string) */
    /* GCC Response Total Length - 2 bytes , set later */
    gcc_size_ptr = s->p; /* RDPGCCUserDataResponseLength */
    out_uint8s(s, 2);
    ud_ptr = s->p; /* User Data */

    out_server_core_data(self, s);

    /* [MS-RDPBCGR] TS_UD_HEADER */
    out_uint16_le(s, SEC_TAG_SRV_CHANNELS); /* type */
    out_uint16_le(s, 8 + (num_channels_even * 2)); /* length */
    /* [MS-RDPBCGR] TS_UD_SC_NET */
    out_uint16_le(s, MCS_GLOBAL_CHANNEL); /* 1003, 0x03eb main channel (MCSChannelId) */
    out_uint16_le(s, num_channels); /* number of other channels (channelCount) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct header [MS-RDPBCGR] TS_UD_HEADER "
              "type 0x%4.4x, length %d",
              SEC_TAG_SRV_CHANNELS, 8 + (num_channels_even * 2));
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] TS_UD_SC_NET "
              "MCSChannelId %d, channelCount %d",
              MCS_GLOBAL_CHANNEL, num_channels);
    for (index = 0; index < num_channels_even; index++)
    {
        if (index < num_channels)
        {
            channel = MCS_GLOBAL_CHANNEL + (index + 1);
            out_uint16_le(s, channel); /* channelIdArray[index] (channel allocated) */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] TS_UD_SC_NET channelIdArray[%d] "
                      "channelId %d", index, channel);
        }
        else
        {
            out_uint16_le(s, 0); /* padding or channelIdArray[index] (channel not allocated) */
        }

    }

    if (self->rsa_key_bytes == 64 || self->rsa_key_bytes == 256)
    {
        if (self->rsa_key_bytes == 64)
        {
            header_length = 0x00ec;       /* length = 236 */
            server_cert_len = 0xb8;       /* serverCertLen (184 bytes) */
            public_key_blob_len = 0x005c; /* wPublicKeyBlobLen (92 bytes) */
            key_len = 0x0048;             /* keylen (72 bytes = (bitlen / 8) modulus + 8 padding) */
            bit_len = 512;                /* bitlen = 512 */
            data_len = 63;                /* datalen (63 = (bitlen / 8) - 1) */
            modulus_len = 64;
        }
        else /* if (self->rsa_key_bytes == 256) */
        {
            header_length = 0x01ac;       /* length = 428 */
            server_cert_len = 0x178;      /* serverCertLen (376 bytes) */
            public_key_blob_len = 0x011c; /* wPublicKeyBlobLen (284 bytes) */
            key_len = 0x0108;             /* keylen (264 bytes = (bitlen / 8) modulus + 8 padding) */
            bit_len = 2048;               /* bitlen = 2048 */
            data_len = 255;               /* datalen (255 = (bitlen / 8) - 1) */
            modulus_len = 256;
        }
        LOG(LOG_LEVEL_DEBUG, "using %d bit RSA key", bit_len);

        /* [MS-RDPBCGR] TS_UD_HEADER */
        out_uint16_le(s, SEC_TAG_SRV_CRYPT);  /* type */
        out_uint16_le(s, header_length);      /* length */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct header [MS-RDPBCGR] TS_UD_HEADER "
                  "type 0x%4.4x, length %d", SEC_TAG_SRV_CRYPT, header_length);

        /* [MS-RDPBCGR] TS_UD_SC_SEC1 */
        out_uint32_le(s, self->crypt_method);   /* encryptionMethod  */
        out_uint32_le(s, self->crypt_level);    /* encryptionLevel */
        out_uint32_le(s, 32);                   /* serverRandomLen */
        out_uint32_le(s, server_cert_len);      /* serverCertLen */
        out_uint8a(s, self->server_random, 32); /* serverRandom */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] TS_UD_SC_SEC1 "
                  "encryptionMethod 0x%8.8x, encryptionLevel 0x%8.8x, "
                  "serverRandomLen 32, serverCertLen %d, serverRandom (omitted), ",
                  self->crypt_method, self->crypt_level, server_cert_len);

        /* (field serverCertificate) [MS-RDPBCGR] SERVER_CERTIFICATE */
        /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
        /* TermService\Parameters\Certificate */
        out_uint32_le(s, 1); /* dwVersion (1 = PROPRIETARYSERVERCERTIFICATE) */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] SERVER_CERTIFICATE "
                  "dwVersion.certChainVersion 1 (CERT_CHAIN_VERSION_1), "
                  "dwVersion.t 0 (permanent)");

        /* [MS-RDPBCGR] PROPRIETARYSERVERCERTIFICATE */
        out_uint32_le(s, 1);              /* dwSigAlgId (1 = RSA) */
        out_uint32_le(s, 1);              /* dwKeyAlgId (1 = RSA) */
        out_uint16_le(s, SEC_TAG_PUBKEY); /* wPublicKeyBlobType (BB_RSA_KEY_BLOB) */
        out_uint16_le(s, public_key_blob_len); /* wPublicKeyBlobLen */

        /* (field PublicKeyBlob) [MS-RDPBCGR] RSA_PUBLIC_KEY */
        out_uint32_le(s, SEC_RSA_MAGIC);  /* magic (0x31415352 'RSA1') */
        out_uint32_le(s, key_len);        /* keylen */
        out_uint32_le(s, bit_len);        /* bitlen */
        out_uint32_le(s, data_len);       /* datalen */
        out_uint8a(s, self->pub_exp, 4);  /* pubExp */
        out_uint8a(s, self->pub_mod, modulus_len); /* modulus */
        out_uint8s(s, 8);                 /* modulus zero padding */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] RSA_PUBLIC_KEY "
                  "magic 0x%8.8x, keylen %d, bitlen %d, datalen %d, "
                  "pubExp <omitted>, modulus <omitted>, ",
                  SEC_RSA_MAGIC, key_len, bit_len, data_len);

        out_uint16_le(s, SEC_TAG_KEYSIG); /* wSignatureBlobType (0x0008 RSA) */
        out_uint16_le(s, 72);             /* wSignatureBlobLen */
        out_uint8a(s, self->pub_sig, 64); /* SignatureBlob */
        out_uint8s(s, 8);                 /* modulus zero padding */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] PROPRIETARYSERVERCERTIFICATE "
                  "dwKeyAlgId 1, "
                  "wPublicKeyBlobType 0x%4.4x, "
                  "wPublicKeyBlobLen %d, PublicKeyBlob <see RSA_PUBLIC_KEY above>"
                  "wSignatureBlobType 0x%4.4x, "
                  "wSignatureBlobLen %d, "
                  "SignatureBlob <omitted>",
                  SEC_TAG_PUBKEY, public_key_blob_len,
                  SEC_TAG_KEYSIG, 72);
    }
    else if (self->rsa_key_bytes == 0) /* no security */
    {
        LOG(LOG_LEVEL_DEBUG, "using no security");
        /* [MS-RDPBCGR] TS_UD_HEADER */
        out_uint16_le(s, SEC_TAG_SRV_CRYPT);  /* type*/
        out_uint16_le(s, 12);                 /* length */
        /* [MS-RDPBCGR] TS_UD_SC_SEC1 */
        out_uint32_le(s, self->crypt_method); /* encryptionMethod  */
        out_uint32_le(s, self->crypt_level);  /* encryptionLevel */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct header [MS-RDPBCGR] TS_UD_HEADER "
                  "type 0x%4.4x, length %d", SEC_TAG_SRV_CRYPT, 12);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding struct [MS-RDPBCGR] TS_UD_SC_SEC1 "
                  "encryptionMethod 0x%8.8x, encryptionMethod 0x%8.8x",
                  self->crypt_method, self->crypt_level);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING,
            "Unsupported xrdp_sec.rsa_key_bytes value: %d, the client "
            "will not be sent a [MS-RDPBCGR] TS_UD_SC_SEC1 message.",
            self->rsa_key_bytes);
    }
    s_mark_end(s);

    gcc_size = (int)(s->end - ud_ptr) | 0x8000;
    gcc_size_ptr[0] = gcc_size >> 8;
    gcc_size_ptr[1] = gcc_size;

    return 0;
}
/*****************************************************************************/
/* Send an [ITU-T T.125] Connect-Response message.
 *
 * Note: the xrdp_mcs_out_gcc_data() function must be called (to populate the
 * xrdp_mcs.server_mcs_data stream) before this method is called.
 *
 * returns error
 */
static int
xrdp_mcs_send_connect_response(struct xrdp_mcs *self)
{
    int data_len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    data_len = (int) (self->server_mcs_data->end - self->server_mcs_data->data);
    xrdp_iso_init(self->iso_layer, s);
    //TODO: we should calculate the whole length include MCS_CONNECT_RESPONSE
    xrdp_mcs_ber_out_header(self, s, MCS_CONNECT_RESPONSE,
                            data_len > 0x80 ? data_len + 38 : data_len + 36);
    xrdp_mcs_ber_out_header(self, s, BER_TAG_RESULT, 1);
    out_uint8(s, 0); /* result choice index 0 = rt-successful */
    xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
    out_uint8(s, 0); /* calledConnectId */
    xrdp_mcs_out_domain_params(self, s, 22, 3, 0, 0xfff8);
    xrdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, data_len);
    /* mcs data */
    out_uint8a(s, self->server_mcs_data->data, data_len);
    s_mark_end(s);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [ITU-T T.125] Connect-Response "
              "result SUCCESS, calledConnectId 0, "
              "domainParameters (see xrdp_mcs_out_domain_params() trace logs), "
              "userData (see xrdp_mcs_out_gcc_data() trace logs and "
              "hex dump below)");
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "[ITU-T T.125] Connect-Response userData",
                      self->server_mcs_data->data, data_len);
    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "Sending [ITU-T T.125] Connect-Response failed");
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Handle all client channel join requests
 *
 * @param self MCS structure
 * @param s Input stream
 * @param[in,out] appid Type of the MCS PDU whose header has just been read.
 * @return 0 for success
 *
 * Called when an MCS PDU header has been read, but the PDU has not
 * been processed.
 *
 * If the PDU is a channel join request, it is processed, and the next
 * PDU header is read. When we've exhausted all the channel join requests,
 * the type of the next PDU is passed back to the caller for the caller
 * to process.
 *
 * We simply take all the join requests which come in,
 * and respond to them. We do this for a number of reasons:-
 *
 * 1) Older clients may not issue exactly the documented number of
 *    channel join requests. See
 *    https://github.com/neutrinolabs/xrdp/issues/2166
 * 2) If we set RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED in our early capabilities
 *    flags, and the client sets RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN, the
 *    client can still send channel join requests and be compliant with the
 *    spec. See [MS-RDPCCGR] 3.2.5.3.8.
 */
static int
handle_channel_join_requests(struct xrdp_mcs *self,
                             struct stream *s, int *appid)
{
    int rv = 0;
    unsigned int expected_join_count = 0;
    if ((self->sec_layer->rdp_layer->client_info.mcs_early_capability_flags &
            RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN) == 0)
    {
        /*
         * The client has indicated it does not support skipping channel
         * join request/confirm PDUs.
         *
         * Expect a channel join request PDU for each of the static
         * virtual channels, plus the user channel (self->chanid) and
         * the I/O channel (MCS_GLOBAL_CHANNEL) */
        expected_join_count = self->channel_list->count + 2;
    }

    unsigned int actual_join_count = 0;

    while (*appid == MCS_CJRQ)
    {
        int userid;
        int chanid;

        if (!s_check_rem_and_log(s, 4, "Parsing [ITU-T T.125] "
                                 "ChannelJoinRequest"))
        {
            rv = 1;
            break;
        }

        in_uint16_be(s, userid);
        in_uint16_be(s, chanid);
        LOG_DEVEL(LOG_LEVEL_TRACE,
                  "Received [ITU-T T.125] ChannelJoinRequest "
                  "initiator 0x%4.4x, channelId 0x%4.4x", userid, chanid);
        ++actual_join_count;

        if (xrdp_mcs_send_cjcf(self, userid, chanid) != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "[ITU-T T.125] Channel join sequence: failed");
        }

        /* Get the next PDU header */
        s = libxrdp_force_read(self->iso_layer->trans);
        if (s == 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_mcs_recv: libxrdp_force_read failed");
            rv = 1;
            break;
        }
        if (get_domain_mcs_pdu_header(self, s, appid) != 0)
        {
            rv = 1;
            break;
        }
    }

    if (rv == 0 && expected_join_count != actual_join_count)
    {
        LOG(LOG_LEVEL_WARNING, "Expected %u channel join PDUs but got %u",
            expected_join_count, actual_join_count);
    }
    return rv;
}

/*****************************************************************************/
/* Process and send the MCS messages for the RDP Connection Sequence
 * [MS-RDPBCGR] 1.3.1.1
 *
 * returns error
 */
int
xrdp_mcs_incoming(struct xrdp_mcs *self)
{
    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] receive connection request");
    if (xrdp_mcs_recv_connect_initial(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] receive connection request failed");
        return 1;
    }

    /* in xrdp_sec.c */
    if (xrdp_sec_process_mcs_data(self->sec_layer) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] Connect Initial PDU with GCC Conference Create Request failed");
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] construct connection response");
    if (xrdp_mcs_out_gcc_data(self->sec_layer) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] construct connection response failed");
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] send connection response");
    if (xrdp_mcs_send_connect_response(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] send connection response failed");
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] receive erect domain request");
    if (xrdp_mcs_recv_edrq(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] receive erect domain request failed");
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] receive attach user request");
    if (xrdp_mcs_recv_aurq(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] receive attach user request failed");
        return 1;
    }

    LOG(LOG_LEVEL_DEBUG, "[MCS Connection Sequence] send attach user confirm");
    if (xrdp_mcs_send_aucf(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "[MCS Connection Sequence] send attach user confirm failed");
        return 1;
    }

    /* Tell the MCS PDU processing loop that (zere or more ) channel
       join requests are expected at this point */
    self->expecting_channel_join_requests = 1;

    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_init(struct xrdp_mcs *self, struct stream *s)
{
    xrdp_iso_init(self->iso_layer, s);
    s_push_layer(s, mcs_hdr, 8);
    return 0;
}

/*****************************************************************************/
/* returns error */
/* Inform the callback that an mcs packet has been sent.  This is needed so
   the module can send any high priority mcs packets like audio. */
static int
xrdp_mcs_call_callback(struct xrdp_mcs *self)
{
    int rv;
    struct xrdp_session *session;

    rv = 0;
    /* if there is a callback, call it here */
    session = self->sec_layer->rdp_layer->session;

    if (session != 0)
    {
        if (session->callback != 0)
        {
            if (session->check_for_app_input)
            {
                /* in xrdp_wm.c */
                rv = session->callback(session->id, 0x5556, 0, 0, 0, 0);
            }
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_WARNING, "session->callback is NULL");
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_WARNING, "session is NULL");
    }

    return rv;
}

/*****************************************************************************/
/* Send a [ITU-T T.125] SendDataIndication message
 * returns error */
int
xrdp_mcs_send(struct xrdp_mcs *self, struct stream *s, int chan)
{
    int len;
    char *lp;
    //static int max_len = 0;

    s_pop_layer(s, mcs_hdr);
    len = (s->end - s->p) - 8;

    if (len > 8192 * 2)
    {
        LOG(LOG_LEVEL_WARNING, "xrdp_mcs_send: stream size too big: %d bytes", len);
    }

    /* The DomainMCSPDU choice index is a 6-bit int with the 2 least
           significant bits of the byte as padding */
    out_uint8(s, MCS_SDIN << 2);    /* DomainMCSPDU choice index */
    out_uint16_be(s, self->userid); /* initiator */
    out_uint16_be(s, chan);         /* channelId */
    out_uint8(s, 0x70);             /* dataPriority (upper 2 bits),
                                       segmentation (next 2 bits),
                                       padding (4 bits) */

    if (len >= 128)
    {
        len = len | 0x8000;
        out_uint16_be(s, len);      /* userData length */
    }
    else
    {
        out_uint8(s, len);          /* userData length */
        /* move everything up one byte */
        lp = s->p;

        while (lp < s->end)
        {
            lp[0] = lp[1];
            lp++;
        }

        s->end--;
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [ITU-T T.125] SendDataIndication "
              "initiator %d, channelId %d, dataPriority %d, segmentation 0x0, "
              "userData length %d",
              self->userid, chan, 0x70 >> 6, (0x70 >> 4) & 0x03);
    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_mcs_send: xrdp_iso_send failed");
        return 1;
    }

    /* todo, do we need to call this for every mcs packet,
       maybe every 5 or so */
    if (chan == MCS_GLOBAL_CHANNEL)
    {
        xrdp_mcs_call_callback(self);
    }

    return 0;
}

/**
 * Internal help function to close the socket
 * @param self
 */
static void
close_rdp_socket(struct xrdp_mcs *self)
{
    if (self->iso_layer != 0)
    {
        if (self->iso_layer->trans != 0)
        {
            trans_shutdown_tls_mode(self->iso_layer->trans);
            g_tcp_close(self->iso_layer->trans->sck);
            self->iso_layer->trans->sck = -1;
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_mcs_disconnect - socket closed");
            return;
        }
    }
    LOG_DEVEL(LOG_LEVEL_WARNING, "Failed to close socket");
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_disconnect(struct xrdp_mcs *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        close_rdp_socket(self);
        LOG(LOG_LEVEL_ERROR, "xrdp_mcs_disconnect: xrdp_iso_init failed");
        return 1;
    }

    out_uint8(s, (MCS_DPUM << 2) | 1);
    out_uint8(s, 0x80); /* reason (upper 3 bits) (4 = rn-channel-purged)*/
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [ITU T.125] DisconnectProviderUltimatum "
              "reason %d", 0x80 >> 5);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        close_rdp_socket(self);
        LOG(LOG_LEVEL_ERROR, "Sending [ITU T.125] DisconnectProviderUltimatum failed");
        return 1;
    }

    free_stream(s);
    close_rdp_socket(self);
    return 0;
}
