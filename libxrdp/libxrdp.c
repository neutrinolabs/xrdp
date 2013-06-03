/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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

#include "libxrdp.h"

/******************************************************************************/
struct xrdp_session *EXPORT_CC
libxrdp_init(tbus id, struct trans *trans)
{
    struct xrdp_session *session;

    session = (struct xrdp_session *)g_malloc(sizeof(struct xrdp_session), 1);
    session->id = id;
    session->rdp = xrdp_rdp_create(session, trans);
    session->orders = xrdp_orders_create(session, (struct xrdp_rdp *)session->rdp);
    session->client_info = &(((struct xrdp_rdp *)session->rdp)->client_info);
    make_stream(session->s);
    init_stream(session->s, 8192 * 2);
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
    free_stream(session->s);
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
libxrdp_process_incomming(struct xrdp_session *session)
{
    return xrdp_rdp_incoming((struct xrdp_rdp *)session->rdp);
}

/******************************************************************************/
int EXPORT_CC
libxrdp_process_data(struct xrdp_session *session)
{
    int cont;
    int rv;
    int code;
    int term;
    int dead_lock_counter;

    term = 0;
    cont = 1;
    rv = 0;
    dead_lock_counter = 0;

    while ((cont || !session->up_and_running) && !term)
    {
        if (session->is_term != 0)
        {
            if (session->is_term())
            {
                term = 1;
            }
        }

        code = 0;

        if (xrdp_rdp_recv((struct xrdp_rdp *)(session->rdp),
                          session->s, &code) != 0)
        {
            rv = 1;
            break;
        }

        DEBUG(("libxrdp_process_data code %d", code));

        switch (code)
        {
            case -1:
                xrdp_rdp_send_demand_active((struct xrdp_rdp *)session->rdp);
                session->up_and_running = 0;
                break;
            case 0:
                dead_lock_counter++;
                break;
            case RDP_PDU_CONFIRM_ACTIVE: /* 3 */
                xrdp_rdp_process_confirm_active((struct xrdp_rdp *)session->rdp,
                                                session->s);
                break;
            case RDP_PDU_DATA: /* 7 */

                if (xrdp_rdp_process_data((struct xrdp_rdp *)session->rdp,
                                          session->s) != 0)
                {
                    DEBUG(("libxrdp_process_data returned non zero"));
                    cont = 0;
                    term = 1;
                }

                break;
            default:
                g_writeln("unknown in libxrdp_process_data");
                dead_lock_counter++;
                break;
        }

        if (dead_lock_counter > 100000)
        {
            /*This situation can happen and this is a workaround*/
            cont = 0;
            g_writeln("Serious programming error we were locked in a deadly loop") ;
            g_writeln("remaining :%d", session->s->end - session->s->next_packet);
            session->s->next_packet = 0;
        }

        if (cont)
        {
            cont = (session->s->next_packet != 0) &&
                   (session->s->next_packet < session->s->end);
        }
    }

    return rv;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_send_palette(struct xrdp_session *session, int *palette)
{
    int i = 0;
    int color = 0;
    struct stream *s = (struct stream *)NULL;

    if (session->client_info->bpp > 8)
    {
        return 0;
    }

    DEBUG(("libxrdp_send_palette sending palette"));
    /* clear orders */
    libxrdp_orders_force_send(session);
    make_stream(s);
    init_stream(s, 8192);
    xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
    out_uint16_le(s, RDP_UPDATE_PALETTE);
    out_uint16_le(s, 0);
    out_uint16_le(s, 256); /* # of colors */
    out_uint16_le(s, 0);

    for (i = 0; i < 256; i++)
    {
        color = palette[i];
        out_uint8(s, color >> 16);
        out_uint8(s, color >> 8);
        out_uint8(s, color);
    }

    s_mark_end(s);
    xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s, RDP_DATA_PDU_UPDATE);
    free_stream(s);
    /* send the orders palette too */
    libxrdp_orders_init(session);
    libxrdp_orders_send_palette(session, palette, 0);
    libxrdp_orders_send(session);
    return 0;
}

/******************************************************************************/
int EXPORT_CC
libxrdp_send_bell(struct xrdp_session *session)
{
    struct stream *s = (struct stream *)NULL;

    DEBUG(("libxrdp_send_bell sending bell signal"));
    /* see MS documentation: Server play sound PDU, TS_PLAY_SOUND_PDU_DATA */

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint32_le(s, 440); /* frequency */
    out_uint32_le(s, 100); /* duration (ms) */
    s_mark_end(s);

    if (xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s, RDP_DATA_PDU_PLAY_SOUND) != 0)
    {
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
    int line_size = 0;
    int i = 0;
    int j = 0;
    int total_lines = 0;
    int lines_sending = 0;
    int Bpp = 0;
    int e = 0;
    int bufsize = 0;
    int total_bufsize = 0;
    int num_updates = 0;
    char *p_num_updates = (char *)NULL;
    char *p = (char *)NULL;
    char *q = (char *)NULL;
    struct stream *s = (struct stream *)NULL;
    struct stream *temp_s = (struct stream *)NULL;

    DEBUG(("libxrdp_send_bitmap sending bitmap"));
    Bpp = (bpp + 7) / 8;
    e = width % 4;

    if (e != 0)
    {
        e = 4 - e;
    }

    line_size = width * Bpp;
    make_stream(s);
    init_stream(s, 8192);

    if (session->client_info->use_bitmap_comp)
    {
        make_stream(temp_s);
        init_stream(temp_s, 65536);
        i = 0;

        if (cy <= height)
        {
            i = cy;
        }

        while (i > 0)
        {
            total_bufsize = 0;
            num_updates = 0;
            xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
            out_uint16_le(s, RDP_UPDATE_BITMAP);
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
                lines_sending = xrdp_bitmap_compress(data, width, height,
                                                     s, bpp,
                                                     4096 - total_bufsize,
                                                     i - 1, temp_s, e);

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
                }

                if (j > 32768)
                {
                    g_writeln("error, decompressed size too big, its %d", j);
                }

                if (bufsize > 8192)
                {
                    g_writeln("error, compressed size too big, its %d", bufsize);
                }

                s->p = s->end;
            }
            while (total_bufsize < 4096 && i > 0);

            p_num_updates[0] = num_updates;
            p_num_updates[1] = num_updates >> 8;
            xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s,
                               RDP_DATA_PDU_UPDATE);

            if (total_bufsize > 8192)
            {
                g_writeln("error, total compressed size too big, its %d",
                          total_bufsize);
            }
        }

        free_stream(temp_s);
    }
    else
    {
        total_lines = height;
        i = 0;
        p = data;

        if (line_size > 0 && total_lines > 0)
        {
            while (i < total_lines)
            {
                lines_sending = 4096 / (line_size + e * Bpp);

                if (i + lines_sending > total_lines)
                {
                    lines_sending = total_lines - i;
                }

                p = p + line_size * lines_sending;
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
                out_uint16_le(s, (line_size + e * Bpp) * lines_sending); /* bufsize */
                q = p;

                for (j = 0; j < lines_sending; j++)
                {
                    q = q - line_size;
                    out_uint8a(s, q, line_size); /* B_ENDIAN doesn't work here, todo */
                    out_uint8s(s, e * Bpp);
                }

                s_mark_end(s);
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
                     char *data, char *mask, int x, int y, int bpp)
{
    struct stream *s;
    char *p;
    tui16 *p16;
    tui32 *p32;
    int i;
    int j;
    int data_bytes;

    DEBUG(("libxrdp_send_pointer sending cursor"));
    if (bpp == 0)
    {
        bpp = 24;
    }
    /* error check */
    if ((session->client_info->pointer_flags & 1) == 0)
    {
        if (bpp != 24)
        {
            g_writeln("libxrdp_send_pointer: error");
            return 1;
        }
    }
    if ((bpp == 15) && (bpp != 16) && (bpp != 24) && (bpp != 32))
    {
        g_writeln("libxrdp_send_pointer: error");
        return 1;
    }
    make_stream(s);
    init_stream(s, 8192);
    xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
    if ((session->client_info->pointer_flags & 1) == 0)
    {
        out_uint16_le(s, RDP_POINTER_COLOR);
        out_uint16_le(s, 0); /* pad */
        data_bytes = 3072;
    }
    else
    {
        out_uint16_le(s, RDP_POINTER_POINTER);
        out_uint16_le(s, 0); /* pad */
        out_uint16_le(s, bpp);
        data_bytes = ((bpp + 7) / 8) * 32 * 32;
    }
    out_uint16_le(s, cache_idx); /* cache_idx */
    out_uint16_le(s, x);
    out_uint16_le(s, y);
    out_uint16_le(s, 32);
    out_uint16_le(s, 32);
    out_uint16_le(s, 128);
    out_uint16_le(s, data_bytes);

    switch (bpp)
    {
        case 15:
        case 16:
            p16 = (tui16 *) data;
            for (i = 0; i < 32; i++)
            {
                for (j = 0; j < 32; j++)
                {
                    out_uint16_le(s, *p16);
                    p16++;
                }
            }
            break;
        case 24:
            p = data;
            for (i = 0; i < 32; i++)
            {
                for (j = 0; j < 32; j++)
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
            for (i = 0; i < 32; i++)
            {
                for (j = 0; j < 32; j++)
                {
                    out_uint32_le(s, *p32);
                    p32++;
                }
            }
            break;
    }

    out_uint8a(s, mask, 128); /* mask */
    s_mark_end(s);
    xrdp_rdp_send_data((struct xrdp_rdp *)(session->rdp), s,
                       RDP_DATA_PDU_POINTER);
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int EXPORT_CC
libxrdp_set_pointer(struct xrdp_session *session, int cache_idx)
{
    struct stream *s;

    DEBUG(("libxrdp_set_pointer sending cursor index"));
    make_stream(s);
    init_stream(s, 8192);
    xrdp_rdp_init_data((struct xrdp_rdp *)session->rdp, s);
    out_uint16_le(s, RDP_POINTER_CACHED);
    out_uint16_le(s, 0); /* pad */
    out_uint16_le(s, cache_idx); /* cache_idx */
    s_mark_end(s);
    xrdp_rdp_send_data((struct xrdp_rdp *)session->rdp, s, RDP_DATA_PDU_POINTER);
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
libxrdp_reset(struct xrdp_session *session,
              int width, int height, int bpp)
{
    if (session->client_info != 0)
    {
        /* older client can't resize */
        if (session->client_info->build <= 419)
        {
            return 0;
        }

        /* if same, don't need to do anything */
        if (session->client_info->width == width &&
                session->client_info->height == height &&
                session->client_info->bpp == bpp)
        {
            return 0;
        }

        session->client_info->width = width;
        session->client_info->height = height;
        session->client_info->bpp = bpp;
    }
    else
    {
        return 1;
    }

    /* this will send any lingering orders */
    if (xrdp_orders_reset((struct xrdp_orders *)session->orders) != 0)
    {
        return 1;
    }

    /* shut down the rdp client */
    if (xrdp_rdp_send_deactive((struct xrdp_rdp *)session->rdp) != 0)
    {
        return 1;
    }

    /* this should do the resizing */
    if (xrdp_rdp_send_demand_active((struct xrdp_rdp *)session->rdp) != 0)
    {
        return 1;
    }

    /* process till up and running */
    session->up_and_running = 0;

    if (libxrdp_process_data(session) != 0)
    {
        g_writeln("non handled error from libxrdp_process_data");
    }

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
/* returns error */
/* this function gets the channel name and its flags, index is zero
   based.  either channel_name or channel_flags can be passed in nil if
   they are not needed */
int EXPORT_CC
libxrdp_query_channel(struct xrdp_session *session, int index,
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
        g_writeln("libxrdp_query_channel - No channel initialized");
        return 1 ;
    }

    count = mcs->channel_list->count;

    if (index < 0 || index >= count)
    {
        DEBUG(("libxrdp_query_channel - Channel out of range %d", index));
        return 1;
    }

    channel_item = (struct mcs_channel_item *)
                   list_get_item(mcs->channel_list, index);

    if (channel_item == 0)
    {
        /* this should not happen */
        g_writeln("libxrdp_query_channel - channel item is 0");
        return 1;
    }

    if (channel_name != 0)
    {
        g_strncpy(channel_name, channel_item->name, 8);
        DEBUG(("libxrdp_query_channel - Channel %d name %s", index, channel_name));
    }

    if (channel_flags != 0)
    {
        *channel_flags = channel_item->flags;
    }

    return 0;
}

/*****************************************************************************/
/* returns a zero based index of the channel, -1 if error or it dosen't
   exist */
int EXPORT_CC
libxrdp_get_channel_id(struct xrdp_session *session, char *name)
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
        g_writeln("libxrdp_get_channel_id No channel initialized");
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
        free_stream(s);
        return 1;
    }

    /* here we make a copy of the data */
    out_uint8a(s, data, data_len);
    s_mark_end(s);

    if (xrdp_channel_send(chan, s, channel_id, total_data_len, flags) != 0)
    {
        g_writeln("Debug - data NOT sent to channel");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
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
int DEFAULT_CC
libxrdp_notify_delete(struct xrdp_session *session,
                      int window_id, int notify_id)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_notify_delete(orders, window_id, notify_id);
}

/*****************************************************************************/
int DEFAULT_CC
libxrdp_monitored_desktop(struct xrdp_session *session,
                          struct rail_monitored_desktop_order *mdo,
                          int flags)
{
    struct xrdp_orders *orders;

    orders = (struct xrdp_orders *)(session->orders);
    return xrdp_orders_send_monitored_desktop(orders, mdo, flags);
}
