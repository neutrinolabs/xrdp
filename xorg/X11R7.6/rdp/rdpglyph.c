/*
 Copyright 2012 Jay Sorg
 
 Permission to use, copy, modify, distribute, and sell this software and its
 documentation for any purpose is hereby granted without fee, provided that
 the above copyright notice appear in all copies and that both that
 copyright notice and this permission notice appear in supporting
 documentation.
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 */

/*
 http://msdn.microsoft.com/en-us/library/cc241863(v=prot.20).aspx
 4.6.1 "d" Character
 This topic has not yet been rated - Rate this topic
 The following shows glyph image data (1 bpp format) for character
 "d" extracted from a Cache Glyph (Revision 2) (section 2.2.2.2.1.2.6)
 Secondary Drawing Order.
 
 Glyph width = 5 pixels
 Glyph height = 9 pixels
 Glyph origin = (0, -9), marked with an "X" on the image grid
 Bitmap = { 0x08, 0x08, 0x08, 0x78, 0x88, 0x88, 0x88, 0x88, 0x78 }
 
 http://msdn.microsoft.com/en-us/library/cc241864(v=prot.20).aspx
 4.6.2 "p" Character
 This topic has not yet been rated - Rate this topic
 The following shows glyph image data (1 bpp format) for character
 "p" extracted from a Cache Glyph (Revision 2) (section 2.2.2.2.1.2.6)
 Secondary Drawing Order.
 
 Glyph width = 5 pixels
 Glyph height = 8 pixels
 Glyph origin = (0, -6), marked with an "X" on the image grid
 Bitmap = { 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0, 0x80, 0x80 }
 */

#include "rdp.h"
#include "rdpdraw.h"
#include "rdpglyph.h"

extern DevPrivateKeyRec g_rdpPixmapIndex; /* from rdpmain.c */
extern int g_do_dirty_os; /* in rdpmain.c */
extern int g_do_alpha_glyphs; /* in rdpmain.c */
extern int g_do_glyph_cache; /* in rdpmain.c */
extern int g_doing_font; /* in rdpmain.c */
extern ScreenPtr g_pScreen; /* in rdpmain.c */
extern rdpScreenInfoRec g_rdpScreen; /* in rdpmain.c */


#define LOG_LEVEL 1
#define LLOG(_level, _args) \
do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

struct font_cache
{
    int offset;
    int baseline;
    int width;
    int height;
    int crc;
    int stamp;
};

static struct font_cache g_font_cache[12][256];
static int g_stamp = 0;

/*****************************************************************************/
static void
set_mono_pixel(char* data, int x, int y, int width, int pixel)
{
    int start;
    int shift;
    
    width = (width + 7) / 8;
    start = (y * width) + x / 8;
    shift = x % 8;
    if (pixel != 0)
    {
        data[start] = data[start] | (0x80 >> shift);
    }
    else
    {
        data[start] = data[start] & ~(0x80 >> shift);
    }
}

/*****************************************************************************/
static int
lget_pixel(char* data, int x, int y, int depth, int stride_bytes)
{
    int start;
    int shift;
    
    if (depth == 1)
    {
        start = (y * stride_bytes) + x / 8;
        shift = x % 8;
        return (data[start] & (0x01 << shift)) ? 0xff : 0;
    }
    else if (depth == 8)
    {
        return data[y * stride_bytes + x];
    }
    return 0;
}

