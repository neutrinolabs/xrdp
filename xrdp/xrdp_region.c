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
 * region
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"

#if defined(XRDP_PIXMAN)
#include <pixman.h>
#else
#include "pixman-region.h"
#endif

/*****************************************************************************/
struct xrdp_region *
xrdp_region_create(struct xrdp_wm *wm)
{
    struct xrdp_region *self;

    self = (struct xrdp_region *)g_malloc(sizeof(struct xrdp_region), 1);
    self->wm = wm;
    self->reg = (struct pixman_region16 *)
                g_malloc(sizeof(struct pixman_region16), 1);
    pixman_region_init(self->reg);
    return self;
}

/*****************************************************************************/
void
xrdp_region_delete(struct xrdp_region *self)
{
    if (self == 0)
    {
        return;
    }
    pixman_region_fini(self->reg);
    g_free(self->reg);
    g_free(self);
}

/*****************************************************************************/
/* returns error */
int
xrdp_region_add_rect(struct xrdp_region *self, struct xrdp_rect *rect)
{
    struct pixman_region16 lreg;

    pixman_region_init_rect(&lreg, rect->left, rect->top,
                            rect->right - rect->left,
                            rect->bottom - rect->top);
    if (!pixman_region_union(self->reg, self->reg, &lreg))
    {
        pixman_region_fini(&lreg);
        return 1;
    }
    pixman_region_fini(&lreg);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_region_subtract_rect(struct xrdp_region *self, struct xrdp_rect *rect)
{
    struct pixman_region16 lreg;

    pixman_region_init_rect(&lreg, rect->left, rect->top,
                            rect->right - rect->left,
                            rect->bottom - rect->top);
    if (!pixman_region_subtract(self->reg, self->reg, &lreg))
    {
        pixman_region_fini(&lreg);
        return 1;
    }
    pixman_region_fini(&lreg);
    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_region_intersect_rect(struct xrdp_region *self, struct xrdp_rect *rect)
{
    struct pixman_region16 lreg;

    pixman_region_init_rect(&lreg, rect->left, rect->top,
                            rect->right - rect->left,
                            rect->bottom - rect->top);
    if (!pixman_region_intersect(self->reg, self->reg, &lreg))
    {
        pixman_region_fini(&lreg);
        return 1;
    }
    pixman_region_fini(&lreg);
    return 0;
}


/*****************************************************************************/
/* returns error */
int
xrdp_region_get_rect(struct xrdp_region *self, int index,
                     struct xrdp_rect *rect)
{
    struct pixman_box16 *box;
    int count;

    box = pixman_region_rectangles(self->reg, &count);
    if ((box != 0) && (index >= 0) && (index < count))
    {
        rect->left = box[index].x1;
        rect->top = box[index].y1;
        rect->right = box[index].x2;
        rect->bottom = box[index].y2;
        return 0;
    }
    return 1;
}

/*****************************************************************************/
/* returns error */
int
xrdp_region_get_bounds(struct xrdp_region *self, struct xrdp_rect *rect)
{
    struct pixman_box16 *box;

    box = pixman_region_extents(self->reg);
    if (box != 0)
    {
        rect->left = box->x1;
        rect->top = box->y1;
        rect->right = box->x2;
        rect->bottom = box->y2;
        return 0;
    }
    return 1;
}

/*****************************************************************************/
/* returns boolean */
int
xrdp_region_not_empty(struct xrdp_region *self)
{
    pixman_bool_t not_empty;

    not_empty = pixman_region_not_empty(self->reg);
    return not_empty;
}
