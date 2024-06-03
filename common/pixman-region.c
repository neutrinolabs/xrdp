/*
 * Copyright 1987, 1988, 1989, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 *
 * Copyright 1987, 1988, 1989 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                    All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Copyright © 1998 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PIXMAN_REGION_MAX
#error "This file should be #included from pixman-region16.c, not compiled directly"
#endif
#ifndef CONFIG_AC_H
#error "config_ac.h not visible in pixman-region.c"
#endif

#define PIXREGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
/* not a region */
#define PIXREGION_NAR(reg)      ((reg)->data == pixman_broken_data)
#define PIXREGION_NUMRECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define PIXREGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define PIXREGION_RECTS(reg) \
    ((reg)->data ? (box_type_t *)((reg)->data + 1) \
     : &(reg)->extents)
#define PIXREGION_BOXPTR(reg) ((box_type_t *)((reg)->data + 1))
#define PIXREGION_BOX(reg, i) (&PIXREGION_BOXPTR (reg)[i])
#define PIXREGION_TOP(reg) PIXREGION_BOX (reg, (reg)->data->numRects)
#define PIXREGION_END(reg) PIXREGION_BOX (reg, (reg)->data->numRects - 1)

#define GOOD_RECT(rect) ((rect)->x1 < (rect)->x2 && (rect)->y1 < (rect)->y2)
#define BAD_RECT(rect) ((rect)->x1 > (rect)->x2 || (rect)->y1 > (rect)->y2)

/* This file is included by pixman-region16.c which defines PREFIX(x).
 * This check allows cppcheck 2.x to scan this file separately */
#ifndef PREFIX
#define PREFIX(x) pixman_region##x
#endif

#ifdef USE_DEVEL_LOGGING

pixman_bool_t PREFIX(_selfcheck) (region_type_t *reg);

#define GOOD(reg)                                                       \
    do                                                                  \
    {                                                                   \
        if (!PREFIX (_selfcheck (reg)))                                 \
            _pixman_log_error (FUNC, "Malformed region " # reg);        \
    } while (0)

#else

#define GOOD(reg)

#endif

static const box_type_t PREFIX (_empty_box_) = { 0, 0, 0, 0 };
static const region_data_type_t PREFIX (_empty_data_) = { 0, 0 };
#if defined (__llvm__) && !defined (__clang__)
static const volatile region_data_type_t PREFIX (_broken_data_) = { 0, 0 };
#else
static const region_data_type_t PREFIX (_broken_data_) = { 0, 0 };
#endif

static box_type_t *pixman_region_empty_box =
    (box_type_t *) &PREFIX (_empty_box_);
static region_data_type_t *pixman_region_empty_data =
    (region_data_type_t *) &PREFIX (_empty_data_);
static region_data_type_t *pixman_broken_data =
    (region_data_type_t *) &PREFIX (_broken_data_);

static pixman_bool_t
pixman_break (region_type_t *region);

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other.
 *
 *  -----------                             -----------
 *  |         |                             |         |             band 0
 *  |         |  --------                   -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is          |         |  |      |
 *  -----------  |      |                   -----------  --------
 *               |      |                                |      |   band 2
 *               --------                                --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines, and
 * added pixman_region_validate in order to support several speed improvements
 * to pixman_region_validate_tree.  Bob Scheifler changed the representation
 * to be more compact when empty or a single rectangle, and did a bunch of
 * gratuitous reformatting. Carl Worth did further gratuitous reformatting
 * while re-merging the server and client region code into libpixregion.
 * Soren Sandmann did even more gratuitous reformatting.
 */

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1, r2)        \
    (!( ((r1)->x2 <= (r2)->x1)  || \
        ((r1)->x1 >= (r2)->x2)  || \
        ((r1)->y2 <= (r2)->y1)  || \
        ((r1)->y1 >= (r2)->y2) ) )

/* true iff (x,y) is in Box */
#define INBOX(r, x, y)  \
    ( ((r)->x2 >  x) && \
      ((r)->x1 <= x) && \
      ((r)->y2 >  y) && \
      ((r)->y1 <= y) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1, r2)        \
    ( ((r1)->x1 <= (r2)->x1) && \
      ((r1)->x2 >= (r2)->x2) && \
      ((r1)->y1 <= (r2)->y1) && \
      ((r1)->y2 >= (r2)->y2) )

static size_t
PIXREGION_SZOF (size_t n)
{
    size_t size = n * sizeof(box_type_t);

    if (n > UINT32_MAX / sizeof(box_type_t))
    {
        return 0;
    }

    if (sizeof(region_data_type_t) > UINT32_MAX - size)
    {
        return 0;
    }

    return size + sizeof(region_data_type_t);
}

static region_data_type_t *
alloc_data (size_t n)
{
    size_t sz = PIXREGION_SZOF (n);

    if (!sz)
    {
        return NULL;
    }

    return (region_data_type_t *) malloc(sz);
}

#define FREE_DATA(reg) if ((reg)->data && (reg)->data->size) free ((reg)->data)

