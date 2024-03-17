/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * this is the interface to libxrdp
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "string_calls.h"
#include "xrdp_orders_rail.h"
#include "ms-rdpedisp.h"
#include "ms-rdpbcgr.h"

#define MAX_BITMAP_BUF_SIZE (16 * 1024) /* 16K */
#define TS_MONITOR_ATTRIBUTES_SIZE 20 /* [MS-RDPBCGR] 2.2.1.3.9 */

/******************************************************************************/
struct xrdp_session *EXPORT_CC
libxrdp_init(tbus id, struct trans *trans, const char *xrdp_ini)
{
    struct xrdp_session *session;

    session = (struct xrdp_session *)g_malloc(sizeof(struct xrdp_session), 1);
    session->id = id;
    session->trans = trans;
    if (xrdp_ini != NULL)
    {
        session->xrdp_ini = g_strdup(xrdp_ini);
    }
    else
    {
        session->xrdp_ini = g_strdup(XRDP_CFG_PATH "/xrdp.ini");
    }
    session->rdp = xrdp_rdp_create(session, trans);
    session->orders = xrdp_orders_create(session, (struct xrdp_rdp *)session->rdp);
    session->client_info = &(((struct xrdp_rdp *)session->rdp)->client_info);
    session->check_for_app_input = 1;
    return session;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_exit(struct xrdp_session *session)
{
    if (session == 0)
    {
        return 0;
    }

    xrdp_orders_delete((struct xrdp_orders *)session->orders);
    xrdp_rdp_delete((struct xrdp_rdp *)session->rdp);
    g_free(session->xrdp_ini);
    g_free(session);
    return 0;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_disconnect(struct xrdp_session *session)
{
    return xrdp_rdp_disconnect((struct xrdp_rdp *)session->rdp);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_process_incoming(struct xrdp_session *session)
{
    int rv;

    rv = xrdp_rdp_incoming((struct xrdp_rdp *)(session->rdp));
    return rv;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_get_pdu_bytes(const char *aheader)
{
    int rv;
    const tui8 *header;

    rv = -1;
    header = (const tui8 *) aheader;

    if (header[0] == 0x03)
    {
        /* TPKT */
        rv = (header[2] << 8) | header[3];
    }
    else
    {
        /* Fast-Path */
        if (header[1] & 0x80)
        {
            rv = ((header[1] & 0x7F) << 8) | header[2];
        }
        else
        {
            rv = header[1];
        }
    }
    return rv;
}

/******************************************************************************/
/* only used during connection */
struct stream *
libxrdp_force_read(struct trans *trans)
{
    int bytes;
    struct stream *s;

    s = trans->in_s;
    init_stream(s, 32 * 1024);

    if (trans_force_read(trans, 4) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_force_read: header read error");
        return NULL;
    }
    bytes = libxrdp_get_pdu_bytes(s->data);
    if (bytes < 4 || bytes > s->size)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_force_read: bad header length %d", bytes);
        return NULL;
    }
    if (trans_force_read(trans, bytes - 4) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_force_read: Can't read PDU");
        return NULL;
    }
    return s;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_process_data(struct xrdp_session *session, struct stream *s)
{
    int cont;
    int rv;
    int code;
    int term;
    int dead_lock_counter;
    int do_read;
    struct xrdp_rdp *rdp;

    do_read = s == 0;
    if (do_read && session->up_and_running)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: error logic");
        return 1;
    }
    if (session->in_process_data != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: error reentry");
        return 1;
    }
    session->in_process_data++;

    term = 0;
    cont = 1;
    rv = 0;
    dead_lock_counter = 0;

    rdp = (struct xrdp_rdp *) (session->rdp);

    while ((cont || !session->up_and_running) && !term)
    {
        if (session->is_term != 0)
        {
            if (session->is_term())
            {
                term = 1;
                break;
            }
        }

        code = 0;

        if (do_read)
        {
            if (s == 0)
            {
                s = libxrdp_force_read(session->trans);
            }
            else
            {
                if ((s->next_packet == 0) || (s->next_packet >= s->end))
                {
                    s = libxrdp_force_read(session->trans);
                }
            }
            if (s == 0)
            {
                LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: libxrdp_force_read failed");
                rv = 1;
                break;
            }
        }

        if (xrdp_rdp_recv(rdp, s, &code) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: xrdp_rdp_recv failed");
            rv = 1;
            break;
        }

        LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_process_data code %d", code);

        switch (code)
        {
            case -1:
                xrdp_caps_send_demand_active(rdp);
                session->up_and_running = 0;
                break;
            case 0:
                dead_lock_counter++;
                break;
            case PDUTYPE_CONFIRMACTIVEPDU:
                LOG_DEVEL(LOG_LEVEL_TRACE, "Processing received "
                          "[MS-RDPBCGR] PDUTYPE_CONFIRMACTIVEPDU");
                xrdp_caps_process_confirm_active(rdp, s);
                break;
            case PDUTYPE_DATAPDU:
                LOG_DEVEL(LOG_LEVEL_TRACE, "Processing received "
                          "[MS-RDPBCGR] PDUTYPE_DATAPDU");
                if (xrdp_rdp_process_data(rdp, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: xrdp_rdp_process_data failed");
                    cont = 0;
                    term = 1;
                }
                break;
            case 2: /* FASTPATH_INPUT_EVENT */
                if (xrdp_fastpath_process_input_event(rdp->sec_layer->fastpath_layer, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "libxrdp_process_data: xrdp_fastpath_process_input_event failed");
                    cont = 0;
                    term = 1;
                }
                break;
            default:
                LOG(LOG_LEVEL_WARNING, "unknown code = %d (ignored)", code);
                dead_lock_counter++;
                break;
        }

        if (dead_lock_counter > 100000)
        {
            /*This situation can happen and this is a workaround*/
            cont = 0;
            LOG(LOG_LEVEL_WARNING,
                "Serious programming error: we were locked in a deadly loop. "
                "Remaining bytes: %d", (int) (s->end - s->next_packet));
            s->next_packet = 0;
        }

        if (cont)
        {
            cont = (s->next_packet != 0) && (s->next_packet < s->end);
        }
    }

    session->in_process_data--;

    return rv;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_send_palette(struct xrdp_session *session, int *palette)
{
    int rv;
    int i = 0;
    int color = 0;
    struct stream *s = (struct stream *)NULL;

    if (session->client_info->bpp > 8)
    {
        return 0;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "sending palette (%s)",
              ((session->client_info->use_fast_path & 1) ?
               "fastpath" : "slowpath"));

    /* clear orders */
    libxrdp_orders_force_send(session);
    make_stream(s);
    init_stream(s, 8192);

    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        if (xrdp_rdp_init_fastpath((struct xrdp_rdp *)session->rdp, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_send_palette: xrdp_rdp_init_fastpath failed");
            free_stream(s);
            return 1;
        }
    }
    else
    {
        xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
    }

    /* TS_UPDATE_PALETTE_DATA */
    out_uint16_le(s, RDP_UPDATE_PALETTE); /* updateType */
    out_uint16_le(s, 0);   /* pad2Octets */
    out_uint16_le(s, 256); /* # of colors (low-bytes) */
    out_uint16_le(s, 0);   /* # of colors (high-bytes) */

    /* paletteEntries */
    for (i = 0; i < 256; i++)
    {
        color = palette[i];
        out_uint8(s, color >> 16);
        out_uint8(s, color >> 8);
        out_uint8(s, color);
    }

    s_mark_end(s);
    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_UPDATE_PALETTE "
                  "paletteUpdateData = { updateType %d (UPDATETYPE_PALETTE), "
                  "pad2Octets <ignored>, numberColors 256, "
                  "paletteEntries <omitted from log> }", RDP_UPDATE_PALETTE);
        if (xrdp_rdp_send_fastpath((struct xrdp_rdp *)session->rdp, s,
                                   FASTPATH_UPDATETYPE_PALETTE) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_send_palette: xrdp_rdp_send_fastpath failed");
            free_stream(s);
            return 1;
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_UPDATE_PALETTE_DATA "
                  "updateType %d (UPDATETYPE_PALETTE), pad2Octets <ignored>, "
                  "numberColors 256, paletteEntries <omitted from log>",
                  RDP_UPDATE_PALETTE);
        xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                           RDP_DATA_PDU_UPDATE);
    }
    free_stream(s);

    /* send the orders palette too */
    rv = libxrdp_orders_init(session);
    if (rv == 0)
    {
        rv = libxrdp_orders_send_palette(session, palette, 0);
    }
    if (rv == 0)
    {
        rv = libxrdp_orders_send(session);
    }
    return rv;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_send_bell(struct xrdp_session *session)
{
    struct stream *s = (struct stream *)NULL;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_send_bell: xrdp_rdp_init_data failed");
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, 100); /* duration (ms) */
    out_uint32_le(s, 440); /* frequency */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_PLAY_SOUND_PDU_DATA "
              "duration 100 ms, frequency 440 Hz");

    if (xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s, RDP_DATA_PDU_PLAY_SOUND) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_send_bell: xrdp_rdp_send_data failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_send_bitmap(struct xrdp_session *session, int width, int height,
                    int bpp, char *data, int x, int y, int cx, int cy)
{
    int line_bytes = 0;
    int i = 0;
    int j = 0;
    int k;
    int total_lines = 0;
    int lines_sending = 0;
    int Bpp = 0;
    int e = 0;
    int bufsize = 0;
    int total_bufsize = 0;
    int num_updates = 0;
    int line_pad_bytes;
    int server_line_bytes;
    char *p_num_updates = (char *)NULL;
    char *p = (char *)NULL;
    char *q = (char *)NULL;
    struct stream *s = (struct stream *)NULL;
    struct stream *temp_s = (struct stream *)NULL;
    tui32 pixel;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: sending bitmap");
    Bpp = (bpp + 7) / 8;
    e = (4 - width) & 3;
    switch (bpp)
    {
        case 15:
        case 16:
            server_line_bytes = width * 2;
            break;
        case 24:
        case 32:
            server_line_bytes = width * 4;
            break;
        default: /* 8 bpp */
            server_line_bytes = width;
            break;
    }
    line_bytes = width * Bpp;
    line_pad_bytes = line_bytes + e * Bpp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: bpp %d Bpp %d line_bytes %d "
              "server_line_bytes %d", bpp, Bpp, line_bytes, server_line_bytes);
    make_stream(s);
    init_stream(s, MAX_BITMAP_BUF_SIZE);

    if (session->client_info->use_bitmap_comp)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: compression");
        make_stream(temp_s);
        init_stream(temp_s, 65536);
        i = 0;

        if (cy <= height)
        {
            i = cy;
        }

        while (i > 0)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: i %d", i);

            total_bufsize = 0;
            num_updates = 0;
            xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
            out_uint16_le(s, RDP_UPDATE_BITMAP); /* updateType */
            p_num_updates = s->p;
            out_uint8s(s, 2); /* num_updates set later */

            do
            {
                if (session->client_info->op1)
                {
                    s_push_layer(s, channel_hdr, 18);
                }
                else
                {
                    s_push_layer(s, channel_hdr, 26);
                }

                p = s->p;

                if (bpp > 24)
                {
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: 32 bpp");
                    lines_sending = xrdp_bitmap32_compress(data, width, height,
                                                           s, 32,
                                                           (MAX_BITMAP_BUF_SIZE - 100) - total_bufsize,
                                                           i - 1, temp_s, e, 0x10);
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: i %d lines_sending %d",
                              i, lines_sending);
                }
                else
                {
                    lines_sending = xrdp_bitmap_compress(data, width, height,
                                                         s, bpp,
                                                         (MAX_BITMAP_BUF_SIZE - 100) - total_bufsize,
                                                         i - 1, temp_s, e);
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: i %d lines_sending %d",
                              i, lines_sending);
                }

                if (lines_sending == 0)
                {
                    break;
                }

                num_updates++;
                bufsize = s->p - p;
                total_bufsize += bufsize;
                i = i - lines_sending;
                s_mark_end(s);
                s_pop_layer(s, channel_hdr);
                out_uint16_le(s, x); /* left */
                out_uint16_le(s, y + i); /* top */
                out_uint16_le(s, (x + cx) - 1); /* right */
                out_uint16_le(s, (y + i + lines_sending) - 1); /* bottom */
                out_uint16_le(s, width + e); /* width */
                out_uint16_le(s, lines_sending); /* height */
                out_uint16_le(s, bpp); /* bpp */

                if (session->client_info->op1)
                {
                    out_uint16_le(s, 0x401); /* compress */
                    out_uint16_le(s, bufsize); /* compressed size */
                    j = (width + e) * Bpp;
                    j = j * lines_sending;
                    total_bufsize += 18; /* bytes since pop layer */
                }
                else
                {
                    out_uint16_le(s, 0x1); /* compress */
                    out_uint16_le(s, bufsize + 8);
                    out_uint8s(s, 2); /* pad */
                    out_uint16_le(s, bufsize); /* compressed size */
                    j = (width + e) * Bpp;
                    out_uint16_le(s, j); /* line size */
                    j = j * lines_sending;
                    out_uint16_le(s, j); /* final size */
                    total_bufsize += 26; /* bytes since pop layer */
                }

                LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: decompressed pixels %d "
                          "decompressed bytes %d compressed bytes %d",
                          lines_sending * (width + e),
                          line_pad_bytes * lines_sending, bufsize);

                if (j > MAX_BITMAP_BUF_SIZE)
                {
                    LOG(LOG_LEVEL_WARNING, "libxrdp_send_bitmap: error, decompressed "
                        "size too big: %d bytes", j);
                }

                if (bufsize > MAX_BITMAP_BUF_SIZE)
                {
                    LOG(LOG_LEVEL_WARNING, "libxrdp_send_bitmap: error, compressed size "
                        "too big: %d bytes", bufsize);
                }

                s->p = s->end;
            }
            while (total_bufsize < MAX_BITMAP_BUF_SIZE && i > 0);

            LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: num_updates %d total_bufsize %d",
                      num_updates, total_bufsize);

            p_num_updates[0] = num_updates;
            p_num_updates[1] = num_updates >> 8;
            LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_UPDATE_BITMAP_DATA "
                      "updateType %d (UPDATETYPE_BITMAP), numberRectangles %d, "
                      "rectangles <omitted from log>",
                      RDP_UPDATE_BITMAP, num_updates);

            xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                               RDP_DATA_PDU_UPDATE);

            if (total_bufsize > MAX_BITMAP_BUF_SIZE)
            {
                LOG(LOG_LEVEL_WARNING, "libxrdp_send_bitmap: error, total compressed "
                    "size too big: %d bytes", total_bufsize);
            }
        }

        free_stream(temp_s);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_bitmap: no compression");
        total_lines = height;
        i = 0;
        p = data;

        if (line_bytes > 0 && total_lines > 0)
        {
            while (i < total_lines)
            {

                lines_sending = (MAX_BITMAP_BUF_SIZE - 100) / line_pad_bytes;

                if (i + lines_sending > total_lines)
                {
                    lines_sending = total_lines - i;
                }

                if (lines_sending == 0)
                {
                    LOG(LOG_LEVEL_WARNING, "libxrdp_send_bitmap: error, lines_sending == zero");
                    break;
                }

                p += server_line_bytes * lines_sending;
                xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
                out_uint16_le(s, RDP_UPDATE_BITMAP);
                out_uint16_le(s, 1); /* num updates */
                out_uint16_le(s, x);
                out_uint16_le(s, y + i);
                out_uint16_le(s, (x + cx) - 1);
                out_uint16_le(s, (y + i + lines_sending) - 1);
                out_uint16_le(s, width + e);
                out_uint16_le(s, lines_sending);
                out_uint16_le(s, bpp); /* bpp */
                out_uint16_le(s, 0); /* compress */
                out_uint16_le(s, line_pad_bytes * lines_sending); /* bufsize */
                q = p;

                switch (bpp)
                {
                    case 8:
                        for (j = 0; j < lines_sending; j++)
                        {
                            q = q - line_bytes;
                            out_uint8a(s, q, line_bytes);
                            out_uint8s(s, e);
                        }
                        break;
                    case 15:
                    case 16:
                        for (j = 0; j < lines_sending; j++)
                        {
                            q = q - server_line_bytes;
                            for (k = 0; k < width; k++)
                            {
                                pixel = *((tui16 *)(q + k * 2));
                                out_uint16_le(s, pixel);
                            }
                            out_uint8s(s, e * 2);
                        }
                        break;
                    case 24:
                        for (j = 0; j < lines_sending; j++)
                        {
                            q = q - server_line_bytes;
                            for (k = 0; k < width; k++)
                            {
                                pixel = *((tui32 *)(q + k * 4));
                                out_uint8(s, pixel);
                                out_uint8(s, pixel >> 8);
                                out_uint8(s, pixel >> 16);
                            }
                            out_uint8s(s, e * 3);
                        }
                        break;
                    case 32:
                        for (j = 0; j < lines_sending; j++)
                        {
                            q = q - server_line_bytes;
                            for (k = 0; k < width; k++)
                            {
                                pixel = *((int *)(q + k * 4));
                                out_uint32_le(s, pixel);
                            }
                            out_uint8s(s, e * 4);
                        }
                        break;
                }

                s_mark_end(s);
                LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_UPDATE_BITMAP_DATA "
                          "updateType %d (UPDATETYPE_BITMAP), numberRectangles 1, "
                          "rectangles <omitted from log>",
                          RDP_UPDATE_BITMAP);
                xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                                   RDP_DATA_PDU_UPDATE);
                i = i + lines_sending;
            }
        }
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_send_pointer(struct xrdp_session *session, int cache_idx,
                     char *data, char *mask, int x, int y, int bpp,
                     int width, int height)
{
    struct stream *s;
    char *p;
    tui16 *p16;
    tui32 *p32;
    int i;
    int j;
    int data_bytes;
    int mask_bytes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "sending cursor");
    if (bpp == 0)
    {
        bpp = 24;
    }
    if (width == 0)
    {
        width = 32;
    }
    if (height == 0)
    {
        height = 32;
    }
    /* error check */
    if ((session->client_info->pointer_flags & 1) == 0)
    {
        if (bpp != 24)
        {
            LOG(LOG_LEVEL_ERROR, "Send pointer: client does not support "
                "new cursors. The only valid bpp is 24, received %d", bpp);
            return 1;
        }
    }

    if ((bpp != 15) && (bpp != 16) && (bpp != 24) && (bpp != 32))
    {
        LOG(LOG_LEVEL_ERROR,
            "Send pointer: invalid bpp value. Expected 15, 16, 24 or 32, "
            "received %d", bpp);
        return 1;
    }
    make_stream(s);
    data_bytes = width * height * ((bpp + 7) / 8);
    mask_bytes = width * height / 8;
    init_stream(s, data_bytes + mask_bytes + 8192);
    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_pointer: fastpath");
        if (xrdp_rdp_init_fastpath((struct xrdp_rdp *)session->rdp, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_send_pointer: xrdp_rdp_init_fastpath failed");
            free_stream(s);
            return 1;
        }

        if ((session->client_info->pointer_flags & 1) != 0)
        {
            out_uint16_le(s, bpp); /* TS_FP_POINTERATTRIBUTE -> newPointerUpdateData.xorBpp */
        }
    }
    else /* slowpath */
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_send_pointer: slowpath");
        xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
        if ((session->client_info->pointer_flags & 1) == 0)
        {
            out_uint16_le(s, RDP_POINTER_COLOR);
            out_uint16_le(s, 0); /* pad */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_POINTER_PDU "
                      "messageType %d (TS_PTRMSGTYPE_COLOR), pad2Octets <ignored>",
                      RDP_POINTER_COLOR);
        }
        else
        {
            out_uint16_le(s, RDP_POINTER_POINTER);
            out_uint16_le(s, 0); /* pad */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_POINTER_PDU "
                      "messageType %d (TS_PTRMSGTYPE_POINTER), pad2Octets <ignored>",
                      RDP_POINTER_POINTER);

            out_uint16_le(s, bpp); /* TS_POINTERATTRIBUTE -> xorBpp */
        }
    }

    /* the TS_COLORPOINTERATTRIBUTE field which is shared by
       all of the pointer attribute PDU types */
    out_uint16_le(s, cache_idx);  /* cache_idx */
    out_uint16_le(s, x);          /* hotSpot.xPos */
    out_uint16_le(s, y);          /* hotSpot.yPos */
    out_uint16_le(s, width);      /* width */
    out_uint16_le(s, height);     /* height */
    out_uint16_le(s, mask_bytes); /* lengthAndMask */
    out_uint16_le(s, data_bytes); /* lengthXorMask */

    /* xorMaskData */
    switch (bpp)
    {
        case 15:
        /* fallthrough */
        case 16:
            p16 = (tui16 *) data;
            for (i = 0; i < height; i++)
            {
                for (j = 0; j < width; j++)
                {
                    out_uint16_le(s, *p16);
                    p16++;
                }
            }
            break;
        case 24:
            p = data;
            for (i = 0; i < height; i++)
            {
                for (j = 0; j < width; j++)
                {
                    out_uint8(s, *p);
                    p++;
                    out_uint8(s, *p);
                    p++;
                    out_uint8(s, *p);
                    p++;
                }
            }
            break;
        case 32:
            p32 = (tui32 *) data;
            for (i = 0; i < height; i++)
            {
                for (j = 0; j < width; j++)
                {
                    out_uint32_le(s, *p32);
                    p32++;
                }
            }
            break;
    }

    out_uint8a(s, mask, mask_bytes); /* andMaskData */
    out_uint8(s, 0); /* pad */
    s_mark_end(s);
    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        if ((session->client_info->pointer_flags & 1) == 0)
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_COLORPOINTERATTRIBUTE "
                      "cachedPointerUpdateData = { cacheIndex %d, "
                      "hotSpot.xPos %d, hotSpot.yPos %d, width %d, "
                      "height %d, lengthAndMask %d, lengthXorMask %d, "
                      "xorMaskData <omitted from log>, "
                      "andMaskData <omitted from log> }",
                      cache_idx, x, y, width, height, mask_bytes, data_bytes);
            if (xrdp_rdp_send_fastpath((struct xrdp_rdp *)session->rdp, s,
                                       FASTPATH_UPDATETYPE_COLOR) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "libxrdp_send_pointer: xrdp_rdp_send_fastpath failed");
                free_stream(s);
                return 1;
            }
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_POINTERATTRIBUTE "
                      "newPointerUpdateData.xorBpp %d, "
                      "newPointerUpdateData.colorPtrAttr = { cacheIndex %d, "
                      "hotSpot.xPos %d, hotSpot.yPos %d, width %d, "
                      "height %d, lengthAndMask %d, lengthXorMask %d, "
                      "xorMaskData <omitted from log>, "
                      "andMaskData <omitted from log> }",
                      bpp, cache_idx, x, y, width, height, mask_bytes, data_bytes);
            if (xrdp_rdp_send_fastpath((struct xrdp_rdp *)session->rdp, s,
                                       FASTPATH_UPDATETYPE_POINTER) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "libxrdp_send_pointer: xrdp_rdp_send_fastpath failed");
                free_stream(s);
                return 1;
            }
        }
    }
    else
    {
        if ((session->client_info->pointer_flags & 1) == 0)
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_COLORPOINTERATTRIBUTE "
                      "cacheIndex %d, "
                      "hotSpot.xPos %d, hotSpot.yPos %d, width %d, "
                      "height %d, lengthAndMask %d, lengthXorMask %d, "
                      "xorMaskData <omitted from log>, "
                      "andMaskData <omitted from log>",
                      cache_idx, x, y, width, height, mask_bytes, data_bytes);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_POINTERATTRIBUTE "
                      "xorBpp %d, colorPtrAttr = { cacheIndex %d, "
                      "hotSpot.xPos %d, hotSpot.yPos %d, width %d, "
                      "height %d, lengthAndMask %d, lengthXorMask %d, "
                      "xorMaskData <omitted from log>, "
                      "andMaskData <omitted from log> }",
                      bpp, cache_idx, x, y, width, height, mask_bytes, data_bytes);
        }
        xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                           RDP_DATA_PDU_POINTER);
    }
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_set_pointer(struct xrdp_session *session, int cache_idx)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        if (xrdp_rdp_init_fastpath((struct xrdp_rdp *)session->rdp, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_set_pointer: xrdp_rdp_init_fastpath failed");
            free_stream(s);
            return 1;
        }
    }
    else
    {
        xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
        out_uint16_le(s, RDP_POINTER_CACHED);
        out_uint16_le(s, 0); /* pad */
        LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_POINTER_PDU "
                  "messageType %d (TS_PTRMSGTYPE_CACHED), pad2Octets <ignored>",
                  RDP_POINTER_CACHED);
    }

    out_uint16_le(s, cache_idx); /* cache_idx */
    s_mark_end(s);

    if (session->client_info->use_fast_path & 1) /* fastpath output supported */
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_CACHEDPOINTERATTRIBUTE "
                  "cachedPointerUpdateData.cacheIndex %d", cache_idx);
        if (xrdp_rdp_send_fastpath((struct xrdp_rdp *)session->rdp, s,
                                   FASTPATH_UPDATETYPE_CACHED) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "libxrdp_set_pointer: xrdp_rdp_send_fastpath failed");
            free_stream(s);
            return 1;
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_CACHEDPOINTERATTRIBUTE "
                  "cacheIndex %d", cache_idx);
        xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                           RDP_DATA_PDU_POINTER);
    }
    free_stream(s);
    return 0;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_init(struct xrdp_session *session)
{
    return xrdp_orders_init((struct xrdp_orders *)session->orders);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_send(struct xrdp_session *session)
{
    return xrdp_orders_send((struct xrdp_orders *)session->orders);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_force_send(struct xrdp_session *session)
{
    return xrdp_orders_force_send((struct xrdp_orders *)session->orders);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_rect(struct xrdp_session *session, int x, int y,
                    int cx, int cy, int color, struct xrdp_rect *rect)
{
    return xrdp_orders_rect((struct xrdp_orders *)session->orders,
                            x, y, cx, cy, color, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_screen_blt(struct xrdp_session *session, int x, int y,
                          int cx, int cy, int srcx, int srcy,
                          int rop, struct xrdp_rect *rect)
{
    return xrdp_orders_screen_blt((struct xrdp_orders *)session->orders,
                                  x, y, cx, cy, srcx, srcy, rop, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_pat_blt(struct xrdp_session *session, int x, int y,
                       int cx, int cy, int rop, int bg_color,
                       int fg_color, struct xrdp_brush *brush,
                       struct xrdp_rect *rect)
{
    return xrdp_orders_pat_blt((struct xrdp_orders *)session->orders,
                               x, y, cx, cy, rop, bg_color, fg_color,
                               brush, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_dest_blt(struct xrdp_session *session, int x, int y,
                        int cx, int cy, int rop,
                        struct xrdp_rect *rect)
{
    return xrdp_orders_dest_blt((struct xrdp_orders *)session->orders,
                                x, y, cx, cy, rop, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_line(struct xrdp_session *session, int mix_mode,
                    int startx, int starty,
                    int endx, int endy, int rop, int bg_color,
                    struct xrdp_pen *pen,
                    struct xrdp_rect *rect)
{
    return xrdp_orders_line((struct xrdp_orders *)session->orders,
                            mix_mode, startx, starty, endx, endy,
                            rop, bg_color, pen, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_mem_blt(struct xrdp_session *session, int cache_id,
                       int color_table, int x, int y, int cx, int cy,
                       int rop, int srcx, int srcy,
                       int cache_idx, struct xrdp_rect *rect)
{
    return xrdp_orders_mem_blt((struct xrdp_orders *)session->orders,
                               cache_id, color_table, x, y, cx, cy, rop,
                               srcx, srcy, cache_idx, rect);
}

/******************************************************************************/
int
libxrdp_orders_composite_blt(struct xrdp_session *session, int srcidx,
                             int srcformat, int srcwidth, int srcrepeat,
                             int *srctransform, int mskflags,
                             int mskidx, int mskformat, int mskwidth,
                             int mskrepeat, int op, int srcx, int srcy,
                             int mskx, int msky, int dstx, int dsty,
                             int width, int height, int dstformat,
                             struct xrdp_rect *rect)
{
    return xrdp_orders_composite_blt((struct xrdp_orders *)session->orders,
                                     srcidx, srcformat, srcwidth, srcrepeat,
                                     srctransform, mskflags,
                                     mskidx, mskformat, mskwidth, mskrepeat,
                                     op, srcx, srcy, mskx, msky, dstx, dsty,
                                     width, height, dstformat, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_text(struct xrdp_session *session,
                    int font, int flags, int mixmode,
                    int fg_color, int bg_color,
                    int clip_left, int clip_top,
                    int clip_right, int clip_bottom,
                    int box_left, int box_top,
                    int box_right, int box_bottom,
                    int x, int y, char *data, int data_len,
                    struct xrdp_rect *rect)
{
    return xrdp_orders_text((struct xrdp_orders *)session->orders,
                            font, flags, mixmode, fg_color, bg_color,
                            clip_left, clip_top, clip_right, clip_bottom,
                            box_left, box_top, box_right, box_bottom,
                            x, y, data, data_len, rect);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_orders_send_palette(struct xrdp_session *session, int *palette,
                            int cache_id)
{
    return xrdp_orders_send_palette((struct xrdp_orders *)session->orders,
                                    palette, cache_id);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_raw_bitmap(struct xrdp_session *session,
                               int width, int height, int bpp, char *data,
                               int cache_id, int cache_idx)
{
    return xrdp_orders_send_raw_bitmap((struct xrdp_orders *)session->orders,
                                       width, height, bpp, data,
                                       cache_id, cache_idx);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_bitmap(struct xrdp_session *session,
                           int width, int height, int bpp, char *data,
                           int cache_id, int cache_idx)
{
    return xrdp_orders_send_bitmap((struct xrdp_orders *)session->orders,
                                   width, height, bpp, data,
                                   cache_id, cache_idx);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_font(struct xrdp_session *session,
                         struct xrdp_font_char *font_char,
                         int font_index, int char_index)
{
    return xrdp_orders_send_font((struct xrdp_orders *)session->orders,
                                 font_char, font_index, char_index);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_reset(struct xrdp_session *session)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_reset:");

    /* this will send any lingering orders */
    if (xrdp_orders_reset((struct xrdp_orders *)session->orders) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_reset: xrdp_orders_reset failed");
        return 1;
    }

    /*
     * Stop output from the client during the deactivation-reactivation
     * sequence [MS-RDPBCGR] 1.3.1.3 */
    xrdp_rdp_suppress_output((struct xrdp_rdp *)session->rdp, 1,
                             XSO_REASON_DEACTIVATE_REACTIVATE, 0, 0, 0, 0);

    /* shut down the rdp client
     *
     * When resetting the lib, disable application input checks, as
     * otherwise we can send a channel message to the other end while
     * the channels are inactive ([MS-RDPBCGR] 3.2.5.5.1 */
    session->check_for_app_input = 0;
    if (xrdp_rdp_send_deactivate((struct xrdp_rdp *)session->rdp) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_reset: xrdp_rdp_send_deactivate failed");
        return 1;
    }

    /* this should do the resizing */
    if (xrdp_caps_send_demand_active((struct xrdp_rdp *)session->rdp) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_reset: xrdp_caps_send_demand_active failed");
        return 1;
    }

    /* Re-enable application input checks */
    session->check_for_app_input = 1;

    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_raw_bitmap2(struct xrdp_session *session,
                                int width, int height, int bpp, char *data,
                                int cache_id, int cache_idx)
{
    return xrdp_orders_send_raw_bitmap2((struct xrdp_orders *)session->orders,
                                        width, height, bpp, data,
                                        cache_id, cache_idx);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_bitmap2(struct xrdp_session *session,
                            int width, int height, int bpp, char *data,
                            int cache_id, int cache_idx, int hints)
{
    return xrdp_orders_send_bitmap2((struct xrdp_orders *)session->orders,
                                    width, height, bpp, data,
                                    cache_id, cache_idx, hints);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_bitmap3(struct xrdp_session *session,
                            int width, int height, int bpp, char *data,
                            int cache_id, int cache_idx, int hints)
{
    return xrdp_orders_send_bitmap3((struct xrdp_orders *)session->orders,
                                    width, height, bpp, data,
                                    cache_id, cache_idx, hints);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_get_channel_count(const struct xrdp_session *session)
{
    int count = 0;
    const struct xrdp_rdp *rdp = (const struct xrdp_rdp *)session->rdp;
    const struct xrdp_mcs *mcs = rdp->sec_layer->mcs_layer;

    if (mcs->channel_list == NULL)
    {
        LOG(LOG_LEVEL_WARNING,
            "libxrdp_get_channel_count - No channel initialized");
    }
    else
    {
        count = mcs->channel_list->count;
    }

    return count;
}

/*****************************************************************************/
/* returns error */
/* this function gets the channel name and its flags, index is zero
   based.  either channel_name or channel_flags can be passed in nil if
   they are not needed */
int EXPORT_CC
libxrdp_query_channel(struct xrdp_session *session, int channel_id,
                      char *channel_name, int *channel_flags)
{
    int count = 0;
    struct xrdp_rdp *rdp = (struct xrdp_rdp *)NULL;
    struct xrdp_mcs *mcs = (struct xrdp_mcs *)NULL;
    struct mcs_channel_item *channel_item = (struct mcs_channel_item *)NULL;

    rdp = (struct xrdp_rdp *)session->rdp;
    mcs = rdp->sec_layer->mcs_layer;

    if (mcs->channel_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_query_channel - No channel initialized");
        return 1 ;
    }

    count = mcs->channel_list->count;

    if (channel_id < 0 || channel_id >= count)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_query_channel: Channel index out of range. "
            "max channel index %d, received channel index %d",
            count, channel_id);
        return 1;
    }

    channel_item = (struct mcs_channel_item *)
                   list_get_item(mcs->channel_list, channel_id);

    if (channel_item == NULL)
    {
        /* this should not happen */
        LOG(LOG_LEVEL_ERROR, "libxrdp_query_channel - channel item is NULL");
        return 1;
    }

    if (channel_name != 0)
    {
        g_strncpy(channel_name, channel_item->name, 8);
        LOG(LOG_LEVEL_DEBUG, "libxrdp_query_channel - Channel %d name %s",
            channel_id, channel_name);
    }

    if (channel_flags != 0)
    {
        *channel_flags = channel_item->flags;
    }

    return 0;
}

/*****************************************************************************/
/* returns a zero based index of the channel, -1 if error or it doesn't
   exist */
int EXPORT_CC
libxrdp_get_channel_id(struct xrdp_session *session, const char *name)
{
    int index = 0;
    int count = 0;
    struct xrdp_rdp *rdp = NULL;
    struct xrdp_mcs *mcs = NULL;
    struct mcs_channel_item *channel_item = NULL;

    rdp = (struct xrdp_rdp *)session->rdp;
    mcs = rdp->sec_layer->mcs_layer;

    if (mcs->channel_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_get_channel_id No channel initialized");
        return -1 ;
    }

    count = mcs->channel_list->count;

    for (index = 0; index < count; index++)
    {
        channel_item = (struct mcs_channel_item *)
                       list_get_item(mcs->channel_list, index);

        if (channel_item != 0)
        {
            if (g_strcasecmp(name, channel_item->name) == 0)
            {
                return index;
            }
        }
    }

    return -1;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_send_to_channel(struct xrdp_session *session, int channel_id,
                        char *data, int data_len,
                        int total_data_len, int flags)
{
    struct xrdp_rdp *rdp = NULL;
    struct xrdp_sec *sec = NULL;
    struct xrdp_channel *chan = NULL;
    struct stream *s = NULL;

    rdp = (struct xrdp_rdp *)session->rdp;
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    make_stream(s);
    init_stream(s, data_len + 1024); /* this should be big enough */

    if (xrdp_channel_init(chan, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_send_to_channel: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    else
    {
        LOG(LOG_LEVEL_TRACE, "libxrdp_send_to_channel: xrdp_channel_init successful!");
    }

    /* here we make a copy of the data */
    out_uint8a(s, data, data_len);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] Virtual Channel PDU "
              "data <omitted from log>");

    if (xrdp_channel_send(chan, s, channel_id, total_data_len, flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_send_to_channel: xrdp_channel_send failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
int
libxrdp_disable_channel(struct xrdp_session *session, int channel_id,
                        int is_disabled)
{
    struct xrdp_rdp *rdp;
    struct xrdp_mcs *mcs;
    struct mcs_channel_item *channel_item;

    rdp = (struct xrdp_rdp *) (session->rdp);
    mcs = rdp->sec_layer->mcs_layer;
    if (mcs->channel_list == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Channel list is NULL");
        return 1;
    }
    channel_item = (struct mcs_channel_item *)
                   list_get_item(mcs->channel_list, channel_id);
    if (channel_item == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Channel item is NULL");
        return 1;
    }
    channel_item->disabled = is_disabled;
    LOG(LOG_LEVEL_DEBUG, "%s channel %d (%s)",
        (is_disabled ? "Disabling" : "Enabling"), channel_item->chanid,
        channel_item->name);
    return 1;
}

/*****************************************************************************/
int
libxrdp_drdynvc_start(struct xrdp_session *session)
{
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_drdynvc_start:");

    rdp = (struct xrdp_rdp *) (session->rdp);
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    return xrdp_channel_drdynvc_start(chan);
}


/*****************************************************************************/
int
libxrdp_drdynvc_open(struct xrdp_session *session, const char *name,
                     int flags, struct xrdp_drdynvc_procs *procs,
                     int *chan_id)
{
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_drdynvc_open:");

    rdp = (struct xrdp_rdp *) (session->rdp);
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    return xrdp_channel_drdynvc_open(chan, name, flags, procs, chan_id);
}

/*****************************************************************************/
int
libxrdp_drdynvc_close(struct xrdp_session *session, int chan_id)
{
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_drdynvc_close:");

    rdp = (struct xrdp_rdp *) (session->rdp);
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    return xrdp_channel_drdynvc_close(chan, chan_id);
}

/*****************************************************************************/
int
libxrdp_drdynvc_data_first(struct xrdp_session *session, int chan_id,
                           const char *data, int data_bytes,
                           int total_data_bytes)
{
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_drdynvc_data_first:");

    rdp = (struct xrdp_rdp *) (session->rdp);
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    return xrdp_channel_drdynvc_data_first(chan, chan_id, data, data_bytes,
                                           total_data_bytes);
}

/*****************************************************************************/
int
libxrdp_drdynvc_data(struct xrdp_session *session, int chan_id,
                     const char *data, int data_bytes)
{
    struct xrdp_rdp *rdp;
    struct xrdp_sec *sec;
    struct xrdp_channel *chan;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_drdynvc_data:");

    rdp = (struct xrdp_rdp *) (session->rdp);
    sec = rdp->sec_layer;
    chan = sec->chan_layer;
    return xrdp_channel_drdynvc_data(chan, chan_id, data, data_bytes);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_brush(struct xrdp_session *session,
                          int width, int height, int bpp, int type,
                          int size, char *data, int cache_id)
{
    return xrdp_orders_send_brush((struct xrdp_orders *)session->orders,
                                  width, height, bpp, type, size, data,
                                  cache_id);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_create_os_surface(struct xrdp_session *session, int id,
                                      int width, int height,
                                      struct list *del_list)
{
    return xrdp_orders_send_create_os_surface
           ((struct xrdp_orders *)(session->orders), id,
            width, height, del_list);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_orders_send_switch_os_surface(struct xrdp_session *session, int id)
{
    return xrdp_orders_send_switch_os_surface
           ((struct xrdp_orders *)(session->orders), id);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_window_new_update(struct xrdp_session *session, int window_id,
                          struct rail_window_state_order *window_state,
                          int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_window_new_update(orders, window_id,
            window_state, flags);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_window_delete(struct xrdp_session *session, int window_id)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_window_delete(orders, window_id);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_window_icon(struct xrdp_session *session, int window_id,
                    int cache_entry, int cache_id,
                    struct rail_icon_info *icon_info, int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_window_icon(orders, window_id, cache_entry,
                                        cache_id, icon_info, flags);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_window_cached_icon(struct xrdp_session *session, int window_id,
                           int cache_entry, int cache_id,
                           int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_window_cached_icon(orders, window_id, cache_entry,
            cache_id, flags);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_notify_new_update(struct xrdp_session *session,
                          int window_id, int notify_id,
                          struct rail_notify_state_order *notify_state,
                          int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_notify_new_update(orders, window_id, notify_id,
            notify_state, flags);
}

/*****************************************************************************/
int
libxrdp_notify_delete(struct xrdp_session *session,
                      int window_id, int notify_id)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_notify_delete(orders, window_id, notify_id);
}

/*****************************************************************************/
int
libxrdp_monitored_desktop(struct xrdp_session *session,
                          struct rail_monitored_desktop_order *mdo,
                          int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_monitored_desktop(orders, mdo, flags);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_codec_jpeg_compress(struct xrdp_session *session,
                            int format, char *inp_data,
                            int width, int height,
                            int stride, int x, int y,
                            int cx, int cy, int quality,
                            char *out_data, int *io_len)
{
    struct xrdp_orders *orders;
    void *jpeg_han;

    orders = (struct xrdp_orders *)(session->orders);
    jpeg_han = orders->jpeg_han;
    return xrdp_codec_jpeg_compress(jpeg_han, format, inp_data,
                                    width, height, stride, x, y,
                                    cx, cy, quality, out_data, io_len);
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_fastpath_send_surface(struct xrdp_session *session,
                              char *data_pad, int pad_bytes,
                              int data_bytes,
                              int destLeft, int destTop,
                              int destRight, int destBottom, int bpp,
                              int codecID, int width, int height)
{
    struct stream ls;
    struct stream *s;
    struct xrdp_rdp *rdp;
    int sec_bytes;
    int rdp_bytes;
    int max_bytes;
    int cmd_bytes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_fastpath_send_surface:");
    if ((session->client_info->use_fast_path & 1) == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending data via fastpath is disabled");
        return 1;
    }
    max_bytes = session->client_info->max_fastpath_frag_bytes;
    if (max_bytes < 32 * 1024)
    {
        max_bytes = 32 * 1024;
    }
    rdp = (struct xrdp_rdp *) (session->rdp);
    rdp_bytes = xrdp_rdp_get_fastpath_bytes(rdp);
    sec_bytes = xrdp_sec_get_fastpath_bytes(rdp->sec_layer);
    cmd_bytes = 10 + 12;
    if (data_bytes + rdp_bytes + sec_bytes + cmd_bytes > max_bytes)
    {
        LOG(LOG_LEVEL_ERROR, "Too much data to send via fastpath. "
            "Max fastpath bytes %d, received bytes %d",
            max_bytes, (data_bytes + rdp_bytes + sec_bytes + cmd_bytes));
        return 1;
    }
    if (sec_bytes + rdp_bytes + cmd_bytes > pad_bytes)
    {
        LOG(LOG_LEVEL_ERROR, "Too much header to send via fastpath. "
            "Max fastpath header bytes %d, received bytes %d",
            pad_bytes, (rdp_bytes + sec_bytes + cmd_bytes));
        return 1;
    }
    g_memset(&ls, 0, sizeof(ls));
    s = &ls;
    s->data = (data_pad + pad_bytes) - (rdp_bytes + sec_bytes + cmd_bytes);
    s->sec_hdr = s->data;
    s->rdp_hdr = s->sec_hdr + sec_bytes;
    s->end = data_pad + pad_bytes + data_bytes;
    s->size = s->end - s->data;
    s->p = s->data + (rdp_bytes + sec_bytes);
    /* TS_SURFCMD_STREAM_SURF_BITS */
    out_uint16_le(s, CMDTYPE_STREAM_SURFACE_BITS);
    out_uint16_le(s, destLeft);
    out_uint16_le(s, destTop);
    out_uint16_le(s, destRight);
    out_uint16_le(s, destBottom);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_SURFCMD_STREAM_SURF_BITS "
              "cmdType 0x%4.4x (CMDTYPE_STREAM_SURFACE_BITS), destLeft %d, "
              "destTop %d, destRight %d, destBottom %d, "
              "bitmapData <see TS_BITMAP_DATA_EX>",
              CMDTYPE_STREAM_SURFACE_BITS, destLeft, destTop, destRight,
              destBottom);

    /* TS_BITMAP_DATA_EX */
    out_uint8(s, bpp);
    out_uint8(s, 0);
    out_uint8(s, 0);
    out_uint8(s, codecID);
    out_uint16_le(s, width);
    out_uint16_le(s, height);
    out_uint32_le(s, data_bytes);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding field bitmapData [MS-RDPBCGR] TS_BITMAP_DATA_EX "
              "bpp %d, flags 0x00, reserved 0, codecID %d, width %d, "
              "height %d, bitmapDataLength %d, exBitmapDataHeader <not present>, "
              "bitmapData <omitted from log>",
              bpp, codecID, width, height, data_bytes);

    if (xrdp_rdp_send_fastpath(rdp, s, FASTPATH_UPDATETYPE_SURFCMDS) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_fastpath_send_surface: xrdp_rdp_send_fastpath failed");
        return 1;
    }
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_fastpath_send_frame_marker(struct xrdp_session *session,
                                   int frame_action, int frame_id)
{
    struct stream *s;
    struct xrdp_rdp *rdp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "libxrdp_fastpath_send_frame_marker:");
    if ((session->client_info->use_fast_path & 1) == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending data via fastpath is disabled");
        return 1;
    }
    if (session->client_info->use_frame_acks == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Fastpath frame acks is disabled");
        return 1;
    }
    rdp = (struct xrdp_rdp *) (session->rdp);
    make_stream(s);
    init_stream(s, 8192);
    xrdp_rdp_init_fastpath(rdp, s);
    out_uint16_le(s, CMDTYPE_FRAME_MARKER);
    out_uint16_le(s, frame_action);
    out_uint32_le(s, frame_id);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FRAME_MARKER "
              "cmdType 0x%4.4x (CMDTYPE_FRAME_MARKER), frameAction 0x%4.4x, "
              "frameId %d",
              CMDTYPE_FRAME_MARKER, frame_action, frame_id);

    if (xrdp_rdp_send_fastpath(rdp, s, FASTPATH_UPDATETYPE_SURFCMDS) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_fastpath_send_frame_marker: xrdp_rdp_send_fastpath failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_send_session_info(struct xrdp_session *session, const char *data,
                          int data_bytes)
{
    struct xrdp_rdp *rdp;

    rdp = (struct xrdp_rdp *) (session->rdp);
    return xrdp_rdp_send_session_info(rdp, data, data_bytes);
}

/*****************************************************************************/
/*
   Sanitise extended monitor attributes

   The extended attributes are received from either
   [MS-RDPEDISP] 2.2.2.2.1 (DISPLAYCONTROL_MONITOR_LAYOUT), or
   [MS-RDPBCGR] 2.2.1.3.9.1 (TS_MONITOR_ATTRIBUTES)

   @param monitor_layout struct containing extended attributes
*/
static void
sanitise_extended_monitor_attributes(struct monitor_info *monitor_layout)
{
    if (monitor_layout->physical_width == 0
            && monitor_layout->physical_width == 0
            && monitor_layout->orientation == 0
            && monitor_layout->desktop_scale_factor == 0
            && monitor_layout->device_scale_factor == 0)
    {
        /* Module expects us to provide defaults */
        monitor_layout->orientation = ORIENTATION_LANDSCAPE;
        monitor_layout->desktop_scale_factor = 100;
        monitor_layout->device_scale_factor = 100;
        return;
    }

    /* if EITHER physical_width or physical_height are
     * out of range, BOTH must be ignored.
     */
    if (monitor_layout->physical_width > 10000
            || monitor_layout->physical_width < 10)
    {
        LOG(LOG_LEVEL_WARNING, "sanitise_extended_monitor_attributes:"
            " physical_width is not within valid range."
            " Setting physical_width to 0mm,"
            " Setting physical_height to 0mm,"
            " physical_width was: %d",
            monitor_layout->physical_width);
        monitor_layout->physical_width = 0;
        monitor_layout->physical_height = 0;
    }

    if (monitor_layout->physical_height > 10000
            || monitor_layout->physical_height < 10)
    {
        LOG(LOG_LEVEL_WARNING, "sanitise_extended_monitor_attributes:"
            " physical_height is not within valid range."
            " Setting physical_width to 0mm,"
            " Setting physical_height to 0mm,"
            " physical_height was: %d",
            monitor_layout->physical_height);
        monitor_layout->physical_width = 0;
        monitor_layout->physical_height = 0;
    }

    switch (monitor_layout->orientation)
    {
        case ORIENTATION_LANDSCAPE:
        case ORIENTATION_PORTRAIT:
        case ORIENTATION_LANDSCAPE_FLIPPED:
        case ORIENTATION_PORTRAIT_FLIPPED:
            break;
        default:
            LOG(LOG_LEVEL_WARNING, "sanitise_extended_monitor_attributes:"
                " Orientation is not one of %d, %d, %d, or %d."
                " Value was %d and ignored and set to default value of LANDSCAPE.",
                ORIENTATION_LANDSCAPE,
                ORIENTATION_PORTRAIT,
                ORIENTATION_LANDSCAPE_FLIPPED,
                ORIENTATION_PORTRAIT_FLIPPED,
                monitor_layout->orientation);
            monitor_layout->orientation = ORIENTATION_LANDSCAPE;
    }

    int check_desktop_scale_factor
        = monitor_layout->desktop_scale_factor < 100
          || monitor_layout->desktop_scale_factor > 500;
    if (check_desktop_scale_factor)
    {
        LOG(LOG_LEVEL_WARNING, "sanitise_extended_monitor_attributes:"
            " desktop_scale_factor is not within valid range"
            " of [100, 500]. Assuming 100. Value was: %d",
            monitor_layout->desktop_scale_factor);
    }

    int check_device_scale_factor
        = monitor_layout->device_scale_factor != 100
          && monitor_layout->device_scale_factor != 140
          && monitor_layout->device_scale_factor != 180;
    if (check_device_scale_factor)
    {
        LOG(LOG_LEVEL_WARNING, "sanitise_extended_monitor_attributes:"
            " device_scale_factor a valid value (One of 100, 140, 180)."
            " Assuming 100. Value was: %d",
            monitor_layout->device_scale_factor);
    }

    if (check_desktop_scale_factor || check_device_scale_factor)
    {
        monitor_layout->desktop_scale_factor = 100;
        monitor_layout->device_scale_factor = 100;
    }
}

/*****************************************************************************/
/*
   Process a [MS-RDPBCGR] TS_UD_CS_MONITOR message.
   reads the client monitors data
*/
int
libxrdp_process_monitor_stream(struct stream *s,
                               struct display_size_description *description,
                               int full_parameters)
{
    uint32_t num_monitor;
    uint32_t monitor_index;
    struct monitor_info monitors[CLIENT_MONITOR_DATA_MAXIMUM_MONITORS];
    struct monitor_info *monitor_layout;
    int monitor_struct_stream_check_bytes;
    const char *monitor_struct_stream_check_message;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_process_monitor_stream:");
    if (description == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR,
                  "libxrdp_process_monitor_stream: description was"
                  " null. Valid pointer to allocated description expected.");
        return SEC_PROCESS_MONITORS_ERR;
    }

    if (!s_check_rem_and_log(s, 4,
                             "libxrdp_process_monitor_stream:"
                             " Parsing [MS-RDPBCGR] TS_UD_CS_MONITOR"))
    {
        return SEC_PROCESS_MONITORS_ERR;
    }

    in_uint32_le(s, num_monitor);
    LOG(LOG_LEVEL_DEBUG, "libxrdp_process_monitor_stream:"
        " The number of monitors received is: %d",
        num_monitor);

    if (num_monitor >= CLIENT_MONITOR_DATA_MAXIMUM_MONITORS)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_process_monitor_stream: [MS-RDPBCGR] Protocol"
            " error: TS_UD_CS_MONITOR monitorCount"
            " MUST be less than %d, received: %d",
            CLIENT_MONITOR_DATA_MAXIMUM_MONITORS, num_monitor);
        return SEC_PROCESS_MONITORS_ERR_TOO_MANY_MONITORS;
    }

    /*
     * Unfortunately the structure length values aren't directly defined in the
     * Microsoft specifications. They are derived from the lengths of the
     * specific structures referenced below.
     */
    if (full_parameters == 0)
    {
        monitor_struct_stream_check_bytes = 20;
        monitor_struct_stream_check_message =
            "libxrdp_process_monitor_stream: Parsing monitor definitions"
            " from [MS-RDPBCGR] 2.2.1.3.6.1 Monitor Definition"
            " (TS_MONITOR_DEF).";
    }
    else
    {
        monitor_struct_stream_check_bytes = 40;
        monitor_struct_stream_check_message =
            "libxrdp_process_monitor_stream: Parsing monitor definitions"
            " from [MS-RDPEDISP] 2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT.";
    }

    memset(monitors, 0, sizeof(monitors[0]) * num_monitor);

    for (monitor_index = 0; monitor_index < num_monitor; ++monitor_index)
    {
        if (!s_check_rem_and_log(
                    s,
                    monitor_struct_stream_check_bytes,
                    monitor_struct_stream_check_message))
        {
            return SEC_PROCESS_MONITORS_ERR;
        }

        monitor_layout = &monitors[monitor_index];

        if (full_parameters != 0)
        {
            in_uint32_le(s, monitor_layout->flags);
        }
        in_uint32_le(s, monitor_layout->left);
        in_uint32_le(s, monitor_layout->top);

        if (full_parameters == 0)
        {
            in_uint32_le(s, monitor_layout->right);
            in_uint32_le(s, monitor_layout->bottom);
            in_uint32_le(s, monitor_layout->is_primary);

            /*
             * 2.2.1.3.6.1 Monitor Definition (TS_MONITOR_DEF)
             */
            LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_process_monitor_stream:"
                      " Received [MS-RDPBCGR] 2.2.1.3.6.1"
                      " TS_UD_CS_MONITOR.TS_MONITOR_DEF"
                      " Index: %d, Left %d, Top %d, Right %d, Bottom %d,"
                      " Flags 0x%8.8x",
                      monitor_index,
                      monitor_layout->left,
                      monitor_layout->top,
                      monitor_layout->right,
                      monitor_layout->bottom,
                      monitor_layout->is_primary);
        }
        else
        {
            /* Per spec (2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT),
             * this is the width.
             * 200 <= width <= 8192 and must not be odd.
             * Ex: in_uint32_le(s, monitor_layout->width);
             */
            int width;
            in_uint32_le(s, width);
            if (width > CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_MONITOR_WIDTH ||
                    width < CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_MONITOR_WIDTH ||
                    width % 2 != 0)
            {
                return SEC_PROCESS_MONITORS_ERR_INVALID_MONITOR;
            }
            monitor_layout->right = monitor_layout->left + width - 1;

            /* Per spec (2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT),
             * this is the height.
             * 200 <= height <= 8192
             * Ex: in_uint32_le(s, monitor_layout->height);
             */
            int height;
            in_uint32_le(s, height);
            if (height > CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_MONITOR_HEIGHT ||
                    height < CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_MONITOR_HEIGHT)
            {
                return SEC_PROCESS_MONITORS_ERR_INVALID_MONITOR;
            }
            monitor_layout->bottom = monitor_layout->top + height - 1;

            in_uint32_le(s, monitor_layout->physical_width);
            in_uint32_le(s, monitor_layout->physical_height);
            in_uint32_le(s, monitor_layout->orientation);
            in_uint32_le(s, monitor_layout->desktop_scale_factor);
            in_uint32_le(s, monitor_layout->device_scale_factor);

            /*
             * 2.2.2.2.1 DISPLAYCONTROL_MONITOR_LAYOUT
             */
            LOG_DEVEL(LOG_LEVEL_INFO, "libxrdp_process_monitor_stream:"
                      " Received [MS-RDPEDISP] 2.2.2.2.1"
                      " DISPLAYCONTROL_MONITOR_LAYOUT_PDU"
                      ".DISPLAYCONTROL_MONITOR_LAYOUT"
                      " Index: %d, Flags 0x%8.8x, Left %d, Top %d, Width %d,"
                      " Height %d, PhysicalWidth %d, PhysicalHeight %d,"
                      " Orientation %d, DesktopScaleFactor %d,"
                      " DeviceScaleFactor %d",
                      monitor_index,
                      monitor_layout->flags,
                      monitor_layout->left,
                      monitor_layout->top,
                      width,
                      height,
                      monitor_layout->physical_width,
                      monitor_layout->physical_height,
                      monitor_layout->orientation,
                      monitor_layout->desktop_scale_factor,
                      monitor_layout->device_scale_factor);

            if (monitor_layout->flags == DISPLAYCONTROL_MONITOR_PRIMARY)
            {
                monitor_layout->is_primary = TS_MONITOR_PRIMARY;
            }
        }
    }

    return libxrdp_init_display_size_description(
               num_monitor, monitors, description);
}

/*****************************************************************************/
int
libxrdp_process_monitor_ex_stream(struct stream *s,
                                  struct display_size_description *description)
{
    uint32_t num_monitor;
    uint32_t monitor_index;
    uint32_t attribute_size;
    struct monitor_info *monitor_layout;

    LOG_DEVEL(LOG_LEVEL_TRACE, "libxrdp_process_monitor_ex_stream:");
    if (description == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "libxrdp_process_monitor_ex_stream: "
                  "description was null. "
                  " Valid pointer to allocated description expected.");
        return SEC_PROCESS_MONITORS_ERR;
    }

    if (!s_check_rem_and_log(s, 4,
                             "libxrdp_process_monitor_ex_stream:"
                             " Parsing [MS-RDPBCGR] TS_UD_CS_MONITOR_EX"))
    {
        return SEC_PROCESS_MONITORS_ERR;
    }

    in_uint32_le(s, attribute_size);
    if (attribute_size != TS_MONITOR_ATTRIBUTES_SIZE)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_process_monitor_ex_stream: [MS-RDPBCGR] Protocol"
            " error: TS_UD_CS_MONITOR_EX monitorAttributeSize"
            " MUST be %d, received: %d",
            TS_MONITOR_ATTRIBUTES_SIZE, attribute_size);
        return SEC_PROCESS_MONITORS_ERR;
    }

    in_uint32_le(s, num_monitor);
    LOG(LOG_LEVEL_DEBUG, "libxrdp_process_monitor_ex_stream:"
        " The number of monitors received is: %d",
        num_monitor);

    if (num_monitor != description->monitorCount)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_process_monitor_ex_stream: [MS-RDPBCGR] Protocol"
            " error: TS_UD_CS_MONITOR monitorCount"
            " MUST be %d, received: %d",
            description->monitorCount, num_monitor);
        return SEC_PROCESS_MONITORS_ERR;
    }

    for (monitor_index = 0; monitor_index < num_monitor; ++monitor_index)
    {
        if (!s_check_rem_and_log(s, attribute_size,
                                 "libxrdp_process_monitor_ex_stream:"
                                 " Parsing TS_UD_CS_MONITOR_EX"))
        {
            return SEC_PROCESS_MONITORS_ERR;
        }

        monitor_layout = description->minfo + monitor_index;

        in_uint32_le(s, monitor_layout->physical_width);
        in_uint32_le(s, monitor_layout->physical_height);
        in_uint32_le(s, monitor_layout->orientation);
        in_uint32_le(s, monitor_layout->desktop_scale_factor);
        in_uint32_le(s, monitor_layout->device_scale_factor);

        sanitise_extended_monitor_attributes(monitor_layout);

        LOG_DEVEL(LOG_LEVEL_INFO, "libxrdp_process_monitor_ex_stream:"
                  " Received [MS-RDPBCGR] 2.2.1.3.9.1 "
                  " TS_MONITOR_ATTRIBUTES"
                  " Index: %d, PhysicalWidth %d, PhysicalHeight %d,"
                  " Orientation %d, DesktopScaleFactor %d,"
                  " DeviceScaleFactor %d",
                  monitor_index,
                  monitor_layout->physical_width,
                  monitor_layout->physical_height,
                  monitor_layout->orientation,
                  monitor_layout->desktop_scale_factor,
                  monitor_layout->device_scale_factor);
    }

    /* Update non negative monitor info values */
    const struct monitor_info *src = description->minfo;
    struct monitor_info *dst = description->minfo_wm;
    for (monitor_index = 0; monitor_index < num_monitor; ++monitor_index)
    {
        dst->physical_width = src->physical_width;
        dst->physical_height = src->physical_height;
        dst->orientation = src->orientation;
        dst->desktop_scale_factor = src->desktop_scale_factor;
        dst->device_scale_factor = src->device_scale_factor;
        ++src;
        ++dst;
    }

    return 0;
}

/*****************************************************************************/
int
libxrdp_init_display_size_description(
    unsigned int num_monitor,
    const struct monitor_info *monitors,
    struct display_size_description *description)
{
    unsigned int monitor_index;
    struct monitor_info *monitor_layout;
    struct xrdp_rect all_monitors_encompassing_bounds = {0};
    int got_primary = 0;

    /* Caller should have checked this, so don't log an error */
    if (num_monitor > CLIENT_MONITOR_DATA_MAXIMUM_MONITORS)
    {
        return SEC_PROCESS_MONITORS_ERR_TOO_MANY_MONITORS;
    }

    description->monitorCount = num_monitor;
    for (monitor_index = 0 ; monitor_index < num_monitor; ++monitor_index)
    {
        monitor_layout = &description->minfo[monitor_index];
        *monitor_layout = monitors[monitor_index];
        sanitise_extended_monitor_attributes(monitor_layout);

        if (monitor_index == 0)
        {
            all_monitors_encompassing_bounds.left = monitor_layout->left;
            all_monitors_encompassing_bounds.top = monitor_layout->top;
            all_monitors_encompassing_bounds.right = monitor_layout->right;
            all_monitors_encompassing_bounds.bottom = monitor_layout->bottom;
        }
        else
        {
            all_monitors_encompassing_bounds.left =
                MIN(monitor_layout->left,
                    all_monitors_encompassing_bounds.left);
            all_monitors_encompassing_bounds.top =
                MIN(monitor_layout->top,
                    all_monitors_encompassing_bounds.top);
            all_monitors_encompassing_bounds.right =
                MAX(all_monitors_encompassing_bounds.right,
                    monitor_layout->right);
            all_monitors_encompassing_bounds.bottom =
                MAX(all_monitors_encompassing_bounds.bottom,
                    monitor_layout->bottom);
        }

        if (monitor_layout->is_primary == TS_MONITOR_PRIMARY)
        {
            if (got_primary)
            {
                // Already got one - don't have two
                monitor_layout->is_primary = 0;
            }
            else
            {
                got_primary = 1;
            }
        }
    }

    if (!got_primary)
    {
        /* no primary monitor was set,
         * choose the leftmost monitor as primary.
         */
        for (monitor_index = 0; monitor_index < num_monitor; ++monitor_index)
        {
            monitor_layout = description->minfo + monitor_index;
            if (monitor_layout->left
                    == all_monitors_encompassing_bounds.left
                    && monitor_layout->top
                    == all_monitors_encompassing_bounds.top)
            {
                monitor_layout->is_primary = TS_MONITOR_PRIMARY;
                break;
            }
        }
    }

    /* set wm geometry if the encompassing area is well formed.
       Otherwise, log and return an error.
    */
    if (all_monitors_encompassing_bounds.right
            > all_monitors_encompassing_bounds.left
            && all_monitors_encompassing_bounds.bottom
            > all_monitors_encompassing_bounds.top)
    {
        description->session_width =
            all_monitors_encompassing_bounds.right
            - all_monitors_encompassing_bounds.left + 1;
        description->session_height =
            all_monitors_encompassing_bounds.bottom
            - all_monitors_encompassing_bounds.top + 1;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "libxrdp_init_display_size_description:"
            " The area encompassing the monitors is not a"
            " well-formed rectangle. Received"
            " (top: %d, left: %d, right: %d, bottom: %d)."
            " This will prevent initialization.",
            all_monitors_encompassing_bounds.top,
            all_monitors_encompassing_bounds.left,
            all_monitors_encompassing_bounds.right,
            all_monitors_encompassing_bounds.bottom);
        return SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP;
    }

    /* Make sure virtual desktop size is OK
     * 2.2.1.3.6 Client Monitor Data (TS_UD_CS_MONITOR)
     */
    if (description->session_width
            > CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_WIDTH
            || description->session_width
            < CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_WIDTH
            || description->session_height
            > CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_HEIGHT
            || description->session_height
            < CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_HEIGHT)
    {
        LOG(LOG_LEVEL_ERROR,
            "libxrdp_init_display_size_description: Calculated virtual"
            " desktop width or height is invalid."
            " Allowed width range: min %d, max %d. Width received: %d."
            " Allowed height range: min %d, max %d. Height received: %d",
            CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_WIDTH,
            CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_WIDTH,
            description->session_width,
            CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_DESKTOP_HEIGHT,
            CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_DESKTOP_HEIGHT,
            description->session_width);
        return SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP;
    }

    /* keep a copy of non negative monitor info values for xrdp_wm usage */
    for (monitor_index = 0; monitor_index < num_monitor; ++monitor_index)
    {
        monitor_layout = description->minfo_wm + monitor_index;

        *monitor_layout = description->minfo[monitor_index];

        monitor_layout->left =
            monitor_layout->left - all_monitors_encompassing_bounds.left;
        monitor_layout->top =
            monitor_layout->top - all_monitors_encompassing_bounds.top;
        monitor_layout->right =
            monitor_layout->right - all_monitors_encompassing_bounds.left;
        monitor_layout->bottom =
            monitor_layout->bottom - all_monitors_encompassing_bounds.top;
    }

    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_planar_compress(char *in_data, int width, int height,
                        struct stream *s, int bpp, int byte_limit,
                        int start_line, struct stream *temp_s,
                        int e, int flags)
{
    return xrdp_bitmap32_compress(in_data, width, height,
                                  s, bpp, byte_limit,
                                  start_line, temp_s,
                                  e, flags);
}
