/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2016
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
 * region, from pixman.h
 */

#if !defined(PIXMAN_PIXMAN_H__)
#define PIXMAN_PIXMAN_H__

typedef int pixman_bool_t;

struct pixman_region16_data
{
    long size;
    long numRects;
};

struct pixman_box16
{
    signed short x1, y1, x2, y2;
};

struct pixman_region16
{
    struct pixman_box16 extents;
    struct pixman_region16_data *data;
};

enum _pixman_region_overlap_t
{
    PIXMAN_REGION_OUT,
    PIXMAN_REGION_IN,
    PIXMAN_REGION_PART
};

typedef enum _pixman_region_overlap_t pixman_region_overlap_t;

typedef struct pixman_region16_data     pixman_region16_data_t;
typedef struct pixman_box16             pixman_box16_t;
typedef struct pixman_region16          pixman_region16_t;

/* creation/destruction */
void                    pixman_region_init               (pixman_region16_t *region);
void                    pixman_region_init_rect          (pixman_region16_t *region,
        /**/                                              int                x,
        /**/                                              int                y,
        /**/                                              unsigned int       width,
        /**/                                              unsigned int       height);
void                    pixman_region_fini               (pixman_region16_t *region);
pixman_bool_t           pixman_region_union              (pixman_region16_t *new_reg,
        /**/                                              pixman_region16_t *reg1,
        /**/                                              pixman_region16_t *reg2);
pixman_bool_t           pixman_region_subtract           (pixman_region16_t *reg_d,
        /**/                                              pixman_region16_t *reg_m,
        /**/                                              pixman_region16_t *reg_s);
pixman_bool_t           pixman_region_intersect          (pixman_region16_t *new_reg,
        /**/                                              pixman_region16_t *reg1,
        /**/                                              pixman_region16_t *reg2);
pixman_box16_t         *pixman_region_rectangles         (pixman_region16_t *region,
        /**/                                              int               *n_rects);

#endif
