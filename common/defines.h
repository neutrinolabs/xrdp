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
 * main define/macro file
 */

#ifndef DEFINES_H
#define DEFINES_H

/* other macros */
#undef MIN
#define MIN(x1, x2) ((x1) < (x2) ? (x1) : (x2))
#undef MAX
#define MAX(x1, x2) ((x1) > (x2) ? (x1) : (x2))
#undef HIWORD
#define HIWORD(in) (((in) & 0xffff0000) >> 16)
#undef LOWORD
#define LOWORD(in) ((in) & 0x0000ffff)
#undef MAKELONG
#define MAKELONG(lo, hi) ((((hi) & 0xffff) << 16) | ((lo) & 0xffff))
#define UNUSED_VAR(x) ((void) (x))

/* graphics macros */
#define MAKERECT(r, x, y, cx, cy) \
    { (r).left = x; (r).top = y; (r).right = (x) + (cx); (r).bottom = (y) + (cy); }
#define ISRECTEMPTY(r) (((r).right <= (r).left) || ((r).bottom <= (r).top))
#define RECTOFFSET(r, dx, dy) \
    { (r).left += dx; (r).top += dy; (r).right += dx; (r).bottom += dy; }
#define GETPIXEL8(d, x, y, w) (*(((unsigned char*)d) + ((y) * (w) + (x))))
#define GETPIXEL16(d, x, y, w) (*(((unsigned short*)d) + ((y) * (w) + (x))))
#define GETPIXEL32(d, x, y, w) (*(((unsigned int*)d) + ((y) * (w) + (x))))
#define SETPIXEL8(d, x, y, w, v) \
    (*(((unsigned char*)d) + ((y) * (w) + (x))) = (v))
#define SETPIXEL16(d, x, y, w, v) \
    (*(((unsigned short*)d) + ((y) * (w) + (x))) = (v))
#define SETPIXEL32(d, x, y, w, v) \
    (*(((unsigned int*)d) + ((y) * (w) + (x))) = (v))
#define COLOR8(r, g, b) \
    ( \
      (((r) >> 5) << 0) | \
      (((g) >> 5) << 3) | \
      (((b) >> 6) << 6) \
    )
#define COLOR15(r, g, b) ((((r) >> 3) << 10) | (((g) >> 3) << 5) | ((b) >> 3))
#define COLOR16(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define COLOR24RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define COLOR24BGR(r, g, b) (((b) << 16) | ((g) << 8) | (r))
#define HRED(c) ((c & 0xff0000) >> 16)
#define HGREEN(c) ((c & 0x00ff00) >> 8)
#define HBLUE(c) ((c & 0x0000ff))
#define HCOLOR(bpp,c) \
    ( \
      (bpp==8?COLOR8(HRED(c),HGREEN(c),HBLUE(c)): \
       (bpp==15?COLOR15(HRED(c),HGREEN(c),HBLUE(c)): \
        (bpp==16?COLOR16(HRED(c),HGREEN(c),HBLUE(c)): \
         (bpp>=24?COLOR24BGR(HRED(c),HGREEN(c),HBLUE(c)):c) \
        ) \
       ) \
      ) \
    )
#define SPLITCOLOR15(r, g, b, c) \
    { \
        r = (((c) >> 7) & 0xf8) | (((c) >> 12) & 0x7); \
        g = (((c) >> 2) & 0xf8) | (((c) >>  8) & 0x7); \
        b = (((c) << 3) & 0xf8) | (((c) >>  2) & 0x7); \
    }
#define SPLITCOLOR16(r, g, b, c) \
    { \
        r = (((c) >> 8) & 0xf8) | (((c) >> 13) & 0x7); \
        g = (((c) >> 3) & 0xfc) | (((c) >>  9) & 0x3); \
        b = (((c) << 3) & 0xf8) | (((c) >>  2) & 0x7); \
    }
#define SPLITCOLOR32(r, g, b, c) \
    { \
        r = ((c) >> 16) & 0xff; \
        g = ((c) >> 8) & 0xff; \
        b = (c) & 0xff; \
    }
/* font macros */
#define FONT_DATASIZE(f) \
    ((((f)->height * (((f)->width + 7) / 8)) + 3) & ~3);
/* use crc for bitmap cache lookups */
#define USE_CRC

#define XR_RGB2BGR(a_ulColor) \
    (a_ulColor & 0xFF000000) | \
    ((a_ulColor & 0x00FF0000) >> 16) | \
    (a_ulColor & 0x0000FF00) |  \
    ((a_ulColor & 0x000000FF) << 16)

#endif
