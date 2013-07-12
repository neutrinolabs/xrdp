/*
 Copyright 2012-2013 Jay Sorg
 
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

#include "rdp.h"
#include "rdpdraw.h"

#define LDEBUG 0

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
  do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
  do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpGCIndex; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpWindowIndex; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpPixmapIndex; /* from rdpmain.c */
extern int g_Bpp; /* from rdpmain.c */
extern ScreenPtr g_pScreen; /* from rdpmain.c */
extern Bool g_wrapPixmap; /* from rdpmain.c */
extern int g_can_do_pix_to_pix; /* from rdpmain.c */
extern int g_do_dirty_os; /* in rdpmain.c */
extern int g_do_glyph_cache; /* in rdpmain.c */
extern int g_doing_font; /* in rdpmain.c */
extern int g_do_composite; /* in rdpmain.c */

extern GCOps g_rdpGCOps; /* from rdpdraw.c */

extern int g_con_number; /* in rdpup.c */

extern int g_crc_seed; /* in rdpmisc.c */
extern int g_crc_table[]; /* in rdpmisc.c */

/******************************************************************************/
int
rdpCreatePicture(PicturePtr pPicture)
{
    PictureScreenPtr ps;
    int rv;
    
    LLOGLN(10, ("rdpCreatePicture:"));
    ps = GetPictureScreen(g_pScreen);
    ps->CreatePicture = g_rdpScreen.CreatePicture;
    rv = ps->CreatePicture(pPicture);
    ps->CreatePicture = rdpCreatePicture;
    return rv;
}

/******************************************************************************/
void
rdpDestroyPicture(PicturePtr pPicture)
{
    PictureScreenPtr ps;
    
    LLOGLN(10, ("rdpDestroyPicture:"));
    ps = GetPictureScreen(g_pScreen);
    ps->DestroyPicture = g_rdpScreen.DestroyPicture;
    ps->DestroyPicture(pPicture);
    ps->DestroyPicture = rdpDestroyPicture;
}

