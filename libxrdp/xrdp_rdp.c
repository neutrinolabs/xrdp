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
 * rdp layer
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"
#include "ssl_calls.h"
#include "string_calls.h"

#if defined(XRDP_NEUTRINORDP)
#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#endif



#define FASTPATH_FRAG_SIZE (16 * 1024 - 128)

/*****************************************************************************/
static int
xrdp_rdp_read_config(const char *xrdp_ini, struct xrdp_client_info *client_info)
{
    int index = 0;
    struct list *items = (struct list *)NULL;
    struct list *values = (struct list *)NULL;
    char *item = NULL;
    char *value = NULL;
    int pos;
    char *tmp = NULL;
    int tmp_length = 0;

    client_info->xrdp_keyboard_overrides.type = -1;
    client_info->xrdp_keyboard_overrides.subtype = -1;
    client_info->xrdp_keyboard_overrides.layout = -1;

    /* initialize (zero out) local variables: */
    items = list_create();
    items->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    LOG_DEVEL(LOG_LEVEL_TRACE, "Reading config file %s", xrdp_ini);
    file_by_name_read_section(xrdp_ini, "globals", items, values);

    for (index = 0; index < items->count; index++)
    {
        item = (char *)list_get_item(items, index);
        value = (char *)list_get_item(values, index);
        LOG(LOG_LEVEL_DEBUG, "item %s, value %s", item, value);

        if (g_strcasecmp(item, "bitmap_cache") == 0)
        {
            client_info->use_bitmap_cache = g_text2bool(value);
        }
        else if (g_strcasecmp(item, "bitmap_compression") == 0)
        {
            client_info->use_bitmap_comp = g_text2bool(value);
        }
        else if (g_strcasecmp(item, "bulk_compression") == 0)
        {
            client_info->use_bulk_comp = g_text2bool(value);
        }
        else if (g_strcasecmp(item, "crypt_level") == 0)
        {
            if (g_strcasecmp(value, "none") == 0)
            {
                client_info->crypt_level = 0;
            }
            else if (g_strcasecmp(value, "low") == 0)
            {
                client_info->crypt_level = 1;
            }
            else if (g_strcasecmp(value, "medium") == 0)
            {
                client_info->crypt_level = 2;
            }
            else if (g_strcasecmp(value, "high") == 0)
            {
                client_info->crypt_level = 3;
            }
            else if (g_strcasecmp(value, "fips") == 0)
            {
                client_info->crypt_level = 4;
            }
            else
            {
                LOG(LOG_LEVEL_WARNING, "Your configured crypt level is "
                    "undefined, 'high' will be used");
                client_info->crypt_level = 3;
            }
        }
        else if (g_strcasecmp(item, "allow_channels") == 0)
        {
            client_info->channels_allowed = g_text2bool(value);
            if (client_info->channels_allowed == 0)
            {
                LOG(LOG_LEVEL_INFO, "All channels are disabled");
            }
        }
        else if (g_strcasecmp(item, "allow_multimon") == 0)
        {
            client_info->multimon = g_text2bool(value);
            if (client_info->multimon == 0)
            {
                LOG(LOG_LEVEL_INFO, "Multi monitor server support disabled");
            }
        }
        else if (g_strcasecmp(item, "max_bpp") == 0)
        {
            client_info->max_bpp = g_atoi(value);
        }
        else if (g_strcasecmp(item, "rfx_min_pixel") == 0)
        {
            client_info->rfx_min_pixel = g_atoi(value);
        }
        else if (g_strcasecmp(item, "new_cursors") == 0)
        {
            client_info->pointer_flags = g_text2bool(value) == 0 ? 2 : 0;
        }
        else if (g_strcasecmp(item, "require_credentials") == 0)
        {
            client_info->require_credentials = g_text2bool(value);
        }
        else if (g_strcasecmp(item, "enable_token_login") == 0)
        {
            client_info->enable_token_login = g_text2bool(value);
        }
        else if (g_strcasecmp(item, "use_fastpath") == 0)
        {
            if (g_strcasecmp(value, "output") == 0)
            {
                client_info->use_fast_path = 1;
            }
            else if (g_strcasecmp(value, "input") == 0)
            {
                client_info->use_fast_path = 2;
            }
            else if (g_strcasecmp(value, "both") == 0)
            {
                client_info->use_fast_path = 3;
            }
            else if (g_strcasecmp(value, "none") == 0)
            {
                client_info->use_fast_path = 0;
            }
            else
            {
                LOG(LOG_LEVEL_WARNING, "Your configured fastpath level is "
                    "undefined, fastpath will not be used");
                client_info->use_fast_path = 0;
            }
        }
        else if (g_strcasecmp(item, "ssl_protocols") == 0)
        {
            /* put leading/trailing comma to properly detect "TLSv1" without regex */
            tmp_length = g_strlen(value) + 3;
            tmp = g_new(char, tmp_length);
            g_snprintf(tmp, tmp_length, "%s%s%s", ",", value, ",");
            /* replace all spaces with comma */
            /* to accept space after comma */
            while ((pos = g_pos(tmp, " ")) != -1)
            {
                tmp[pos] = ',';
            }
            ssl_get_protocols_from_string(tmp, &(client_info->ssl_protocols));
            g_free(tmp);
        }
        else if (g_strcasecmp(item, "tls_ciphers") == 0)
        {
            client_info->tls_ciphers = g_strdup(value);
        }
        else if (g_strcasecmp(item, "security_layer") == 0)
        {
            if (g_strcasecmp(value, "rdp") == 0)
            {
                client_info->security_layer = PROTOCOL_RDP;
            }
            else if (g_strcasecmp(value, "tls") == 0)
            {
                client_info->security_layer = PROTOCOL_SSL;
            }
            else if (g_strcasecmp(value, "hybrid") == 0)
            {
                client_info->security_layer = PROTOCOL_SSL | PROTOCOL_HYBRID;
            }
            else if (g_strcasecmp(value, "negotiate") == 0)
            {
                client_info->security_layer = PROTOCOL_SSL | PROTOCOL_HYBRID | PROTOCOL_HYBRID_EX;
            }
            else
            {
                LOG(LOG_LEVEL_WARNING, "security_layer=%s is not "
                    "recognized, will use security_layer=negotiate",
                    value);
                client_info->security_layer = PROTOCOL_SSL | PROTOCOL_HYBRID | PROTOCOL_HYBRID_EX;
            }
        }
        else if (g_strcasecmp(item, "certificate") == 0)
        {
            g_memset(client_info->certificate, 0, sizeof(char) * 1024);
            if (g_strlen(value) == 0)
            {
                /* default certificate path */
                g_snprintf(client_info->certificate, 1023, "%s/cert.pem", XRDP_CFG_PATH);
                LOG(LOG_LEVEL_INFO,
                    "Using default X.509 certificate: %s",
                    client_info->certificate);

            }
            else if (value[0] != '/')
            {
                /* default certificate path */
                g_snprintf(client_info->certificate, 1023, "%s/cert.pem", XRDP_CFG_PATH);
                LOG(LOG_LEVEL_WARNING,
                    "X.509 certificate should use absolute path, using "
                    "default instead: %s", client_info->certificate);
            }
            else
            {
                /* use user defined certificate */
                g_strncpy(client_info->certificate, value, 1023);
            }

            if (!g_file_readable(client_info->certificate))
            {
                LOG(LOG_LEVEL_ERROR, "Cannot read certificate file %s: %s",
                    client_info->certificate, g_get_strerror());
            }
        }
        else if (g_strcasecmp(item, "key_file") == 0)
        {
            g_memset(client_info->key_file, 0, sizeof(char) * 1024);
            if (g_strlen(value) == 0)
            {
                /* default key_file path */
                g_snprintf(client_info->key_file, 1023, "%s/key.pem", XRDP_CFG_PATH);
                LOG(LOG_LEVEL_INFO, "Using default X.509 key file: %s",
                    client_info->key_file);
            }
            else if (value[0] != '/')
            {
                /* default key_file path */
                g_snprintf(client_info->key_file, 1023, "%s/key.pem", XRDP_CFG_PATH);
                LOG(LOG_LEVEL_WARNING,
                    "X.509 key file should use absolute path, using "
                    "default instead: %s", client_info->key_file);
            }
            else
            {
                /* use user defined key_file */
                g_strncpy(client_info->key_file, value, 1023);
            }

            if (!g_file_readable(client_info->key_file))
            {
                LOG(LOG_LEVEL_ERROR, "Cannot read private key file %s: %s",
                    client_info->key_file, g_get_strerror());
            }
        }
        else if (g_strcasecmp(item, "domain_user_separator") == 0
                 && g_strlen(value) > 0)
        {
            g_strncpy(client_info->domain_user_separator, value, sizeof(client_info->domain_user_separator) - 1);
        }
        else if (g_strcasecmp(item, "xrdp.override_keyboard_type") == 0)
        {
            client_info->xrdp_keyboard_overrides.type = g_atoix(value);
        }
        else if (g_strcasecmp(item, "xrdp.override_keyboard_subtype") == 0)
        {
            client_info->xrdp_keyboard_overrides.subtype = g_atoix(value);
        }
        else if (g_strcasecmp(item, "xrdp.override_keylayout") == 0)
        {
            client_info->xrdp_keyboard_overrides.layout = g_atoix(value);
        }
    }

    list_delete(items);
    list_delete(values);
    return 0;
}