#define RECTALLOC_BAIL(region, n, bail)                                 \
    do                                                                  \
    {                                                                   \
        if (!(region)->data ||                                          \
                (((region)->data->numRects + (n)) > (region)->data->size))  \
        {                                                               \
            if (!pixman_rect_alloc (region, n))                         \
                goto bail;                                              \
        }                                                               \
    } while (0)

#define RECTALLOC(region, n)                                            \
    do                                                                  \
    {                                                                   \
        if (!(region)->data ||                                          \
                (((region)->data->numRects + (n)) > (region)->data->size))  \
        {                                                               \
            if (!pixman_rect_alloc (region, n)) {                       \
                return FALSE;                                           \
            }                                                           \
        }                                                               \
    } while (0)

#define ADDRECT(next_rect, nx1, ny1, nx2, ny2)      \
    do                                              \
    {                                               \
        next_rect->x1 = nx1;                        \
        next_rect->y1 = ny1;                        \
        next_rect->x2 = nx2;                        \
        next_rect->y2 = ny2;                        \
        next_rect++;                                \
    }                                               \
    while (0)

#define NEWRECT(region, next_rect, nx1, ny1, nx2, ny2)                  \
    do                                                                  \
    {                                                                   \
        if (!(region)->data ||                                          \
                ((region)->data->numRects == (region)->data->size))         \
        {                                                               \
            if (!pixman_rect_alloc (region, 1))                         \
                return FALSE;                                           \
            next_rect = PIXREGION_TOP (region);                         \
        }                                                               \
        ADDRECT (next_rect, nx1, ny1, nx2, ny2);                        \
        region->data->numRects++;                                       \
        critical_if_fail (region->data->numRects <= region->data->size);                \
    } while (0)

#define DOWNSIZE(reg, numRects)                                         \
    do                                                                  \
    {                                                                   \
        if (((numRects) < ((reg)->data->size >> 1)) &&                  \
                ((reg)->data->size > 50))                                   \
        {                                                               \
            region_data_type_t * new_data;                              \
            size_t data_size = PIXREGION_SZOF (numRects);               \
            \
            if (!data_size)                                             \
            {                                                           \
                new_data = NULL;                                        \
            }                                                           \
            else                                                        \
            {                                                           \
                new_data = (region_data_type_t *)                       \
                           realloc ((reg)->data, data_size);                   \
            }                                                           \
            \
            if (new_data)                                               \
            {                                                           \
                new_data->size = (numRects);                            \
                (reg)->data = new_data;                                 \
            }                                                           \
        }                                                               \
    } while (0)


PIXMAN_EXPORT void
PREFIX (_init) (region_type_t *region)
{
    region->extents = *pixman_region_empty_box;
    region->data = pixman_region_empty_data;
}

PIXMAN_EXPORT void
PREFIX (_init_rect) (region_type_t     *region,
                     int                x,
                     int                y,
                     unsigned int       width,
                     unsigned int       height)
{
    region->extents.x1 = x;
    region->extents.y1 = y;
    region->extents.x2 = x + width;
    region->extents.y2 = y + height;

    if (!GOOD_RECT (&region->extents))
    {
        if (BAD_RECT (&region->extents))
        {
            _pixman_log_error (FUNC, "Invalid rectangle passed");
        }
        PREFIX (_init) (region);
        return;
    }

    region->data = NULL;
}

PIXMAN_EXPORT void
PREFIX (_fini) (region_type_t *region)
{
    GOOD (region);
    FREE_DATA (region);
}

PIXMAN_EXPORT box_type_t *
PREFIX (_rectangles) (region_type_t *region,
                      int               *n_rects)
{
    if (n_rects)
    {
        *n_rects = PIXREGION_NUMRECTS (region);
    }

    return PIXREGION_RECTS (region);
}

static pixman_bool_t
pixman_break (region_type_t *region)
{
    FREE_DATA (region);

    region->extents = *pixman_region_empty_box;
    region->data = pixman_broken_data;

    return FALSE;
}

static pixman_bool_t
pixman_rect_alloc (region_type_t *region,
                   int             n)
{
    region_data_type_t *data;

    if (!region->data)
    {
        n++;
        region->data = alloc_data (n);

        if (!region->data)
        {
            return pixman_break (region);
        }

        region->data->numRects = 1;
        *PIXREGION_BOXPTR (region) = region->extents;
    }
    else if (!region->data->size)
    {
        region->data = alloc_data (n);

        if (!region->data)
        {
            return pixman_break (region);
        }

        region->data->numRects = 0;
    }
    else
    {
        size_t data_size;

        if (n == 1)
        {
            n = region->data->numRects;
            if (n > 500) /* XXX pick numbers out of a hat */
            {
                n = 250;
            }
        }

        n += region->data->numRects;
        data_size = PIXREGION_SZOF (n);

        if (!data_size)
        {
            data = NULL;
        }
        else
        {
            data = (region_data_type_t *)
                   realloc (region->data, PIXREGION_SZOF (n));
        }

        if (!data)
        {
            return pixman_break (region);
        }

        region->data = data;
    }

    region->data->size = n;

    return TRUE;
}

