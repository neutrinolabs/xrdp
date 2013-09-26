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
 * libxup main file
 */

#include "xup.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { g_write _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { g_writeln _args ; } } while (0)

/******************************************************************************/
/* returns error */
int DEFAULT_CC
lib_recv(struct mod *mod, char *data, int len)
{
    int rcvd;

    if (mod->sck_closed)
    {
        return 1;
    }

    while (len > 0)
    {
        rcvd = g_tcp_recv(mod->sck, data, len, 0);

        if (rcvd == -1)
        {
            if (g_tcp_last_error_would_block(mod->sck))
            {
                if (mod->server_is_term(mod))
                {
                    return 1;
                }

                g_tcp_can_recv(mod->sck, 10);
            }
            else
            {
                return 1;
            }
        }
        else if (rcvd == 0)
        {
            mod->sck_closed = 1;
            return 1;
        }
        else
        {
            data += rcvd;
            len -= rcvd;
        }
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
lib_send(struct mod *mod, char *data, int len)
{
    int sent;

    if (mod->sck_closed)
    {
        return 1;
    }

    while (len > 0)
    {
        sent = g_tcp_send(mod->sck, data, len, 0);

        if (sent == -1)
        {
            if (g_tcp_last_error_would_block(mod->sck))
            {
                if (mod->server_is_term(mod))
                {
                    return 1;
                }

                g_tcp_can_send(mod->sck, 10);
            }
            else
            {
                return 1;
            }
        }
        else if (sent == 0)
        {
            mod->sck_closed = 1;
            return 1;
        }
        else
        {
            data += sent;
            len -= sent;
        }
    }

    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_start(struct mod *mod, int w, int h, int bpp)
{
    LIB_DEBUG(mod, "in lib_mod_start");
    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;
    LIB_DEBUG(mod, "out lib_mod_start");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_connect(struct mod *mod)
{
    int error;
    int len;
    int i;
    int index;
    int use_uds;
    struct stream *s;
    char con_port[256];

    LIB_DEBUG(mod, "in lib_mod_connect");
    /* clear screen */
    mod->server_begin_update(mod);
    mod->server_set_fgcolor(mod, 0);
    mod->server_fill_rect(mod, 0, 0, mod->width, mod->height);
    mod->server_end_update(mod);
    mod->server_msg(mod, "started connecting", 0);

    /* only support 8, 15, 16, and 24 bpp connections from rdp client */
    if (mod->bpp != 8 && mod->bpp != 15 && mod->bpp != 16 && mod->bpp != 24)
    {
        mod->server_msg(mod,
                        "error - only supporting 8, 15, 16, and 24 bpp rdp connections", 0);
        LIB_DEBUG(mod, "out lib_mod_connect error");
        return 1;
    }

    if (g_strcmp(mod->ip, "") == 0)
    {
        mod->server_msg(mod, "error - no ip set", 0);
        LIB_DEBUG(mod, "out lib_mod_connect error");
        return 1;
    }

    make_stream(s);
    g_sprintf(con_port, "%s", mod->port);
    use_uds = 0;

    if (con_port[0] == '/')
    {
        use_uds = 1;
    }

    mod->sck_closed = 0;
    i = 0;

    while (1)
    {
        if (use_uds)
        {
            mod->sck = g_tcp_local_socket();
        }
        else
        {
            mod->sck = g_tcp_socket();
            g_tcp_set_non_blocking(mod->sck);
            g_tcp_set_no_delay(mod->sck);
        }

        mod->server_msg(mod, "connecting...", 0);

        if (use_uds)
        {
            error = g_tcp_local_connect(mod->sck, con_port);
        }
        else
        {
            error = g_tcp_connect(mod->sck, mod->ip, con_port);
        }

        if (error == -1)
        {
            if (g_tcp_last_error_would_block(mod->sck))
            {
                error = 0;
                index = 0;

                while (!g_tcp_can_send(mod->sck, 100))
                {
                    index++;

                    if ((index >= 30) || mod->server_is_term(mod))
                    {
                        mod->server_msg(mod, "connect timeout", 0);
                        error = 1;
                        break;
                    }
                }
            }
            else
            {
                mod->server_msg(mod, "connect error", 0);
            }
        }

        if (error == 0)
        {
            break;
        }

        g_tcp_close(mod->sck);
        mod->sck = 0;
        i++;

        if (i >= 4)
        {
            mod->server_msg(mod, "connection problem, giving up", 0);
            break;
        }

        g_sleep(250);
    }

    if (error == 0)
    {
        /* send version message */
        init_stream(s, 8192);
        s_push_layer(s, iso_hdr, 4);
        out_uint16_le(s, 103);
        out_uint32_le(s, 301);
        out_uint32_le(s, 0);
        out_uint32_le(s, 0);
        out_uint32_le(s, 0);
        out_uint32_le(s, 1);
        s_mark_end(s);
        len = (int)(s->end - s->data);
        s_pop_layer(s, iso_hdr);
        out_uint32_le(s, len);
        lib_send(mod, s->data, len);
    }

    if (error == 0)
    {
        /* send screen size message */
        init_stream(s, 8192);
        s_push_layer(s, iso_hdr, 4);
        out_uint16_le(s, 103);
        out_uint32_le(s, 300);
        out_uint32_le(s, mod->width);
        out_uint32_le(s, mod->height);
        out_uint32_le(s, mod->bpp);
        out_uint32_le(s, 0);
        s_mark_end(s);
        len = (int)(s->end - s->data);
        s_pop_layer(s, iso_hdr);
        out_uint32_le(s, len);
        lib_send(mod, s->data, len);
    }

    if (error == 0)
    {
        /* send invalidate message */
        init_stream(s, 8192);
        s_push_layer(s, iso_hdr, 4);
        out_uint16_le(s, 103);
        out_uint32_le(s, 200);
        /* x and y */
        i = 0;
        out_uint32_le(s, i);
        /* width and height */
        i = ((mod->width & 0xffff) << 16) | mod->height;
        out_uint32_le(s, i);
        out_uint32_le(s, 0);
        out_uint32_le(s, 0);
        s_mark_end(s);
        len = (int)(s->end - s->data);
        s_pop_layer(s, iso_hdr);
        out_uint32_le(s, len);
        lib_send(mod, s->data, len);
    }

    free_stream(s);

    if (error != 0)
    {
        mod->server_msg(mod, "some problem", 0);
        LIB_DEBUG(mod, "out lib_mod_connect error");
        return 1;
    }
    else
    {
        mod->server_msg(mod, "connected ok", 0);
        mod->sck_obj = g_create_wait_obj_from_socket(mod->sck, 0);
    }

    LIB_DEBUG(mod, "out lib_mod_connect");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_event(struct mod *mod, int msg, tbus param1, tbus param2,
              tbus param3, tbus param4)
{
    struct stream *s;
    int len;
    int key;
    int rv;

    LIB_DEBUG(mod, "in lib_mod_event");
    make_stream(s);

    if ((msg >= 15) && (msg <= 16)) /* key events */
    {
        key = param2;

        if (key > 0)
        {
            if (key == 65027) /* altgr */
            {
                if (mod->shift_state)
                {
                    g_writeln("special");
                    /* fix for mstsc sending left control down with altgr */
                    /* control down / up
                    msg param1 param2 param3 param4
                    15  0      65507  29     0
                    16  0      65507  29     49152 */
                    init_stream(s, 8192);
                    s_push_layer(s, iso_hdr, 4);
                    out_uint16_le(s, 103);
                    out_uint32_le(s, 16); /* key up */
                    out_uint32_le(s, 0);
                    out_uint32_le(s, 65507); /* left control */
                    out_uint32_le(s, 29); /* RDP scan code */
                    out_uint32_le(s, 0xc000); /* flags */
                    s_mark_end(s);
                    len = (int)(s->end - s->data);
                    s_pop_layer(s, iso_hdr);
                    out_uint32_le(s, len);
                    lib_send(mod, s->data, len);
                }
            }

            if (key == 65507) /* left control */
            {
                mod->shift_state = msg == 15;
            }
        }
    }

    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 103);
    out_uint32_le(s, msg);
    out_uint32_le(s, param1);
    out_uint32_le(s, param2);
    out_uint32_le(s, param3);
    out_uint32_le(s, param4);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    rv = lib_send(mod, s->data, len);
    free_stream(s);
    LIB_DEBUG(mod, "out lib_mod_event");
    return rv;
}

/******************************************************************************/
/* return error */
static int APP_CC
process_server_window_new_update(struct mod *mod, struct stream *s)
{
    int flags;
    int window_id;
    int title_bytes;
    int index;
    int bytes;
    int rv;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, rwso.owner_window_id);
    in_uint32_le(s, rwso.style);
    in_uint32_le(s, rwso.extended_style);
    in_uint32_le(s, rwso.show_state);
    in_uint16_le(s, title_bytes);

    if (title_bytes > 0)
    {
        rwso.title_info = g_malloc(title_bytes + 1, 0);
        in_uint8a(s, rwso.title_info, title_bytes);
        rwso.title_info[title_bytes] = 0;
    }

    in_uint32_le(s, rwso.client_offset_x);
    in_uint32_le(s, rwso.client_offset_y);
    in_uint32_le(s, rwso.client_area_width);
    in_uint32_le(s, rwso.client_area_height);
    in_uint32_le(s, rwso.rp_content);
    in_uint32_le(s, rwso.root_parent_handle);
    in_uint32_le(s, rwso.window_offset_x);
    in_uint32_le(s, rwso.window_offset_y);
    in_uint32_le(s, rwso.window_client_delta_x);
    in_uint32_le(s, rwso.window_client_delta_y);
    in_uint32_le(s, rwso.window_width);
    in_uint32_le(s, rwso.window_height);
    in_uint16_le(s, rwso.num_window_rects);

    if (rwso.num_window_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_window_rects;
        rwso.window_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_window_rects; index++)
        {
            in_uint16_le(s, rwso.window_rects[index].left);
            in_uint16_le(s, rwso.window_rects[index].top);
            in_uint16_le(s, rwso.window_rects[index].right);
            in_uint16_le(s, rwso.window_rects[index].bottom);
        }
    }

    in_uint32_le(s, rwso.visible_offset_x);
    in_uint32_le(s, rwso.visible_offset_y);
    in_uint16_le(s, rwso.num_visibility_rects);

    if (rwso.num_visibility_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_visibility_rects;
        rwso.visibility_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_visibility_rects; index++)
        {
            in_uint16_le(s, rwso.visibility_rects[index].left);
            in_uint16_le(s, rwso.visibility_rects[index].top);
            in_uint16_le(s, rwso.visibility_rects[index].right);
            in_uint16_le(s, rwso.visibility_rects[index].bottom);
        }
    }

    in_uint32_le(s, flags);
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    g_free(rwso.title_info);
    g_free(rwso.window_rects);
    g_free(rwso.visibility_rects);
    return rv;
}

/******************************************************************************/
/* return error */
static int APP_CC
process_server_window_show(struct mod* mod, struct stream* s)
{
    int window_id;
    int rv;
    int flags;
    struct rail_window_state_order rwso;
    
    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, flags);
    in_uint32_le(s, rwso.show_state);
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int APP_CC
process_server_window_delete(struct mod *mod, struct stream *s)
{
    int window_id;
    int rv;

    in_uint32_le(s, window_id);
    mod->server_window_delete(mod, window_id);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int APP_CC
process_server_set_pointer_ex(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int bpp;
    int Bpp;
    char cur_data[32 * (32 * 4)];
    char cur_mask[32 * (32 / 8)];

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, bpp);
    Bpp = (bpp == 0) ? 3 : (bpp + 7) / 8;
    in_uint8a(s, cur_data, 32 * (32 * Bpp));
    in_uint8a(s, cur_mask, 32 * (32 / 8));
    rv = mod->server_set_cursor_ex(mod, x, y, cur_data, cur_mask, bpp);
    return rv;
}

/******************************************************************************/
/* return error */
static int APP_CC
send_paint_rect_ack(struct mod *mod, int flags, int x, int y, int cx, int cy,
                    int frame_id)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 105);
    out_uint32_le(s, flags);
    out_uint32_le(s, frame_id);
    out_uint32_le(s, x);
    out_uint32_le(s, y);
    out_uint32_le(s, cx);
    out_uint32_le(s, cy);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send(mod, s->data, len);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int APP_CC