/******************************************************************************/
static int
glyph_get_data(ScreenPtr pScreen, GlyphPtr glyph, struct rdp_font_char* rfd)
{
    int i;
    int j;
    int src_xoff;
    int src_yoff;
    int src_stride_bytes;
    int dst_stride_bytes;
    int hh;
    int ww;
    int src_depth;
    unsigned char pixel;
    PicturePtr pPicture;
    pixman_image_t *src;
    uint32_t* pi32;
    char* pi8;
    
    pPicture = GlyphPicture(glyph)[pScreen->myNum];
    if (pPicture == 0)
    {
        return 0;
    }
    src = image_from_pict(pPicture, FALSE, &src_xoff, &src_yoff);
    if (src == 0)
    {
        return 0;
    }
    
    src_stride_bytes = pixman_image_get_stride(src);
    if (g_do_alpha_glyphs)
    {
        dst_stride_bytes = (glyph->info.width + 3) & ~3;
        rfd->bpp = 8;
    }
    else
    {
        dst_stride_bytes = (((glyph->info.width + 7) / 8) + 3) & ~3;
        rfd->bpp = 1;
    }
    src_depth = pixman_image_get_depth(src);
    ww = pixman_image_get_width(src);
    hh = pixman_image_get_height(src);
    if ((ww != glyph->info.width) || (hh != glyph->info.height) ||
        ((src_depth != 1) && (src_depth != 8)))
    {
        LLOGLN(0, ("glyph_get_data: bad glyph"));
        free_pixman_pict(pPicture, src);
        return 0;
    }
    rfd->data_bytes = glyph->info.height * dst_stride_bytes;
    rfd->data = (char*)g_malloc(rfd->data_bytes, 1);
    rfd->offset = -glyph->info.x;
    rfd->baseline = -glyph->info.y;
    rfd->width = glyph->info.width;
    rfd->height = glyph->info.height;
    pi32 = pixman_image_get_data(src);
    pi8 = (char*)pi32;
    for (j = 0; j < rfd->height; j++)
    {
        for (i = 0; i < rfd->width; i++)
        {
            pixel = lget_pixel(pi8, i, j, src_depth, src_stride_bytes);
            if (g_do_alpha_glyphs)
            {
                rfd->data[j * dst_stride_bytes + i] = pixel;
            }
            else
            {
                if (pixel > 0x7f)
                {
                    set_mono_pixel(rfd->data, i, j, rfd->width, 1);
                }
                else
                {
                    set_mono_pixel(rfd->data, i, j, rfd->width, 0);
                }
            }
        }
    }
    free_pixman_pict(pPicture, src);
    return 0;
}

/******************************************************************************/
struct rdp_text*
create_rdp_text(ScreenPtr pScreen, int nlists, GlyphListPtr lists,
                GlyphPtr* glyphs)
{
    struct rdp_text* rv;
    struct rdp_text* rtext;
    struct rdp_text* last_rtext;
    BoxRec box;
    RegionRec reg1;
    int n;
    int lxoff;
    int lyoff;
    int count;
    int lx;
    int ly;
    int font_index;
    int max_height;
    int min_height;
    int force_new;
    GlyphPtr glyph;
    struct rdp_font_char* rfd;
    
    LLOGLN(10, ("create_rdp_text: nlists %d", nlists));
    
    max_height = 0;
    min_height = 0x7fffffff;
    lx = lists->xOff;
    ly = lists->yOff;
    lxoff = 0;
    lyoff = 0;
    force_new = 0;
    
    rtext = (struct rdp_text*)g_malloc(sizeof(struct rdp_text), 1);
    rtext->reg = RegionCreate(NullBox, 0);
    rtext->flags = 3;
    rtext->mixmode = 0;
    rtext->x = lx;
    rtext->y = ly;
    
    rv = rtext;
    last_rtext = rtext;
    
    count = 0;
    while (nlists--)
    {
        LLOGLN(10, ("lists->xOff %d lists->yOff %d", lists->xOff, lists->yOff));
        if (count != 0)
        {
            lx += lists->xOff;
            ly += lists->yOff;
            force_new = 1;
        }
        count++;
        n = lists->len;
        lists++;
        while (n--)
        {
            glyph = *glyphs++;
            /* process glyph here */
            if ((glyph->info.width > 0) && (glyph->info.height > 0))
            {
                if (force_new)
                {
                    LLOGLN(10, ("create_rdp_text: too many chars"));
                    force_new = 0;
                    rtext = (struct rdp_text*)g_malloc(sizeof(struct rdp_text), 1);
                    rtext->reg = RegionCreate(NullBox, 0);
                    rtext->flags = 3;
                    rtext->mixmode = 0;
                    rtext->x = lx;
                    rtext->y = ly;
                    last_rtext->next = rtext;
                    last_rtext = rtext;
                    lxoff = 0;
                    lyoff = 0;
                }
                LLOGLN(10, ("x %d y %d width %d height %d xOff %d yOff %d "
                            "num_chars %d lxoff %d lyoff %d lx %d ly %d",
                            glyph->info.x, glyph->info.y,
                            glyph->info.width, glyph->info.height,
                            glyph->info.xOff, glyph->info.yOff, rtext->num_chars,
                            lxoff, lyoff, lx, ly));
                rfd = (struct rdp_font_char*)g_malloc(sizeof(struct rdp_font_char), 1);
                rtext->chars[rtext->num_chars] = rfd;
                box.x1 = lx - glyph->info.x;
                box.y1 = ly - glyph->info.y;
                box.x2 = box.x1 + glyph->info.width;
                box.y2 = box.y1 + glyph->info.height;
                if (glyph->info.height > max_height)
                {
                    max_height = glyph->info.height;
                }
                if (glyph->info.height < min_height)
                {
                    min_height = glyph->info.height;
                }
                RegionInit(&reg1, &box, 0);
                RegionUnion(rtext->reg, &reg1, rtext->reg);
                RegionUninit(&reg1);
                
                glyph_get_data(pScreen, glyph, rfd);
                
                rfd->incby = lxoff;
                lxoff = glyph->info.xOff;
                lyoff = glyph->info.yOff;
                rtext->num_chars++;
                if (rtext->num_chars > 63)
                {
                    force_new = 1;
                }
            }
            else
            {
                lxoff += glyph->info.xOff;
                lyoff += glyph->info.yOff;
            }
            lx += glyph->info.xOff;
            ly += glyph->info.yOff;
        }
    }
    if (max_height > 10)
    {
        font_index = 8;
    }
    else if (max_height < 7)
    {
        font_index = 6;
    }
    else
    {
        font_index = 7;
    }
    LLOGLN(10, ("create_rdp_text: min_height %d max_height %d font_index %d",
                min_height, max_height, font_index));
    rtext = rv;
    while (rtext != 0)
    {
        rtext->font = font_index;
        rtext = rtext->next;
    }
    return rv;
}