static PIXMAN_EXPORT pixman_bool_t
PREFIX (_copy) (region_type_t *dst, region_type_t *src)
{
    GOOD (dst);
    GOOD (src);

    if (dst == src)
    {
        return TRUE;
    }

    dst->extents = src->extents;

    if (!src->data || !src->data->size)
    {
        FREE_DATA (dst);
        dst->data = src->data;
        return TRUE;
    }

    if (!dst->data || (dst->data->size < src->data->numRects))
    {
        FREE_DATA (dst);

        dst->data = alloc_data (src->data->numRects);

        if (!dst->data)
        {
            return pixman_break (dst);
        }

        dst->data->size = src->data->numRects;
    }

    dst->data->numRects = src->data->numRects;

    memmove ((char *)PIXREGION_BOXPTR (dst), (char *)PIXREGION_BOXPTR (src),
             dst->data->numRects * sizeof(box_type_t));

    return TRUE;
}

/*======================================================================
 *          Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_coalesce --
 *      Attempt to merge the boxes in the current band with those in the
 *      previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by pixman_op.
 *
 * Results:
 *      The new index for the previous band.
 *
 * Side Effects:
 *      If coalescing takes place:
 *          - rectangles in the previous band will have their y2 fields
 *            altered.
 *          - region->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
static inline int
pixman_coalesce (region_type_t *region,       /* Region to coalesce              */
                 int             prev_start,  /* Index of start of previous band */
                 int             cur_start)   /* Index of start of current band  */
{
    box_type_t *prev_box;       /* Current box in previous band      */
    box_type_t *cur_box;        /* Current box in current band       */
    int numRects;               /* Number rectangles in both bands   */
    int y2;                     /* Bottom of current band            */

    /*
     * Figure out how many rectangles are in the band.
     */
    numRects = cur_start - prev_start;
    critical_if_fail (numRects == region->data->numRects - cur_start);

    if (!numRects)
    {
        return cur_start;
    }

    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    prev_box = PIXREGION_BOX (region, prev_start);
    cur_box = PIXREGION_BOX (region, cur_start);
    if (prev_box->y2 != cur_box->y1)
    {
        return cur_start;
    }

    /*
     * Make sure the bands have boxes in the same places. This
     * assumes that boxes have been added in such a way that they
     * cover the most area possible. I.e. two boxes in a band must
     * have some horizontal space between them.
     */
    y2 = cur_box->y2;

    do
    {
        if ((prev_box->x1 != cur_box->x1) || (prev_box->x2 != cur_box->x2))
        {
            return (cur_start);
        }

        prev_box++;
        cur_box++;
        numRects--;
    }
    while (numRects);

    /*
     * The bands may be merged, so set the bottom y of each box
     * in the previous band to the bottom y of the current band.
     */
    numRects = cur_start - prev_start;
    region->data->numRects -= numRects;

    do
    {
        prev_box--;
        prev_box->y2 = y2;
        numRects--;
    }
    while (numRects);

    return prev_start;
}

/* Quicky macro to avoid trivial reject procedure calls to pixman_coalesce */

#define COALESCE(new_reg, prev_band, cur_band)                          \
    do                                                                  \
    {                                                                   \
        if (cur_band - prev_band == new_reg->data->numRects - cur_band) \
            prev_band = pixman_coalesce (new_reg, prev_band, cur_band); \
        else                                                            \
            prev_band = cur_band;                                       \
    } while (0)

/*-
 *-----------------------------------------------------------------------
 * pixman_region_append_non_o --
 *      Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      region->data->numRects is incremented and the rectangles overwritten
 *      with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */
static inline pixman_bool_t
pixman_region_append_non_o (region_type_t *region,
                            box_type_t     *r,
                            box_type_t     *r_end,
                            int             y1,
                            int             y2)
{
    box_type_t *next_rect;
    int new_rects;

    new_rects = r_end - r;

    critical_if_fail (y1 < y2);
    critical_if_fail (new_rects != 0);

    /* Make sure we have enough space for all rectangles to be added */
    RECTALLOC (region, new_rects);
    next_rect = PIXREGION_TOP (region);
    region->data->numRects += new_rects;

    do
    {
        critical_if_fail (r->x1 < r->x2);
        ADDRECT (next_rect, r->x1, y1, r->x2, y2);
        r++;
    }
    while (r != r_end);

    return TRUE;
}

#define FIND_BAND(r, r_band_end, r_end, ry1)                         \
    do                                                               \
    {                                                                \
        ry1 = r->y1;                                                 \
        r_band_end = r + 1;                                          \
        while ((r_band_end != r_end) && (r_band_end->y1 == ry1)) {   \
            r_band_end++;                                            \
        }                                                            \
    } while (0)

#define APPEND_REGIONS(new_reg, r, r_end)                               \
    do                                                                  \
    {                                                                   \
        int new_rects;                                                  \
        if ((new_rects = r_end - r)) {                                  \
            RECTALLOC_BAIL (new_reg, new_rects, bail);                  \
            memmove ((char *)PIXREGION_TOP (new_reg), (char *)r,        \
                     new_rects * sizeof(box_type_t));                   \
            new_reg->data->numRects += new_rects;                       \
        }                                                               \
    } while (0)