/******************************************************************************/
static int
print_format(PictFormatShort format)
{
    switch (format)
    {
        case PIXMAN_a2r10g10b10:
            LLOGLN(0, ("  PIXMAN_x2r10g10b10"));
            break;
        case PIXMAN_x2r10g10b10:
            LLOGLN(0, ("  PIXMAN_x2r10g10b10"));
            break;
        case PIXMAN_a2b10g10r10:
            LLOGLN(0, ("  PIXMAN_a2b10g10r10"));
            break;
        case PIXMAN_x2b10g10r10:
            LLOGLN(0, ("  PIXMAN_x2b10g10r10"));
            break;
            
        case PIXMAN_a8r8g8b8:
            LLOGLN(0, ("  PIXMAN_a8r8g8b8"));
            break;
        case PIXMAN_x8r8g8b8:
            LLOGLN(0, ("  PIXMAN_x8r8g8b8"));
            break;
        case PIXMAN_a8b8g8r8:
            LLOGLN(0, ("  PIXMAN_a8b8g8r8"));
            break;
        case PIXMAN_x8b8g8r8:
            LLOGLN(0, ("  PIXMAN_x8b8g8r8"));
            break;
        case PIXMAN_b8g8r8a8:
            LLOGLN(0, ("  PIXMAN_b8g8r8a8"));
            break;
        case PIXMAN_b8g8r8x8:
            LLOGLN(0, ("  PIXMAN_b8g8r8x8"));
            break;
            
            /* 24bpp formats */
        case PIXMAN_r8g8b8:
            LLOGLN(0, ("  PIXMAN_r8g8b8"));
            break;
        case PIXMAN_b8g8r8:
            LLOGLN(0, ("  PIXMAN_b8g8r8"));
            break;
            
            /* 16bpp formats */
        case PIXMAN_r5g6b5:
            LLOGLN(0, ("  PIXMAN_r5g6b5"));
            break;
        case PIXMAN_b5g6r5:
            LLOGLN(0, ("  PIXMAN_b5g6r5"));
            break;
            
        case PIXMAN_a1r5g5b5:
            LLOGLN(0, ("  PIXMAN_a1r5g5b5"));
            break;
        case PIXMAN_x1r5g5b5:
            LLOGLN(0, ("  PIXMAN_x1r5g5b5"));
            break;
        case PIXMAN_a1b5g5r5:
            LLOGLN(0, ("  PIXMAN_a1b5g5r5"));
            break;
        case PIXMAN_x1b5g5r5:
            LLOGLN(0, ("  PIXMAN_x1b5g5r5"));
            break;
        case PIXMAN_a4r4g4b4:
            LLOGLN(0, ("  PIXMAN_a4r4g4b4"));
            break;
        case PIXMAN_x4r4g4b4:
            LLOGLN(0, ("  PIXMAN_x4r4g4b4"));
            break;
        case PIXMAN_a4b4g4r4:
            LLOGLN(0, ("  PIXMAN_a4b4g4r4"));
            break;
        case PIXMAN_x4b4g4r4:
            LLOGLN(0, ("  PIXMAN_x4b4g4r4"));
            break;
            
            /* 8bpp formats */
        case PIXMAN_a8:
            LLOGLN(0, ("  PIXMAN_a8"));
            break;
        case PIXMAN_r3g3b2:
            LLOGLN(0, ("  PIXMAN_r3g3b2"));
            break;
        case PIXMAN_b2g3r3:
            LLOGLN(0, ("  PIXMAN_b2g3r3"));
            break;
        case PIXMAN_a2r2g2b2:
            LLOGLN(0, ("  PIXMAN_a2r2g2b2"));
            break;
        case PIXMAN_a2b2g2r2:
            LLOGLN(0, ("  PIXMAN_a2b2g2r2"));
            break;
            
        case PIXMAN_c8:
            LLOGLN(0, ("  PIXMAN_c8"));
            break;
        case PIXMAN_g8:
            LLOGLN(0, ("  PIXMAN_g8"));
            break;
            
        case PIXMAN_x4a4:
            LLOGLN(0, ("  PIXMAN_x4a4"));
            break;
            
            /* 4bpp formats */
        case PIXMAN_a4:
            LLOGLN(0, ("  PIXMAN_a4"));
            break;
        case PIXMAN_r1g2b1:
            LLOGLN(0, ("  PIXMAN_r1g2b1"));
            break;
        case PIXMAN_b1g2r1:
            LLOGLN(0, ("  PIXMAN_b1g2r1"));
            break;
        case PIXMAN_a1r1g1b1:
            LLOGLN(0, ("  PIXMAN_a1r1g1b1"));
            break;
        case PIXMAN_a1b1g1r1:
            LLOGLN(0, ("  PIXMAN_a1b1g1r1"));
            break;
            
        case PIXMAN_c4:
            LLOGLN(0, ("  PIXMAN_c4"));
            break;
        case PIXMAN_g4:
            LLOGLN(0, ("  PIXMAN_g4"));
            break;
            
            /* 1bpp formats */
        case PIXMAN_a1:
            LLOGLN(0, ("  PIXMAN_a1"));
            break;
        case PIXMAN_g1:
            LLOGLN(0, ("  PIXMAN_g1"));
            break;
    }
    return 0;
}

