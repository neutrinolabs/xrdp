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

#include "libxrdp.h"
#include "log.h"

#if defined(XRDP_NEUTRINORDP)
#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#endif

/*****************************************************************************/
static int APP_CC
xrdp_rdp_read_config(struct xrdp_client_info *client_info)
{
    int index = 0;
    struct list *items = (struct list *)NULL;
    struct list *values = (struct list *)NULL;
    char *item = (char *)NULL;
    char *value = (char *)NULL;
    char cfg_file[256];

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
            if (g_strcasecmp(value, "low") == 0)
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
                log_message(LOG_LEVEL_ALWAYS,"Warning: Your configured crypt level is"
                          "undefined 'high' will be used");
                client_info->crypt_level = 3;
            }
        }
        else if (g_strcasecmp(item, "allow_channels") == 0)
        {
            client_info->channel_code = g_text2bool(value);
            if (client_info->channel_code == 0)
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
                log_message(LOG_LEVEL_ALWAYS,"Warning: Your configured fastpath level is"
                          "undefined, fastpath will not be used");
                client_info->use_fast_path = 0;
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
struct xrdp_rdp *APP_CC
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
    self->sec_layer = xrdp_sec_create(self, trans,
                                      self->client_info.crypt_level,
                                      self->client_info.channel_code,
                                      self->client_info.multimon);
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
void APP_CC
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
    g_free(self);
}

