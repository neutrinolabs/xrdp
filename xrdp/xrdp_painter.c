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
 * painter, gc
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"

#if defined(XRDP_PAINTER)
#include <painter.h> /* libpainter */
#endif

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
        g_write("xrdp:xrdp_painter [%10.10u]: ", g_time3()); \
        g_writeln _args ; \
    } \
  } \
  while (0)

#if defined(XRDP_PAINTER)

/*****************************************************************************/
static int
xrdp_painter_add_dirty_rect(struct xrdp_painter *self, int x, int y,
                            int cx, int cy, struct xrdp_rect *clip_rect)
{
    int x2;
    int y2;
    struct xrdp_rect rect;

    if (clip_rect != 0)
    {
        x2 = x + cx;
        y2 = y + cy;
        x = MAX(x, clip_rect->left);
        y = MAX(y, clip_rect->top);
        x2 = MIN(x2, clip_rect->right);
        y2 = MIN(y2, clip_rect->bottom);
        cx = x2 - x;
        cy = y2 - y;
    }
    if (cx < 1 || cy < 1)
    {
        return 0;
    }
    rect.left = x;
    rect.top = y;
    rect.right = x + cx;
    rect.bottom = y + cy;
    xrdp_region_add_rect(self->dirty_region, &rect);
    LLOGLN(10, ("xrdp_painter_add_dirty_rect: x %d y %d cx %d cy %d",
           x, y, cx, cy));
    return 0;
}

/*****************************************************************************/
static int
xrdp_painter_send_dirty(struct xrdp_painter *self)
{
    int cx;
    int cy;
    int bpp;
    int Bpp;
    int index;
    int jndex;
    int error;
    char *ldata;
    char *src;
    char *dst;
    struct xrdp_rect rect;

    LLOGLN(10, ("xrdp_painter_send_dirty:"));

    bpp = self->wm->screen->bpp;
    Bpp = (bpp + 7) / 8;
    if (Bpp == 3)
    {
        Bpp = 4;
    }

    jndex = 0;
    error = xrdp_region_get_rect(self->dirty_region, jndex, &rect);
    while (error == 0)
    {
        cx = rect.right - rect.left;
        cy = rect.bottom - rect.top;
        ldata = (char *)g_malloc(cx * cy * Bpp, 0);
        if (ldata == 0)
        {
            return 1;
        }
        src = self->wm->screen->data;
        src += self->wm->screen->line_size * rect.top;
        src += rect.left * Bpp;
        dst = ldata;
        for (index = 0; index < cy; index++)
        {
            g_memcpy(dst, src, cx * Bpp);
            src += self->wm->screen->line_size;
            dst += cx * Bpp;
        }
        LLOGLN(10, ("xrdp_painter_send_dirty: x %d y %d cx %d cy %d",
               rect.left, rect.top, cx, cy));
        libxrdp_send_bitmap(self->session, cx, cy, bpp,
                            ldata, rect.left, rect.top, cx, cy);
        g_free(ldata);

        jndex++;
        error = xrdp_region_get_rect(self->dirty_region, jndex, &rect);
    }

    xrdp_region_delete(self->dirty_region);
    self->dirty_region = xrdp_region_create(self->wm);

    return 0;
}

#endif

/*****************************************************************************/
struct xrdp_painter *
xrdp_painter_create(struct xrdp_wm *wm, struct xrdp_session *session)
{
    struct xrdp_painter *self;

    LLOGLN(10, ("xrdp_painter_create:"));
    self = (struct xrdp_painter *)g_malloc(sizeof(struct xrdp_painter), 1);
    self->wm = wm;
    self->session = session;
    self->rop = 0xcc; /* copy will use 0xcc */
    self->clip_children = 1;


    if (self->session->client_info->no_orders_supported)
    {
#if defined(XRDP_PAINTER)
        if (painter_create(&(self->painter)) != PT_ERROR_NONE)
        {
            self->painter = 0;
            LLOGLN(0, ("xrdp_painter_create: painter_create failed"));
        }
        else
        {
            LLOGLN(10, ("xrdp_painter_create: painter_create success"));
        }
        self->dirty_region = xrdp_region_create(wm);
#endif
    }

    return self;
}

/*****************************************************************************/
void
xrdp_painter_delete(struct xrdp_painter *self)
{
    LLOGLN(10, ("xrdp_painter_delete:"));
    if (self == 0)
    {
        return;
    }

#if defined(XRDP_PAINTER)
    painter_delete(self->painter);
    xrdp_region_delete(self->dirty_region);
#endif

    g_free(self);
}