lib_mod_process_orders(struct mod *mod, int type, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int srcx;
    int srcy;
    int mskx;
    int msky;
    int dstx;
    int dsty;
    int len_bmpdata;
    int style;
    int x1;
    int y1;
    int x2;
    int y2;
    int bpp;
    int rdpid;
    int hints;
    int mask;
    int width;
    int height;
    int fgcolor;
    int bgcolor;
    int opcode;
    int flags;
    int shmem_id;
    int shmem_offset;
    int frame_id;
    int charactor;
    int font;
    int mixmode;
    int clip_left;
    int clip_top;
    int clip_right;
    int clip_bottom;
    int box_left;
    int box_top;
    int box_right;
    int box_bottom;
    int srcrepeat;
    int srcidx;
    int srcformat;
    int srcwidth;
    int mskflags;
    int mskidx;
    int mskformat;
    int mskwidth;
    int mskrepeat;
    int dstformat;
    int op;
    int transform[10];
    char *bmpdata;
    char cur_data[32 * (32 * 3)];
    char cur_mask[32 * (32 / 8)];

    rv = 0;

    switch (type)
    {
        case 1: /* server_begin_update */
            rv = mod->server_begin_update(mod);
            break;
        case 2: /* server_end_update */
            rv = mod->server_end_update(mod);
            break;
        case 3: /* server_fill_rect */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            rv = mod->server_fill_rect(mod, x, y, cx, cy);
            break;
        case 4: /* server_screen_blt */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            rv = mod->server_screen_blt(mod, x, y, cx, cy, srcx, srcy);
            break;
        case 5: /* server_paint_rect */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint32_le(s, len_bmpdata);
            in_uint8p(s, bmpdata, len_bmpdata);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            rv = mod->server_paint_rect(mod, x, y, cx, cy,
                                        bmpdata, width, height,
                                        srcx, srcy);
            break;
        case 10: /* server_set_clip */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            rv = mod->server_set_clip(mod, x, y, cx, cy);
            break;
        case 11: /* server_reset_clip */
            rv = mod->server_reset_clip(mod);
            break;
        case 12: /* server_set_fgcolor */
            in_uint32_le(s, fgcolor);
            rv = mod->server_set_fgcolor(mod, fgcolor);
            break;
        case 13: /* server_set_bgcolor */
            in_uint32_le(s, bgcolor);
            rv = mod->server_set_bgcolor(mod, bgcolor);
            break;
        case 14:
            in_uint16_le(s, opcode);
            rv = mod->server_set_opcode(mod, opcode);
            break;
        case 17:
            in_uint16_le(s, style);
            in_uint16_le(s, width);
            rv = mod->server_set_pen(mod, style, width);
            break;
        case 18:
            in_sint16_le(s, x1);
            in_sint16_le(s, y1);
            in_sint16_le(s, x2);
            in_sint16_le(s, y2);
            rv = mod->server_draw_line(mod, x1, y1, x2, y2);
            break;
        case 19:
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint8a(s, cur_data, 32 * (32 * 3));
            in_uint8a(s, cur_mask, 32 * (32 / 8));
            rv = mod->server_set_cursor(mod, x, y, cur_data, cur_mask);
            break;
        case 20:
            in_uint32_le(s, rdpid);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            rv = mod->server_create_os_surface(mod, rdpid, width, height);
            break;
        case 21:
            in_uint32_le(s, rdpid);
            rv = mod->server_switch_os_surface(mod, rdpid);
            break;
        case 22:
            in_uint32_le(s, rdpid);
            rv = mod->server_delete_os_surface(mod, rdpid);
            break;
        case 23: /* server_paint_rect_os */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint32_le(s, rdpid);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            rv = mod->server_paint_rect_os(mod, x, y, cx, cy,
                                           rdpid, srcx, srcy);
            break;
        case 24: /* server_set_hints */
            in_uint32_le(s, hints);
            in_uint32_le(s, mask);
            rv = mod->server_set_hints(mod, hints, mask);
            break;
        case 25: /* server_window_new_update */
            rv = process_server_window_new_update(mod, s);
            break;
        case 26: /* server_window_delete */
            rv = process_server_window_delete(mod, s);
            break;
        case 27: /* server_window_new_update - show */
            rv = process_server_window_show(mod, s);
            break;
        case 28: /* server_add_char */
            in_uint16_le(s, font);
            in_uint16_le(s, charactor);
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint16_le(s, len_bmpdata);
            in_uint8p(s, bmpdata, len_bmpdata);
            rv = mod->server_add_char(mod, font, charactor, x, y, cx, cy, bmpdata);
            break;
        case 29: /* server_add_char_alpha */
            in_uint16_le(s, font);
            in_uint16_le(s, charactor);
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint16_le(s, len_bmpdata);
            in_uint8p(s, bmpdata, len_bmpdata);
            rv = mod->server_add_char_alpha(mod, font, charactor, x, y, cx, cy, bmpdata);
            break;
        case 30: /* server_draw_text */
            in_uint16_le(s, font);
            in_uint16_le(s, flags);
            in_uint16_le(s, mixmode);
            in_sint16_le(s, clip_left);
            in_sint16_le(s, clip_top);
            in_sint16_le(s, clip_right);
            in_sint16_le(s, clip_bottom);
            in_sint16_le(s, box_left);
            in_sint16_le(s, box_top);
            in_sint16_le(s, box_right);
            in_sint16_le(s, box_bottom);
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, len_bmpdata);
            in_uint8p(s, bmpdata, len_bmpdata);
            rv = mod->server_draw_text(mod, font, flags, mixmode, clip_left, clip_top,
                                       clip_right, clip_bottom, box_left, box_top,
                                       box_right, box_bottom, x, y, bmpdata, len_bmpdata);
            break;
        case 31: /* server_create_os_surface_bpp */
            in_uint32_le(s, rdpid);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            in_uint8(s, bpp);
            rv = mod->server_create_os_surface_bpp(mod, rdpid, width, height, bpp);
            break;
        case 32: /* server_paint_rect_bpp */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint32_le(s, len_bmpdata);
            in_uint8p(s, bmpdata, len_bmpdata);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            in_uint8(s, bpp);
            rv = mod->server_paint_rect_bpp(mod, x, y, cx, cy,
                                            bmpdata, width, height,
                                            srcx, srcy, bpp);
            break;
        case 33:
            in_uint16_le(s, srcidx);
            in_uint32_le(s, srcformat);
            in_uint16_le(s, srcwidth);
            in_uint8(s, srcrepeat);
            g_memcpy(transform, s->p, 40);
            in_uint8s(s, 40);
            in_uint8(s, mskflags);
            in_uint16_le(s, mskidx);
            in_uint32_le(s, mskformat);
            in_uint16_le(s, mskwidth);
            in_uint8(s, mskrepeat);
            in_uint8(s, op);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            in_sint16_le(s, mskx);
            in_sint16_le(s, msky);
            in_sint16_le(s, dstx);
            in_sint16_le(s, dsty);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            in_uint32_le(s, dstformat);
            rv = mod->server_composite(mod, srcidx, srcformat, srcwidth, srcrepeat,
                                       transform, mskflags, mskidx, mskformat,
                                       mskwidth, mskrepeat, op, srcx, srcy, mskx, msky,
                                       dstx, dsty, width, height, dstformat);
            break;
        case 51: /* server_set_pointer_ex */
            rv = process_server_set_pointer_ex(mod, s);
            break;
        case 60: /* server_paint_rect_shmem */
            in_sint16_le(s, x);
            in_sint16_le(s, y);
            in_uint16_le(s, cx);
            in_uint16_le(s, cy);
            in_uint32_le(s, flags);
            in_uint32_le(s, frame_id);
            in_uint32_le(s, shmem_id);
            in_uint32_le(s, shmem_offset);
            in_uint16_le(s, width);
            in_uint16_le(s, height);
            in_sint16_le(s, srcx);
            in_sint16_le(s, srcy);
            bmpdata = 0;
            if (flags == 0) /* screen */
            {
                if (mod->screen_shmem_id == 0)
                {
                    mod->screen_shmem_id = shmem_id;
                    mod->screen_shmem_pixels = shmat(mod->screen_shmem_id, 0, 0);
                }
                if (mod->screen_shmem_pixels != 0)
                {
                    bmpdata = mod->screen_shmem_pixels + shmem_offset;
                }
            }
            if (bmpdata != 0)
            {
                rv = mod->server_paint_rect(mod, x, y, cx, cy,
                                            bmpdata, width, height,
                                            srcx, srcy);
            }
            else
            {
                rv = 1;
            }
            send_paint_rect_ack(mod, flags, x, y, cx, cy, frame_id);
            break;

        default:
            g_writeln("lib_mod_process_orders: unknown order type %d", type);
            rv = 0;
            break;
    }

    return rv;
}