/*****************************************************************************/
int APP_CC
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
int APP_CC
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
int APP_CC
xrdp_rdp_recv(struct xrdp_rdp *self, struct stream *s, int *code)
{
    int error = 0;
    int len = 0;
    int pdu_code = 0;
    int chan = 0;
    const tui8 *header;
    header = (const tui8 *) (self->session->trans->in_s->p);

    DEBUG(("in xrdp_rdp_recv"));
    if (s->next_packet == 0 || s->next_packet >= s->end)
    {
        /* check for fastpath first */
        if ((header[0] != 0x3) && (header[0] != 0x3c))
        {
            if (xrdp_sec_recv_fastpath(self->sec_layer, s) != 0)
            {
              return 1;
            }
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
int APP_CC
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
int APP_CC
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
            g_writeln("mppc_encode not ok: type %d flags %d", mppc_enc->protocol_type, mppc_enc->flags);
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
int APP_CC
xrdp_rdp_send_data_update_sync(struct xrdp_rdp *self)
{
    struct stream *s = (struct stream *)NULL;

    make_stream(s);
    init_stream(s, 8192);
    DEBUG(("in xrdp_rdp_send_data_update_sync"));

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        DEBUG(("out xrdp_rdp_send_data_update_sync error"));
        free_stream(s);
        return 1;
    }

    out_uint16_le(s, RDP_UPDATE_SYNCHRONIZE);
    out_uint8s(s, 2);
    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_UPDATE) != 0)
    {
        DEBUG(("out xrdp_rdp_send_data_update_sync error"));
        free_stream(s);
        return 1;
    }

    DEBUG(("out xrdp_rdp_send_data_update_sync"));
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_incoming(struct xrdp_rdp *self)
{
    DEBUG(("in xrdp_rdp_incoming"));

    if (xrdp_sec_incoming(self->sec_layer) != 0)
    {
        return 1;
    }
    self->mcs_channel = self->sec_layer->mcs_layer->userid +
                        MCS_USERCHANNEL_BASE;
    DEBUG(("out xrdp_rdp_incoming mcs channel %d", self->mcs_channel));
    g_strncpy(self->client_info.client_addr,
              self->sec_layer->mcs_layer->iso_layer->trans->addr,
              sizeof(self->client_info.client_addr) - 1);
    g_strncpy(self->client_info.client_port,
              self->sec_layer->mcs_layer->iso_layer->trans->port,
              sizeof(self->client_info.client_port) - 1);
    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_demand_active(struct xrdp_rdp *self)
{
    struct stream *s;
    int caps_count;
    int caps_size;
    int codec_caps_count;
    int codec_caps_size;
    int flags;
    char *caps_count_ptr;
    char *caps_size_ptr;
    char *caps_ptr;
    char *codec_caps_count_ptr;
    char *codec_caps_size_ptr;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    caps_count = 0;
    out_uint32_le(s, self->share_id);
    out_uint16_le(s, 4); /* 4 chars for RDP\0 */
    /* 2 bytes size after num caps, set later */
    caps_size_ptr = s->p;
    out_uint8s(s, 2);
    out_uint8a(s, "RDP", 4);
    /* 4 byte num caps, set later */
    caps_count_ptr = s->p;
    out_uint8s(s, 4);
    caps_ptr = s->p;

    /* Output share capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_SHARE);
    out_uint16_le(s, RDP_CAPLEN_SHARE);
    out_uint16_le(s, self->mcs_channel);
    out_uint16_be(s, 0xb5e2); /* 0x73e1 */

    /* Output general capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_GENERAL); /* 1 */
    out_uint16_le(s, RDP_CAPLEN_GENERAL); /* 24(0x18) */
    out_uint16_le(s, 1); /* OS major type */
    out_uint16_le(s, 3); /* OS minor type */
    out_uint16_le(s, 0x200); /* Protocol version */
    out_uint16_le(s, 0); /* pad */
    out_uint16_le(s, 0); /* Compression types */
    /* NO_BITMAP_COMPRESSION_HDR 0x0400
       FASTPATH_OUTPUT_SUPPORTED 0x0001 */
    if (self->client_info.use_fast_path & 1)
    {
        out_uint16_le(s, 0x401);
    }
    else
    {
        out_uint16_le(s, 0x400);
    }
    out_uint16_le(s, 0); /* Update capability */
    out_uint16_le(s, 0); /* Remote unshare capability */
    out_uint16_le(s, 0); /* Compression level */
    out_uint16_le(s, 0); /* Pad */

    /* Output bitmap capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_BITMAP); /* 2 */
    out_uint16_le(s, RDP_CAPLEN_BITMAP); /* 28(0x1c) */
    out_uint16_le(s, self->client_info.bpp); /* Preferred BPP */
    out_uint16_le(s, 1); /* Receive 1 BPP */
    out_uint16_le(s, 1); /* Receive 4 BPP */
    out_uint16_le(s, 1); /* Receive 8 BPP */
    out_uint16_le(s, self->client_info.width); /* width */
    out_uint16_le(s, self->client_info.height); /* height */
    out_uint16_le(s, 0); /* Pad */
    out_uint16_le(s, 1); /* Allow resize */
    out_uint16_le(s, 1); /* bitmap compression */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* unknown */
    out_uint16_le(s, 0); /* pad */

    /* Output font capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_FONT); /* 14 */
    out_uint16_le(s, RDP_CAPLEN_FONT); /* 4 */

    /* Output order capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_ORDER); /* 3 */
    out_uint16_le(s, RDP_CAPLEN_ORDER); /* 88(0x58) */
    out_uint8s(s, 16);
    out_uint32_be(s, 0x40420f00);
    out_uint16_le(s, 1); /* Cache X granularity */
    out_uint16_le(s, 20); /* Cache Y granularity */
    out_uint16_le(s, 0); /* Pad */
    out_uint16_le(s, 1); /* Max order level */
    out_uint16_le(s, 0x2f); /* Number of fonts */
    out_uint16_le(s, 0x22); /* Capability flags */
    /* caps */
    out_uint8(s, 1); /* NEG_DSTBLT_INDEX                0x00 0 */
    out_uint8(s, 1); /* NEG_PATBLT_INDEX                0x01 1 */
    out_uint8(s, 1); /* NEG_SCRBLT_INDEX                0x02 2 */
    out_uint8(s, 1); /* NEG_MEMBLT_INDEX                0x03 3 */
    out_uint8(s, 0); /* NEG_MEM3BLT_INDEX               0x04 4 */
    out_uint8(s, 0); /* NEG_ATEXTOUT_INDEX              0x05 5 */
    out_uint8(s, 0); /* NEG_AEXTTEXTOUT_INDEX           0x06 6 */
    out_uint8(s, 0); /* NEG_DRAWNINEGRID_INDEX          0x07 7 */
    out_uint8(s, 1); /* NEG_LINETO_INDEX                0x08 8 */
    out_uint8(s, 0); /* NEG_MULTI_DRAWNINEGRID_INDEX    0x09 9 */
    out_uint8(s, 1); /* NEG_OPAQUE_RECT_INDEX           0x0A 10 */
    out_uint8(s, 0); /* NEG_SAVEBITMAP_INDEX            0x0B 11 */
    out_uint8(s, 0); /* NEG_WTEXTOUT_INDEX              0x0C 12 */
    out_uint8(s, 0); /* NEG_MEMBLT_V2_INDEX             0x0D 13 */
    out_uint8(s, 0); /* NEG_MEM3BLT_V2_INDEX            0x0E 14 */
    out_uint8(s, 0); /* NEG_MULTIDSTBLT_INDEX           0x0F 15 */
    out_uint8(s, 0); /* NEG_MULTIPATBLT_INDEX           0x10 16 */
    out_uint8(s, 0); /* NEG_MULTISCRBLT_INDEX           0x11 17 */
    out_uint8(s, 1); /* NEG_MULTIOPAQUERECT_INDEX       0x12 18 */
    out_uint8(s, 0); /* NEG_FAST_INDEX_INDEX            0x13 19 */
    out_uint8(s, 0); /* NEG_POLYGON_SC_INDEX            0x14 20 */
    out_uint8(s, 0); /* NEG_POLYGON_CB_INDEX            0x15 21 */
    out_uint8(s, 0); /* NEG_POLYLINE_INDEX              0x16 22 */
    out_uint8(s, 0); /* unused                          0x17 23 */
    out_uint8(s, 0); /* NEG_FAST_GLYPH_INDEX            0x18 24 */
    out_uint8(s, 0); /* NEG_ELLIPSE_SC_INDEX            0x19 25 */
    out_uint8(s, 0); /* NEG_ELLIPSE_CB_INDEX            0x1A 26 */
    out_uint8(s, 1); /* NEG_GLYPH_INDEX_INDEX           0x1B 27 */
    out_uint8(s, 0); /* NEG_GLYPH_WEXTTEXTOUT_INDEX     0x1C 28 */
    out_uint8(s, 0); /* NEG_GLYPH_WLONGTEXTOUT_INDEX    0x1D 29 */
    out_uint8(s, 0); /* NEG_GLYPH_WLONGEXTTEXTOUT_INDEX 0x1E 30 */
    out_uint8(s, 0); /* unused                          0x1F 31 */
    out_uint16_le(s, 0x6a1);
    /* declare support of bitmap cache rev3 */
    out_uint16_le(s, XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT);
    out_uint32_le(s, 0x0f4240); /* desk save */
    out_uint32_le(s, 0x0f4240); /* desk save */
    out_uint32_le(s, 1); /* ? */
    out_uint32_le(s, 0); /* ? */

    /* Output bmpcodecs capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_BMPCODECS);
    codec_caps_size_ptr = s->p;
    out_uint8s(s, 2); /* cap len set later */
    codec_caps_count = 0;
    codec_caps_count_ptr = s->p;
    out_uint8s(s, 1); /* bitmapCodecCount set later */
    /* nscodec */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_NSCODEC, 16);
    out_uint8(s, 1); /* codec id, must be 1 */
    out_uint16_le(s, 3);
    out_uint8(s, 0x01); /* fAllowDynamicFidelity */
    out_uint8(s, 0x01); /* fAllowSubsampling */
    out_uint8(s, 0x03); /* colorLossLevel */
    /* remotefx */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_REMOTEFX, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 256);
    out_uint8s(s, 256);
    /* jpeg */
    codec_caps_count++;
    out_uint8a(s, XR_CODEC_GUID_JPEG, 16);
    out_uint8(s, 0); /* codec id, client sets */
    out_uint16_le(s, 1); /* ext length */
    out_uint8(s, 75);
    /* calculate and set size and count */
    codec_caps_size = (int)(s->p - codec_caps_size_ptr);
    codec_caps_size += 2; /* 2 bytes for RDP_CAPSET_BMPCODECS above */
    codec_caps_size_ptr[0] = codec_caps_size;
    codec_caps_size_ptr[1] = codec_caps_size >> 8;
    codec_caps_count_ptr[0] = codec_caps_count;

    /* Output color cache capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_COLCACHE);
    out_uint16_le(s, RDP_CAPLEN_COLCACHE);
    out_uint16_le(s, 6); /* cache size */
    out_uint16_le(s, 0); /* pad */

    /* Output pointer capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_POINTER);
    out_uint16_le(s, RDP_CAPLEN_POINTER);
    out_uint16_le(s, 1); /* Colour pointer */
    out_uint16_le(s, 0x19); /* Cache size */
    out_uint16_le(s, 0x19); /* Cache size */

    /* Output input capability set */
    caps_count++;
    out_uint16_le(s, RDP_CAPSET_INPUT); /* 13(0xd) */
    out_uint16_le(s, RDP_CAPLEN_INPUT); /* 88(0x58) */

    /* INPUT_FLAG_SCANCODES 0x0001
       INPUT_FLAG_MOUSEX 0x0004
       INPUT_FLAG_FASTPATH_INPUT 0x0008
       INPUT_FLAG_FASTPATH_INPUT2 0x0020 */
    flags = 0x0001 | 0x0004;
    if (self->client_info.use_fast_path & 2)
        flags |= 0x0008 | 0x0020;
    out_uint16_le(s, flags);
    out_uint8s(s, 82);

    /* Remote Programs Capability Set */
    caps_count++;
    out_uint16_le(s, 0x0017); /* CAPSETTYPE_RAIL */
    out_uint16_le(s, 8);
    out_uint32_le(s, 3); /* TS_RAIL_LEVEL_SUPPORTED
                          TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED */

    /* Window List Capability Set */
    caps_count++;
    out_uint16_le(s, 0x0018); /* CAPSETTYPE_WINDOW */
    out_uint16_le(s, 11);
    out_uint32_le(s, 2); /* TS_WINDOW_LEVEL_SUPPORTED_EX */
    out_uint8(s, 3); /* NumIconCaches */
    out_uint16_le(s, 12); /* NumIconCacheEntries */

    /* 6 - bitmap cache v3 codecid */
    caps_count++;
    out_uint16_le(s, 0x0006);
    out_uint16_le(s, 5);
    out_uint8(s, 0); /* client sets */

    out_uint8s(s, 4); /* pad */

    s_mark_end(s);

    caps_size = (int)(s->end - caps_ptr);
    caps_size_ptr[0] = caps_size;
    caps_size_ptr[1] = caps_size >> 8;

    caps_count_ptr[0] = caps_count;
    caps_count_ptr[1] = caps_count >> 8;
    caps_count_ptr[2] = caps_count >> 16;
    caps_count_ptr[3] = caps_count >> 24;

    if (xrdp_rdp_send(self, s, RDP_PDU_DEMAND_ACTIVE) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_general(struct xrdp_rdp *self, struct stream *s,
                            int len)
{
    int i;

    if (len < 10 + 2)
    {
        g_writeln("xrdp_process_capset_general: error");
        return 1;
    }
    in_uint8s(s, 10);
    in_uint16_le(s, i);
    /* use_compact_packets is pretty much 'use rdp5' */
    self->client_info.use_compact_packets = (i != 0);
    /* op2 is a boolean to use compact bitmap headers in bitmap cache */
    /* set it to same as 'use rdp5' boolean */
    self->client_info.op2 = self->client_info.use_compact_packets;
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_order(struct xrdp_rdp *self, struct stream *s,
                          int len)
{
    int i;
    char order_caps[32];
    int ex_flags;
    int cap_flags;

    DEBUG(("order capabilities"));
    if (len < 20 + 2 + 2 + 2 + 2 + 2 + 2 + 32 + 2 + 2 + 4 + 4 + 4 + 4)
    {
        g_writeln("xrdp_process_capset_order: error");
        return 1;
    }
    in_uint8s(s, 20); /* Terminal desc, pad */
    in_uint8s(s, 2); /* Cache X granularity */
    in_uint8s(s, 2); /* Cache Y granularity */
    in_uint8s(s, 2); /* Pad */
    in_uint8s(s, 2); /* Max order level */
    in_uint8s(s, 2); /* Number of fonts */
    in_uint16_le(s, cap_flags); /* Capability flags */
    in_uint8a(s, order_caps, 32); /* Orders supported */
    g_memcpy(self->client_info.orders, order_caps, 32);
    DEBUG(("dest blt-0 %d", order_caps[0]));
    DEBUG(("pat blt-1 %d", order_caps[1]));
    DEBUG(("screen blt-2 %d", order_caps[2]));
    DEBUG(("memblt-3-13 %d %d", order_caps[3], order_caps[13]));
    DEBUG(("triblt-4-14 %d %d", order_caps[4], order_caps[14]));
    DEBUG(("line-8 %d", order_caps[8]));
    DEBUG(("line-9 %d", order_caps[9]));
    DEBUG(("rect-10 %d", order_caps[10]));
    DEBUG(("desksave-11 %d", order_caps[11]));
    DEBUG(("polygon-20 %d", order_caps[20]));
    DEBUG(("polygon2-21 %d", order_caps[21]));
    DEBUG(("polyline-22 %d", order_caps[22]));
    DEBUG(("ellipse-25 %d", order_caps[25]));
    DEBUG(("ellipse2-26 %d", order_caps[26]));
    DEBUG(("text2-27 %d", order_caps[27]));
    DEBUG(("order_caps dump"));
#if defined(XRDP_DEBUG)
    g_hexdump(order_caps, 32);
#endif
    in_uint8s(s, 2); /* Text capability flags */
    /* read extended order support flags */
    in_uint16_le(s, ex_flags); /* Ex flags */

    if (cap_flags & 0x80) /* ORDER_FLAGS_EXTRA_SUPPORT */
    {
        self->client_info.order_flags_ex = ex_flags;
        if (ex_flags & XR_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT)
        {
            g_writeln("xrdp_process_capset_order: bitmap cache v3 supported");
            self->client_info.bitmap_cache_version |= 4;
        }
    }
    in_uint8s(s, 4); /* Pad */

    in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
    self->client_info.desktop_cache = i;
    DEBUG(("desktop cache size %d", i));
    in_uint8s(s, 4); /* Unknown */
    in_uint8s(s, 4); /* Unknown */
    return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_process_capset_bmpcache(struct xrdp_rdp *self, struct stream *s,
                             int len)
{
    int i;

    if (len < 24 + 2 + 2 + 2 + 2 + 2 + 2)
    {
        g_writeln("xrdp_process_capset_bmpcache: error");
        return 1;
    }
    self->client_info.bitmap_cache_version |= 1;
    in_uint8s(s, 24);
    /* cache 1 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache1_entries = i;
    in_uint16_le(s, self->client_info.cache1_size);
    /* cache 2 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache2_entries = i;
    in_uint16_le(s, self->client_info.cache2_size);
    /* caceh 3 */
    in_uint16_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache3_entries = i;
    in_uint16_le(s, self->client_info.cache3_size);
    DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
           self->client_info.cache1_size));
    DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
           self->client_info.cache2_size));
    DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
           self->client_info.cache3_size));
    return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_process_capset_bmpcache2(struct xrdp_rdp *self, struct stream *s,
                              int len)
{
    int Bpp = 0;
    int i = 0;

    if (len < 2 + 2 + 4 + 4 + 4)
    {
        g_writeln("xrdp_process_capset_bmpcache2: error");
        return 1;
    }
    self->client_info.bitmap_cache_version |= 2;
    Bpp = (self->client_info.bpp + 7) / 8;
    in_uint16_le(s, i); /* cache flags */
    self->client_info.bitmap_cache_persist_enable = i;
    in_uint8s(s, 2); /* number of caches in set, 3 */
    in_uint32_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache1_entries = i;
    self->client_info.cache1_size = 256 * Bpp;
    in_uint32_le(s, i);
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache2_entries = i;
    self->client_info.cache2_size = 1024 * Bpp;
    in_uint32_le(s, i);
    i = i & 0x7fffffff;
    i = MIN(i, XRDP_MAX_BITMAP_CACHE_IDX);
    i = MAX(i, 0);
    self->client_info.cache3_entries = i;
    self->client_info.cache3_size = 4096 * Bpp;
    DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
           self->client_info.cache1_size));
    DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
           self->client_info.cache2_size));
    DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
           self->client_info.cache3_size));
    return 0;
}