/*-
 *-----------------------------------------------------------------------
 * pixman_op --
 *      Apply an operation to two regions. Called by pixman_region_union, pixman_region_inverse,
 *      pixman_region_subtract, pixman_region_intersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *      TRUE if successful.
 *
 * Side Effects:
 *      The new region is overwritten.
 *      overlap set to TRUE if overlap_func ever returns TRUE.
 *
 * Notes:
 *      The idea behind this function is to view the two regions as sets.
 *      Together they cover a rectangle of area that this function divides
 *      into horizontal bands where points are covered only by one region
 *      or by both. For the first case, the non_overlap_func is called with
 *      each the band and the band's upper and lower extents. For the
 *      second, the overlap_func is called to process the entire band. It
 *      is responsible for clipping the rectangles in the band, though
 *      this function provides the boundaries.
 *      At the end of each band, the new region is coalesced, if possible,
 *      to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */

typedef pixman_bool_t (*overlap_proc_ptr) (region_type_t *region,
        box_type_t    *r1,
        box_type_t    *r1_end,
        box_type_t    *r2,
        box_type_t    *r2_end,
        int            y1,
        int            y2);

static pixman_bool_t
pixman_op (region_type_t   *new_reg,               /* Place to store result         */
           region_type_t   *reg1,                  /* First region in operation     */
           region_type_t   *reg2,                  /* 2d region in operation        */
           overlap_proc_ptr overlap_func,          /* Function to call for over-
                                                    * lapping bands                 */
           int              append_non1,           /* Append non-overlapping bands
                                                    * in region 1 ?
                                                    */
           int              append_non2            /* Append non-overlapping bands
                                                    * in region 2 ?
                                                    */
          )
{
    box_type_t *r1;                 /* Pointer into first region     */
    box_type_t *r2;                 /* Pointer into 2d region        */
    box_type_t *r1_end;             /* End of 1st region             */
    box_type_t *r2_end;             /* End of 2d region              */
    int ybot;                       /* Bottom of intersection        */
    int ytop;                       /* Top of intersection           */
    region_data_type_t *old_data;   /* Old data for new_reg          */
    int prev_band;                  /* Index of start of
                                     * previous band in new_reg       */
    int cur_band;                   /* Index of start of current
                                     * band in new_reg               */
    box_type_t *r1_band_end;        /* End of current band in r1     */
    box_type_t *r2_band_end;        /* End of current band in r2     */
    int top;                        /* Top of non-overlapping band   */
    int bot;                        /* Bottom of non-overlapping band*/
    int r1y1;                       /* Temps for r1->y1 and r2->y1   */
    int r2y1;
    int new_size;
    int numRects;

    /*
     * Break any region computed from a broken region
     */
    if (PIXREGION_NAR (reg1) || PIXREGION_NAR (reg2))
    {
        return pixman_break (new_reg);
    }

    /*
     * Initialization:
     *  set r1, r2, r1_end and r2_end appropriately, save the rectangles
     * of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */

    r1 = PIXREGION_RECTS (reg1);
    new_size = PIXREGION_NUMRECTS (reg1);
    r1_end = r1 + new_size;

    numRects = PIXREGION_NUMRECTS (reg2);
    r2 = PIXREGION_RECTS (reg2);
    r2_end = r2 + numRects;

    critical_if_fail (r1 != r1_end);
    critical_if_fail (r2 != r2_end);

    old_data = (region_data_type_t *)NULL;

    if (((new_reg == reg1) && (new_size > 1)) ||
            ((new_reg == reg2) && (numRects > 1)))
    {
        old_data = new_reg->data;
        new_reg->data = pixman_region_empty_data;
    }

    /* guess at new size */
    if (numRects > new_size)
    {
        new_size = numRects;
    }

    new_size <<= 1;

    if (!new_reg->data)
    {
        new_reg->data = pixman_region_empty_data;
    }
    else if (new_reg->data->size)
    {
        new_reg->data->numRects = 0;
    }

    if (new_size > new_reg->data->size)
    {
        if (!pixman_rect_alloc (new_reg, new_size))
        {
            free (old_data);
            return FALSE;
        }
    }

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     *  In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *  For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = MIN (r1->y1, r2->y1);

    /*
     * prev_band serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. pixman_coalesce, above.
     * In the beginning, there is no previous band, so prev_band == cur_band
     * (cur_band is set later on, of course, but the first band will always
     * start at index 0). prev_band and cur_band must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prev_band = 0;

    do
    {
        /*
         * This algorithm proceeds one source-band (as opposed to a
         * destination band, which is determined by where the two regions
         * intersect) at a time. r1_band_end and r2_band_end serve to mark the
         * rectangle after the last one in the current band for their
         * respective regions.
         */
        critical_if_fail (r1 != r1_end);
        critical_if_fail (r2 != r2_end);

        FIND_BAND (r1, r1_band_end, r1_end, r1y1);
        FIND_BAND (r2, r2_band_end, r2_end, r2y1);

        /*
         * First handle the band that doesn't intersect, if any.
         *
         * Note that attention is restricted to one band in the
         * non-intersecting region at once, so if a region has n
         * bands between the current position and the next place it overlaps
         * the other, this entire loop will be passed through n times.
         */
        if (r1y1 < r2y1)
        {
            if (append_non1)
            {
                top = MAX (r1y1, ybot);
                bot = MIN (r1->y2, r2y1);
                if (top != bot)
                {
                    cur_band = new_reg->data->numRects;
                    if (!pixman_region_append_non_o (new_reg, r1, r1_band_end, top, bot))
                    {
                        goto bail;
                    }
                    COALESCE (new_reg, prev_band, cur_band);
                }
            }
            ytop = r2y1;
        }
        else if (r2y1 < r1y1)
        {
            if (append_non2)
            {
                top = MAX (r2y1, ybot);
                bot = MIN (r2->y2, r1y1);

                if (top != bot)
                {
                    cur_band = new_reg->data->numRects;

                    if (!pixman_region_append_non_o (new_reg, r2, r2_band_end, top, bot))
                    {
                        goto bail;
                    }

                    COALESCE (new_reg, prev_band, cur_band);
                }
            }
            ytop = r1y1;
        }
        else
        {
            ytop = r1y1;
        }

        /*
         * Now see if we've hit an intersecting band. The two bands only
         * intersect if ybot > ytop
         */
        ybot = MIN (r1->y2, r2->y2);
        if (ybot > ytop)
        {
            cur_band = new_reg->data->numRects;

            if (!(*overlap_func)(new_reg,
                                 r1, r1_band_end,
                                 r2, r2_band_end,
                                 ytop, ybot))
            {
                goto bail;
            }

            COALESCE (new_reg, prev_band, cur_band);
        }

        /*
         * If we've finished with a band (y2 == ybot) we skip forward
         * in the region to the next band.
         */
        if (r1->y2 == ybot)
        {
            r1 = r1_band_end;
        }

        if (r2->y2 == ybot)
        {
            r2 = r2_band_end;
        }

    }
    while (r1 != r1_end && r2 != r2_end);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1_end) && append_non1)
    {
        /* Do first non_overlap1Func call, which may be able to coalesce */
        FIND_BAND (r1, r1_band_end, r1_end, r1y1);

        cur_band = new_reg->data->numRects;

        if (!pixman_region_append_non_o (new_reg,
                                         r1, r1_band_end,
                                         MAX (r1y1, ybot), r1->y2))
        {
            goto bail;
        }

        COALESCE (new_reg, prev_band, cur_band);

        /* Just append the rest of the boxes  */
        APPEND_REGIONS (new_reg, r1_band_end, r1_end);
    }
    else if ((r2 != r2_end) && append_non2)
    {
        /* Do first non_overlap2Func call, which may be able to coalesce */
        FIND_BAND (r2, r2_band_end, r2_end, r2y1);

        cur_band = new_reg->data->numRects;

        if (!pixman_region_append_non_o (new_reg,
                                         r2, r2_band_end,
                                         MAX (r2y1, ybot), r2->y2))
        {
            goto bail;
        }

        COALESCE (new_reg, prev_band, cur_band);

        /* Append rest of boxes */
        APPEND_REGIONS (new_reg, r2_band_end, r2_end);
    }

    free (old_data);

    if (!(numRects = new_reg->data->numRects))
    {
        FREE_DATA (new_reg);
        new_reg->data = pixman_region_empty_data;
    }
    else if (numRects == 1)
    {
        new_reg->extents = *PIXREGION_BOXPTR (new_reg);
        FREE_DATA (new_reg);
        new_reg->data = (region_data_type_t *)NULL;
    }
    else
    {
        DOWNSIZE (new_reg, numRects);
    }

    return TRUE;