/******************************************************************************/
static int
compsoite_print(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
                INT16 yDst, CARD16 width, CARD16 height)
{
    PixmapPtr pSrcPixmap;
    PixmapPtr pDstPixmap;
    rdpPixmapRec* pSrcPriv;
    rdpPixmapRec* pDstPriv;
    
    LLOGLN(0, ("compsoite_print: op %d xSrc %d ySrc %d xDst %d yDst %d "
               "width %d height %d",
               op, xSrc, ySrc, xDst, yDst, width, height));
    
    if (pSrc != 0)
    {
        LLOGLN(0, ("  src depth %d width %d height %d repeat %d repeatType %d "
                   "dither %d filter %d alphaMap %p componentAlpha %d", pSrc->pDrawable->depth,
                   pSrc->pDrawable->width, pSrc->pDrawable->height,
                   pSrc->repeat, pSrc->repeatType, pSrc->dither, pSrc->filter,
                   pSrc->alphaMap, pSrc->componentAlpha));
        LLOGLN(0, ("  transform %p", pSrc->transform));
        LLOGLN(0, ("  detail format red %d red mask %d green %d green mask %d "
                   "blue %d blue mask %d",
                   pSrc->pFormat->direct.red, pSrc->pFormat->direct.redMask,
                   pSrc->pFormat->direct.green, pSrc->pFormat->direct.greenMask,
                   pSrc->pFormat->direct.blue, pSrc->pFormat->direct.blueMask));
        print_format(pSrc->format);
        if (pSrc->pDrawable->type == DRAWABLE_PIXMAP)
        {
            pSrcPixmap = (PixmapPtr)(pSrc->pDrawable);
            pSrcPriv = GETPIXPRIV(pSrcPixmap);
            LLOGLN(0, ("  DRAWABLE_PIXMAP pSrcPriv %p status %d", pSrcPriv, pSrcPriv->status));
        }
        else if (pSrc->pDrawable->type == DRAWABLE_WINDOW)
        {
            LLOGLN(0, ("  DRAWABLE_WINDOW"));
        }
        else
        {
            LLOGLN(0, ("  OTHER"));
        }
    }
    if (pMask != 0)
    {
        LLOGLN(0, ("  msk depth %d width %d height %d repeat %d repeatType %d",
                   pMask->pDrawable->depth,
                   pMask->pDrawable->width,
                   pMask->pDrawable->height, pMask->repeat, pMask->repeatType));
        print_format(pMask->format);
    }
    if (pDst != 0)
    {
        LLOGLN(0, ("  dst depth %d width %d height %d repeat %d repeatType %d "
                   "dither %d filter %d alphaMap %p", pDst->pDrawable->depth,
                   pDst->pDrawable->width, pDst->pDrawable->height,
                   pDst->repeat, pDst->repeatType, pDst->dither, pDst->filter,
                   pDst->alphaMap));
        LLOGLN(0, ("  transform %p", pDst->transform));
        print_format(pDst->format);
        if (pDst->pDrawable->type == DRAWABLE_PIXMAP)
        {
            pDstPixmap = (PixmapPtr)(pDst->pDrawable);
            pDstPriv = GETPIXPRIV(pDstPixmap);
            LLOGLN(0, ("  DRAWABLE_PIXMAP pDstPriv %p status %d", pDstPriv, pDstPriv->status));
        }
        else if (pDst->pDrawable->type == DRAWABLE_WINDOW)
        {
            LLOGLN(0, ("  DRAWABLE_WINDOW"));
        }
        else
        {
            LLOGLN(0, ("  OTHER"));
        }
    }
    return 0;
}

/******************************************************************************/
static int
src_alpha_needed(CARD8 op)
{
    int rv;
    
    rv = 0;
    switch (op)
    {
        case 3: /* Over */
        case 6: /* InReverse */
        case 8: /* OutReverse */
        case 9: /* Atop */
        case 10: /* AtopReverse */
        case 11: /* Xor */
        case 13: /* Saturate */
        case 17: /* DisjointOver */
        case 18: /* DisjointOverReverse */
        case 19: /* DisjointIn */
        case 20: /* DisjointInReverse */
        case 21: /* DisjointOut */
        case 22: /* DisjointOutReverse */
        case 23: /* DisjointAtop */
        case 24: /* DisjointAtopReverse */
        case 25: /* DisjointXor */
        case 29: /* ConjointOver */
        case 30: /* ConjointOverReverse */
        case 31: /* ConjointIn */
        case 32: /* ConjointInReverse */
        case 33: /* ConjointOut */
        case 34: /* ConjointOutReverse */
        case 35: /* ConjointAtop */
        case 36: /* ConjointAtopReverse */
        case 37: /* ConjointXor */
            rv = 1;
            break;
    }
    return rv;
}

/******************************************************************************/
static int
dst_alpha_needed(CARD8 op)
{
    int rv;
    
    rv = 0;
    switch (op)
    {
        case 4: /* OverReverse */
        case 5: /* In */
        case 7: /* Out */
        case 9: /* Atop */
        case 10: /* AtopReverse */
        case 11: /* Xor */
        case 13: /* Saturate */
        case 17: /* DisjointOver */
        case 18: /* DisjointOverReverse */
        case 19: /* DisjointIn */
        case 20: /* DisjointInReverse */
        case 21: /* DisjointOut */
        case 22: /* DisjointOutReverse */
        case 23: /* DisjointAtop */
        case 24: /* DisjointAtopReverse */
        case 25: /* DisjointXor */
        case 29: /* ConjointOver */
        case 30: /* ConjointOverReverse */
        case 31: /* ConjointIn */
        case 32: /* ConjointInReverse */
        case 33: /* ConjointOut */
        case 34: /* ConjointOutReverse */
        case 35: /* ConjointAtop */
        case 36: /* ConjointAtopReverse */
        case 37: /* ConjointXor */
            rv = 1;
            break;
    }
    return rv;
}

struct msk_info
{
    int flags;
    int idx;
    int format;
    int width;
    int repeat;
};