#if defined(XRDP_NEUTRINORDP)
/*****************************************************************************/
static void
cpuid(tui32 info, tui32 *eax, tui32 *ebx, tui32 *ecx, tui32 *edx)
{
#ifdef __GNUC__
#if defined(__i386__) || defined(__x86_64__)
    __asm volatile
    (
        /* The EBX (or RBX register on x86_64) is used for the PIC base address
           and must not be corrupted by our inline assembly. */
#if defined(__i386__)
        "mov %%ebx, %%esi;"
        "cpuid;"
        "xchg %%ebx, %%esi;"
#else
        "mov %%rbx, %%rsi;"
        "cpuid;"
        "xchg %%rbx, %%rsi;"
#endif
        : "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (info)
    );
#endif
#endif
}

/*****************************************************************************/
static tui32
xrdp_rdp_detect_cpu(void)
{
    tui32 eax;
    tui32 ebx;
    tui32 ecx;
    tui32 edx;
    tui32 cpu_opt;

    eax = 0;
    ebx = 0;
    ecx = 0;
    edx = 0;
    cpu_opt = 0;
    cpuid(1, &eax, &ebx, &ecx, &edx);

    if (edx & (1 << 26))
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "SSE2 detected");
        cpu_opt |= CPU_SSE2;
    }

    return cpu_opt;
}
#endif

/*****************************************************************************/
struct xrdp_rdp *
xrdp_rdp_create(struct xrdp_session *session, struct trans *trans)
{
    struct xrdp_rdp *self = (struct xrdp_rdp *)NULL;

    LOG_DEVEL(LOG_LEVEL_TRACE, "in xrdp_rdp_create");
    self = (struct xrdp_rdp *)g_malloc(sizeof(struct xrdp_rdp), 1);
    self->session = session;
    self->share_id = 66538;
    /* read ini settings */
    xrdp_rdp_read_config(session->xrdp_ini, &self->client_info);
    /* create sec layer */
    self->sec_layer = xrdp_sec_create(self, trans);
    /* default 8 bit v1 color bitmap cache entries and size */
    self->client_info.cache1_entries = 600;
    self->client_info.cache1_size = 256;
    self->client_info.cache2_entries = 300;
    self->client_info.cache2_size = 1024;
    self->client_info.cache3_entries = 262;
    self->client_info.cache3_size = 4096;
    /* load client ip info */
    g_sck_get_peer_ip_address(trans->sck,
                              self->client_info.client_ip,
                              sizeof(self->client_info.client_ip),
                              NULL);
    g_sck_get_peer_description(trans->sck,
                               self->client_info.client_description,
                               sizeof(self->client_info.client_description));
    self->mppc_enc = mppc_enc_new(PROTO_RDP_50);
#if defined(XRDP_NEUTRINORDP)
    self->rfx_enc = rfx_context_new();
    rfx_context_set_cpu_opt(self->rfx_enc, xrdp_rdp_detect_cpu());
#endif
    self->client_info.size = sizeof(self->client_info);
    self->client_info.version = CLIENT_INFO_CURRENT_VERSION;
    LOG_DEVEL(LOG_LEVEL_TRACE, "out xrdp_rdp_create");
    return self;
}