bail:
    free (old_data);

    return pixman_break (new_reg);
}

/*-
 *-----------------------------------------------------------------------
 * pixman_set_extents --
 *      Reset the extents of a region to what they should be. Called by
 *      pixman_region_subtract and pixman_region_intersect as they can't
 *      figure it out along the way or do so easily, as pixman_region_union can.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void
pixman_set_extents (region_type_t *region)
{
    box_type_t *box, *box_end;

    if (!region->data)
    {
        return;
    }

    if (!region->data->size)
    {
        region->extents.x2 = region->extents.x1;
        region->extents.y2 = region->extents.y1;
        return;
    }

    box = PIXREGION_BOXPTR (region);
    box_end = PIXREGION_END (region);

    /*
     * Since box is the first rectangle in the region, it must have the
     * smallest y1 and since box_end is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  box and box_end, resp., as good things to initialize them
     * to...
     */
    region->extents.x1 = box->x1;
    region->extents.y1 = box->y1;
    region->extents.x2 = box_end->x2;
    region->extents.y2 = box_end->y2;

    critical_if_fail (region->extents.y1 < region->extents.y2);

    while (box <= box_end)
    {
        if (box->x1 < region->extents.x1)
        {
            region->extents.x1 = box->x1;
        }
        if (box->x2 > region->extents.x2)
        {
            region->extents.x2 = box->x2;
        }
        box++;
    }

    critical_if_fail (region->extents.x1 < region->extents.x2);
}