/*****************************************************************************/
int
wm_painter_set_target(struct xrdp_painter *self)
{
    int surface_index;
    int index;
    struct list *del_list;

    LLOGLN(10, ("wm_painter_set_target:"));

    if (self->painter != 0)
    {
        return 0;
    }

    if (self->wm->target_surface->type == WND_TYPE_SCREEN)
    {
        if (self->wm->current_surface_index != 0xffff)
        {
            libxrdp_orders_send_switch_os_surface(self->session, 0xffff);
            self->wm->current_surface_index = 0xffff;
        }
    }
    else if (self->wm->target_surface->type == WND_TYPE_OFFSCREEN)
    {
        surface_index = self->wm->target_surface->item_index;

        if (surface_index != self->wm->current_surface_index)
        {
            if (self->wm->target_surface->tab_stop == 0) /* tab_stop is hack */
            {
                del_list = self->wm->cache->xrdp_os_del_list;
                index = list_index_of(del_list, surface_index);
                list_remove_item(del_list, index);
                libxrdp_orders_send_create_os_surface(self->session, surface_index,
                                                      self->wm->target_surface->width,
                                                      self->wm->target_surface->height,
                                                      del_list);
                self->wm->target_surface->tab_stop = 1;
                list_clear(del_list);
            }

            libxrdp_orders_send_switch_os_surface(self->session, surface_index);
            self->wm->current_surface_index = surface_index;
        }
    }
    else
    {
        g_writeln("xrdp_painter_begin_update: bad target_surface");
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_painter_begin_update(struct xrdp_painter *self)
{
    LLOGLN(10, ("xrdp_painter_begin_update:"));
    if (self == 0)
    {
        return 0;
    }

    self->begin_end_level++;

    if (self->painter != 0)
    {
        return 0;
    }

    libxrdp_orders_init(self->session);
    wm_painter_set_target(self);
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_end_update(struct xrdp_painter *self)
{
    LLOGLN(10, ("xrdp_painter_end_update:"));
    if (self == 0)
    {
        return 0;
    }

    self->begin_end_level--;

    if (self->painter != 0)
    {
#if defined(XRDP_PAINTER)
        if (self->begin_end_level == 0)
        {
            xrdp_painter_send_dirty(self);
            return 0;
        }
#endif
    }

    libxrdp_orders_send(self->session);
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_font_needed(struct xrdp_painter *self)
{
    if (self->font == 0)
    {
        self->font = self->wm->default_font;
    }

    return 0;
}

#if 0
/*****************************************************************************/
/* returns boolean, true if there is something to draw */
static int
xrdp_painter_clip_adj(struct xrdp_painter *self, int *x, int *y,
                      int *cx, int *cy)
{
    int dx;
    int dy;

    if (!self->use_clip)
    {
        return 1;
    }

    if (self->clip.left > *x)
    {
        dx = self->clip.left - *x;
    }
    else
    {
        dx = 0;
    }

    if (self->clip.top > *y)
    {
        dy = self->clip.top - *y;
    }
    else
    {
        dy = 0;
    }

    if (*x + *cx > self->clip.right)
    {
        *cx = *cx - ((*x + *cx) - self->clip.right);
    }

    if (*y + *cy > self->clip.bottom)
    {
        *cy = *cy - ((*y + *cy) - self->clip.bottom);
    }

    *cx = *cx - dx;
    *cy = *cy - dy;

    if (*cx <= 0)
    {
        return 0;
    }

    if (*cy <= 0)
    {
        return 0;
    }

    *x = *x + dx;
    *y = *y + dy;
    return 1;
}
#endif

/*****************************************************************************/
int
xrdp_painter_set_clip(struct xrdp_painter *self,
                      int x, int y, int cx, int cy)
{
    self->use_clip = &self->clip;
    self->clip.left = x;
    self->clip.top = y;
    self->clip.right = x + cx;
    self->clip.bottom = y + cy;
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_clr_clip(struct xrdp_painter *self)
{
    self->use_clip = 0;
    return 0;
}

#if 0
/*****************************************************************************/
static int
xrdp_painter_rop(int rop, int src, int dst)
{
    switch (rop & 0x0f)
    {
        case 0x0:
            return 0;
        case 0x1:
            return ~(src | dst);
        case 0x2:
            return (~src) & dst;
        case 0x3:
            return ~src;
        case 0x4:
            return src & (~dst);
        case 0x5:
            return ~(dst);
        case 0x6:
            return src ^ dst;
        case 0x7:
            return ~(src & dst);
        case 0x8:
            return src & dst;
        case 0x9:
            return ~(src) ^ dst;
        case 0xa:
            return dst;
        case 0xb:
            return (~src) | dst;
        case 0xc:
            return src;
        case 0xd:
            return src | (~dst);
        case 0xe:
            return src | dst;
        case 0xf:
            return ~0;
    }

    return dst;
}
#endif

/*****************************************************************************/
int
xrdp_painter_text_width(struct xrdp_painter *self, const char *text)
{
    int index;
    int rv;
    int len;
    struct xrdp_font_char *font_item;
    twchar *wstr;

    LLOGLN(10, ("xrdp_painter_text_width:"));
    xrdp_painter_font_needed(self);

    if (self->font == 0)
    {
        return 0;
    }

    if (text == 0)
    {
        return 0;
    }

    rv = 0;
    len = g_mbstowcs(0, text, 0);
    wstr = (twchar *)g_malloc((len + 2) * sizeof(twchar), 0);
    g_mbstowcs(wstr, text, len + 1);

    for (index = 0; index < len; index++)
    {
        font_item = self->font->font_items + wstr[index];
        rv = rv + font_item->incby;
    }

    g_free(wstr);
    return rv;
}

/*****************************************************************************/
int
xrdp_painter_text_height(struct xrdp_painter *self, const char *text)
{
    int index;
    int rv;
    int len;
    struct xrdp_font_char *font_item;
    twchar *wstr;

    LLOGLN(10, ("xrdp_painter_text_height:"));
    xrdp_painter_font_needed(self);

    if (self->font == 0)
    {
        return 0;
    }

    if (text == 0)
    {
        return 0;
    }

    rv = 0;
    len = g_mbstowcs(0, text, 0);
    wstr = (twchar *)g_malloc((len + 2) * sizeof(twchar), 0);
    g_mbstowcs(wstr, text, len + 1);

    for (index = 0; index < len; index++)
    {
        font_item = self->font->font_items + wstr[index];
        rv = MAX(rv, font_item->height);
    }

    g_free(wstr);
    return rv;
}

/*****************************************************************************/
static int
xrdp_painter_setup_brush(struct xrdp_painter *self,
                         struct xrdp_brush *out_brush,
                         struct xrdp_brush *in_brush)
{
    int cache_id;

    LLOGLN(10, ("xrdp_painter_setup_brush:"));

    if (self->painter != 0)
    {
        return 0;
    }

    g_memcpy(out_brush, in_brush, sizeof(struct xrdp_brush));

    if (in_brush->style == 3)
    {
        if (self->session->client_info->brush_cache_code == 1)
        {
            cache_id = xrdp_cache_add_brush(self->wm->cache, in_brush->pattern);
            g_memset(out_brush->pattern, 0, 8);
            out_brush->pattern[0] = cache_id;
            out_brush->style = 0x81;
        }
    }

    return 0;
}

#if defined(XRDP_PAINTER)

/*****************************************************************************/
static int
get_pt_format(struct xrdp_painter *self)
{
    switch (self->wm->screen->bpp)
    {
        case 8:
            return PT_FORMAT_r3g3b2;
        case 15:
            return PT_FORMAT_a1r5g5b5;
        case 16:
            return PT_FORMAT_r5g6b5;
    }
    return PT_FORMAT_a8r8g8b8;
}

/*****************************************************************************/
static int
get_rgb_from_rdp_color(struct xrdp_painter *self, int rdp_color)
{
    if (self->wm->screen->bpp < 24)
    {
        return rdp_color;
    }
    /* well, this is really BGR2RGB */
    return XR_RGB2BGR(rdp_color);
}

#endif

/*****************************************************************************/
/* fill in an area of the screen with one color */
int
xrdp_painter_fill_rect(struct xrdp_painter *self,
                       struct xrdp_bitmap *dst,
                       int x, int y, int cx, int cy)
{
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_rect rect;
    struct xrdp_region *region;
    struct xrdp_brush brush;
    int k;
    int dx;
    int dy;
    int rop;

    LLOGLN(10, ("xrdp_painter_fill_rect:"));

    if (self == 0)
    {
        return 0;
    }

    dx = 0;
    dy = 0;

    if (self->painter != 0)
    {
#if defined(XRDP_PAINTER)
        struct painter_bitmap dst_pb;
        struct xrdp_bitmap *ldst;
        struct painter_bitmap pat;

        LLOGLN(10, ("xrdp_painter_fill_rect: dst->type %d", dst->type));
        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            LLOGLN(10, ("xrdp_painter_fill_rect: using painter"));

            ldst = self->wm->screen;

            g_memset(&dst_pb, 0, sizeof(dst_pb));
            dst_pb.format = get_pt_format(self);
            dst_pb.width = ldst->width;
            dst_pb.stride_bytes = ldst->line_size;
            dst_pb.height = ldst->height;
            dst_pb.data = ldst->data;

            LLOGLN(10, ("xrdp_painter_fill_rect: ldst->width %d ldst->height %d "
                       "dst->data %p self->fg_color %d",
                       ldst->width, ldst->height, ldst->data, self->fg_color));

            xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
            region = xrdp_region_create(self->wm);
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy, region,
                                   self->clip_children);
            x += dx;
            y += dy;

            rop = self->rop;
            switch (self->rop)
            {
                case 0x5a:
                    rop = PT_ROP_DSx;
                    break;
                case 0xf0:
                    rop = PT_ROP_S;
                    break;
                case 0xfb:
                    rop = PT_ROP_D;
                    break;
                case 0xc0:
                    rop = PT_ROP_DSa;
                    break;
            }
            painter_set_rop(self->painter, rop);

            if (self->mix_mode == 0)
            {
                painter_set_pattern_mode(self->painter, PT_PATTERN_MODE_OPAQUE);
                painter_set_fgcolor(self->painter, get_rgb_from_rdp_color(self, self->fg_color));
                k = 0;
                while (xrdp_region_get_rect(region, k, &rect) == 0)
                {
                    if (rect_intersect(&rect, &clip_rect, &draw_rect))
                    {
                        painter_set_clip(self->painter,
                                         draw_rect.left, draw_rect.top,
                                         draw_rect.right - draw_rect.left,
                                         draw_rect.bottom - draw_rect.top);
                        painter_fill_rect(self->painter, &dst_pb, x, y, cx, cy);
                        xrdp_painter_add_dirty_rect(self, x, y, cx, cy, &draw_rect);
                    }
                    k++;
                }
            }
            else
            {
                painter_set_pattern_mode(self->painter, PT_PATTERN_MODE_OPAQUE);
                painter_set_fgcolor(self->painter, get_rgb_from_rdp_color(self, self->fg_color));
                painter_set_bgcolor(self->painter, get_rgb_from_rdp_color(self, self->bg_color));
                painter_set_pattern_origin(self->painter, self->brush.x_origin, self->brush.y_origin);
                g_memset(&pat, 0, sizeof(pat));
                pat.format = PT_FORMAT_c1;
                pat.width = 8;
                pat.stride_bytes = 1;
                pat.height = 8;
                pat.data = self->brush.pattern;
                k = 0;
                while (xrdp_region_get_rect(region, k, &rect) == 0)
                {
                    if (rect_intersect(&rect, &clip_rect, &draw_rect))
                    {
                        painter_set_clip(self->painter,
                                         draw_rect.left, draw_rect.top,
                                         draw_rect.right - draw_rect.left,
                                         draw_rect.bottom - draw_rect.top);
                        painter_fill_pattern(self->painter, &dst_pb, &pat,
                                             x, y, x, y, cx, cy);
                        xrdp_painter_add_dirty_rect(self, x, y, cx, cy, &draw_rect);
                    }
                    k++;
                }
            }
            painter_clear_clip(self->painter);
            xrdp_region_delete(region);
        }
        return 0;
#endif
    }

    /* todo data */

    if (dst->type == WND_TYPE_BITMAP) /* 0 */
    {
        return 0;
    }

    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);

    if (dst->type != WND_TYPE_OFFSCREEN)
    {
        xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy, region,
                               self->clip_children);
    }
    else
    {
        xrdp_region_add_rect(region, &clip_rect);
    }

    x += dx;
    y += dy;

    if (self->mix_mode == 0 && self->rop == 0xcc)
    {
        k = 0;

        while (xrdp_region_get_rect(region, k, &rect) == 0)
        {
            if (rect_intersect(&rect, &clip_rect, &draw_rect))
            {
                libxrdp_orders_rect(self->session, x, y, cx, cy,
                                    self->fg_color, &draw_rect);
            }

            k++;
        }
    }
    else if (self->mix_mode == 0 &&
             ((self->rop & 0xf) == 0x0 || /* black */
              (self->rop & 0xf) == 0xf || /* white */
              (self->rop & 0xf) == 0x5))  /* DSTINVERT */
    {
        k = 0;

        while (xrdp_region_get_rect(region, k, &rect) == 0)
        {
            if (rect_intersect(&rect, &clip_rect, &draw_rect))
            {
                libxrdp_orders_dest_blt(self->session, x, y, cx, cy,
                                        self->rop, &draw_rect);
            }

            k++;
        }
    }
    else
    {
        k = 0;
        rop = self->rop;

        /* if opcode is in the form 0x00, 0x11, 0x22, ... convert it */
        if (((rop & 0xf0) >> 4) == (rop & 0xf))
        {
            switch (rop)
            {
                case 0x66: /* xor */
                    rop = 0x5a;
                    break;
                case 0xaa: /* noop */
                    rop = 0xfb;
                    break;
                case 0xcc: /* copy */
                    rop = 0xf0;
                    break;
                case 0x88: /* and */
                    rop = 0xc0;
                    break;
            }
        }

        xrdp_painter_setup_brush(self, &brush, &self->brush);

        while (xrdp_region_get_rect(region, k, &rect) == 0)
        {
            if (rect_intersect(&rect, &clip_rect, &draw_rect))
            {
                libxrdp_orders_pat_blt(self->session, x, y, cx, cy,
                                       rop, self->bg_color, self->fg_color,
                                       &brush, &draw_rect);
            }

            k++;
        }
    }

    xrdp_region_delete(region);
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_draw_text(struct xrdp_painter *self,
                       struct xrdp_bitmap *dst,
                       int x, int y, const char *text)
{
    int i;
    int f;
    int c;
    int k;
    int x1;
    int y1;
    int flags;
    int len;
    int index;
    int total_width;
    int total_height;
    int dx;
    int dy;
    char *data;
    struct xrdp_region *region;
    struct xrdp_rect rect;
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_font *font;
    struct xrdp_font_char *font_item;
    twchar *wstr;

    LLOGLN(10, ("xrdp_painter_draw_text:"));

    if (self == 0)
    {
        return 0;
    }


    len = g_mbstowcs(0, text, 0);

    if (len < 1)
    {
        return 0;
    }

    /* todo data */

    if (dst->type == 0)
    {
        return 0;
    }

    xrdp_painter_font_needed(self);

    if (self->font == 0)
    {
        return 0;
    }

    if (self->painter != 0)
    {
#if defined(XRDP_PAINTER)
        struct painter_bitmap pat;
        struct painter_bitmap dst_pb;
        struct xrdp_bitmap *ldst;

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            ldst = self->wm->screen;
            /* convert to wide char */
            wstr = (twchar *)g_malloc((len + 2) * sizeof(twchar), 0);
            g_mbstowcs(wstr, text, len + 1);
            font = self->font;
            total_width = 0;
            total_height = 0;
            for (index = 0; index < len; index++)
            {
                font_item = font->font_items + wstr[index];
                k = font_item->incby;
                total_width += k;
                total_height = MAX(total_height, font_item->height);
            }
            xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
            region = xrdp_region_create(self->wm);
            xrdp_wm_get_vis_region(self->wm, dst, x, y,
                                   total_width, total_height,
                                   region, self->clip_children);
            x += dx;
            y += dy;
            g_memset(&dst_pb, 0, sizeof(dst_pb));
            dst_pb.format = get_pt_format(self);
            dst_pb.width = ldst->width;
            dst_pb.stride_bytes = ldst->line_size;
            dst_pb.height = ldst->height;
            dst_pb.data = ldst->data;
            painter_set_rop(self->painter, PT_ROP_S);
            painter_set_pattern_origin(self->painter, 0, 0);
            painter_set_pattern_mode(self->painter, PT_PATTERN_MODE_NORMAL);
            painter_set_fgcolor(self->painter,
                                get_rgb_from_rdp_color(self, self->fg_color));
            k = 0;
            while (xrdp_region_get_rect(region, k, &rect) == 0)
            {
                if (rect_intersect(&rect, &clip_rect, &draw_rect))
                {
                    painter_set_clip(self->painter,
                                     draw_rect.left, draw_rect.top,
                                     draw_rect.right - draw_rect.left,
                                     draw_rect.bottom - draw_rect.top);
                    for (index = 0; index < len; index++)
                    {
                        font_item = font->font_items + wstr[index];
                        g_memset(&pat, 0, sizeof(pat));
                        pat.format = PT_FORMAT_c1;
                        pat.width = font_item->width;
                        pat.stride_bytes = (font_item->width + 7) / 8;
                        pat.height = font_item->height;
                        pat.data = font_item->data;
                        x1 = x + font_item->offset;
                        y1 = y + (font_item->height + font_item->baseline);
                        painter_fill_pattern(self->painter, &dst_pb, &pat,
                                             0, 0, x1, y1,
                                             font_item->width,
                                             font_item->height);
                        xrdp_painter_add_dirty_rect(self, x, y,
                                                    font_item->width,
                                                    font_item->height,
                                                    &draw_rect);
                        x += font_item->incby;
                    }
                }
                k++;
            }
            painter_clear_clip(self->painter);
            xrdp_region_delete(region);
            g_free(wstr);
        }
        return 0;
#endif
    }

    /* convert to wide char */
    wstr = (twchar *)g_malloc((len + 2) * sizeof(twchar), 0);
    g_mbstowcs(wstr, text, len + 1);
    font = self->font;
    f = 0;
    k = 0;
    total_width = 0;
    total_height = 0;
    data = (char *)g_malloc(len * 4, 1);

    for (index = 0; index < len; index++)
    {
        font_item = font->font_items + wstr[index];
        i = xrdp_cache_add_char(self->wm->cache, font_item);
        f = HIWORD(i);
        c = LOWORD(i);
        data[index * 2] = c;
        data[index * 2 + 1] = k;
        k = font_item->incby;
        total_width += k;
        total_height = MAX(total_height, font_item->height);
    }

    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);

    if (dst->type != WND_TYPE_OFFSCREEN)
    {
        xrdp_wm_get_vis_region(self->wm, dst, x, y, total_width, total_height,
                               region, self->clip_children);
    }
    else
    {
        xrdp_region_add_rect(region, &clip_rect);
    }

    x += dx;
    y += dy;
    k = 0;

    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
        if (rect_intersect(&rect, &clip_rect, &draw_rect))
        {
            x1 = x;
            y1 = y + total_height;
            flags = 0x03; /* 0x03 0x73; TEXT2_IMPLICIT_X and something else */
            libxrdp_orders_text(self->session, f, flags, 0,
                                self->fg_color, 0,
                                x - 1, y - 1, x + total_width, y + total_height,
                                0, 0, 0, 0,
                                x1, y1, data, len * 2, &draw_rect);
        }

        k++;
    }

    xrdp_region_delete(region);
    g_free(data);
    g_free(wstr);
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_draw_text2(struct xrdp_painter *self,
                        struct xrdp_bitmap *dst,
                        int font, int flags, int mixmode,
                        int clip_left, int clip_top,
                        int clip_right, int clip_bottom,
                        int box_left, int box_top,
                        int box_right, int box_bottom,
                        int x, int y, char *data, int data_len)
{
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_rect rect;
    struct xrdp_region *region;
    int k;
    int dx;
    int dy;

    LLOGLN(0, ("xrdp_painter_draw_text2:"));

    if (self == 0)
    {
        return 0;
    }

    if (self->painter != 0)
    {
        return 0;
    }

    /* todo data */

    if (dst->type == WND_TYPE_BITMAP)
    {
        return 0;
    }

    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);

    if (dst->type != WND_TYPE_OFFSCREEN)
    {
        if (box_right - box_left > 1)
        {
            xrdp_wm_get_vis_region(self->wm, dst, box_left, box_top,
                                   box_right - box_left, box_bottom - box_top,
                                   region, self->clip_children);
        }
        else
        {
            xrdp_wm_get_vis_region(self->wm, dst, clip_left, clip_top,
                                   clip_right - clip_left, clip_bottom - clip_top,
                                   region, self->clip_children);
        }
    }
    else
    {
        xrdp_region_add_rect(region, &clip_rect);
    }

    clip_left += dx;
    clip_top += dy;
    clip_right += dx;
    clip_bottom += dy;
    box_left += dx;
    box_top += dy;
    box_right += dx;
    box_bottom += dy;
    x += dx;
    y += dy;
    k = 0;

    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
        if (rect_intersect(&rect, &clip_rect, &draw_rect))
        {
            libxrdp_orders_text(self->session, font, flags, mixmode,
                                self->fg_color, self->bg_color,
                                clip_left, clip_top, clip_right, clip_bottom,
                                box_left, box_top, box_right, box_bottom,
                                x, y, data, data_len, &draw_rect);
        }

        k++;
    }

    xrdp_region_delete(region);
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_copy(struct xrdp_painter *self,
                  struct xrdp_bitmap *src,
                  struct xrdp_bitmap *dst,
                  int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_rect rect1;
    struct xrdp_rect rect2;
    struct xrdp_region *region;
    struct xrdp_bitmap *b;
    int i;
    int j;
    int k;
    int dx;
    int dy;
    int palette_id;
    int bitmap_id;
    int cache_id;
    int cache_idx;
    int dstx;
    int dsty;
    int w;
    int h;
    int index;
    struct list *del_list;

    LLOGLN(10, ("xrdp_painter_copy:"));

    if (self == 0 || src == 0 || dst == 0)
    {
        return 0;
    }

    if (self->painter != 0)
    {
#if defined(XRDP_PAINTER)
        struct painter_bitmap src_pb;
        struct painter_bitmap dst_pb;
        struct xrdp_bitmap *ldst;

        LLOGLN(10, ("xrdp_painter_copy: src->type %d dst->type %d", src->type, dst->type));
        LLOGLN(10, ("xrdp_painter_copy: self->rop 0x%2.2x", self->rop));

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            LLOGLN(10, ("xrdp_painter_copy: using painter"));
            ldst = self->wm->screen;

            g_memset(&dst_pb, 0, sizeof(dst_pb));
            dst_pb.format = get_pt_format(self);
            dst_pb.width = ldst->width;
            dst_pb.stride_bytes = ldst->line_size;
            dst_pb.height = ldst->height;
            dst_pb.data = ldst->data;

            g_memset(&src_pb, 0, sizeof(src_pb));
            src_pb.format = get_pt_format(self);
            src_pb.width = src->width;
            src_pb.stride_bytes = src->line_size;
            src_pb.height = src->height;
            src_pb.data = src->data;

            xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
            region = xrdp_region_create(self->wm);
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy, region,
                                   self->clip_children);
            x += dx;
            y += dy;
            k = 0;

            painter_set_rop(self->painter, self->rop);
            while (xrdp_region_get_rect(region, k, &rect1) == 0)
            {
                if (rect_intersect(&rect1, &clip_rect, &draw_rect))
                {
                    painter_set_clip(self->painter,
                                     draw_rect.left, draw_rect.top,
                                     draw_rect.right - draw_rect.left,
                                     draw_rect.bottom - draw_rect.top);
                    LLOGLN(10, ("  x %d y %d cx %d cy %d srcx %d srcy %d",
                           x, y, cx, cy, srcx, srcy));
                    painter_copy(self->painter, &dst_pb, x, y, cx, cy,
                                 &src_pb, srcx, srcy);
                    xrdp_painter_add_dirty_rect(self, x, y, cx, cy,
                                                &draw_rect);
                }
                k++;
            }
            painter_clear_clip(self->painter);
            xrdp_region_delete(region);
        }

        return 0;