/*****************************************************************************/
void
xrdp_rdp_delete(struct xrdp_rdp *self)
{
    if (self == 0)
    {
        return;
    }

    xrdp_sec_delete(self->sec_layer);
    mppc_enc_free(self->mppc_enc);
#if defined(XRDP_NEUTRINORDP)
    rfx_context_free((RFX_CONTEXT *)(self->rfx_enc));
#endif
    g_free(self->client_info.tls_ciphers);
    g_free(self);
}

/*****************************************************************************/
/* Initialize the stream for sending a [MS-RDPBCGR] Control PDU */
int
xrdp_rdp_init(struct xrdp_rdp *self, struct stream *s)
{
    if (xrdp_sec_init(self->sec_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_init: xrdp_sec_init failed");
        return 1;
    }

    s_push_layer(s, rdp_hdr, 6); /* 6 = sizeof(TS_SHARECONTROLHEADER) */
    return 0;
}

/*****************************************************************************/
/* Initialize the stream for sending a [MS-RDPBCGR] Data PDU */
int
xrdp_rdp_init_data(struct xrdp_rdp *self, struct stream *s)
{
    if (xrdp_sec_init(self->sec_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_init_data: xrdp_sec_init failed");
        return 1;
    }

    s_push_layer(s, rdp_hdr, 18);  /* 18 = sizeof(TS_SHAREDATAHEADER) */
    return 0;
}

/*****************************************************************************/
/*
  Receives and parses pdu code from next data unit.

  @param self
  @param (in/out) s: the stream to read from. Upon return the stream is ?
  @param (out) code: the pdu code from the packet
  returns error
  */
int
xrdp_rdp_recv(struct xrdp_rdp *self, struct stream *s, int *code)
{
    int error = 0;
    int len = 0;
    int pdu_code = 0;
    int chan = 0;
    const tui8 *header;


    if (s->next_packet == 0 || s->next_packet >= s->end)
    {
        /* check for fastpath first */
        header = (const tui8 *) (s->p);
        if (header[0] != 0x3)
        {
            if (xrdp_sec_recv_fastpath(self->sec_layer, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "xrdp_rdp_recv: xrdp_sec_recv_fastpath failed");
                return 1;
            }
            /* next_packet gets set in xrdp_sec_recv_fastpath */
            *code = 2; // special code for fastpath input
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp_recv: out code 2 (fastpath)");
            return 0;
        }

        /* not fastpath, do tpkt */
        chan = 0;
        error = xrdp_sec_recv(self->sec_layer, s, &chan);

        if (error == -1) /* special code for send demand active */
        {
            s->next_packet = 0;
            *code = -1;
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp_recv: out code -1 (send demand active)");
            return 0;
        }

        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_rdp_recv: xrdp_sec_recv failed");
            return 1;
        }

        if ((chan != MCS_GLOBAL_CHANNEL) && (chan > 0))
        {
            if (chan > MCS_GLOBAL_CHANNEL)
            {
                if (xrdp_channel_process(self->sec_layer->chan_layer, s, chan) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "xrdp_rdp_recv: xrdp_channel_process failed");
                }
            }
            else
            {
                if (chan != 1)
                {
                    LOG_DEVEL(LOG_LEVEL_WARNING,
                              "xrdp_rdp_recv: Wrong channel Id to be handled "
                              "by xrdp_channel_process, channel id %d", chan);
                }
            }

            s->next_packet = 0;
            *code = 0;
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp_recv: out code 0 (skip data) "
                      "data processed by channel id %d", chan);
            return 0;
        }

        s->next_packet = s->p;
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp_recv: stream not touched");
        s->p = s->next_packet;
    }

    if (!s_check_rem_and_log(s, 6, "Parsing [MS-RDPBCGR] TS_SHARECONTROLHEADER"))
    {
        s->next_packet = 0;
        *code = 0;
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_recv: out code 0 (skip data) "
            "bad RDP packet");
        return 0;
    }
    else
    {
        in_uint16_le(s, len);      /* totalLength */
        in_uint16_le(s, pdu_code); /* pduType */
        *code = pdu_code & 0xf;
        in_uint8s(s, 2);           /* pduSource */
        s->next_packet += len;
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_SHARECONTROLHEADER "
                  "totalLength %d, pduType.type %s (%d), pduType.PDUVersion %d, "
                  "pduSource (ignored)", len, PDUTYPE_TO_STR(*code), *code,
                  ((pdu_code & 0xfff0) >> 4));
        return 0;
    }
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] Control PDU with for the given pduType with the headers
   added */
int
xrdp_rdp_send(struct xrdp_rdp *self, struct stream *s, int pdu_type)
{
    int len = 0;

    s_pop_layer(s, rdp_hdr);
    len = s->end - s->p;

    /* TS_SHARECONTROLHEADER */
    out_uint16_le(s, len);               /* totalLength */
    out_uint16_le(s, 0x10 | pdu_type);   /* pduType */
    out_uint16_le(s, self->mcs_channel); /* pduSource */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SHARECONTROLHEADER "
              "totalLength %d, pduType.type %s (%d), pduType.PDUVersion %d, "
              "pduSource %d", len, PDUTYPE_TO_STR(pdu_type & 0xf),
              pdu_type & 0xf, (((0x10 | pdu_type) & 0xfff0) >> 4),
              self->mcs_channel);

    if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send: xrdp_sec_send failed");
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] Data PDU for the given pduType2 from
 * the specified source with the headers
    added and data compressed */