/*****************************************************************************/
static int
xrdp_process_capset_cache_v3_codec_id(struct xrdp_rdp *self, struct stream *s,
                                      int len)
{
    int codec_id;

    if (len < 1)
    {
        g_writeln("xrdp_process_capset_cache_v3_codec_id: error");
        return 1;
    }
    in_uint8(s, codec_id);
    g_writeln("xrdp_process_capset_cache_v3_codec_id: cache_v3_codec_id %d",
              codec_id);
    self->client_info.v3_codec_id = codec_id;
    return 0;
}

/*****************************************************************************/
/* get the number of client cursor cache */
static int APP_CC
xrdp_process_capset_pointercache(struct xrdp_rdp *self, struct stream *s,
                                 int len)
{
    int i;
    int colorPointerFlag;
    int no_new_cursor;

    if (len < 2 + 2 + 2)
    {
        g_writeln("xrdp_process_capset_pointercache: error");
        return 1;
    }
    no_new_cursor = self->client_info.pointer_flags & 2;
    in_uint16_le(s, colorPointerFlag);
    self->client_info.pointer_flags = colorPointerFlag;
    in_uint16_le(s, i);
    i = MIN(i, 32);
    self->client_info.pointer_cache_entries = i;
    if (colorPointerFlag & 1)
    {
        g_writeln("xrdp_process_capset_pointercache: client supports "
                  "new(color) cursor");
        in_uint16_le(s, i);
        i = MIN(i, 32);
        self->client_info.pointer_cache_entries = i;
    }
    else
    {
        g_writeln("xrdp_process_capset_pointercache: client does not support "
                  "new(color) cursor");
    }
    if (no_new_cursor)
    {
        g_writeln("xrdp_process_capset_pointercache: new(color) cursor is "
                  "disabled by config");
        self->client_info.pointer_flags = 0;
    }
    return 0;
}