#endif
    }

    /* todo data */

    if (dst->type == WND_TYPE_BITMAP)
    {
        return 0;
    }

    if (src->type == WND_TYPE_SCREEN)
    {
        xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
        region = xrdp_region_create(self->wm);

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                                   region, self->clip_children);
        }
        else
        {
            xrdp_region_add_rect(region, &clip_rect);
        }

        x += dx;
        y += dy;
        srcx += dx;
        srcy += dy;
        k = 0;

        while (xrdp_region_get_rect(region, k, &rect1) == 0)
        {
            if (rect_intersect(&rect1, &clip_rect, &draw_rect))
            {
                libxrdp_orders_screen_blt(self->session, x, y, cx, cy,
                                          srcx, srcy, self->rop, &draw_rect);
            }

            k++;
        }

        xrdp_region_delete(region);
    }
    else if (src->type == WND_TYPE_OFFSCREEN)
    {
        //g_writeln("xrdp_painter_copy: todo");

        xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
        region = xrdp_region_create(self->wm);

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            //g_writeln("off screen to screen");
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                                   region, self->clip_children);
        }
        else
        {
            //g_writeln("off screen to off screen");
            xrdp_region_add_rect(region, &clip_rect);
        }

        x += dx;
        y += dy;

        palette_id = 0;
        cache_id = 255; // todo
        cache_idx = src->item_index; // todo

        if (src->tab_stop == 0)
        {
            g_writeln("xrdp_painter_copy: warning src not created");
            del_list = self->wm->cache->xrdp_os_del_list;
            index = list_index_of(del_list, cache_idx);
            list_remove_item(del_list, index);
            libxrdp_orders_send_create_os_surface(self->session,
                                                  cache_idx,
                                                  src->width,
                                                  src->height,
                                                  del_list);
            src->tab_stop = 1;
            list_clear(del_list);
        }


        k = 0;

        while (xrdp_region_get_rect(region, k, &rect1) == 0)
        {
            if (rect_intersect(&rect1, &clip_rect, &rect2))
            {
                MAKERECT(rect1, x, y, cx, cy);

                if (rect_intersect(&rect2, &rect1, &draw_rect))
                {
                    libxrdp_orders_mem_blt(self->session, cache_id, palette_id,
                                           x, y, cx, cy, self->rop, srcx, srcy,
                                           cache_idx, &draw_rect);
                }
            }

            k++;
        }

        xrdp_region_delete(region);
    }
    else if (src->data != 0)
        /* todo, the non bitmap cache part is gone, it should be put back */
    {
        xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
        region = xrdp_region_create(self->wm);

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                                   region, self->clip_children);
        }
        else
        {
            xrdp_region_add_rect(region, &clip_rect);
        }

        x += dx;
        y += dy;
        palette_id = 0;
        j = srcy;

        while (j < (srcy + cy))
        {
            i = srcx;

            while (i < (srcx + cx))
            {
                w = MIN(64, ((srcx + cx) - i));
                h = MIN(64, ((srcy + cy) - j));
                b = xrdp_bitmap_create(w, h, src->bpp, 0, self->wm);
#if 1
                xrdp_bitmap_copy_box_with_crc(src, b, i, j, w, h);
#else
                xrdp_bitmap_copy_box(src, b, i, j, w, h);
                xrdp_bitmap_hash_crc(b);
#endif
                bitmap_id = xrdp_cache_add_bitmap(self->wm->cache, b, self->wm->hints);
                cache_id = HIWORD(bitmap_id);
                cache_idx = LOWORD(bitmap_id);
                dstx = (x + i) - srcx;
                dsty = (y + j) - srcy;
                k = 0;

                while (xrdp_region_get_rect(region, k, &rect1) == 0)
                {
                    if (rect_intersect(&rect1, &clip_rect, &rect2))
                    {
                        MAKERECT(rect1, dstx, dsty, w, h);

                        if (rect_intersect(&rect2, &rect1, &draw_rect))
                        {
                            libxrdp_orders_mem_blt(self->session, cache_id, palette_id,
                                                   dstx, dsty, w, h, self->rop, 0, 0,
                                                   cache_idx, &draw_rect);
                        }
                    }

                    k++;
                }

                i += 64;
            }

            j += 64;
        }

        xrdp_region_delete(region);
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_painter_composite(struct xrdp_painter* self,
                       struct xrdp_bitmap* src,
                       int srcformat,
                       int srcwidth,
                       int srcrepeat,
                       struct xrdp_bitmap* dst,
                       int* srctransform,
                       int mskflags,
                       struct xrdp_bitmap* msk,
                       int mskformat, int mskwidth, int mskrepeat, int op,
                       int srcx, int srcy, int mskx, int msky,
                       int dstx, int dsty, int width, int height, int dstformat)
{
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_rect rect1;
    struct xrdp_rect rect2;
    struct xrdp_region* region;
    int k;
    int dx;
    int dy;
    int cache_srcidx;
    int cache_mskidx;

    LLOGLN(0, ("xrdp_painter_composite:"));

    if (self == 0 || src == 0 || dst == 0)
    {
        return 0;
    }

    if (self->painter != 0)
    {
        return 0;
    }

    /* todo data */

    if (dst->type == WND_TYPE_BITMAP)
    {
        return 0;
    }

    if (src->type == WND_TYPE_OFFSCREEN)
    {
        xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
        region = xrdp_region_create(self->wm);
        xrdp_region_add_rect(region, &clip_rect);
        dstx += dx;
        dsty += dy;

        cache_srcidx = src->item_index;
        cache_mskidx = -1;
        if (mskflags & 1)
        {
            if (msk != 0)
            {
                cache_mskidx = msk->item_index; // todo
            }
        }

        k = 0;
        while (xrdp_region_get_rect(region, k, &rect1) == 0)
        {
            if (rect_intersect(&rect1, &clip_rect, &rect2))
            {
                MAKERECT(rect1, dstx, dsty, width, height);
                if (rect_intersect(&rect2, &rect1, &draw_rect))
                {
                    libxrdp_orders_composite_blt(self->session, cache_srcidx, srcformat, srcwidth,
                                                 srcrepeat, srctransform, mskflags, cache_mskidx,
                                                 mskformat, mskwidth, mskrepeat, op, srcx, srcy,
                                                 mskx, msky, dstx, dsty, width, height, dstformat,
                                                 &draw_rect);
                }
            }
            k++;
        }
        xrdp_region_delete(region);
    }
    return 0;
}