/******************************************************************************/
int
delete_rdp_text(struct rdp_text* rtext)
{
    int index;
    
    if (rtext == 0)
    {
        return 0;
    }
    for (index = 0; index < rtext->num_chars; index++)
    {
        if (rtext->chars[index] != 0)
        {
            g_free(rtext->chars[index]->data);
            g_free(rtext->chars[index]);
        }
    }
    RegionDestroy(rtext->reg);
    delete_rdp_text(rtext->next);
    g_free(rtext);
    return 0;
}

/******************************************************************************/
static int
get_color(PicturePtr pPicture)
{
    int src_xoff;
    int src_yoff;
    int rv;
    uint32_t* pi32;
    pixman_image_t *src;
    
    src = image_from_pict(pPicture, FALSE, &src_xoff, &src_yoff);
    if (src == 0)
    {
        return 0;
    }
    pi32 = pixman_image_get_data(src);
    rv = *pi32;
    LLOGLN(10, ("get_color: 0x%8.8x width %d height %d ", rv,
                pixman_image_get_width(src),
                pixman_image_get_height(src)));
    free_pixman_pict(pPicture, src);
    return rv;
}

/******************************************************************************/
static int
find_or_add_char(int font, struct rdp_font_char* rfd)
{
    int crc;
    int index;
    int char_index;
    int oldest;
    
    crc = get_crc(rfd->data, rfd->data_bytes);
    LLOGLN(10, ("find_or_add_char: crc 0x%8.8x", crc));
    char_index = 0;
    oldest = 0x7fffffff;
    for (index = 0; index < 250; index++)
    {
        if ((g_font_cache[font][index].crc == crc) &&
            (g_font_cache[font][index].width == rfd->width) &&
            (g_font_cache[font][index].height == rfd->height) &&
            (g_font_cache[font][index].offset == rfd->offset) &&
            (g_font_cache[font][index].baseline == rfd->baseline))
        {
            g_stamp++;
            g_font_cache[font][index].stamp = g_stamp;
            LLOGLN(10, ("find_or_add_char: found char at %d %d", font, index));
            return index;
        }
        if (g_font_cache[font][index].stamp < oldest)
        {
            oldest = g_font_cache[font][index].stamp;
            char_index = index;
        }
    }
    g_stamp++;
    g_font_cache[font][char_index].stamp = g_stamp;
    g_font_cache[font][char_index].crc = crc;
    g_font_cache[font][char_index].width = rfd->width;
    g_font_cache[font][char_index].height = rfd->height;
    g_font_cache[font][char_index].offset = rfd->offset;
    g_font_cache[font][char_index].baseline = rfd->baseline;
    LLOGLN(10, ("find_or_add_char: adding char at %d %d", font, char_index));
    if (rfd->bpp == 8)
    {
        rdpup_add_char_alpha(font, char_index, rfd->offset, rfd->baseline,
                             rfd->width, rfd->height,
                             rfd->data, rfd->data_bytes);
    }
    else
    {
        rdpup_add_char(font, char_index, rfd->offset, rfd->baseline,
                       rfd->width, rfd->height,
                       rfd->data, rfd->data_bytes);
    }
    return char_index;
}