int
xrdp_rdp_send_data_from_channel(struct xrdp_rdp *self, struct stream *s,
                                int data_pdu_type, int channel_id,
                                int compress)
{
    int len;
    int ctype;
    int clen;
    int dlen;
    int pdulen;
    int pdutype;
    int tocomplen;
    int iso_offset;
    int mcs_offset;
    int sec_offset;
    int rdp_offset;
    struct stream ls;
    struct xrdp_mppc_enc *mppc_enc;

    s_pop_layer(s, rdp_hdr);
    len = (int)(s->end - s->p);
    pdutype = 0x10 | PDUTYPE_DATAPDU;
    pdulen = len;
    dlen = len;
    ctype = 0;
    clen = len;
    tocomplen = pdulen - 18;

    if (compress && self->client_info.rdp_compression &&
            self->session->up_and_running)
    {
        mppc_enc = self->mppc_enc;
        if (compress_rdp(mppc_enc, (tui8 *)(s->p + 18), tocomplen))
        {
            clen = mppc_enc->bytes_in_opb + 18;
            pdulen = clen;
            ctype = mppc_enc->flags;
            iso_offset = (int)(s->iso_hdr - s->data);
            mcs_offset = (int)(s->mcs_hdr - s->data);
            sec_offset = (int)(s->sec_hdr - s->data);
            rdp_offset = (int)(s->rdp_hdr - s->data);

            /* outputBuffer has 64 bytes preceding it */
            ls.data = mppc_enc->outputBuffer - (rdp_offset + 18);
            ls.p = ls.data + rdp_offset;
            ls.end = ls.p + clen;
            ls.size = s->end - s->data;
            ls.iso_hdr = ls.data + iso_offset;
            ls.mcs_hdr = ls.data + mcs_offset;
            ls.sec_hdr = ls.data + sec_offset;
            ls.rdp_hdr = ls.data + rdp_offset;
            ls.channel_hdr = 0;
            ls.next_packet = 0;
            s = &ls;
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_TRACE,
                      "xrdp_rdp_send_data_from_channel: "
                      "compress_rdp failed, sending "
                      "uncompressed data. type %d, flags %d",
                      mppc_enc->protocol_type, mppc_enc->flags);
        }
    }

    /* TS_SHARECONTROLHEADER */
    out_uint16_le(s, pdulen);            /* totalLength */
    out_uint16_le(s, pdutype);           /* pduType */
    out_uint16_le(s, channel_id);        /* pduSource */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SHARECONTROLHEADER "
              "totalLength %d, pduType.type %s (%d), pduType.PDUVersion %d, "
              "pduSource %d", pdulen, PDUTYPE_TO_STR(pdutype & 0xf),
              pdutype & 0xf, ((pdutype & 0xfff0) >> 4), channel_id);

    /* TS_SHAREDATAHEADER */
    out_uint32_le(s, self->share_id);
    out_uint8(s, 0); /* pad */
    out_uint8(s, 1); /* streamID */
    out_uint16_le(s, dlen); /* uncompressedLength */
    out_uint8(s, data_pdu_type); /* pduType2 */
    out_uint8(s, ctype); /* compressedType */
    out_uint16_le(s, clen); /* compressedLength */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SHAREDATAHEADER "
              "shareID %d, streamID 1, uncompressedLength %d, "
              "pduType2 0x%2.2x, compressedType 0x%2.2x, compressedLength %d",
              self->share_id, dlen, data_pdu_type, ctype, clen);

    if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_data_from_channel: "
            "xrdp_sec_send failed");
        return 1;
    }

    return 0;
}


/*****************************************************************************/
/* Send a [MS-RDPBCGR] Data PDU on the MCS channel for the given pduType2
 * with the headers added and data compressed */
int
xrdp_rdp_send_data(struct xrdp_rdp *self, struct stream *s,
                   int data_pdu_type)
{
    return xrdp_rdp_send_data_from_channel(self, s, data_pdu_type,
                                           self->mcs_channel, 1);
}

/*****************************************************************************/
/* returns the fastpath rdp byte count */
int
xrdp_rdp_get_fastpath_bytes(struct xrdp_rdp *self)
{
    if (self->client_info.rdp_compression)
    {
        return 4;
    }
    return 3;
}