/*======================================================================
 *          Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * pixman_region_intersect_o --
 *      Handle an overlapping band for pixman_region_intersect.
 *
 * Results:
 *      TRUE if successful.
 *
 * Side Effects:
 *      Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_bool_t
pixman_region_intersect_o (region_type_t *region,
                           box_type_t    *r1,
                           box_type_t    *r1_end,
                           box_type_t    *r2,
                           box_type_t    *r2_end,
                           int            y1,
                           int            y2)
{
    int x1;
    int x2;
    box_type_t         *next_rect;

    next_rect = PIXREGION_TOP (region);

    critical_if_fail (y1 < y2);
    critical_if_fail (r1 != r1_end && r2 != r2_end);

    do
    {
        x1 = MAX (r1->x1, r2->x1);
        x2 = MIN (r1->x2, r2->x2);

        /*
         * If there's any overlap between the two rectangles, add that
         * overlap to the new region.
         */
        if (x1 < x2)
        {
            NEWRECT (region, next_rect, x1, y1, x2, y2);
        }

        /*
         * Advance the pointer(s) with the leftmost right side, since the next
         * rectangle on that list may still overlap the other region's
         * current rectangle.
         */
        if (r1->x2 == x2)
        {
            r1++;
        }
        if (r2->x2 == x2)
        {
            r2++;
        }
    }
    while ((r1 != r1_end) && (r2 != r2_end));

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
PREFIX (_intersect) (region_type_t      *new_reg,
                     region_type_t         *reg1,
                     region_type_t         *reg2)
{
    GOOD (reg1);
    GOOD (reg2);
    GOOD (new_reg);

    /* check for trivial reject */
    if (PIXREGION_NIL (reg1) || PIXREGION_NIL (reg2) ||
            !EXTENTCHECK (&reg1->extents, &reg2->extents))
    {
        /* Covers about 20% of all cases */
        FREE_DATA (new_reg);
        new_reg->extents.x2 = new_reg->extents.x1;
        new_reg->extents.y2 = new_reg->extents.y1;
        if (PIXREGION_NAR (reg1) || PIXREGION_NAR (reg2))
        {
            new_reg->data = pixman_broken_data;
            return FALSE;
        }
        else
        {
            new_reg->data = pixman_region_empty_data;
        }
    }
    else if (!reg1->data && !reg2->data)
    {
        /* Covers about 80% of cases that aren't trivially rejected */
        new_reg->extents.x1 = MAX (reg1->extents.x1, reg2->extents.x1);
        new_reg->extents.y1 = MAX (reg1->extents.y1, reg2->extents.y1);
        new_reg->extents.x2 = MIN (reg1->extents.x2, reg2->extents.x2);
        new_reg->extents.y2 = MIN (reg1->extents.y2, reg2->extents.y2);

        FREE_DATA (new_reg);

        new_reg->data = (region_data_type_t *)NULL;
    }
    else if (!reg2->data && SUBSUMES (&reg2->extents, &reg1->extents))
    {
        return PREFIX (_copy) (new_reg, reg1);
    }
    else if (!reg1->data && SUBSUMES (&reg1->extents, &reg2->extents))
    {
        return PREFIX (_copy) (new_reg, reg2);
    }
    else if (reg1 == reg2)
    {
        return PREFIX (_copy) (new_reg, reg1);
    }
    else
    {
        /* General purpose intersection */

        if (!pixman_op (new_reg, reg1, reg2, pixman_region_intersect_o, FALSE, FALSE))
        {
            return FALSE;
        }

        pixman_set_extents (new_reg);
    }

    GOOD (new_reg);
    return (TRUE);
}

#define MERGERECT(r)                                                    \
    do                                                                  \
    {                                                                   \
        if (r->x1 <= x2)                                                \
        {                                                               \
            /* Merge with current rectangle */                          \
            if (x2 < r->x2)                                             \
                x2 = r->x2;                                             \
        }                                                               \
        else                                                            \
        {                                                               \
            /* Add current rectangle, start new one */                  \
            NEWRECT (region, next_rect, x1, y1, x2, y2);                \
            x1 = r->x1;                                                 \
            x2 = r->x2;                                                 \
        }                                                               \
        r++;                                                            \
    } while (0)