/*****************************************************************************/
/* get the type of client brush cache */
static int APP_CC
xrdp_process_capset_brushcache(struct xrdp_rdp *self, struct stream *s,
                               int len)
{
    int code;

    if (len < 4)
    {
        g_writeln("xrdp_process_capset_brushcache: error");
        return 1;
    }
    in_uint32_le(s, code);
    self->client_info.brush_cache_code = code;
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_offscreen_bmpcache(struct xrdp_rdp *self, struct stream *s,
                                int len)
{
    int i32;

    if (len < 4 + 2 + 2)
    {
        g_writeln("xrdp_process_offscreen_bmpcache: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.offscreen_support_level = i32;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_size = i32 * 1024;
    in_uint16_le(s, i32);
    self->client_info.offscreen_cache_entries = i32;
    g_writeln("xrdp_process_offscreen_bmpcache: support level %d "
              "cache size %d MB cache entries %d",
              self->client_info.offscreen_support_level,
              self->client_info.offscreen_cache_size,
              self->client_info.offscreen_cache_entries);
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_rail(struct xrdp_rdp *self, struct stream *s, int len)
{
    int i32;

    if (len < 4)
    {
        g_writeln("xrdp_process_capset_rail: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.rail_support_level = i32;
    g_writeln("xrdp_process_capset_rail: rail_support_level %d",
              self->client_info.rail_support_level);
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_window(struct xrdp_rdp *self, struct stream *s, int len)
{
    int i32;

    if (len < 4 + 1 + 2)
    {
        g_writeln("xrdp_process_capset_window: error");
        return 1;
    }
    in_uint32_le(s, i32);
    self->client_info.wnd_support_level = i32;
    in_uint8(s, i32);
    self->client_info.wnd_num_icon_caches = i32;
    in_uint16_le(s, i32);
    self->client_info.wnd_num_icon_cache_entries = i32;
    g_writeln("xrdp_process_capset_window wnd_support_level %d "
              "wnd_num_icon_caches %d wnd_num_icon_cache_entries %d",
              self->client_info.wnd_support_level,
              self->client_info.wnd_num_icon_caches,
              self->client_info.wnd_num_icon_cache_entries);
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_codecs(struct xrdp_rdp *self, struct stream *s, int len)
{
    int codec_id;
    int codec_count;
    int index;
    int codec_properties_length;
    int i1;
    char *codec_guid;
    char *next_guid;

    if (len < 1)
    {
        g_writeln("xrdp_process_capset_codecs: error");
        return 1;
    }
    in_uint8(s, codec_count);
    len--;

    for (index = 0; index < codec_count; index++)
    {
        codec_guid = s->p;
        if (len < 16 + 1 + 2)
        {
            g_writeln("xrdp_process_capset_codecs: error");
            return 1;
        }
        in_uint8s(s, 16);
        in_uint8(s, codec_id);
        in_uint16_le(s, codec_properties_length);
        len -= 16 + 1 + 2;
        if (len < codec_properties_length)
        {
            g_writeln("xrdp_process_capset_codecs: error");
            return 1;
        }
        len -= codec_properties_length;
        next_guid = s->p + codec_properties_length;

        if (g_memcmp(codec_guid, XR_CODEC_GUID_NSCODEC, 16) == 0)
        {
            g_writeln("xrdp_process_capset_codecs: nscodec codec id %d prop len %d",
                      codec_id, codec_properties_length);
            self->client_info.ns_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.ns_prop, s->p, i1);
            self->client_info.ns_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_REMOTEFX, 16) == 0)
        {
            g_writeln("xrdp_process_capset_codecs: rfx codec id %d prop len %d",
                      codec_id, codec_properties_length);
            self->client_info.rfx_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.rfx_prop, s->p, i1);
            self->client_info.rfx_prop_len = i1;
        }
        else if (g_memcmp(codec_guid, XR_CODEC_GUID_JPEG, 16) == 0)
        {
            g_writeln("xrdp_process_capset_codecs: jpeg codec id %d prop len %d",
                      codec_id, codec_properties_length);
            self->client_info.jpeg_codec_id = codec_id;
            i1 = MIN(64, codec_properties_length);
            g_memcpy(self->client_info.jpeg_prop, s->p, i1);
            self->client_info.jpeg_prop_len = i1;
            g_writeln("  jpeg quality %d", self->client_info.jpeg_prop[0]);
        }
        else
        {
            g_writeln("xrdp_process_capset_codecs: unknown codec id %d", codec_id);
        }

        s->p = next_guid;
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_process_confirm_active(struct xrdp_rdp *self, struct stream *s)
{
    int cap_len;
    int source_len;
    int num_caps;
    int index;
    int type;
    int len;
    char *p;

    DEBUG(("in xrdp_rdp_process_confirm_active"));
    in_uint8s(s, 4); /* rdp_shareid */
    in_uint8s(s, 2); /* userid */
    in_uint16_le(s, source_len); /* sizeof RDP_SOURCE */
    in_uint16_le(s, cap_len);
    in_uint8s(s, source_len);
    in_uint16_le(s, num_caps);
    in_uint8s(s, 2); /* pad */

    for (index = 0; index < num_caps; index++)
    {
        p = s->p;
        if (!s_check_rem(s, 4))
        {
            g_writeln("xrdp_rdp_process_confirm_active: error 1");
            return 1;
        }
        in_uint16_le(s, type);
        in_uint16_le(s, len);
        if ((len < 4) || !s_check_rem(s, len - 4))
        {
            g_writeln("xrdp_rdp_process_confirm_active: error len %d", len, s->end - s->p);
            return 1;
        }
        len -= 4;
        switch (type)
        {
            case RDP_CAPSET_GENERAL: /* 1 */
                DEBUG(("RDP_CAPSET_GENERAL"));
                xrdp_process_capset_general(self, s, len);
                break;
            case RDP_CAPSET_BITMAP: /* 2 */
                DEBUG(("RDP_CAPSET_BITMAP"));
                break;
            case RDP_CAPSET_ORDER: /* 3 */
                DEBUG(("RDP_CAPSET_ORDER"));
                xrdp_process_capset_order(self, s, len);
                break;
            case RDP_CAPSET_BMPCACHE: /* 4 */
                DEBUG(("RDP_CAPSET_BMPCACHE"));
                xrdp_process_capset_bmpcache(self, s, len);
                break;
            case RDP_CAPSET_CONTROL: /* 5 */
                DEBUG(("RDP_CAPSET_CONTROL"));
                break;
            case 6:
                xrdp_process_capset_cache_v3_codec_id(self, s, len);
                break;
            case RDP_CAPSET_ACTIVATE: /* 7 */
                DEBUG(("RDP_CAPSET_ACTIVATE"));
                break;
            case RDP_CAPSET_POINTER: /* 8 */
                DEBUG(("RDP_CAPSET_POINTER"));
                xrdp_process_capset_pointercache(self, s, len);
                break;
            case RDP_CAPSET_SHARE: /* 9 */
                DEBUG(("RDP_CAPSET_SHARE"));
                break;
            case RDP_CAPSET_COLCACHE: /* 10 */
                DEBUG(("RDP_CAPSET_COLCACHE"));
                break;
            case 12: /* 12 */
                DEBUG(("--12"));
                break;
            case 13: /* 13 */
                DEBUG(("--13"));
                break;
            case 14: /* 14 */
                DEBUG(("--14"));
                break;
            case RDP_CAPSET_BRUSHCACHE: /* 15 */
                xrdp_process_capset_brushcache(self, s, len);
                break;
            case 16: /* 16 */
                DEBUG(("--16"));
                break;
            case 17: /* 17 */
                DEBUG(("CAPSET_TYPE_OFFSCREEN_CACHE"));
                xrdp_process_offscreen_bmpcache(self, s, len);
                break;
            case RDP_CAPSET_BMPCACHE2: /* 19 */
                DEBUG(("RDP_CAPSET_BMPCACHE2"));
                xrdp_process_capset_bmpcache2(self, s, len);
                break;
            case 20: /* 20 */
                DEBUG(("--20"));
                break;
            case 21: /* 21 */
                DEBUG(("--21"));
                break;
            case 22: /* 22 */
                DEBUG(("--22"));
                break;
            case 0x0017: /* 23 CAPSETTYPE_RAIL */
                xrdp_process_capset_rail(self, s, len);
                break;
            case 0x0018: /* 24 CAPSETTYPE_WINDOW */
                xrdp_process_capset_window(self, s, len);
                break;
            case 26: /* 26 */
                DEBUG(("--26"));
                break;
            case RDP_CAPSET_BMPCODECS: /* 0x1d(29) */
                xrdp_process_capset_codecs(self, s, len);
                break;
            default:
                g_writeln("unknown in xrdp_rdp_process_confirm_active %d", type);
                break;
        }

        s->p = p + len + 4;
    }

    DEBUG(("out xrdp_rdp_process_confirm_active"));
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_process_data_pointer(struct xrdp_rdp *self, struct stream *s)
{
    return 0;
}

/*****************************************************************************/
/* RDP_DATA_PDU_INPUT */
static int APP_CC
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
static int APP_CC
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

    out_uint16_le(s, 1);
    out_uint16_le(s, 1002);
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
static int APP_CC
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
static int APP_CC
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
static int APP_CC
xrdp_rdp_process_data_sync(struct xrdp_rdp *self)
{
    DEBUG(("xrdp_rdp_process_data_sync"));
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_process_screen_update(struct xrdp_rdp *self, struct stream *s)
{
    int op;
    int left;
    int top;
    int right;
    int bottom;
    int cx;
    int cy;

    in_uint32_le(s, op);
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
static int APP_CC
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
int APP_CC
xrdp_rdp_send_monitorlayout(struct xrdp_rdp *self)
{
    struct stream *s;
    int i;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data(self, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, self->client_info.monitorCount); /* MonitorCount */

    /* TODO: validate for allowed monitors in terminal server (maybe by config?) */
    for (i = 0; i < self->client_info.monitorCount; i++)
    {
        out_uint32_le(s, self->client_info.minfo[i].left);
        out_uint32_le(s, self->client_info.minfo[i].top);
        out_uint32_le(s, self->client_info.minfo[i].right);
        out_uint32_le(s, self->client_info.minfo[i].bottom);
        out_uint32_le(s, self->client_info.minfo[i].is_primary);
    }

    s_mark_end(s);

    if (xrdp_rdp_send_data(self, s, 0x37) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}
/*****************************************************************************/
static int APP_CC
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
static int APP_CC
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
static int APP_CC
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
/* RDP_PDU_DATA */
int APP_CC
xrdp_rdp_process_data(struct xrdp_rdp *self, struct stream *s)
{
    int len;
    int data_type;
    int ctype;
    int clen;

    in_uint8s(s, 6);
    in_uint16_le(s, len);
    in_uint8(s, data_type);
    in_uint8(s, ctype);
    in_uint16_le(s, clen);
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
            /* 35 ?? this comes when minimuzing a full screen mstsc.exe 2600 */
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
        default:
            g_writeln("unknown in xrdp_rdp_process_data %d", data_type);
            break;
    }

    return 0;
}
/*****************************************************************************/
int APP_CC
xrdp_rdp_disconnect(struct xrdp_rdp *self)
{
    int rv;

    DEBUG(("in xrdp_rdp_disconnect"));
    rv = xrdp_sec_disconnect(self->sec_layer);
    DEBUG(("out xrdp_rdp_disconnect"));
    return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_deactive(struct xrdp_rdp *self)
{
    struct stream *s;

    DEBUG(("in xrdp_rdp_send_deactive"));
    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init(self, s) != 0)
    {
        free_stream(s);
        DEBUG(("out xrdp_rdp_send_deactive error"));
        return 1;
    }

    s_mark_end(s);

    if (xrdp_rdp_send(self, s, RDP_PDU_DEACTIVATE) != 0)
    {
        free_stream(s);
        DEBUG(("out xrdp_rdp_send_deactive error"));
        return 1;
    }

    free_stream(s);
    DEBUG(("out xrdp_rdp_send_deactive"));
    return 0;
}
