/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2007

   main define/macro file

*/

#if !defined(DEFINES_H)
#define DEFINES_H

/* check for debug */
#ifdef XRDP_DEBUG
#define DEBUG(args) g_writeln args;
#define LIB_DEBUG(_mod, _text) _mod->server_msg(_mod, _text, 1);
#else
#define DEBUG(args)
#define LIB_DEBUG(_mod, _text)
#endif
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

#endif