static char g_com_fail_strings[][128] =
{
    "OK",
    "src not remotable",
    "dst not remotable",
    "msk not remotable"
};

/******************************************************************************/
/* returns boolean */
static int
check_drawables(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
                INT16 yDst, CARD16 width, CARD16 height, struct msk_info* msk)
{
    int rv;
    int fail_reason;
    PixmapPtr pSrcPixmap;
    PixmapPtr pDstPixmap;
    PixmapPtr pMskPixmap;
    rdpPixmapRec* pSrcPriv;
    rdpPixmapRec* pDstPriv;
    rdpPixmapRec* pMskPriv;
    
    fail_reason = 0;
    pSrcPixmap = 0;
    pDstPixmap = 0;
    pMskPixmap = 0;
    pSrcPriv = 0;
    pDstPriv = 0;
    pMskPriv = 0;
    rv = 0;
    if (pSrc != 0)
    {
        if (pSrc->pDrawable != 0)
        {
            if (pSrc->pDrawable->type == DRAWABLE_PIXMAP)
            {
                pSrcPixmap = (PixmapPtr)(pSrc->pDrawable);
                pSrcPriv = GETPIXPRIV(pSrcPixmap);
                if (xrdp_is_os(pSrcPixmap, pSrcPriv))
                {
                    if (pDst != 0)
                    {
                        if (pDst->pDrawable != 0)
                        {
                            if (pDst->pDrawable->type == DRAWABLE_PIXMAP)
                            {
                                pDstPixmap = (PixmapPtr)(pDst->pDrawable);
                                pDstPriv = GETPIXPRIV(pDstPixmap);
                                if (xrdp_is_os(pDstPixmap, pDstPriv))
                                {
                                    rv = 1;
                                }
                                else
                                {
                                    fail_reason = 2;
                                }
                            }
                        }
                    }
                }
                else
                {
                    fail_reason = 1;
                }
            }
        }
    }
    if (rv)
    {
        if (pMask != 0)
        {
#if 1
            rv = 0;
            if (pMask->pDrawable != 0)
            {
                if (pMask->pDrawable->type == DRAWABLE_PIXMAP)
                {
                    pMskPixmap = (PixmapPtr)(pMask->pDrawable);
                    pMskPriv = GETPIXPRIV(pMskPixmap);
                    if (xrdp_is_os(pMskPixmap, pMskPriv))
                    {
                        rv = 1;
                        msk->flags = 1;
                        msk->idx = pMskPriv->rdpindex;
                        msk->format = pMask->format;
                        msk->width = pMask->pDrawable->width;
                        msk->repeat = pMask->repeatType;
                    }
                    else
                    {
                        fail_reason = 3;
                    }
                }
            }
#endif
        }
    }
    if (rv != 0)
    {
        /* TODO: figure out why source XRGB does not work
           skipping for now because they rarely happen
           happens when drawing Firefox open file dialog, the button icons */
        if (PIXMAN_FORMAT_A(pSrc->format) == 0)
        {
            rv = 0;
            LLOGLN(10, ("check_drawables: src format"));
        }
    }
    if (rv == 0)
    {
        LLOGLN(10, ("check_drawables: can not remote [%s]", g_com_fail_strings[fail_reason]));
#if 0
        compsoite_print(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
                        xDst, yDst, width, height);
#endif
    }
    else
    {
        LLOGLN(10, ("check_drawables: can remote  [%s]", g_com_fail_strings[fail_reason]));
#if 0
        compsoite_print(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
                        xDst, yDst, width, height);
#endif
    }
    return rv;
}