/*======================================================================
 *          Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_union_o --
 *      Handle an overlapping band for the union operation. Picks the
 *      left-most rectangle each time and merges it into the region.
 *
 * Results:
 *      TRUE if successful.
 *
 * Side Effects:
 *      region is overwritten.
 *      overlap is set to TRUE if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static pixman_bool_t
pixman_region_union_o (region_type_t *region,
                       box_type_t    *r1,
                       box_type_t    *r1_end,
                       box_type_t    *r2,
                       box_type_t    *r2_end,
                       int            y1,
                       int            y2)
{
    box_type_t *next_rect;
    int x1;            /* left and right side of current union */
    int x2;

    critical_if_fail (y1 < y2);
    critical_if_fail (r1 != r1_end && r2 != r2_end);

    next_rect = PIXREGION_TOP (region);

    /* Start off current rectangle */
    if (r1->x1 < r2->x1)
    {
        x1 = r1->x1;
        x2 = r1->x2;
        r1++;
    }
    else
    {
        x1 = r2->x1;
        x2 = r2->x2;
        r2++;
    }
    while (r1 != r1_end && r2 != r2_end)
    {
        if (r1->x1 < r2->x1)
        {
            MERGERECT (r1);
        }
        else
        {
            MERGERECT (r2);
        }
    }

    /* Finish off whoever (if any) is left */
    if (r1 != r1_end)
    {
        do
        {
            MERGERECT (r1);
        }
        while (r1 != r1_end);
    }
    else if (r2 != r2_end)
    {
        do
        {
            MERGERECT (r2);
        }
        while (r2 != r2_end);
    }

    /* Add current rectangle */
    NEWRECT (region, next_rect, x1, y1, x2, y2);

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
PREFIX (_union) (region_type_t *new_reg,
                 region_type_t *reg1,
                 region_type_t *reg2)
{
    /* Return TRUE if some overlap
     * between reg1, reg2
     */
    GOOD (reg1);
    GOOD (reg2);
    GOOD (new_reg);

    /*  checks all the simple cases */

    /*
     * Region 1 and 2 are the same
     */
    if (reg1 == reg2)
    {
        return PREFIX (_copy) (new_reg, reg1);
    }

    /*
     * Region 1 is empty
     */
    if (PIXREGION_NIL (reg1))
    {
        if (PIXREGION_NAR (reg1))
        {
            return pixman_break (new_reg);
        }

        if (new_reg != reg2)
        {
            return PREFIX (_copy) (new_reg, reg2);
        }

        return TRUE;
    }

    /*
     * Region 2 is empty
     */
    if (PIXREGION_NIL (reg2))
    {
        if (PIXREGION_NAR (reg2))
        {
            return pixman_break (new_reg);
        }

        if (new_reg != reg1)
        {
            return PREFIX (_copy) (new_reg, reg1);
        }

        return TRUE;
    }

    /*
     * Region 1 completely subsumes region 2
     */
    if (!reg1->data && SUBSUMES (&reg1->extents, &reg2->extents))
    {
        if (new_reg != reg1)
        {
            return PREFIX (_copy) (new_reg, reg1);
        }

        return TRUE;
    }

    /*
     * Region 2 completely subsumes region 1
     */
    if (!reg2->data && SUBSUMES (&reg2->extents, &reg1->extents))
    {
        if (new_reg != reg2)
        {
            return PREFIX (_copy) (new_reg, reg2);
        }

        return TRUE;
    }

    if (!pixman_op (new_reg, reg1, reg2, pixman_region_union_o, TRUE, TRUE))
    {
        return FALSE;
    }

    new_reg->extents.x1 = MIN (reg1->extents.x1, reg2->extents.x1);
    new_reg->extents.y1 = MIN (reg1->extents.y1, reg2->extents.y1);
    new_reg->extents.x2 = MAX (reg1->extents.x2, reg2->extents.x2);
    new_reg->extents.y2 = MAX (reg1->extents.y2, reg2->extents.y2);

    GOOD (new_reg);

    return TRUE;
}

/*======================================================================
 *          Batch Rectangle Union
 *====================================================================*/

#define EXCHANGE_RECTS(a, b)    \
    {                           \
        box_type_t t;           \
        t = rects[a];           \
        rects[a] = rects[b];    \
        rects[b] = t;           \
    }

/*======================================================================
 *                Region Subtraction
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtract_o --
 *      Overlapping band subtraction. x1 is the left-most point not yet
 *      checked.
 *
 * Results:
 *      TRUE if successful.
 *
 * Side Effects:
 *      region may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_bool_t
pixman_region_subtract_o (region_type_t *region,
                          box_type_t     *r1,
                          box_type_t     *r1_end,
                          box_type_t     *r2,
                          box_type_t     *r2_end,
                          int             y1,
                          int             y2)
{
    box_type_t         *next_rect;
    int x1;

    x1 = r1->x1;

    critical_if_fail (y1 < y2);
    critical_if_fail (r1 != r1_end && r2 != r2_end);

    next_rect = PIXREGION_TOP (region);

    do
    {
        if (r2->x2 <= x1)
        {
            /*
             * Subtrahend entirely to left of minuend: go to next subtrahend.
             */
            r2++;
        }
        else if (r2->x1 <= x1)
        {
            /*
             * Subtrahend precedes minuend: nuke left edge of minuend.
             */
            x1 = r2->x2;
            if (x1 >= r1->x2)
            {
                /*
                 * Minuend completely covered: advance to next minuend and
                 * reset left fence to edge of new minuend.
                 */
                r1++;
                if (r1 != r1_end)
                {
                    x1 = r1->x1;
                }
            }
            else
            {
                /*
                 * Subtrahend now used up since it doesn't extend beyond
                 * minuend
                 */
                r2++;
            }
        }
        else if (r2->x1 < r1->x2)
        {
            /*
             * Left part of subtrahend covers part of minuend: add uncovered
             * part of minuend to region and skip to next subtrahend.
             */
            critical_if_fail (x1 < r2->x1);
            NEWRECT (region, next_rect, x1, y1, r2->x1, y2);

            x1 = r2->x2;
            if (x1 >= r1->x2)
            {
                /*
                 * Minuend used up: advance to new...
                 */
                r1++;
                if (r1 != r1_end)
                {
                    x1 = r1->x1;
                }
            }
            else
            {
                /*
                 * Subtrahend used up
                 */
                r2++;
            }
        }
        else
        {
            /*
             * Minuend used up: add any remaining piece before advancing.
             */
            if (r1->x2 > x1)
            {
                NEWRECT (region, next_rect, x1, y1, r1->x2, y2);
            }

            r1++;

            if (r1 != r1_end)
            {
                x1 = r1->x1;
            }
        }
    }
    while ((r1 != r1_end) && (r2 != r2_end));

    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1_end)
    {
        critical_if_fail (x1 < r1->x2);

        NEWRECT (region, next_rect, x1, y1, r1->x2, y2);

        r1++;
        if (r1 != r1_end)
        {
            x1 = r1->x1;
        }
    }
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtract --
 *      Subtract reg_s from reg_m and leave the result in reg_d.
 *      S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *      TRUE if successful.
 *
 * Side Effects:
 *      reg_d is overwritten.
 *
 *-----------------------------------------------------------------------
 */