/*****************************************************************************/
int
xrdp_rdp_init_fastpath(struct xrdp_rdp *self, struct stream *s)
{
    if (xrdp_sec_init_fastpath(self->sec_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_init_fastpath: xrdp_sec_init_fastpath failed");
        return 1;
    }
    if (self->client_info.rdp_compression)
    {
        s_push_layer(s, rdp_hdr, 4);
    }
    else
    {
        s_push_layer(s, rdp_hdr, 3);
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* 2.2.9.1.2.1 Fast-Path Update (TS_FP_UPDATE)
 * http://msdn.microsoft.com/en-us/library/cc240622.aspx */
int
xrdp_rdp_send_fastpath(struct xrdp_rdp *self, struct stream *s,
                       int data_pdu_type)
{
    int updateHeader;
    int updateCode;
    int fragmentation;
    int compression;
    int comp_type;
    int comp_len;
    int no_comp_len;
    int send_len;
    int cont;
    int header_bytes;
    int sec_bytes;
    int to_comp_len;
    int sec_offset;
    int rdp_offset;
    struct stream frag_s;
    struct stream comp_s;
    struct stream send_s;
    struct xrdp_mppc_enc *mppc_enc;
    char comp_type_str[7];
    comp_type_str[0] = '\0';

    s_pop_layer(s, rdp_hdr);
    updateCode = data_pdu_type;
    if (self->client_info.rdp_compression)
    {
        compression = 2;
        header_bytes = 4;
    }
    else
    {
        compression = 0;
        header_bytes = 3;
    }
    sec_bytes = xrdp_sec_get_fastpath_bytes(self->sec_layer);
    fragmentation = 0;
    frag_s = *s;
    sec_offset = (int)(frag_s.sec_hdr - frag_s.data);
    rdp_offset = (int)(frag_s.rdp_hdr - frag_s.data);
    cont = 1;
    while (cont)
    {
        comp_type = 0;
        send_s = frag_s;
        no_comp_len = (int)(frag_s.end - frag_s.p);
        if (no_comp_len > FASTPATH_FRAG_SIZE)
        {
            no_comp_len = FASTPATH_FRAG_SIZE;
            if (fragmentation == 0)
            {
                fragmentation = 2; /* FASTPATH_FRAGMENT_FIRST */
            }
            else if (fragmentation == 2)
            {
                fragmentation = 3; /* FASTPATH_FRAGMENT_NEXT */
            }
        }
        else
        {
            if (fragmentation != 0)
            {
                fragmentation = 1; /* FASTPATH_FRAGMENT_LAST */
            }
        }
        send_len = no_comp_len;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp_send_fastpath: no_comp_len %d, fragmentation %d",
                  no_comp_len, fragmentation);
        if ((compression != 0) && (no_comp_len > header_bytes + 16))
        {
            to_comp_len = no_comp_len - header_bytes;
            mppc_enc = self->mppc_enc;
            if (compress_rdp(mppc_enc, (tui8 *)(frag_s.p + header_bytes),
                             to_comp_len))
            {
                comp_len = mppc_enc->bytes_in_opb + header_bytes;
                send_len = comp_len;
                comp_type = mppc_enc->flags;
                /* outputBuffer has 64 bytes preceding it */
                g_memset(&comp_s, 0, sizeof(comp_s));
                comp_s.data = mppc_enc->outputBuffer -
                              (rdp_offset + header_bytes);
                comp_s.p = comp_s.data + rdp_offset;
                comp_s.end = comp_s.p + send_len;
                comp_s.size = send_len;
                comp_s.sec_hdr = comp_s.data + sec_offset;
                comp_s.rdp_hdr = comp_s.data + rdp_offset;
                send_s = comp_s;
            }
            else
            {
                LOG(LOG_LEVEL_DEBUG,
                    "compress_rdp failed, sending uncompressed data. "
                    "type %d, flags %d", mppc_enc->protocol_type,
                    mppc_enc->flags);
            }
        }
        updateHeader = (updateCode & 15) |
                       ((fragmentation & 3) << 4) |
                       ((compression & 3) << 6);

        send_s.end = send_s.p + send_len;
        send_s.size = send_s.end - send_s.data;
        out_uint8(&send_s, updateHeader);
        if (compression != 0)
        {
            out_uint8(&send_s, comp_type);
            g_snprintf(comp_type_str, 7, "0x%4.4x", comp_type);
        }
        send_len -= header_bytes;
        out_uint16_le(&send_s, send_len);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_FP_UPDATE "
                  "updateCode %d, fragmentation %d, compression %d, compressionFlags %s, size %d",
                  updateCode, fragmentation, compression,
                  (compression ? comp_type_str : "(not present)"), send_len);
        if (xrdp_sec_send_fastpath(self->sec_layer, &send_s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_fastpath: xrdp_sec_send_fastpath failed");
            return 1;
        }
        frag_s.p += no_comp_len;
        cont = frag_s.p < frag_s.end;
        frag_s.p -= header_bytes;
        frag_s.sec_hdr = frag_s.p - sec_bytes;
        frag_s.data = frag_s.sec_hdr;
    }
    return 0;
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] TS_UPDATE_SYNC or TS_FP_UPDATE_SYNCHRONIZE message
   depending on if the client supports the fast path capability or not */
int
xrdp_rdp_send_data_update_sync(struct xrdp_rdp *self)
{
    struct stream *s = (struct stream *)NULL;

    make_stream(s);
    init_stream(s, 8192);

    if (self->client_info.use_fast_path & 1) /* fastpath output supported */
    {
        if (xrdp_rdp_init_fastpath(self, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_data_update_sync: xrdp_rdp_init_fastpath failed");
            free_stream(s);
            return 1;
        }
    }
    else /* slowpath */
    {
        if (xrdp_rdp_init_data(self, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_data_update_sync: xrdp_rdp_init_data failed");
            free_stream(s);
            return 1;
        }
        out_uint16_le(s, RDP_UPDATE_SYNCHRONIZE); /* updateType */
        out_uint16_le(s, 0); /* pad */

    }

    s_mark_end(s);

    if (self->client_info.use_fast_path & 1) /* fastpath output supported */
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_UPDATE_SYNCHRONIZE");
        if (xrdp_rdp_send_fastpath(self, s,
                                   FASTPATH_UPDATETYPE_SYNCHRONIZE) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_FP_UPDATE_SYNCHRONIZE failed");
            free_stream(s);
            return 1;
        }
    }
    else /* slowpath */
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_UPDATE_SYNC "
                  "updateType %s (%d)",
                  GRAPHICS_UPDATE_TYPE_TO_STR(RDP_UPDATE_SYNCHRONIZE),
                  RDP_UPDATE_SYNCHRONIZE);
        if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_UPDATE) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_UPDATE_SYNC failed");
            free_stream(s);
            return 1;
        }
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
int
xrdp_rdp_incoming(struct xrdp_rdp *self)
{
    struct xrdp_iso *iso;

    iso = self->sec_layer->mcs_layer->iso_layer;

    if (xrdp_sec_incoming(self->sec_layer) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_incoming: xrdp_sec_incoming failed");
        return 1;
    }
    self->mcs_channel = self->sec_layer->mcs_layer->userid +
                        MCS_USERCHANNEL_BASE;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_rdp->mcs_channel %d", self->mcs_channel);

    /* log TLS version and cipher of TLS connections */
    if (iso->selectedProtocol > PROTOCOL_RDP)
    {
        LOG(LOG_LEVEL_INFO,
            "TLS connection established from %s %s with cipher %s",
            self->client_info.client_description,
            iso->trans->ssl_protocol,
            iso->trans->cipher_name);
    }
    /* log non-TLS connections */
    else
    {
        int crypt_level = self->sec_layer->crypt_level;
        const char *security_level =
            (crypt_level == CRYPT_LEVEL_NONE) ? "none" :
            (crypt_level == CRYPT_LEVEL_LOW) ? "low" :
            (crypt_level == CRYPT_LEVEL_CLIENT_COMPATIBLE) ? "medium" :
            (crypt_level == CRYPT_LEVEL_HIGH) ? "high" :
            (crypt_level == CRYPT_LEVEL_FIPS) ? "fips" :
            /* default */ "unknown";

        LOG(LOG_LEVEL_INFO,
            "Non-TLS connection established from %s with security level : %s",
            self->client_info.client_description, security_level);
    }

    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_POINTER_PDU message */
static int
xrdp_rdp_process_data_pointer(struct xrdp_rdp *self, struct stream *s)
{
    LOG_DEVEL(LOG_LEVEL_WARNING, "Protocol error ignored: a [MS-RDPBCGR] "
              "TS_SHAREDATAHEADER PDUTYPE2_POINTER was received by the server "
              "but this type of PDU is only suppose to be sent by the server "
              "to the client.");
    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_INPUT_PDU_DATA message */
static int
xrdp_rdp_process_data_input(struct xrdp_rdp *self, struct stream *s)
{
    int num_events;
    int index;
    int msg_type;
    int device_flags;
    int param1;
    int param2;
    int time;

    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_INPUT_PDU_DATA"))
    {
        return 1;
    }
    in_uint16_le(s, num_events);
    in_uint8s(s, 2); /* pad */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_INPUT_PDU_DATA "
              "numEvents %d", num_events);

    for (index = 0; index < num_events; index++)
    {
        if (!s_check_rem_and_log(s, 12, "Parsing [MS-RDPBCGR] TS_INPUT_EVENT"))
        {
            return 1;
        }
        in_uint32_le(s, time);
        in_uint16_le(s, msg_type);
        in_uint16_le(s, device_flags);
        in_sint16_le(s, param1);
        in_sint16_le(s, param2);
        LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT "
                  "eventTime %d, messageType 0x%4.4x", time, msg_type);

        switch (msg_type)
        {
            case RDP_INPUT_SYNCHRONIZE:
                LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT - TS_SYNC_EVENT "
                          "toggleFlags 0x%8.8x", ((param2 << 16) | param1));
                break;
            case RDP_INPUT_SCANCODE:
                LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT - TS_KEYBOARD_EVENT "
                          "keyboardFlags 0x%4.4x, keyCode %d", device_flags, param1);
                break;
            case RDP_INPUT_UNICODE:
                LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT - TS_UNICODE_KEYBOARD_EVENT "
                          "keyboardFlags 0x%4.4x, unicodeCode %d", device_flags, param1);
                break;
            case RDP_INPUT_MOUSE:
                LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT - TS_POINTER_EVENT "
                          "pointerFlags 0x%4.4x, xPos %d, yPos %d",
                          device_flags, param1, param2);
                break;
            case RDP_INPUT_MOUSEX:
                LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_INPUT_EVENT - TS_POINTERX_EVENT "
                          "pointerFlags 0x%4.4x, xPos %d, yPos %d",
                          device_flags, param1, param2);
                break;
            default:
                LOG_DEVEL(LOG_LEVEL_WARNING, "Received unknown [MS-RDPBCGR] TS_INPUT_EVENT "
                          "messageType 0x%4.4x", msg_type);
                break;
        }

        if (self->session->callback != 0)
        {
            /* msg_type can be
               RDP_INPUT_SYNCHRONIZE - 0
               RDP_INPUT_SCANCODE - 4
               RDP_INPUT_MOUSE - 0x8001
               RDP_INPUT_MOUSEX - 0x8002 */
            /* call to xrdp_wm.c : callback */
            self->session->callback(self->session->id, msg_type, param1, param2,
                                    device_flags, time);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Bug: no callback registered for xrdp_rdp_process_data_input");
        }
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "Processing [MS-RDPBCGR] TS_INPUT_PDU_DATA complete");
    return 0;
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] TS_SYNCHRONIZE_PDU message */
static int
xrdp_rdp_send_synchronise(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_synchronise: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, 1); /* messageType (2 bytes) */
    out_uint16_le(s, 1002); /* targetUser (2 bytes) */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_SYNCHRONIZE_PDU "
              "messageType 1, targetUser 1002");

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_SYNCHRONISE) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_SYNCHRONIZE_PDU failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] TS_CONTROL_PDU message */
static int
xrdp_rdp_send_control(struct xrdp_rdp *self, int action)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_control: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, action);
    out_uint16_le(s, 0); /* userid */
    out_uint32_le(s, 1002); /* control id */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_CONTROL_PDU "
              "action %d, grantId 0, controlId 1002", action);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_CONTROL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_CONTROL_PDU failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_CONTROL_PDU message */
