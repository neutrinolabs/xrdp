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
 * mcs layer
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "log.h"

/*****************************************************************************/
struct xrdp_mcs *
xrdp_mcs_create(struct xrdp_sec *owner, struct trans *trans,
                struct stream *client_mcs_data,
                struct stream *server_mcs_data)
{
    struct xrdp_mcs *self;

    DEBUG(("  in xrdp_mcs_create"));
    self = (struct xrdp_mcs *)g_malloc(sizeof(struct xrdp_mcs), 1);
    self->sec_layer = owner;
    self->userid = 1;
    self->chanid = 1001;
    self->client_mcs_data = client_mcs_data;
    self->server_mcs_data = server_mcs_data;
    self->iso_layer = xrdp_iso_create(self, trans);
    self->channel_list = list_create();
    DEBUG(("  out xrdp_mcs_create"));
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
    DEBUG(("xrdp_mcs_delete processed"))
    g_memset(self, 0, sizeof(struct xrdp_mcs)) ;
    g_free(self);
}

/*****************************************************************************/
/* This function sends channel join confirm */
/* returns error = 1 ok = 0 */
static int
xrdp_mcs_send_cjcf(struct xrdp_mcs *self, int userid, int chanid)
{
    struct stream *s;

    DEBUG(("  in xrdp_mcs_send_cjcf"));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        DEBUG(("  out xrdp_mcs_send_cjcf error"));
        return 1;
    }

    out_uint8(s, (MCS_CJCF << 2) | 2);
    out_uint8(s, 0);
    out_uint16_be(s, userid);
    out_uint16_be(s, chanid); /* TODO Explain why we send this two times */
    out_uint16_be(s, chanid);
    s_mark_end(s);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        DEBUG(("  out xrdp_mcs_send_cjcf error"));
        return 1;
    }

    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_cjcf"));
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_recv(struct xrdp_mcs *self, struct stream *s, int *chan)
{
    int appid;
    int opcode;
    int len;
    int userid;
    int chanid;
    DEBUG(("  in xrdp_mcs_recv"));

    while (1)
    {
        if (xrdp_iso_recv(self->iso_layer, s) != 0)
        {
            DEBUG(("   out xrdp_mcs_recv, xrdp_iso_recv return non zero"));
            g_writeln("xrdp_mcs_recv: xrdp_iso_recv failed");
            return 1;
        }

        if (!s_check_rem(s, 1))
        {
            return 1;
        }

        in_uint8(s, opcode);
        appid = opcode >> 2;

        if (appid == MCS_DPUM) /* Disconnect Provider Ultimatum */
        {
            g_writeln("received Disconnect Provider Ultimatum");
            DEBUG(("  out xrdp_mcs_recv appid != MCS_DPUM"));
            return 1;
        }

        /* this is channels getting added from the client */
        if (appid == MCS_CJRQ)
        {

            if (!s_check_rem(s, 4))
            {
                return 1;
            }

            in_uint16_be(s, userid);
            in_uint16_be(s, chanid);
            log_message(LOG_LEVEL_DEBUG,"MCS_CJRQ - channel join request received");
            DEBUG(("xrdp_mcs_recv  adding channel %4.4x", chanid));

            if (xrdp_mcs_send_cjcf(self, userid, chanid) != 0)
            {
                log_message(LOG_LEVEL_ERROR,"Non handled error from xrdp_mcs_send_cjcf") ;
            }

            s = libxrdp_force_read(self->iso_layer->trans);
            if (s == 0)
            {
                g_writeln("xrdp_mcs_recv: libxrdp_force_read failed");
                return 1;
            }

            continue;
        }

        if (appid == MCS_SDRQ || appid == MCS_SDIN)
        {
            break;
        }
        else
        {
            log_message(LOG_LEVEL_DEBUG,"Received an unhandled appid:%d",appid);
        }

        break;
    }

    if (appid != MCS_SDRQ)
    {
        DEBUG(("  out xrdp_mcs_recv err got 0x%x need MCS_SDRQ", appid));
        return 1;
    }

    if (!s_check_rem(s, 6))
    {
        return 1;
    }

    in_uint8s(s, 2);
    in_uint16_be(s, *chan);
    in_uint8s(s, 1);
    in_uint8(s, len);

    if (len & 0x80)
    {
        if (!s_check_rem(s, 1))
        {
            return 1;
        }
        in_uint8s(s, 1);
    }

    DEBUG(("  out xrdp_mcs_recv"));
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_ber_parse_header(struct xrdp_mcs *self, struct stream *s,
                          int tag_val, int *len)
{
    int tag;
    int l;
    int i;

    if (tag_val > 0xff)
    {
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint16_be(s, tag);
    }
    else
    {
        if (!s_check_rem(s, 1))
        {
            return 1;
        }
        in_uint8(s, tag);
    }

    if (tag != tag_val)
    {
        return 1;
    }

    if (!s_check_rem(s, 1))
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
            if (!s_check_rem(s, 1))
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

    if (s_check(s))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_parse_domain_params(struct xrdp_mcs *self, struct stream *s)
{
    int len;

    if (xrdp_mcs_ber_parse_header(self, s, MCS_TAG_DOMAIN_PARAMS, &len) != 0)
    {
        return 1;
    }

    if ((len < 0) || !s_check_rem(s, len))
    {
        return 1;
    }

    in_uint8s(s, len);

    if (s_check(s))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_recv_connect_initial(struct xrdp_mcs *self)
{
    int len;
    struct stream *s;

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, MCS_CONNECT_INITIAL, &len) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        return 1;
    }

    if ((len < 0) || !s_check_rem(s, len))
    {
        return 1;
    }

    in_uint8s(s, len);

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        return 1;
    }

    if ((len < 0) || !s_check_rem(s, len))
    {
        return 1;
    }

    in_uint8s(s, len);

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_BOOLEAN, &len) != 0)
    {
        return 1;
    }

    if ((len < 0) || !s_check_rem(s, len))
    {
        return 1;
    }

    in_uint8s(s, len);

    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_parse_domain_params(self, s) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_ber_parse_header(self, s, BER_TAG_OCTET_STRING, &len) != 0)
    {
        return 1;
    }

    /* mcs data can not be zero length */
    if ((len <= 0) || (len > 16 * 1024))
    {
        return 1;
    }

    if (!s_check_rem(s, len))
    {
        return 1;
    }

    /* make a copy of client mcs data */
    init_stream(self->client_mcs_data, len);
    out_uint8a(self->client_mcs_data, s->p, len);
    in_uint8s(s, len);
    s_mark_end(self->client_mcs_data);

    if (s_check_end(s))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_recv_edrq(struct xrdp_mcs *self)
{
    int opcode;
    struct stream *s;

    DEBUG(("    in xrdp_mcs_recv_edrq"));

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        return 1;
    }

    if (!s_check_rem(s, 1))
    {
        return 1;
    }

    in_uint8(s, opcode);

    if ((opcode >> 2) != MCS_EDRQ)
    {
        return 1;
    }

    if (!s_check_rem(s, 4))
    {
        return 1;
    }

    in_uint8s(s, 2);
    in_uint8s(s, 2);

    if (opcode & 2)
    {
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint16_be(s, self->userid);
    }

    if (!(s_check_end(s)))
    {
        return 1;
    }

    DEBUG(("    out xrdp_mcs_recv_edrq"));
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_recv_aurq(struct xrdp_mcs *self)
{
    int opcode;
    struct stream *s;

    DEBUG(("    in xrdp_mcs_recv_aurq"));

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        return 1;
    }

    if (!s_check_rem(s, 1))
    {
        return 1;
    }

    in_uint8(s, opcode);

    if ((opcode >> 2) != MCS_AURQ)
    {
        return 1;
    }

    if (opcode & 2)
    {
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint16_be(s, self->userid);
    }

    if (!(s_check_end(s)))
    {
        return 1;
    }

    DEBUG(("    out xrdp_mcs_recv_aurq"));
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_send_aucf(struct xrdp_mcs *self)
{
    struct stream *s;

    DEBUG(("  in xrdp_mcs_send_aucf"));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        DEBUG(("  out xrdp_mcs_send_aucf error"));
        return 1;
    }

    out_uint8(s, ((MCS_AUCF << 2) | 2));
    out_uint8s(s, 1);
    out_uint16_be(s, self->userid);
    s_mark_end(s);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        DEBUG(("  out xrdp_mcs_send_aucf error"));
        return 1;
    }

    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_aucf"));
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_recv_cjrq(struct xrdp_mcs *self)
{
    int opcode;
    struct stream *s;

    s = libxrdp_force_read(self->iso_layer->trans);
    if (s == 0)
    {
        return 1;
    }

    if (xrdp_iso_recv(self->iso_layer, s) != 0)
    {
        return 1;
    }

    if (!s_check_rem(s, 1))
    {
        return 1;
    }

    in_uint8(s, opcode);

    if ((opcode >> 2) != MCS_CJRQ)
    {
        return 1;
    }

    if (!s_check_rem(s, 4))
    {
        return 1;
    }

    in_uint8s(s, 4);

    if (opcode & 2)
    {
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint8s(s, 2);
    }

    if (!(s_check_end(s)))
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
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
    xrdp_mcs_ber_out_int8(self, s, 1);
    xrdp_mcs_ber_out_int8(self, s, 0);
    xrdp_mcs_ber_out_int8(self, s, 1);
    xrdp_mcs_ber_out_int24(self, s, max_pdu_size);
    xrdp_mcs_ber_out_int8(self, s, 2);
    return 0;
}
/*****************************************************************************/
/* prepare server gcc data to send in mcs response msg */
int
xrdp_mcs_out_gcc_data(struct xrdp_sec *self)
{
    struct stream *s;
    int num_channels_even;
    int num_channels;
    int index;
    int channel;
    int gcc_size;
    char* gcc_size_ptr;
    char* ud_ptr;

    num_channels = self->mcs_layer->channel_list->count;
    num_channels_even = num_channels + (num_channels & 1);
    s = &(self->server_mcs_data);
    init_stream(s, 8192);
    out_uint16_be(s, 5); /* AsnBerObjectIdentifier */
    out_uint16_be(s, 0x14);
    out_uint8(s, 0x7c);
    out_uint16_be(s, 1); /* -- */
    out_uint8(s, 0x2a);  /* ConnectPDULen */
    out_uint8(s, 0x14);
    out_uint8(s, 0x76);
    out_uint8(s, 0x0a);
    out_uint8(s, 1);
    out_uint8(s, 1);
    out_uint8(s, 0);
    out_uint16_le(s, 0xc001);
    out_uint8(s, 0);
    out_uint8(s, 0x4d); /* M */
    out_uint8(s, 0x63); /* c */
    out_uint8(s, 0x44); /* D */
    out_uint8(s, 0x6e); /* n */
    /* GCC Response Total Length - 2 bytes , set later */
    gcc_size_ptr = s->p; /* RDPGCCUserDataResponseLength */
    out_uint8s(s, 2);
    ud_ptr = s->p; /* User Data */

    out_uint16_le(s, SEC_TAG_SRV_INFO);
    if (self->mcs_layer->iso_layer->rdpNegData)
    {
        out_uint16_le(s, 12); /* len */
    }
    else
    {
        out_uint16_le(s, 8); /* len */
    }
    out_uint8(s, 4); /* 4 = rdp5 1 = rdp4 */
    out_uint8(s, 0);
    out_uint8(s, 8);
    out_uint8(s, 0);
    if (self->mcs_layer->iso_layer->rdpNegData)
    {
         /* RequestedProtocol */
        out_uint32_le(s, self->mcs_layer->iso_layer->requestedProtocol);
    }
    out_uint16_le(s, SEC_TAG_SRV_CHANNELS);
    out_uint16_le(s, 8 + (num_channels_even * 2)); /* len */
    out_uint16_le(s, MCS_GLOBAL_CHANNEL); /* 1003, 0x03eb main channel */
    out_uint16_le(s, num_channels); /* number of other channels */

    for (index = 0; index < num_channels_even; index++)
    {
        if (index < num_channels)
        {
            channel = MCS_GLOBAL_CHANNEL + (index + 1);
            out_uint16_le(s, channel);
        }
        else
        {
            out_uint16_le(s, 0);
        }
    }

    if (self->rsa_key_bytes == 64)
    {
        g_writeln("xrdp_sec_out_mcs_data: using 512 bit RSA key");
        out_uint16_le(s, SEC_TAG_SRV_CRYPT);
        out_uint16_le(s, 0x00ec); /* len is 236 */
        out_uint32_le(s, self->crypt_method);
        out_uint32_le(s, self->crypt_level);
        out_uint32_le(s, 32); /* 32 bytes random len */
        out_uint32_le(s, 0xb8); /* 184 bytes rsa info(certificate) len */
        out_uint8a(s, self->server_random, 32);
        /* here to end is certificate */
        /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
        /* TermService\Parameters\Certificate */
        out_uint32_le(s, 1);
        out_uint32_le(s, 1);
        out_uint32_le(s, 1);
        out_uint16_le(s, SEC_TAG_PUBKEY); /* 0x0006 */
        out_uint16_le(s, 0x005c); /* 92 bytes length of SEC_TAG_PUBKEY */
        out_uint32_le(s, SEC_RSA_MAGIC); /* 0x31415352 'RSA1' */
        out_uint32_le(s, 0x0048); /* 72 bytes modulus len */
        out_uint32_be(s, 0x00020000); /* bit len */
        out_uint32_be(s, 0x3f000000); /* data len */
        out_uint8a(s, self->pub_exp, 4); /* pub exp */
        out_uint8a(s, self->pub_mod, 64); /* pub mod */
        out_uint8s(s, 8); /* pad */
        out_uint16_le(s, SEC_TAG_KEYSIG); /* 0x0008 */
        out_uint16_le(s, 72); /* len */
        out_uint8a(s, self->pub_sig, 64); /* pub sig */
        out_uint8s(s, 8); /* pad */
    }
    else if (self->rsa_key_bytes == 256)
    {
        g_writeln("xrdp_sec_out_mcs_data: using 2048 bit RSA key");
        out_uint16_le(s, SEC_TAG_SRV_CRYPT);
        out_uint16_le(s, 0x01ac); /* len is 428 */
        out_uint32_le(s, self->crypt_method);
        out_uint32_le(s, self->crypt_level);
        out_uint32_le(s, 32); /* 32 bytes random len */
        out_uint32_le(s, 0x178); /* 376 bytes rsa info(certificate) len */
        out_uint8a(s, self->server_random, 32);
        /* here to end is certificate */
        /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
        /* TermService\Parameters\Certificate */
        out_uint32_le(s, 1);
        out_uint32_le(s, 1);
        out_uint32_le(s, 1);
        out_uint16_le(s, SEC_TAG_PUBKEY); /* 0x0006 */
        out_uint16_le(s, 0x011c); /* 284 bytes length of SEC_TAG_PUBKEY */
        out_uint32_le(s, SEC_RSA_MAGIC); /* 0x31415352 'RSA1' */
        out_uint32_le(s, 0x0108); /* 264 bytes modulus len */
        out_uint32_be(s, 0x00080000); /* bit len */
        out_uint32_be(s, 0xff000000); /* data len */
        out_uint8a(s, self->pub_exp, 4); /* pub exp */
        out_uint8a(s, self->pub_mod, 256); /* pub mod */
        out_uint8s(s, 8); /* pad */
        out_uint16_le(s, SEC_TAG_KEYSIG); /* 0x0008 */
        out_uint16_le(s, 72); /* len */
        out_uint8a(s, self->pub_sig, 64); /* pub sig */
        out_uint8s(s, 8); /* pad */
    }
    else if (self->rsa_key_bytes == 0) /* no security */
    {
        g_writeln("xrdp_sec_out_mcs_data: using no security");
        out_uint16_le(s, SEC_TAG_SRV_CRYPT);
        out_uint16_le(s, 12); /* len is 12 */
        out_uint32_le(s, self->crypt_method);
        out_uint32_le(s, self->crypt_level);
    }
    else
    {
        g_writeln("xrdp_sec_out_mcs_data: error");
    }
    /* end certificate */
    s_mark_end(s);

    gcc_size = (int)(s->end - ud_ptr) | 0x8000;
    gcc_size_ptr[0] = gcc_size >> 8;
    gcc_size_ptr[1] = gcc_size;

    return 0;
}
/*****************************************************************************/
/* returns error */
static int
xrdp_mcs_send_connect_response(struct xrdp_mcs *self)
{
    int data_len;
    struct stream *s;

    DEBUG(("  in xrdp_mcs_send_connect_response"));
    make_stream(s);
    init_stream(s, 8192);
    data_len = (int) (self->server_mcs_data->end - self->server_mcs_data->data);
    xrdp_iso_init(self->iso_layer, s);
    //TODO: we should calculate the whole length include MCS_CONNECT_RESPONSE
    xrdp_mcs_ber_out_header(self, s, MCS_CONNECT_RESPONSE,
            data_len > 0x80 ? data_len + 38 : data_len + 36);
    xrdp_mcs_ber_out_header(self, s, BER_TAG_RESULT, 1);
    out_uint8(s, 0);
    xrdp_mcs_ber_out_header(self, s, BER_TAG_INTEGER, 1);
    out_uint8(s, 0);
    xrdp_mcs_out_domain_params(self, s, 22, 3, 0, 0xfff8);
    xrdp_mcs_ber_out_header(self, s, BER_TAG_OCTET_STRING, data_len);
    /* mcs data */
    out_uint8a(s, self->server_mcs_data->data, data_len);
    s_mark_end(s);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        DEBUG(("  out xrdp_mcs_send_connect_response error"));
        return 1;
    }

    free_stream(s);
    DEBUG(("  out xrdp_mcs_send_connect_response"));
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_incoming(struct xrdp_mcs *self)
{
    int index;

    DEBUG(("  in xrdp_mcs_incoming"));

    if (xrdp_mcs_recv_connect_initial(self) != 0)
    {
        return 1;
    }

    /* in xrdp_sec.c */
    if (xrdp_sec_process_mcs_data(self->sec_layer) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_out_gcc_data(self->sec_layer) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_send_connect_response(self) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_recv_edrq(self) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_recv_aurq(self) != 0)
    {
        return 1;
    }

    if (xrdp_mcs_send_aucf(self) != 0)
    {
        return 1;
    }

    for (index = 0; index < self->channel_list->count + 2; index++)
    {
        if (xrdp_mcs_recv_cjrq(self) != 0)
        {
            return 1;
        }

        if (xrdp_mcs_send_cjcf(self, self->userid,
                      self->userid + MCS_USERCHANNEL_BASE + index) != 0)
        {
            return 1;
        }
    }

    DEBUG(("  out xrdp_mcs_incoming"));
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
            /* in xrdp_wm.c */
            rv = session->callback(session->id, 0x5556, 0, 0, 0, 0);
        }
        else
        {
            g_writeln("in xrdp_mcs_send, session->callback is nil");
        }
    }
    else
    {
        g_writeln("in xrdp_mcs_send, session is nil");
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_send(struct xrdp_mcs *self, struct stream *s, int chan)
{
    int len;
    char *lp;
    //static int max_len = 0;

    DEBUG(("  in xrdp_mcs_send"));
    s_pop_layer(s, mcs_hdr);
    len = (s->end - s->p) - 8;

    if (len > 8192 * 2)
    {
        g_writeln("error in xrdp_mcs_send, size too big: %d bytes", len);
    }

    //if (len > max_len)
    //{
    //  max_len = len;
    //  g_printf("mcs max length is %d\r\n", max_len);
    //}
    //g_printf("mcs length %d max length is %d\r\n", len, max_len);
    //g_printf("mcs length %d\r\n", len);
    out_uint8(s, MCS_SDIN << 2);
    out_uint16_be(s, self->userid);
    out_uint16_be(s, chan);
    out_uint8(s, 0x70);

    if (len >= 128)
    {
        len = len | 0x8000;
        out_uint16_be(s, len);
    }
    else
    {
        out_uint8(s, len);
        /* move everything up one byte */
        lp = s->p;

        while (lp < s->end)
        {
            lp[0] = lp[1];
            lp++;
        }

        s->end--;
    }

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        DEBUG(("  out xrdp_mcs_send error"));
        return 1;
    }

    /* todo, do we need to call this for every mcs packet,
       maybe every 5 or so */
    if (chan == MCS_GLOBAL_CHANNEL)
    {
        xrdp_mcs_call_callback(self);
    }

    DEBUG(("  out xrdp_mcs_send"));
    return 0;
}

