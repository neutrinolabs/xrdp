/**
 * painter utils header
 *
 * Copyright 2015-2016 Jay Sorg <jay.sorg@gmail.com>
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
 */

#if !defined(__PAINTER_UTILS_H)
#define __PAINTER_UTILS_H

#define SPLIT_a8r8g8b8(_c, _a, _r, _g, _b) \
do { \
    _a = ((_c) & 0xff000000) >> 24; \
    _r = ((_c) & 0x00ff0000) >> 16; \
    _g = ((_c) & 0x0000ff00) >> 8; \
    _b = ((_c) & 0x000000ff) >> 0; \
} while (0)

#define SPLIT_a8b8g8r8(_c, _a, _r, _g, _b) \
do { \
    _a = ((_c) & 0xff000000) >> 24; \
    _b = ((_c) & 0x00ff0000) >> 16; \
    _g = ((_c) & 0x0000ff00) >> 8; \
    _r = ((_c) & 0x000000ff) >> 0; \
} while (0)

#define SPLIT_a1r5g5b5(_c, _a, _r, _g, _b) \
do { \
    _a = (((_c) >> 15) & 1) * 0xff; \
    _r = (((_c) >> 7) & 0xf8) | (((_c) >> 12) & 0x7); \
    _g = (((_c) >> 2) & 0xf8) | (((_c) >>  8) & 0x7); \
    _b = (((_c) << 3) & 0xf8) | (((_c) >>  2) & 0x7); \
} while (0)

#define SPLIT_r5g6b5(_c, _a, _r, _g, _b) \
do { \
    _a = 0xff; \
    _r = (((_c) >> 8) & 0xf8) | (((_c) >> 13) & 0x7); \
    _g = (((_c) >> 3) & 0xfc) | (((_c) >>  9) & 0x3); \
    _b = (((_c) << 3) & 0xf8) | (((_c) >>  2) & 0x7); \
} while (0)

#define SPLIT_r3g3b2(_c, _a, _r, _g, _b) \
do { \
    _a = 0xff; \
    _r = 0; \
    _g = 0; \
    _b = 0; \
} while (0)

#define MAKE_a1r5g5b5(_c, _a, _r, _g, _b) \
do { \
    _c = (((_a) & 0xff) >> 7) << 15 | \
         (((_r) & 0xff) >> 3) << 10 | \
         (((_g) & 0xff) >> 3) <<  5 | \
         (((_b) & 0xff) >> 3) <<  0; \
} while (0)

#define MAKE_r5g6b5(_c, _a, _r, _g, _b) \
do { \
    _c = \
         (((_r) & 0xff) >> 3) << 11 | \
         (((_g) & 0xff) >> 2) <<  5 | \
         (((_b) & 0xff) >> 3) <<  0; \
} while (0)

#define MAKE_a8r8g8b8(_c, _a, _r, _g, _b) \
do { \
    _c = ((_a) & 0xff) << 24 | \
         ((_r) & 0xff) << 16 | \
         ((_g) & 0xff) <<  8 | \
         ((_b) & 0xff) <<  0; \
} while (0)

#define MAKE_a8b8g8r8(_c, _a, _r, _g, _b) \
do { \
    _c = ((_a) & 0xff) << 24 | \
         ((_b) & 0xff) << 16 | \
         ((_g) & 0xff) <<  8 | \
         ((_r) & 0xff) <<  0; \
} while (0)

struct painter_rect
{
    short x1;
    short y1;
    short x2;
    short y2;
};

struct painter
{
    int rop;
    int fgcolor;
    int bgcolor;
    int pattern_mode;
    int clip_valid;
    int pad0;
    struct painter_rect clip;
    int origin_x;
    int origin_y;
    int palette[256];
};

int
painter_rop(int rop, int src, int dst);
int
painter_get_pixel(struct painter *painter, struct painter_bitmap *bitmap,
                  int x, int y);
int
painter_set_pixel(struct painter *painter, struct painter_bitmap *dst,
                  int x, int y, int pixel, int pixel_format);
int
painter_warp_coords(struct painter *painter,
                    int *x, int *y, int *cx, int *cy,
                    int *srcx, int *srcy);
char *
bitmap_get_ptr(struct painter_bitmap *bitmap, int x, int y);
int
bitmap_get_pixel(struct painter_bitmap *bitmap, int x, int y);
int
bitmap_set_pixel(struct painter_bitmap *bitmap, int x, int y, int pixel);

#endif