/******************************************************************************/
int
rdp_text_chars_to_data(struct rdp_text* rtext)
{
    int index;
    int data_bytes;
    int char_index;
    struct rdp_font_char* rfd;
    
    LLOGLN(10, ("rdp_text_chars_to_data: rtext->num_chars %d", rtext->num_chars));
    data_bytes = 0;
    for (index = 0; index < rtext->num_chars; index++)
    {
        rfd = rtext->chars[index];
        if (rfd == 0)
        {
            LLOGLN(0, ("rdp_text_chars_to_data: error rfd is nil"));
            continue;
        }
        char_index = find_or_add_char(rtext->font, rfd);
        rtext->data[data_bytes] = char_index;
        data_bytes++;
        if (rfd->incby > 127)
        {
            rtext->data[data_bytes] = 0x80;
            data_bytes++;
            rtext->data[data_bytes] = (rfd->incby >> 0) & 0xff;
            data_bytes++;
            rtext->data[data_bytes] = (rfd->incby >> 8) & 0xff;
            data_bytes++;
        }
        else
        {
            rtext->data[data_bytes] = rfd->incby;
            data_bytes++;
        }
    }
    rtext->data_bytes = data_bytes;
    return 0;
}

/******************************************************************************/
/*
 typedef struct _GlyphList {
 INT16    xOff;
 INT16    yOff;
 CARD8    len;
 PictFormatPtr   format;
 } GlyphListRec, *GlyphListPtr;
 */
/* see ghyphstr.h but the follow is not in there
 typedef struct _XGlyphInfo {
 unsigned short  width;
 unsigned short  height;
 short    x;
 short    y;
 short    xOff;
 short    yOff;
 } XGlyphInfo;
 */