/**
 * Internal help function to close the socket
 * @param self
 */
void
close_rdp_socket(struct xrdp_mcs *self)
{
    if (self->iso_layer != 0)
    {
        if (self->iso_layer->trans != 0)
        {
            trans_shutdown_tls_mode(self->iso_layer->trans);
            g_tcp_close(self->iso_layer->trans->sck);
            self->iso_layer->trans->sck = 0 ;
            g_writeln("xrdp_mcs_disconnect - socket closed");
            return;
        }
    }
    g_writeln("Failed to close socket");
}

/*****************************************************************************/
/* returns error */
int
xrdp_mcs_disconnect(struct xrdp_mcs *self)
{
    struct stream *s;

    DEBUG(("  in xrdp_mcs_disconnect"));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_iso_init(self->iso_layer, s) != 0)
    {
        free_stream(s);
        close_rdp_socket(self);
        DEBUG(("  out xrdp_mcs_disconnect error - 1"));
        return 1;
    }

    out_uint8(s, (MCS_DPUM << 2) | 1);
    out_uint8(s, 0x80);
    s_mark_end(s);

    if (xrdp_iso_send(self->iso_layer, s) != 0)
    {
        free_stream(s);
        close_rdp_socket(self);
        DEBUG(("  out xrdp_mcs_disconnect error - 2"));
        return 1;
    }

    free_stream(s);
    close_rdp_socket(self);
    DEBUG(("xrdp_mcs_disconnect - close sent"));
    return 0;
}
