/**
 * painter main header
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

#if !defined(__PAINTER_H)
#define __PAINTER_H

#define LIBPAINTER_VERSION_MAJOR 0
#define LIBPAINTER_VERSION_MINOR 1
#define LIBPAINTER_VERSION_MICRO 0

#define PT_FORMAT_a8b8g8r8 \
((32 << 24) | (3 << 16) | (8 << 12) | (8 << 8) | (8 << 4) | 8)
#define PT_FORMAT_a8r8g8b8 \
((32 << 24) | (2 << 16) | (8 << 12) | (8 << 8) | (8 << 4) | 8)
#define PT_FORMAT_r5g6b5 \
((16 << 24) | (2 << 16) | (0 << 12) | (5 << 8) | (6 << 4) | 5)
#define PT_FORMAT_a1r5g5b5 \
((16 << 24) | (2 << 16) | (1 << 12) | (5 << 8) | (5 << 4) | 5)
#define PT_FORMAT_r3g3b2 \
((8 << 24) | (2 << 16) | (0 << 12) | (3 << 8) | (3 << 4) | 2)

#define PT_FORMAT_c1 \
((1 << 24) | (4 << 16) | (0 << 12) | (0 << 8) | (0 << 4) | 0)
#define PT_FORMAT_c8 \
((8 << 24) | (4 << 16) | (0 << 12) | (0 << 8) | (0 << 4) | 0)

struct painter_bitmap
{
    int format;
    int width;
    int stride_bytes;
    int height;
    char *data;
};

#define PT_ERROR_NONE        0
#define PT_ERROR_OUT_OF_MEM  1
#define PT_ERROR_PARAM       2
#define PT_ERROR_NOT_IMP     3

#define PT_PATTERN_MODE_NORMAL 0
#define PT_PATTERN_MODE_OPAQUE 1

#define PT_LINE_FLAGS_NONE 0

                         /* reverse  Windows     X11 */
                         /* polish */
#define PT_ROP_0    0x00 /* 0        BLACKNESS   GXclear        */
#define PT_ROP_DSon 0x11 /* DSon     NOTSRCERASE GXnor          */
#define PT_ROP_DSna 0x22 /* DSna                 GXandInverted  */
#define PT_ROP_Sn   0x33 /* Sn       NOTSRCCOPY  GXcopyInverted */
#define PT_ROP_SDna 0x44 /* SDna     SRCERASE    GXandReverse   */
#define PT_ROP_Dn   0x55 /* Dn       DSTINVERT   GXinvert       */
#define PT_ROP_DSx  0x66 /* DSx      SRCINVERT   GXxor          */
#define PT_ROP_DSan 0x77 /* DSan                 GXnand         */
#define PT_ROP_DSa  0x88 /* DSa      SRCAND      GXand          */
#define PT_ROP_DSxn 0x99 /* DSxn                 GXequiv        */
#define PT_ROP_D    0xAA /* D                    GXnoop         */
#define PT_ROP_DSno 0xBB /* DSno     MERGEPAINT  GXorInverted   */
#define PT_ROP_S    0xCC /* S        SRCCOPY     GXcopy         */
#define PT_ROP_SDno 0xDD /* SDno                 GXorReverse    */
#define PT_ROP_DSo  0xEE /* DSo                  GXor           */
#define PT_ROP_1    0xFF /* 1        WHITENESS   GXset          */

int
painter_create(void **handle);
int
painter_delete(void *handle);
int
painter_set_fgcolor(void *handle, int color);
int
painter_set_bgcolor(void *handle, int color);
int
painter_set_rop(void *handle, int rop);
int
painter_set_pattern_mode(void *handle, int mode);
int
painter_set_pattern_origin(void *handle, int x, int y);
int
painter_set_clip(void *handle, int x, int y, int cx, int cy);
int
painter_clear_clip(void *handle);
int
painter_fill_rect(void *handle, struct painter_bitmap *dst,
                  int x, int y, int cx, int cy);
int
painter_fill_pattern(void *handle, struct painter_bitmap *dst,
                     struct painter_bitmap *pat, int patx, int paty,
                     int x, int y, int cx, int cy);
int
painter_copy(void *handle, struct painter_bitmap *dst,
             int x, int y, int cx, int cy,
             struct painter_bitmap *src, int srcx, int srcy);
int
painter_line(void *handle, struct painter_bitmap *dst,
             int x1, int y1, int x2, int y2, int width, int flags);
int
painter_get_version(int *major, int *minor, int *micro);

#endif