/******************************************************************************/
static int
rdpRemoteComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                   INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
                   INT16 yDst, CARD16 width, CARD16 height)
{
    int ok_to_remote;
    PixmapPtr pSrcPixmap;
    PixmapPtr pMskPixmap;
    PixmapPtr pDstPixmap;
    rdpPixmapRec* pSrcPriv;
    rdpPixmapRec* pMskPriv;
    rdpPixmapRec* pDstPriv;
    BoxRec box;
    RegionRec reg1;
    RegionRec reg2;
    DrawablePtr p;
    int j;
    int num_clips;
    struct msk_info msk;
    
    LLOGLN(10, ("rdpRemoteComposite:"));
    
    memset(&msk, 0, sizeof(msk));
    ok_to_remote = check_drawables(op, pSrc, pMask, pDst, xSrc, ySrc,
                                   xMask, yMask, xDst, yDst, width, height,
                                   &msk);
    if (!ok_to_remote)
    {
        return 1;
    }
    
    ValidatePicture(pSrc);
    pSrcPixmap = (PixmapPtr)(pSrc->pDrawable);
    pSrcPriv = GETPIXPRIV(pSrcPixmap);
    rdpup_check_dirty(pSrcPixmap, pSrcPriv);
    if (PIXMAN_FORMAT_A(pSrc->format) > 0)
    {
        if (src_alpha_needed(op))
        {
            rdpup_check_alpha_dirty(pSrcPixmap, pSrcPriv);
        }
    }

    ValidatePicture(pDst);
    pDstPixmap = (PixmapPtr)(pDst->pDrawable);
    pDstPriv = GETPIXPRIV(pDstPixmap);
    rdpup_check_dirty(pDstPixmap, pDstPriv);
    
    if (PIXMAN_FORMAT_A(pDst->format) > 0)
    {
        if (dst_alpha_needed(op))
        {
            rdpup_check_alpha_dirty(pDstPixmap, pDstPriv);
        }
    }

    if (pMask != 0)
    {
        ValidatePicture(pMask);
        pMskPixmap = (PixmapPtr)(pMask->pDrawable);
        pMskPriv = GETPIXPRIV(pMskPixmap);
        rdpup_check_dirty(pMskPixmap, pMskPriv);
        if (PIXMAN_FORMAT_A(msk.format) > 0)
        {
            rdpup_check_alpha_dirty(pMskPixmap, pMskPriv);
        }
    }

    p = pDst->pDrawable;
    rdpup_switch_os_surface(pDstPriv->rdpindex);
    if (pDst->pCompositeClip != 0)
    {
        box.x1 = xDst;
        box.y1 = yDst;
        box.x2 = box.x1 + width;
        box.y2 = box.y1 + height;
        RegionInit(&reg1, &box, 0);
        RegionInit(&reg2, NullBox, 0);
        RegionCopy(&reg2, pDst->pCompositeClip);
        RegionIntersect(&reg1, &reg1, &reg2);
        RegionTranslate(&reg1, p->x, p->y);
        num_clips = REGION_NUM_RECTS(&reg1);
        if (num_clips > 0)
        {
            LLOGLN(10, ("num_clips %d", num_clips));
            rdpup_begin_update();
            for (j = num_clips - 1; j >= 0; j--)
            {
                box = REGION_RECTS(&reg1)[j];
                rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                LLOGLN(10, ("pSrc->format 0x%x 0x%x 0x%x %d %d %d %d %d %d %d %d",
                            pSrc->format, msk.format, pDst->format, xSrc, ySrc,
                            xMask, yMask, xDst, yDst, width, height));
                rdpup_composite(pSrcPriv->rdpindex, pSrc->format,
                                pSrc->pDrawable->width, pSrc->repeatType,
                                pSrc->transform, msk.flags, msk.idx, msk.format,
                                msk.width, msk.repeat, op, xSrc, ySrc,
                                xMask, yMask, xDst, yDst, width, height, pDst->format);
            }
            rdpup_reset_clip();
            rdpup_end_update();
        }
        RegionUninit(&reg1);
        RegionUninit(&reg2);
    }
    else
    {
        rdpup_begin_update();
        rdpup_composite(pSrcPriv->rdpindex, pSrc->format,
                        pSrc->pDrawable->width, pSrc->repeatType,
                        pSrc->transform, msk.flags, msk.idx, msk.format,
                        msk.width, msk.repeat, op, xSrc, ySrc,
                        xMask, yMask, xDst, yDst, width, height, pDst->format);
        rdpup_end_update();
    }
    rdpup_switch_os_surface(-1);
    
    return 0;
}

/******************************************************************************/
static void
rdpCompositeOrg(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
                INT16 yDst, CARD16 width, CARD16 height)
{
    PictureScreenPtr ps;
    
    ps = GetPictureScreen(g_pScreen);
    ps->Composite = g_rdpScreen.Composite;
    ps->Composite(op, pSrc, pMask, pDst, xSrc, ySrc,
                  xMask, yMask, xDst, yDst, width, height);
    ps->Composite = rdpComposite;
}