/*****************************************************************************/
int
xrdp_painter_line(struct xrdp_painter *self,
                  struct xrdp_bitmap *dst,
                  int x1, int y1, int x2, int y2)
{
    struct xrdp_rect clip_rect;
    struct xrdp_rect draw_rect;
    struct xrdp_rect rect;
    struct xrdp_region *region;
    int k;
    int dx;
    int dy;
    int rop;

    LLOGLN(10, ("xrdp_painter_line:"));
    if (self == 0)
    {
        return 0;
    }
    if (self->painter != 0)
    {
#if defined(XRDP_PAINTER)
        int x;
        int y;
        int cx;
        int cy;
        struct painter_bitmap dst_pb;
        struct xrdp_bitmap *ldst;

        LLOGLN(10, ("xrdp_painter_line: dst->type %d", dst->type));
        LLOGLN(10, ("xrdp_painter_line: self->rop 0x%2.2x", self->rop));

        if (dst->type != WND_TYPE_OFFSCREEN)
        {
            LLOGLN(10, ("xrdp_painter_line: using painter"));
            ldst = self->wm->screen;

            g_memset(&dst_pb, 0, sizeof(dst_pb));
            dst_pb.format = get_pt_format(self);
            dst_pb.width = ldst->width;
            dst_pb.stride_bytes = ldst->line_size;
            dst_pb.height = ldst->height;
            dst_pb.data = ldst->data;

            xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
            region = xrdp_region_create(self->wm);
            x = MIN(x1, x2);
            y = MIN(y1, y2);
            cx = g_abs(x1 - x2) + 1;
            cy = g_abs(y1 - y2) + 1;
            xrdp_wm_get_vis_region(self->wm, dst, x, y, cx, cy,
                                   region, self->clip_children);
            x1 += dx;
            y1 += dy;
            x2 += dx;
            y2 += dy;
            k = 0;
            rop = self->rop;

            painter_set_rop(self->painter, rop);
            painter_set_fgcolor(self->painter, self->pen.color);
            while (xrdp_region_get_rect(region, k, &rect) == 0)
            {
                if (rect_intersect(&rect, &clip_rect, &draw_rect))
                {
                    painter_set_clip(self->painter,
                                     draw_rect.left, draw_rect.top,
                                     draw_rect.right - draw_rect.left,
                                     draw_rect.bottom - draw_rect.top);
                    painter_line(self->painter, &dst_pb, x1, y1, x2, y2,
                                 self->pen.width, 0);
                    xrdp_painter_add_dirty_rect(self, x, y, cx, cy,
                                                &draw_rect);
                }
                k++;
            }
            painter_clear_clip(self->painter);
            xrdp_region_delete(region);
        }

        return 0;
#endif
    }

    /* todo data */

    if (dst->type == WND_TYPE_BITMAP)
    {
        return 0;
    }

    xrdp_bitmap_get_screen_clip(dst, self, &clip_rect, &dx, &dy);
    region = xrdp_region_create(self->wm);

    if (dst->type != WND_TYPE_OFFSCREEN)
    {
        xrdp_wm_get_vis_region(self->wm, dst, MIN(x1, x2), MIN(y1, y2),
                               g_abs(x1 - x2) + 1, g_abs(y1 - y2) + 1,
                               region, self->clip_children);
    }
    else
    {
        xrdp_region_add_rect(region, &clip_rect);
    }

    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
    k = 0;
    rop = self->rop;

    if (rop < 0x01 || rop > 0x10)
    {
        rop = (rop & 0xf) + 1;
    }

    while (xrdp_region_get_rect(region, k, &rect) == 0)
    {
        if (rect_intersect(&rect, &clip_rect, &draw_rect))
        {
            libxrdp_orders_line(self->session, 1, x1, y1, x2, y2,
                                rop, self->bg_color,
                                &self->pen, &draw_rect);
        }

        k++;
    }

    xrdp_region_delete(region);
    return 0;
}
