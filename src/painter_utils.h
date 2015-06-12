/**
 * painter utils header
 *
 * Copyright 2015 Jay Sorg <jay.sorg@gmail.com>
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

#define SPLIT_a1r5g5b5(c, a, r, g, b) \
do { \
    a = (c & 0x8000) ? 0xff : 0; \
    r = ((c >> 7) & 0xf8) | ((c >> 12) & 0x7); \
    g = ((c >> 2) & 0xf8) | ((c >>  8) & 0x7); \
    b = ((c << 3) & 0xf8) | ((c >>  2) & 0x7); \
} while (0)

#define SPLIT_r5g6b5(c, a, r, g, b) \
do { \
    a = 0xff; \
    r = ((c >> 8) & 0xf8) | ((c >> 13) & 0x7); \
    g = ((c >> 3) & 0xfc) | ((c >>  9) & 0x3); \
    b = ((c << 3) & 0xf8) | ((c >>  2) & 0x7); \
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
    int fill_mode;
    int clip_valid;
    struct painter_rect clip;
    int origin_x;
    int origin_y;
    int palette[256];
};

int
painter_rop(int rop, int src, int dst);
int
painter_set_pixel(struct painter *painter, struct painter_bitmap *dst,
                  int x, int y, int pixel);

#endif
