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
#include "log.h"
#include "ssl_calls.h"

#if defined(XRDP_NEUTRINORDP)
#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#endif

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { g_write _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { g_writeln _args ; } } while (0)

#define FASTPATH_FRAG_SIZE (16 * 1024 - 128)

/*****************************************************************************/
static int
xrdp_rdp_read_config(struct xrdp_client_info *client_info)
{
    int index = 0;
    struct list *items = (struct list *)NULL;
    struct list *values = (struct list *)NULL;
    char *item = NULL;
    char *value = NULL;
    char cfg_file[256];
    int pos;
    char *tmp = NULL;
    int tmp_length = 0;

    /* initialize (zero out) local variables: */
    g_memset(cfg_file, 0, sizeof(char) * 256);

    items = list_create();
    items->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    g_snprintf(cfg_file, 255, "%s/xrdp.ini", XRDP_CFG_PATH);
    DEBUG(("cfg_file %s", cfg_file));
    file_by_name_read_section(cfg_file, "globals", items, values);

    for (index = 0; index < items->count; index++)
    {
        item = (char *)list_get_item(items, index);
        value = (char *)list_get_item(values, index);
        DEBUG(("item %s value %s", item, value));

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
                log_message(LOG_LEVEL_ALWAYS,"Warning: Your configured crypt level is "
                          "undefined, 'high' will be used");
                client_info->crypt_level = 3;
            }
        }
        else if (g_strcasecmp(item, "allow_channels") == 0)
        {
            client_info->channels_allowed = g_text2bool(value);
            if (client_info->channels_allowed == 0)
            {
                log_message(LOG_LEVEL_DEBUG,"Info - All channels are disabled");
            }
        }
        else if (g_strcasecmp(item, "allow_multimon") == 0)
                {
                    client_info->multimon = g_text2bool(value);
                    if (client_info->multimon == 0)
                    {
                        log_message(LOG_LEVEL_DEBUG,"Info - Multi monitor server support disabled");
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
                log_message(LOG_LEVEL_ALWAYS,"Warning: Your configured fastpath level is "
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
                log_message(LOG_LEVEL_ERROR, "security_layer=%s is not "
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
                log_message(LOG_LEVEL_INFO,
                            "Using default X.509 certificate: %s",
                            client_info->certificate);

            }
            else if (value[0] != '/')
            {
                /* default certificate path */
                g_snprintf(client_info->certificate, 1023, "%s/cert.pem", XRDP_CFG_PATH);
                log_message(LOG_LEVEL_WARNING,
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
                log_message(LOG_LEVEL_ERROR, "Cannot read certificate file %s: %s",
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
                log_message(LOG_LEVEL_INFO, "Using default X.509 key file: %s",
                            client_info->key_file);
            }
            else if (value[0] != '/')
            {
                /* default key_file path */
                g_snprintf(client_info->key_file, 1023, "%s/key.pem", XRDP_CFG_PATH);
                log_message(LOG_LEVEL_WARNING,
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
                log_message(LOG_LEVEL_ERROR, "Cannot read private key file %s: %s",
                            client_info->key_file, g_get_strerror());
            }
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
        DEBUG(("SSE2 detected"));
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
    int bytes;

    DEBUG(("in xrdp_rdp_create"));
    self = (struct xrdp_rdp *)g_malloc(sizeof(struct xrdp_rdp), 1);
    self->session = session;
    self->share_id = 66538;
    /* read ini settings */
    xrdp_rdp_read_config(&self->client_info);
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
    bytes = sizeof(self->client_info.client_ip) - 1;
    g_write_ip_address(trans->sck, self->client_info.client_ip, bytes);
    self->mppc_enc = mppc_enc_new(PROTO_RDP_50);
#if defined(XRDP_NEUTRINORDP)
    self->rfx_enc = rfx_context_new();
    rfx_context_set_cpu_opt(self->rfx_enc, xrdp_rdp_detect_cpu());
#endif
    self->client_info.size = sizeof(self->client_info);
    DEBUG(("out xrdp_rdp_create"));
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
int
xrdp_rdp_init(struct xrdp_rdp *self, struct stream *s)
{
    if (xrdp_sec_init(self->sec_layer, s) != 0)
    {
        return 1;
    }

    s_push_layer(s, rdp_hdr, 6);
    return 0;
}

/*****************************************************************************/
int
xrdp_rdp_init_data(struct xrdp_rdp *self, struct stream *s)
{
    if (xrdp_sec_init(self->sec_layer, s) != 0)
    {
        return 1;
    }

    s_push_layer(s, rdp_hdr, 18);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_rdp_recv(struct xrdp_rdp *self, struct stream *s, int *code)
{
    int error = 0;
    int len = 0;
    int pdu_code = 0;
    int chan = 0;
    const tui8 *header;

    DEBUG(("in xrdp_rdp_recv"));
    if (s->next_packet == 0 || s->next_packet >= s->end)
    {
        /* check for fastpath first */
        header = (const tui8 *) (s->p);
        if (header[0] != 0x3)
        {
            if (xrdp_sec_recv_fastpath(self->sec_layer, s) != 0)
            {
                return 1;
            }
            /* next_packet gets set in xrdp_sec_recv_fastpath */
            *code = 2; // special code for fastpath input
            DEBUG(("out (fastpath) xrdp_rdp_recv"));
            return 0;
        }

        /* not fastpath, do tpkt */
        chan = 0;
        error = xrdp_sec_recv(self->sec_layer, s, &chan);

        if (error == -1) /* special code for send demand active */
        {
            s->next_packet = 0;
            *code = -1;
            DEBUG(("out (1) xrdp_rdp_recv"));
            return 0;
        }

        if (error != 0)
        {
            DEBUG(("out xrdp_rdp_recv error"));
            g_writeln("xrdp_rdp_recv: xrdp_sec_recv failed");
            return 1;
        }

        if ((chan != MCS_GLOBAL_CHANNEL) && (chan > 0))
        {
            if (chan > MCS_GLOBAL_CHANNEL)
            {
                if (xrdp_channel_process(self->sec_layer->chan_layer, s, chan) != 0)
                {
                    g_writeln("xrdp_channel_process returned unhandled error") ;
                }
            }
            else
            {
                if (chan != 1)
                {
                    g_writeln("Wrong channel Id to be handled by xrdp_channel_process %d", chan);
                }
            }

            s->next_packet = 0;
            *code = 0;
            DEBUG(("out (2) xrdp_rdp_recv"));
            return 0;
        }

        s->next_packet = s->p;
    }
    else
    {
        DEBUG(("xrdp_rdp_recv stream not touched"))
        s->p = s->next_packet;
    }

    if (!s_check_rem(s, 6))
    {
        s->next_packet = 0;
        *code = 0;
        DEBUG(("out (3) xrdp_rdp_recv"));
        len = (int)(s->end - s->p);
        g_writeln("xrdp_rdp_recv: bad RDP packet, length [%d]", len);
        return 0;
    }
    else
    {
        in_uint16_le(s, len);
        /*g_writeln("New len received : %d next packet: %d s_end: %d",len,s->next_packet,s->end); */
        in_uint16_le(s, pdu_code);
        *code = pdu_code & 0xf;
        in_uint8s(s, 2); /* mcs user id */
        s->next_packet += len;
        DEBUG(("out (4) xrdp_rdp_recv"));
        return 0;
    }
}

/*****************************************************************************/
int
xrdp_rdp_send(struct xrdp_rdp *self, struct stream *s, int pdu_type)
{
    int len = 0;

    DEBUG(("in xrdp_rdp_send"));
    s_pop_layer(s, rdp_hdr);
    len = s->end - s->p;
    out_uint16_le(s, len);
    out_uint16_le(s, 0x10 | pdu_type);
    out_uint16_le(s, self->mcs_channel);

    if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        DEBUG(("out xrdp_rdp_send error"));
        return 1;
    }

    DEBUG(("out xrdp_rdp_send"));
    return 0;
}

/*****************************************************************************/
int
xrdp_rdp_send_data(struct xrdp_rdp *self, struct stream *s,
                   int data_pdu_type)
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

    DEBUG(("in xrdp_rdp_send_data"));
    s_pop_layer(s, rdp_hdr);
    len = (int)(s->end - s->p);
    pdutype = 0x10 | RDP_PDU_DATA;
    pdulen = len;
    dlen = len;
    ctype = 0;
    clen = len;
    tocomplen = pdulen - 18;

    if (self->client_info.rdp_compression && self->session->up_and_running)
    {
        mppc_enc = self->mppc_enc;
        if (compress_rdp(mppc_enc, (tui8 *)(s->p + 18), tocomplen))
        {
            DEBUG(("mppc_encode ok flags 0x%x bytes_in_opb %d historyOffset %d "
                   "tocomplen %d", mppc_enc->flags, mppc_enc->bytes_in_opb,
                   mppc_enc->historyOffset, tocomplen));

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
            ls.size = clen;
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
            LLOGLN(10, ("xrdp_rdp_send_data: mppc_encode not ok "
                   "type %d flags %d", mppc_enc->protocol_type,
                   mppc_enc->flags));
        }
    }

    out_uint16_le(s, pdulen);
    out_uint16_le(s, pdutype);
    out_uint16_le(s, self->mcs_channel);
    out_uint32_le(s, self->share_id);
    out_uint8(s, 0);
    out_uint8(s, 1);
    out_uint16_le(s, dlen);
    out_uint8(s, data_pdu_type);
    out_uint8(s, ctype);
    out_uint16_le(s, clen);

    if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        DEBUG(("out xrdp_rdp_send_data error"));
        return 1;
    }

    DEBUG(("out xrdp_rdp_send_data"));
    return 0;
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

    LLOGLN(10, ("xrdp_rdp_send_fastpath:"));
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
        LLOGLN(10, ("xrdp_rdp_send_fastpath: no_comp_len %d fragmentation %d",
               no_comp_len, fragmentation));
        if ((compression != 0) && (no_comp_len > header_bytes + 16))
        {
            to_comp_len = no_comp_len - header_bytes;
            mppc_enc = self->mppc_enc;
            if (compress_rdp(mppc_enc, (tui8 *)(frag_s.p + header_bytes),
                             to_comp_len))
            {
                comp_len = mppc_enc->bytes_in_opb + header_bytes;
                LLOGLN(10, ("xrdp_rdp_send_fastpath: no_comp_len %d "
                       "comp_len %d", no_comp_len, comp_len));
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
                LLOGLN(10, ("xrdp_rdp_send_fastpath: mppc_encode not ok "
                       "type %d flags %d", mppc_enc->protocol_type,
                       mppc_enc->flags));
            }
        }
        updateHeader = (updateCode & 15) |
                      ((fragmentation & 3) << 4) |
                      ((compression & 3) << 6);
        out_uint8(&send_s, updateHeader);
        if (compression != 0)
        {
            out_uint8(&send_s, comp_type);
        }
        send_len -= header_bytes;
        out_uint16_le(&send_s, send_len);
        send_s.end = send_s.p + send_len;
        if (xrdp_sec_send_fastpath(self->sec_layer, &send_s) != 0)
        {
            LLOGLN(0, ("xrdp_rdp_send_fastpath: xrdp_fastpath_send failed"));
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
int
xrdp_rdp_send_data_update_sync(struct xrdp_rdp *self)
{
    struct stream *s = (struct stream *)NULL;

    make_stream(s);
    init_stream(s, 8192);
    DEBUG(("in xrdp_rdp_send_data_update_sync"));

    if (self->client_info.use_fast_path & 1) /* fastpath output supported */
    {
        LLOGLN(10, ("xrdp_rdp_send_data_update_sync: fastpath"));
        if (xrdp_rdp_init_fastpath(self, s) != 0)
        {
            free_stream(s);
            return 1;
        }
    }
    else /* slowpath */
    {
        if (xrdp_rdp_init_data(self, s) != 0)
        {
            DEBUG(("out xrdp_rdp_send_data_update_sync error"));
            free_stream(s);
            return 1;
        }
        out_uint16_le(s, RDP_UPDATE_SYNCHRONIZE);
        out_uint16_le(s, 0); /* pad */
    }

    s_mark_end(s);

    if (self->client_info.use_fast_path & 1) /* fastpath output supported */
    {
        if (xrdp_rdp_send_fastpath(self, s,
                                   FASTPATH_UPDATETYPE_SYNCHRONIZE) != 0)
        {
            free_stream(s);
            return 1;
        }
    }
    else /* slowpath */
    {
        if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_UPDATE) != 0)
        {
            DEBUG(("out xrdp_rdp_send_data_update_sync error"));
            free_stream(s);
            return 1;
        }
    }


    DEBUG(("out xrdp_rdp_send_data_update_sync"));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int
xrdp_rdp_incoming(struct xrdp_rdp *self)
{
    struct xrdp_iso *iso;
    iso = self->sec_layer->mcs_layer->iso_layer;

    DEBUG(("in xrdp_rdp_incoming"));

    if (xrdp_sec_incoming(self->sec_layer) != 0)
    {
        return 1;
    }
    self->mcs_channel = self->sec_layer->mcs_layer->userid +
                        MCS_USERCHANNEL_BASE;
    DEBUG(("out xrdp_rdp_incoming mcs channel %d", self->mcs_channel));
    g_strncpy(self->client_info.client_addr, iso->trans->addr,
              sizeof(self->client_info.client_addr) - 1);
    g_strncpy(self->client_info.client_port, iso->trans->port,
              sizeof(self->client_info.client_port) - 1);

    /* log TLS version and cipher of TLS connections */
    if (iso->selectedProtocol > PROTOCOL_RDP)
    {
        log_message(LOG_LEVEL_INFO,
                    "TLS connection established from %s port %s: %s with cipher %s",
                    self->client_info.client_addr,
                    self->client_info.client_port,
                    iso->trans->ssl_protocol,
                    iso->trans->cipher_name);
    }
    /* log non-TLS connections */
    else
    {
        log_message(LOG_LEVEL_INFO,
                    "Non-TLS connection established from %s port %s: "
                    "encrypted with standard RDP security",
                    self->client_info.client_addr,
                    self->client_info.client_port);
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_process_data_pointer(struct xrdp_rdp *self, struct stream *s)
{
    return 0;
}

/*****************************************************************************/
/* RDP_DATA_PDU_INPUT */
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

    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint16_le(s, num_events);
    in_uint8s(s, 2); /* pad */
    DEBUG(("in xrdp_rdp_process_data_input %d events", num_events));

    for (index = 0; index < num_events; index++)
    {
        if (!s_check_rem(s, 12))
        {
            return 1;
        }
        in_uint32_le(s, time);
        in_uint16_le(s, msg_type);
        in_uint16_le(s, device_flags);
        in_sint16_le(s, param1);
        in_sint16_le(s, param2);
        DEBUG(("xrdp_rdp_process_data_input event %4.4x flags %4.4x param1 %d "
               "param2 %d time %d", msg_type, device_flags, param1, param2, time));

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
    }

    DEBUG(("out xrdp_rdp_process_data_input"));
    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_send_synchronise(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, 1); /* messageType (2 bytes) */
    out_uint16_le(s, 1002); /* targetUser (2 bytes) */
    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_SYNCHRONISE) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_send_control(struct xrdp_rdp *self, int action)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, action);
    out_uint16_le(s, 0); /* userid */
    out_uint32_le(s, 1002); /* control id */
    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_CONTROL) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_process_data_control(struct xrdp_rdp *self, struct stream *s)
{
    int action;

    DEBUG(("xrdp_rdp_process_data_control"));
    in_uint16_le(s, action);
    in_uint8s(s, 2); /* user id */
    in_uint8s(s, 4); /* control id */

    if (action == RDP_CTL_REQUEST_CONTROL)
    {
        DEBUG(("xrdp_rdp_process_data_control got RDP_CTL_REQUEST_CONTROL"));
        DEBUG(("xrdp_rdp_process_data_control calling xrdp_rdp_send_synchronise"));
        xrdp_rdp_send_synchronise(self);
        DEBUG(("xrdp_rdp_process_data_control sending RDP_CTL_COOPERATE"));
        xrdp_rdp_send_control(self, RDP_CTL_COOPERATE);
        DEBUG(("xrdp_rdp_process_data_control sending RDP_CTL_GRANT_CONTROL"));
        xrdp_rdp_send_control(self, RDP_CTL_GRANT_CONTROL);
    }
    else
    {
        DEBUG(("xrdp_rdp_process_data_control unknown action"));
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_process_data_sync(struct xrdp_rdp *self)
{
    DEBUG(("xrdp_rdp_process_data_sync"));
    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_process_screen_update(struct xrdp_rdp *self, struct stream *s)
{
    int left;
    int top;
    int right;
    int bottom;
    int cx;
    int cy;

    in_uint8s(s, 4); /* op */
    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, right);
    in_uint16_le(s, bottom);
    cx = (right - left) + 1;
    cy = (bottom - top) + 1;

    if (self->session->callback != 0)
    {
        self->session->callback(self->session->id, 0x4444, left, top, cx, cy);
    }

    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_send_fontmap(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, 0); /* numberEntries */
    out_uint16_le(s, 0); /* totalNumEntries */
    out_uint16_le(s, 0x3); /* mapFlags (sequence flags) */
    out_uint16_le(s, 0x4); /* entrySize */

    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, 0x28) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
xrdp_rdp_process_data_font(struct xrdp_rdp *self, struct stream *s)
{
    int seq;

    DEBUG(("in xrdp_rdp_process_data_font"));
    in_uint8s(s, 2); /* NumberFonts: 0x0, SHOULD be set to 0 */
    in_uint8s(s, 2); /* TotalNumberFonts: 0x0, SHOULD be set to 0 */
    in_uint16_le(s, seq); /* ListFlags */

    /* 419 client sends Seq 1, then 2 */
    /* 2600 clients sends only Seq 3 */
    if (seq == 2 || seq == 3) /* after second font message, we are up and */
    {
        /* running */
        DEBUG(("sending fontmap"));
        xrdp_rdp_send_fontmap(self);

        self->session->up_and_running = 1;
        g_writeln("yeah, up_and_running");
        DEBUG(("up_and_running set"));
        xrdp_rdp_send_data_update_sync(self);
    }

    DEBUG(("out xrdp_rdp_process_data_font"));
    return 0;
}

/*****************************************************************************/
/* sent 37 pdu */
static int
xrdp_rdp_send_disconnect_query_response(struct xrdp_rdp *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, 37) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

#if 0 /* not used */
/*****************************************************************************/
/* sent RDP_DATA_PDU_DISCONNECT 47 pdu */
static int
xrdp_rdp_send_disconnect_reason(struct xrdp_rdp *self, int reason)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, reason);
    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_DISCONNECT) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}
#endif

/*****************************************************************************/
static int
xrdp_rdp_process_frame_ack(struct xrdp_rdp *self, struct stream *s)
{
    int frame_id;

    //g_writeln("xrdp_rdp_process_frame_ack:");
    in_uint32_le(s, frame_id);
    //g_writeln("  frame_id %d", frame_id);
    if (self->session->callback != 0)
    {
        /* call to xrdp_wm.c : callback */
        self->session->callback(self->session->id, 0x5557, frame_id, 0,
                                0, 0);
    }
    return 0;
}

/*****************************************************************************/
/* RDP_PDU_DATA */
int
xrdp_rdp_process_data(struct xrdp_rdp *self, struct stream *s)
{
    int data_type;

    in_uint8s(s, 6);
    in_uint8s(s, 2); /* len */
    in_uint8(s, data_type);
    in_uint8s(s, 1); /* ctype */
    in_uint8s(s, 2); /* clen */
    DEBUG(("xrdp_rdp_process_data code %d", data_type));

    switch (data_type)
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
        case 33: /* 33(0x21) ?? Invalidate an area I think */
            xrdp_rdp_process_screen_update(self, s);
            break;
        case 35: /* 35(0x23) */
            /* 35 ?? this comes when minimizing a full screen mstsc.exe 2600 */
            /* I think this is saying the client no longer wants screen */
            /* updates and it will issue a 33 above to catch up */
            /* so minimized apps don't take bandwidth */
            break;
        case 36: /* 36(0x24) ?? disconnect query? */
            /* when this message comes, send a 37 back so the client */
            /* is sure the connection is alive and it can ask if user */
            /* really wants to disconnect */
            xrdp_rdp_send_disconnect_query_response(self); /* send a 37 back */
            break;
        case RDP_DATA_PDU_FONT2: /* 39(0x27) */
            xrdp_rdp_process_data_font(self, s);
            break;
        case 56: /* PDUTYPE2_FRAME_ACKNOWLEDGE 0x38 */
            xrdp_rdp_process_frame_ack(self, s);
            break;
        default:
            g_writeln("unknown in xrdp_rdp_process_data %d", data_type);
            break;
    }

    return 0;
}
/*****************************************************************************/
int
xrdp_rdp_disconnect(struct xrdp_rdp *self)
{
    int rv;

    DEBUG(("in xrdp_rdp_disconnect"));
    rv = xrdp_sec_disconnect(self->sec_layer);
    DEBUG(("out xrdp_rdp_disconnect"));
    return rv;
}

/*****************************************************************************/
int
xrdp_rdp_send_deactivate(struct xrdp_rdp *self)
{
    struct stream *s;

    DEBUG(("in xrdp_rdp_send_deactivate"));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init(self, s) != 0)
    {
        free_stream(s);
        DEBUG(("out xrdp_rdp_send_deactivate error"));
        return 1;
    }

    s_mark_end(s);

    if (xrdp_rdp_send(self, s, RDP_PDU_DEACTIVATE) != 0)
    {
        free_stream(s);
        DEBUG(("out xrdp_rdp_send_deactivate error"));
        return 1;
    }

    free_stream(s);
    DEBUG(("out xrdp_rdp_send_deactivate"));
    return 0;
}

/*****************************************************************************/
int
xrdp_rdp_send_session_info(struct xrdp_rdp *self, const char *data,
                           int data_bytes)
{
    struct stream *s;

    LLOGLN(0, ("xrdp_rdp_send_session_info: data_bytes %d", data_bytes));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    if (s_check_rem_out(s, data_bytes))
    {
        out_uint8a(s, data, data_bytes); 
    }
    else
    {
        free_stream(s);
        return 1;
    }

    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_LOGON) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