static int
xrdp_rdp_process_data_control(struct xrdp_rdp *self, struct stream *s)
{
    int action;

    in_uint16_le(s, action);
    in_uint8s(s, 2); /* user id */
    in_uint8s(s, 4); /* control id */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_CONTROL_PDU "
              "action 0x%4.4x, grantId (ignored), controlId (ignored)",
              action);

    if (action == RDP_CTL_REQUEST_CONTROL)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "Responding to [MS-RDPBCGR] TS_CONTROL_PDU "
                  "action CTRLACTION_REQUEST_CONTROL with 3 messages: "
                  "TS_SYNCHRONIZE_PDU, TS_CONTROL_PDU with CTRLACTION_COOPERATE, "
                  " and TS_CONTROL_PDU with CTRLACTION_GRANTED_CONTROL");
        xrdp_rdp_send_synchronise(self);
        xrdp_rdp_send_control(self, RDP_CTL_COOPERATE);
        xrdp_rdp_send_control(self, RDP_CTL_GRANT_CONTROL);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_WARNING, "Received [MS-RDPBCGR] TS_CONTROL_PDU "
                  "action %d is unknown (skipped)", action);
    }

    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_SYNCHRONIZE_PDU message */
static int
xrdp_rdp_process_data_sync(struct xrdp_rdp *self)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_SYNCHRONIZE_PDU - no-op");
    return 0;
}

/*****************************************************************************/
/* 2.2.11.2.1 Refresh Rect PDU Data (TS_REFRESH_RECT_PDU) */
static int
xrdp_rdp_process_screen_update(struct xrdp_rdp *self, struct stream *s)
{
    int index;
    int num_rects;
    int left;
    int top;
    int right;
    int bottom;
    int cx;
    int cy;

    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_REFRESH_RECT_PDU"))
    {
        return 1;
    }
    in_uint8(s, num_rects);
    in_uint8s(s, 3); /* pad */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_REFRESH_RECT_PDU "
              "numberOfAreas %d", num_rects);
    for (index = 0; index < num_rects; index++)
    {
        if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] TS_RECTANGLE16"))
        {
            return 1;
        }
        /* Inclusive Rectangle (TS_RECTANGLE16) */
        in_uint16_le(s, left);
        in_uint16_le(s, top);
        in_uint16_le(s, right);
        in_uint16_le(s, bottom);
        LOG_DEVEL(LOG_LEVEL_TRACE, "With field [MS-RDPBCGR] TS_RECTANGLE16 "
                  "left %d, top %d, right %d, bottom %d",
                  left, top, right, bottom);
        cx = (right - left) + 1;
        cy = (bottom - top) + 1;
        if (self->session->callback != 0)
        {
            self->session->callback(self->session->id, 0x4444,
                                    left, top, cx, cy);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_WARNING,
                      "Bug: no callback registered for xrdp_rdp_process_screen_update");
        }
    }
    return 0;
}