/******************************************************************************/
/* return error */
static int APP_CC
lib_send_client_info(struct mod *mod)
{
    struct stream *s;
    int len;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 104);
    g_memcpy(s->p, &(mod->client_info), sizeof(mod->client_info));
    s->p += sizeof(mod->client_info);
    s_mark_end(s);
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    lib_send(mod, s->data, len);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_signal(struct mod *mod)
{
    struct stream *s;
    int num_orders;
    int index;
    int rv;
    int len;
    int type;
    char *phold;

    LIB_DEBUG(mod, "in lib_mod_signal");
    make_stream(s);
    init_stream(s, 8192);
    rv = lib_recv(mod, s->data, 8);

    if (rv == 0)
    {
        in_uint16_le(s, type);
        in_uint16_le(s, num_orders);
        in_uint32_le(s, len);

        if (type == 1) /* original order list */
        {
            init_stream(s, len);
            rv = lib_recv(mod, s->data, len);

            if (rv == 0)
            {
                for (index = 0; index < num_orders; index++)
                {
                    in_uint16_le(s, type);
                    rv = lib_mod_process_orders(mod, type, s);

                    if (rv != 0)
                    {
                        break;
                    }
                }
            }
        }
        else if (type == 2) /* caps */
        {
            g_writeln("lib_mod_signal: type 2 len %d", len);
            init_stream(s, len);
            rv = lib_recv(mod, s->data, len);

            if (rv == 0)
            {
                for (index = 0; index < num_orders; index++)
                {
                    phold = s->p;
                    in_uint16_le(s, type);
                    in_uint16_le(s, len);

                    switch (type)
                    {
                        default:
                            g_writeln("lib_mod_signal: unknown cap type %d len %d",
                                      type, len);
                            break;
                    }

                    s->p = phold + len;
                }

                lib_send_client_info(mod);
            }
        }
        else if (type == 3) /* order list with len after type */
        {
            init_stream(s, len);
            rv = lib_recv(mod, s->data, len);

            if (rv == 0)
            {
                for (index = 0; index < num_orders; index++)
                {
                    phold = s->p;
                    in_uint16_le(s, type);
                    in_uint16_le(s, len);
                    rv = lib_mod_process_orders(mod, type, s);

                    if (rv != 0)
                    {
                        break;
                    }

                    s->p = phold + len;
                }
            }
        }
        else
        {
            g_writeln("unknown type %d", type);
        }
    }

    free_stream(s);
    LIB_DEBUG(mod, "out lib_mod_signal");
    return rv;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_end(struct mod *mod)
{
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_set_param(struct mod *mod, char *name, char *value)
{
    if (g_strcasecmp(name, "username") == 0)
    {
        g_strncpy(mod->username, value, 255);
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
        g_strncpy(mod->password, value, 255);
    }
    else if (g_strcasecmp(name, "ip") == 0)
    {
        g_strncpy(mod->ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
        g_strncpy(mod->port, value, 255);
    }
    else if (g_strcasecmp(name, "client_info") == 0)
    {
        g_memcpy(&(mod->client_info), value, sizeof(mod->client_info));
    }

    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_get_wait_objs(struct mod *mod, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    int i;

    i = *rcount;

    if (mod != 0)
    {
        if (mod->sck_obj != 0)
        {
            read_objs[i++] = mod->sck_obj;
        }
    }

    *rcount = i;
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_check_wait_objs(struct mod *mod)
{
    int rv;

    rv = 0;

    if (mod != 0)
    {
        if (mod->sck_obj != 0)
        {
            if (g_is_wait_obj_set(mod->sck_obj))
            {
                rv = lib_mod_signal(mod);
            }
        }
    }

    return rv;
}

/******************************************************************************/
struct mod *EXPORT_CC
mod_init(void)
{
    struct mod *mod;

    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (tbus)mod;
    mod->mod_connect = lib_mod_connect;
    mod->mod_start = lib_mod_start;
    mod->mod_event = lib_mod_event;
    mod->mod_signal = lib_mod_signal;
    mod->mod_end = lib_mod_end;
    mod->mod_set_param = lib_mod_set_param;
    mod->mod_get_wait_objs = lib_mod_get_wait_objs;
    mod->mod_check_wait_objs = lib_mod_check_wait_objs;
    return mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct mod *mod)
{
    if (mod == 0)
    {
        return 0;
    }

    g_delete_wait_obj_from_socket(mod->sck_obj);
    g_tcp_close(mod->sck);
    g_free(mod);
    return 0;
}