static void
rdpGlyphu(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
          PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
          int nlists, GlyphListPtr lists, GlyphPtr* glyphs,
          BoxPtr extents)
{
    BoxRec box;
    RegionRec reg1;
    RegionRec reg2;
    DrawablePtr p;
    int dirty_type;
    int j;
    int num_clips;
    int post_process;
    int reset_surface;
    int got_id;
    int fg_color;
    WindowPtr pDstWnd;
    PixmapPtr pDstPixmap;
    rdpPixmapRec* pDstPriv;
    rdpPixmapRec* pDirtyPriv;
    struct image_data id;
    struct rdp_text* rtext;
    struct rdp_text* trtext;
    
    LLOGLN(10, ("rdpGlyphu: xSrc %d ySrc %d", xSrc, ySrc));
    
    p = pDst->pDrawable;
    
    dirty_type = 0;
    pDirtyPriv = 0;
    post_process = 0;
    reset_surface = 0;
    got_id = 0;
    if (p->type == DRAWABLE_PIXMAP)
    {
        pDstPixmap = (PixmapPtr)p;
        pDstPriv = GETPIXPRIV(pDstPixmap);
        if (XRDP_IS_OS(pDstPriv))
        {
            rdpup_check_dirty(pDstPixmap, pDstPriv);
            post_process = 1;
            if (g_do_dirty_os)
            {
                LLOGLN(10, ("rdpGlyphu: gettig dirty"));
                pDstPriv->is_dirty = 1;
                dirty_type = RDI_IMGLL;
                pDirtyPriv = pDstPriv;
                
            }
            else
            {
                rdpup_switch_os_surface(pDstPriv->rdpindex);
                reset_surface = 1;
                rdpup_get_pixmap_image_rect(pDstPixmap, &id);
                got_id = 1;
                LLOGLN(10, ("rdpGlyphu: offscreen"));
            }
        }
    }
    else
    {
        if (p->type == DRAWABLE_WINDOW)
        {
            pDstWnd = (WindowPtr)p;
            if (pDstWnd->viewable)
            {
                post_process = 1;
                rdpup_get_screen_image_rect(&id);
                got_id = 1;
                LLOGLN(10, ("rdpGlyphu: screen"));
            }
        }
    }
    if (!post_process)
    {
        return;
    }
    
    rtext = create_rdp_text(pDst->pDrawable->pScreen, nlists, lists, glyphs);
    if (rtext == 0)
    {
        LLOGLN(0, ("rdpGlyphu: create_rdp_text failed"));
        return;
    }
    fg_color = get_color(pSrc);
    
    LLOGLN(10, ("rdpGlyphu: pDst->clientClipType %d pCompositeClip %p",
                pDst->clientClipType, pDst->pCompositeClip));
    
    if (pDst->pCompositeClip != 0)
    {
        box.x1 = p->x + extents->x1;
        box.y1 = p->y + extents->y1;
        box.x2 = p->x + extents->x2;
        box.y2 = p->y + extents->y2;
        RegionInit(&reg1, &box, 0);
        RegionInit(&reg2, NullBox, 0);
        RegionCopy(&reg2, pDst->pCompositeClip);
        RegionIntersect(&reg1, &reg1, &reg2);
        if (dirty_type != 0)
        {
            LLOGLN(10, ("1"));
            draw_item_add_text_region(pDirtyPriv, &reg1, fg_color, GXcopy, rtext);
            rtext = 0;
        }
        else if (got_id)
        {
            num_clips = REGION_NUM_RECTS(&reg1);
            if (num_clips > 0)
            {
                LLOGLN(10, ("  num_clips %d", num_clips));
                rdpup_begin_update();
                rdpup_set_fgcolor(fg_color);
                trtext = rtext;
                while (trtext != 0)
                {
                    rdp_text_chars_to_data(trtext);
                    for (j = num_clips - 1; j >= 0; j--)
                    {
                        box = REGION_RECTS(&reg1)[j];
                        LLOGLN(10, ("2"));
                        rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                        LLOGLN(10, ("rdpGlyphu: rdpup_draw_text"));
                        box = RegionExtents(trtext->reg)[0];
                        rdpup_draw_text(trtext->font, trtext->flags, trtext->mixmode,
                                        box.x1 + p->x, box.y1 + p->y,
                                        box.x2 + p->x, box.y2 + p->y,
                                        //box.x1 + p->x, box.y1 + p->y,
                                        //box.x2 + p->x, box.y2 + p->y,
                                        0, 0, 0, 0,
                                        trtext->x + p->x, trtext->y + p->y,
                                        trtext->data, trtext->data_bytes);
                    }
                    trtext = trtext->next;
                }
                rdpup_reset_clip();
                rdpup_end_update();
            }
        }
        RegionUninit(&reg1);
        RegionUninit(&reg2);
    }
    else
    {
        box.x1 = p->x + extents->x1;
        box.y1 = p->y + extents->y1;
        box.x2 = p->x + extents->x2;
        box.y2 = p->y + extents->y2;
        if (dirty_type != 0)
        {
            RegionInit(&reg1, &box, 0);
            LLOGLN(10, ("3"));
            draw_item_add_text_region(pDirtyPriv, &reg1, fg_color, GXcopy, rtext);
            rtext = 0;
            RegionUninit(&reg1);
        }
        else if (got_id)
        {
            rdpup_begin_update();
            LLOGLN(10, ("4"));
            rdpup_set_fgcolor(fg_color);
            trtext = rtext;
            while (trtext != 0)
            {
                LLOGLN(10, ("rdpGlyphu: rdpup_draw_text"));
                rdp_text_chars_to_data(trtext);
                box = RegionExtents(trtext->reg)[0];
                rdpup_draw_text(trtext->font, trtext->flags, trtext->mixmode,
                                box.x1 + p->x, box.y1 + p->y,
                                box.x2 + p->x, box.y2 + p->y,
                                //box.x1 + p->x, box.y1 + p->y,
                                //box.x2 + p->x, box.y2 + p->y,
                                0, 0, 0, 0,
                                trtext->x + p->x, trtext->y + p->y,
                                trtext->data, trtext->data_bytes);
                trtext = trtext->next;
            }
            rdpup_end_update();
        }
    }
    if (reset_surface)
    {
        rdpup_switch_os_surface(-1);
    }
    delete_rdp_text(rtext);
}