PIXMAN_EXPORT pixman_bool_t
PREFIX (_subtract) (region_type_t *reg_d,
                    region_type_t *reg_m,
                    region_type_t *reg_s)
{
    GOOD (reg_m);
    GOOD (reg_s);
    GOOD (reg_d);

    /* check for trivial rejects */
    if (PIXREGION_NIL (reg_m) || PIXREGION_NIL (reg_s) ||
            !EXTENTCHECK (&reg_m->extents, &reg_s->extents))
    {
        if (PIXREGION_NAR (reg_s))
        {
            return pixman_break (reg_d);
        }

        return PREFIX (_copy) (reg_d, reg_m);
    }
    else if (reg_m == reg_s)
    {
        FREE_DATA (reg_d);
        reg_d->extents.x2 = reg_d->extents.x1;
        reg_d->extents.y2 = reg_d->extents.y1;
        reg_d->data = pixman_region_empty_data;

        return TRUE;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky subtraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!pixman_op (reg_d, reg_m, reg_s, pixman_region_subtract_o, TRUE, FALSE))
    {
        return FALSE;
    }

    /*
     * Can't alter reg_d's extents before we call pixman_op because
     * it might be one of the source regions and pixman_op depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    pixman_set_extents (reg_d);
    GOOD (reg_d);
    return TRUE;
}

/*======================================================================
 *          Region Inversion
 *====================================================================*/

PIXMAN_EXPORT int
PREFIX (_not_empty) (region_type_t *region)
{
    GOOD (region);

    return (!PIXREGION_NIL (region));
}

PIXMAN_EXPORT box_type_t *
PREFIX (_extents) (region_type_t *region)
{
    GOOD (region);

    return (&region->extents);
}

#ifdef USE_DEVEL_LOGGING
/*
 * Clip a list of scanlines to a region.  The caller has allocated the
 * space.  FSorted is non-zero if the scanline origins are in ascending order.
 *
 * returns the number of new, clipped scanlines.
 *
 * NB: For xrdp this function is only used if we are running devel logging
 */

PIXMAN_EXPORT pixman_bool_t
PREFIX (_selfcheck) (region_type_t *reg)
{
    int i, numRects;

    if ((reg->extents.x1 > reg->extents.x2) ||
            (reg->extents.y1 > reg->extents.y2))
    {
        return FALSE;
    }

    numRects = PIXREGION_NUMRECTS (reg);
    if (!numRects)
    {
        return ((reg->extents.x1 == reg->extents.x2) &&
                (reg->extents.y1 == reg->extents.y2) &&
                (reg->data->size || (reg->data == pixman_region_empty_data)));
    }
    else if (numRects == 1)
    {
        return (!reg->data);
    }
    else
    {
        box_type_t *pbox_p, *pbox_n;
        box_type_t box;

        pbox_p = PIXREGION_RECTS (reg);
        box = *pbox_p;
        box.y2 = pbox_p[numRects - 1].y2;
        pbox_n = pbox_p + 1;

        for (i = numRects; --i > 0; pbox_p++, pbox_n++)
        {
            if ((pbox_n->x1 >= pbox_n->x2) ||
                    (pbox_n->y1 >= pbox_n->y2))
            {
                return FALSE;
            }

            if (pbox_n->x1 < box.x1)
            {
                box.x1 = pbox_n->x1;
            }

            if (pbox_n->x2 > box.x2)
            {
                box.x2 = pbox_n->x2;
            }

            if ((pbox_n->y1 < pbox_p->y1) ||
                    ((pbox_n->y1 == pbox_p->y1) &&
                     ((pbox_n->x1 < pbox_p->x2) || (pbox_n->y2 != pbox_p->y2))))
            {
                return FALSE;
            }
        }

        return ((box.x1 == reg->extents.x1) &&
                (box.x2 == reg->extents.x2) &&
                (box.y1 == reg->extents.y1) &&
                (box.y2 == reg->extents.y2));
    }
}
#endif // USE_DEVEL_LOGGING