/*****************************************************************************/
/* Send a [MS-RDPBCGR] TS_FONT_MAP_PDU message */
static int
xrdp_rdp_send_fontmap(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_rdp_send_fontmap: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, 0); /* numberEntries */
    out_uint16_le(s, 0); /* totalNumEntries */
    out_uint16_le(s, 0x3); /* mapFlags (sequence flags) */
    out_uint16_le(s, 0x4); /* entrySize */

    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FONT_MAP_PDU "
              "numberEntries 0, totalNumEntries 0, mapFlags 0x0003, entrySize 4");

    if (xrdp_rdp_send_data(self, s, 0x28) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Sending [MS-RDPBCGR] TS_FONT_MAP_PDU failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_FONT_LIST_PDU message */
static int
xrdp_rdp_process_data_font(struct xrdp_rdp *self, struct stream *s)
{
    int seq;

    if (!s_check_rem_and_log(s, 6, "Parsing [MS-RDPBCGR] TS_FONT_LIST_PDU"))
    {
        return 1;
    }

    in_uint8s(s, 2); /* NumberFonts: 0x0, SHOULD be set to 0 */
    in_uint8s(s, 2); /* TotalNumberFonts: 0x0, SHOULD be set to 0 */
    in_uint16_le(s, seq); /* ListFlags */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FONT_LIST_PDU "
              "numberFonts (ignored), totalNumFonts (ignored), listFlags 0x%4.4x",
              seq);

    /* 419 client sends Seq 1, then 2 */
    /* 2600 clients sends only Seq 3 */
    /* listFlags SHOULD be set to 0x0003, which is the logical OR'd value of
       FONTLIST_FIRST (0x0001) and FONTLIST_LAST (0x0002) */
    if (seq == 2 || seq == 3) /* after second font message, we are up and */
    {
        /* running */
        LOG_DEVEL(LOG_LEVEL_DEBUG,
                  "Client sent FONTLIST_LAST, replying with server fontmap");
        xrdp_rdp_send_fontmap(self);

        self->session->up_and_running = 1;
        LOG_DEVEL(LOG_LEVEL_INFO, "yeah, up_and_running");
        xrdp_rdp_send_data_update_sync(self);

        /* This is also the end of an Deactivation-reactivation
         * sequence [MS-RDPBCGR] 1.3.1.3 */
        xrdp_rdp_suppress_output(self, 0, XSO_REASON_DEACTIVATE_REACTIVATE,
                                 0, 0,
                                 self->client_info.display_sizes.session_width,
                                 self->client_info.display_sizes.session_height);

        if (self->session->callback != 0)
        {
            /* call to xrdp_wm.c : callback */
            self->session->callback(self->session->id, 0x555a, 0, 0,
                                    0, 0);
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "Received [MS-RDPBCGR] TS_FONT_LIST_PDU "
                  "without FONTLIST_LAST in the listFlags field. Ignoring message.");
    }

    return 0;
}

/*****************************************************************************/
/* Send a Sending [MS-RDPBCGR] TS_SHUTDOWN_DENIED_PDU message */
static int
xrdp_rdp_send_disconnect_query_response(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_rdp_send_disconnect_query_response: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_SHUTDOWN_DENIED_PDU");

    if (xrdp_rdp_send_data(self, s, PDUTYPE2_SHUTDOWN_DENIED) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Sending [MS-RDPBCGR] TS_SHUTDOWN_DENIED_PDU failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

#if 0 /* not used */
/*****************************************************************************/
/* Send a [MS-RDPBCGR] TS_SET_ERROR_INFO_PDU message */
static int
xrdp_rdp_send_disconnect_reason(struct xrdp_rdp *self, int reason)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_rdp_send_disconnect_reason: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, reason);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_SET_ERROR_INFO_PDU "
              "errorInfo 0x%8.8x", reason);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_DISCONNECT) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Sending [MS-RDPBCGR] TS_SET_ERROR_INFO_PDU failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}
#endif

/*****************************************************************************/
/* Process a [MS-RDPRFX] TS_FRAME_ACKNOWLEDGE_PDU message */
static int
xrdp_rdp_process_frame_ack(struct xrdp_rdp *self, struct stream *s)
{
    int frame_id;

    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPRFX] TS_FRAME_ACKNOWLEDGE_PDU"))
    {
        return 1;
    }
    in_uint32_le(s, frame_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPRFX] TS_FRAME_ACKNOWLEDGE_PDU "
              "frameID %d", frame_id);
    if (self->session->callback != 0)
    {
        /* call to xrdp_wm.c : callback */
        self->session->callback(self->session->id, 0x5557, frame_id, 0,
                                0, 0);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_WARNING,
                  "Bug: no callback registered for xrdp_rdp_process_frame_ack");
    }
    return 0;
}

/*****************************************************************************/
void
xrdp_rdp_suppress_output(struct xrdp_rdp *self, int suppress,
                         enum suppress_output_reason reason,
                         int left, int top, int right, int bottom)
{
    int old_suppress = self->client_info.suppress_output_mask != 0;
    if (suppress)
    {
        self->client_info.suppress_output_mask |= (unsigned int)reason;
    }
    else
    {
        self->client_info.suppress_output_mask &= ~(unsigned int)reason;
    }

    int current_suppress =  self->client_info.suppress_output_mask != 0;
    if (current_suppress != old_suppress && self->session->callback != 0)
    {
        self->session->callback(self->session->id, 0x5559, suppress,
                                MAKELONG(left, top),
                                MAKELONG(right, bottom), 0);
    }
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_SUPPRESS_OUTPUT_PDU message */
static int
xrdp_rdp_process_suppress(struct xrdp_rdp *self, struct stream *s)
{
    int rv = 1;
    int allowDisplayUpdates;
    int left;
    int top;
    int right;
    int bottom;

    if (s_check_rem_and_log(s, 1, "Parsing [MS-RDPBCGR] TS_SUPPRESS_OUTPUT_PDU"))
    {
        in_uint8(s, allowDisplayUpdates);
        LOG_DEVEL(LOG_LEVEL_TRACE,
                  "Received [MS-RDPBCGR] TS_SUPPRESS_OUTPUT_PDU "
                  "allowDisplayUpdates %d", allowDisplayUpdates);
        switch (allowDisplayUpdates)
        {
            case 0: /* SUPPRESS_DISPLAY_UPDATES */
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "Client requested display output to be suppressed");
                xrdp_rdp_suppress_output(self, 1,
                                         XSO_REASON_CLIENT_REQUEST,
                                         0, 0, 0, 0);
                rv = 0;
                break;
            case 1: /* ALLOW_DISPLAY_UPDATES */
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "Client requested display output to be enabled");
                if (s_check_rem_and_log(s, 11,
                                        "Parsing [MS-RDPBCGR] Padding and TS_RECTANGLE16"))
                {
                    in_uint8s(s, 3); /* pad */
                    in_uint16_le(s, left);
                    in_uint16_le(s, top);
                    in_uint16_le(s, right);
                    in_uint16_le(s, bottom);
                    LOG_DEVEL(LOG_LEVEL_TRACE,
                              "Received [MS-RDPBCGR] TS_RECTANGLE16 "
                              "left %d, top %d, right %d, bottom %d",
                              left, top, right, bottom);
                    xrdp_rdp_suppress_output(self, 0,
                                             XSO_REASON_CLIENT_REQUEST,
                                             left, top, right, bottom);
                    rv = 0;
                }
                break;
        }
    }
    return rv;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_SHAREDATAHEADER message based on it's pduType2 */