/******************************************************************************/
static void
GlyphExtents(int nlist, GlyphListPtr list, GlyphPtr* glyphs, BoxPtr extents)
{
    int x1;
    int x2;
    int y1;
    int y2;
    int n;
    int x;
    int y;
    GlyphPtr glyph;
    
    x = 0;
    y = 0;
    extents->x1 = MAXSHORT;
    extents->x2 = MINSHORT;
    extents->y1 = MAXSHORT;
    extents->y2 = MINSHORT;
    while (nlist--)
    {
        x += list->xOff;
        y += list->yOff;
        n = list->len;
        list++;
        while (n--)
        {
            glyph = *glyphs++;
            x1 = x - glyph->info.x;
            if (x1 < MINSHORT)
            {
                x1 = MINSHORT;
            }
            y1 = y - glyph->info.y;
            if (y1 < MINSHORT)
            {
                y1 = MINSHORT;
            }
            x2 = x1 + glyph->info.width;
            if (x2 > MAXSHORT)
            {
                x2 = MAXSHORT;
            }
            y2 = y1 + glyph->info.height;
            if (y2 > MAXSHORT)
            {
                y2 = MAXSHORT;
            }
            if (x1 < extents->x1)
            {
                extents->x1 = x1;
            }
            if (x2 > extents->x2)
            {
                extents->x2 = x2;
            }
            if (y1 < extents->y1)
            {
                extents->y1 = y1;
            }
            if (y2 > extents->y2)
            {
                extents->y2 = y2;
            }
            x += glyph->info.xOff;
            y += glyph->info.yOff;
        }
    }
}

/******************************************************************************/
static void
rdpGlypht(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
          PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
          int nlists, GlyphListPtr lists, GlyphPtr* glyphs)
{
    BoxRec extents;
    
    GlyphExtents(nlists, lists, glyphs, &extents);
    if ((extents.x2 <= extents.x1) || (extents.y2 <= extents.y1))
    {
        return;
    }
    rdpGlyphu(op, pSrc, pDst, maskFormat, xSrc, ySrc, nlists, lists,
              glyphs, &extents);
}

/******************************************************************************/
/* make sure no glyph is too big */
/* returns boolean */
static int
rdpGlyphCheck(int nlist, GlyphListPtr list, GlyphPtr* glyphs)
{
    int n;
    GlyphPtr glyph;

    while (nlist--)
    {
        n = list->len;
        list++;
        while (n--)
        {
            glyph = *glyphs++;
            if ((glyph->info.width * glyph->info.height) > 8192)
            {
                LLOGLN(10, ("rdpGlyphCheck: too big"));
                return 0;
            }
        }
    }
    return 1;
}

/******************************************************************************/
void
rdpGlyphs(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
          PictFormatPtr maskFormat,
          INT16 xSrc, INT16 ySrc, int nlists, GlyphListPtr lists,
          GlyphPtr* glyphs)
{
    PictureScreenPtr ps;

    LLOGLN(10, ("rdpGlyphs: op %d xSrc %d ySrc %d maskFormat %p",
           op, xSrc, ySrc, maskFormat));

    if (g_do_glyph_cache && rdpGlyphCheck(nlists, lists, glyphs))
    {
        g_doing_font = 2;
        rdpGlypht(op, pSrc, pDst, maskFormat, xSrc, ySrc, nlists, lists, glyphs);
        ps = GetPictureScreen(g_pScreen);
        ps->Glyphs = g_rdpScreen.Glyphs;
        ps->Glyphs(op, pSrc, pDst, maskFormat, xSrc, ySrc,
                   nlists, lists, glyphs);
        ps->Glyphs = rdpGlyphs;
    }
    else
    {
        g_doing_font = 1;
        rdpup_set_hints(1, 1);
        ps = GetPictureScreen(g_pScreen);
        ps->Glyphs = g_rdpScreen.Glyphs;
        ps->Glyphs(op, pSrc, pDst, maskFormat, xSrc, ySrc,
                   nlists, lists, glyphs);
        ps->Glyphs = rdpGlyphs;
        rdpup_set_hints(0, 1);
    }
    g_doing_font = 0;
    LLOGLN(10, ("rdpGlyphs: out"));
}

/******************************************************************************/
int
rdpGlyphInit(void)
{
    memset(&g_font_cache, 0, sizeof(g_font_cache));
    return 0;
}