/******************************************************************************/
/* it looks like all the antialias draws go through here
 op is one of the following
 #define PictOpMinimum       0
 #define PictOpClear         0
 #define PictOpSrc           1
 #define PictOpDst           2
 #define PictOpOver          3
 #define PictOpOverReverse   4
 #define PictOpIn            5
 #define PictOpInReverse     6
 #define PictOpOut           7
 #define PictOpOutReverse    8
 #define PictOpAtop          9
 #define PictOpAtopReverse   10
 #define PictOpXor           11
 #define PictOpAdd           12
 #define PictOpSaturate      13
 #define PictOpMaximum       13
 
 see for porter duff
 http://www.svgopen.org/2005/papers/abstractsvgopen/
 
 */
void
rdpComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
             INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
             INT16 yDst, CARD16 width, CARD16 height)
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
    WindowPtr pDstWnd;
    PixmapPtr pDstPixmap;
    rdpPixmapRec* pDstPriv;
    rdpPixmapRec* pDirtyPriv;
    struct image_data id;

    LLOGLN(10, ("rdpComposite:"));

    if (g_doing_font == 2)
    {
        rdpCompositeOrg(op, pSrc, pMask, pDst, xSrc, ySrc,
                        xMask, yMask, xDst, yDst, width, height);

        return;
    }

#if 0
    if (g_do_glyph_cache && g_do_alpha_glyphs)
    {
        if (pSrc->pDrawable->width == 1 &&
            pSrc->pDrawable->height == 1)
        {
            if (pMask != 0)
            {
                /* TODO: here we can try to send it as a gylph */
            }
        }
    }
#endif

    /* try to remote the composite call */
    if (g_do_composite &&
        rdpRemoteComposite(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
                           xDst, yDst, width, height) == 0)
    {
        rdpCompositeOrg(op, pSrc, pMask, pDst, xSrc, ySrc,
                        xMask, yMask, xDst, yDst, width, height);
        return;
    }

    rdpCompositeOrg(op, pSrc, pMask, pDst, xSrc, ySrc,
                    xMask, yMask, xDst, yDst, width, height);

    LLOGLN(10, ("rdpComposite: op %d %p %p %p w %d h %d", op, pSrc, pMask, pDst, width, height));
    
    p = pDst->pDrawable;
    
    pDstPriv = 0;
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
            post_process = 1;
            if (g_do_dirty_os)
            {
                LLOGLN(10, ("rdpComposite: gettig dirty"));
                pDstPriv->is_dirty = 1;
                dirty_type = g_doing_font ? RDI_IMGLL : RDI_IMGLY;
                pDirtyPriv = pDstPriv;
            }
            else
            {
                rdpup_switch_os_surface(pDstPriv->rdpindex);
                reset_surface = 1;
                rdpup_get_pixmap_image_rect(pDstPixmap, &id);
                got_id = 1;
                LLOGLN(10, ("rdpComposite: offscreen"));
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
                LLOGLN(10, ("rdpComposite: screen"));
            }
        }
    }
    
    if (!post_process)
    {
        return;
    }
    
    if (pDst->pCompositeClip != 0)
    {
        box.x1 = p->x + xDst;
        box.y1 = p->y + yDst;
        box.x2 = box.x1 + width;
        box.y2 = box.y1 + height;
        RegionInit(&reg1, &box, 0);
        RegionInit(&reg2, NullBox, 0);
        RegionCopy(&reg2, pDst->pCompositeClip);
        RegionIntersect(&reg1, &reg1, &reg2);
        if (dirty_type != 0)
        {
            draw_item_add_img_region(pDirtyPriv, &reg1, GXcopy, dirty_type, TAG_COMPOSITE);
        }
        else if (got_id)
        {
            num_clips = REGION_NUM_RECTS(&reg1);
            if (num_clips > 0)
            {
                rdpup_begin_update();
                for (j = num_clips - 1; j >= 0; j--)
                {
                    box = REGION_RECTS(&reg1)[j];
                    rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                }
                rdpup_end_update();
            }
        }
        RegionUninit(&reg1);
        RegionUninit(&reg2);
    }
    else
    {
        box.x1 = p->x + xDst;
        box.y1 = p->y + yDst;
        box.x2 = box.x1 + width;
        box.y2 = box.y1 + height;
        if (dirty_type != 0)
        {
            RegionInit(&reg1, &box, 0);
            draw_item_add_img_region(pDirtyPriv, &reg1, GXcopy, dirty_type, TAG_COMPOSITE);
            RegionUninit(&reg1);
        }
        else if (got_id)
        {
            rdpup_begin_update();
            rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            rdpup_end_update();
        }
    }
    if (reset_surface)
    {
        rdpup_switch_os_surface(-1);
    }
}