int
xrdp_rdp_process_data(struct xrdp_rdp *self, struct stream *s)
{
    int uncompressedLength;
    int pduType2;
    int compressedType;
    int compressedLength;

    if (!s_check_rem_and_log(s, 12, "Parsing [MS-RDPBCGR] TS_SHAREDATAHEADER"))
    {
        return 1;
    }
    in_uint8s(s, 6); /* shareID (4 bytes), padding (1 byte), streamID (1 byte) */
    in_uint16_le(s, uncompressedLength); /* shareID */
    in_uint8(s, pduType2);
    in_uint8(s, compressedType);
    in_uint16_le(s, compressedLength);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_SHAREDATAHEADER "
              "shareID (ignored), streamID (ignored), uncompressedLength %d, "
              "pduType2 0x%2.2x, compressedType 0x%2.2x, compressedLength %d",
              uncompressedLength, pduType2, compressedType, compressedLength);

    if (compressedType != 0)
    {
        /* don't support compression */
        /* PACKET_COMPR_TYPE_8K = 0x00 */
        LOG(LOG_LEVEL_ERROR, "Only RDP 4.0 bulk compression "
            "(PACKET_COMPR_TYPE_8K) is supported by XRDP");
        return 1;
    }
    if (compressedLength > uncompressedLength)
    {
        LOG(LOG_LEVEL_ERROR, "The compressed length %d is larger than "
            "the uncompressed length %d, failing the processing of this "
            "PDU", compressedLength, uncompressedLength);
        return 1;
    }

    switch (pduType2)
    {
        case RDP_DATA_PDU_POINTER: /* 27(0x1b) */
            xrdp_rdp_process_data_pointer(self, s);
            break;
        case RDP_DATA_PDU_INPUT: /* 28(0x1c) */
            xrdp_rdp_process_data_input(self, s);
            break;
        case RDP_DATA_PDU_CONTROL: /* 20(0x14) */
            xrdp_rdp_process_data_control(self, s);
            break;
        case RDP_DATA_PDU_SYNCHRONISE: /* 31(0x1f) */
            xrdp_rdp_process_data_sync(self);
            break;
        case PDUTYPE2_REFRESH_RECT:
            xrdp_rdp_process_screen_update(self, s);
            break;
        case PDUTYPE2_SUPPRESS_OUTPUT: /* 35(0x23) */
            xrdp_rdp_process_suppress(self, s);
            break;
        case PDUTYPE2_SHUTDOWN_REQUEST: /* 36(0x24) ?? disconnect query? */
            /* when this message comes, send a 37 back so the client */
            /* is sure the connection is alive and it can ask if user */
            /* really wants to disconnect */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_SHUTDOWN_REQ_PDU");
            xrdp_rdp_send_disconnect_query_response(self); /* send a 37 back */
            break;
        case RDP_DATA_PDU_FONT2: /* 39(0x27) */
            xrdp_rdp_process_data_font(self, s);
            break;
        case 56: /* PDUTYPE2_FRAME_ACKNOWLEDGE 0x38 */
            xrdp_rdp_process_frame_ack(self, s);
            break;
        default:
            LOG(LOG_LEVEL_WARNING,
                "Received unknown [MS-RDPBCGR] TS_SHAREDATAHEADER pduType2 %d (ignoring)",
                pduType2);
            break;
    }
    return 0;
}
/*****************************************************************************/
int
xrdp_rdp_disconnect(struct xrdp_rdp *self)
{
    return xrdp_sec_disconnect(self->sec_layer);
}

/*****************************************************************************/
int
xrdp_rdp_send_deactivate(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init(self, s) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_deactivate: xrdp_rdp_init failed");
        return 1;
    }

    /* TODO: why are all the fields missing from the TS_DEACTIVATE_ALL_PDU? */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_DEACTIVATE_ALL_PDU "
              "shareID <not set>, lengthSourceDescriptor <not set>, "
              "sourceDescriptor <not set>");

    if (xrdp_rdp_send(self, s, PDUTYPE_DEACTIVATEALLPDU) != 0)
    {
        free_stream(s);
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_DEACTIVATE_ALL_PDU failed");
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/** Send a [MS-RDPBCGR] TS_SAVE_SESSION_INFO_PDU_DATA message.
 *
 * @param self
 * @param data the data to send to the client in the
 *      TS_SAVE_SESSION_INFO_PDU_DATA message. The first 4 bytes of the data
 *      buffer MUST by the infoType value as specified in MS-RDPBCGR 2.2.10.1.1
 * @param data_bytes the length of the data buffer
 * @returns error code
 */
int
xrdp_rdp_send_session_info(struct xrdp_rdp *self, const char *data,
                           int data_bytes)
{
    struct stream *s;

    if (data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "data must not be null");
        return 1;
    }
    if (data_bytes < 4)
    {
        LOG(LOG_LEVEL_ERROR, "data_bytes must greater than or equal to 4");
        return 1;
    }

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_rdp_send_session_info: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    if (!s_check_rem_out_and_log(s, data_bytes, "Sending [MS-RDPBCGR] TS_SAVE_SESSION_INFO_PDU_DATA"))
    {
        free_stream(s);
        return 1;
    }

    out_uint8a(s, data, data_bytes);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_SAVE_SESSION_INFO_PDU_DATA "
              "infoType 0x%8.8x, infoData <omitted from log>",
              *((unsigned int *) data));

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_LOGON) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPBCGR] TS_SAVE_SESSION_INFO_PDU_DATA failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

