/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   Parsing structs and macros

   based on parse.h from rdesktop
   this is a super fast stream method, you bet
   needed functions g_malloc, g_free, g_memset, g_memcpy
*/

#if !defined(PARSE_H)
#define PARSE_H

#include "arch.h"

#if defined(L_ENDIAN)
#elif defined(B_ENDIAN)
#else
#error Unknown endianness.
#endif

/* parser state */
struct stream
{
  char* p;
  char* end;
  char* data;
  int size;
  /* offsets of various headers */
  char* iso_hdr;
  char* mcs_hdr;
  char* sec_hdr;
  char* rdp_hdr;
  char* channel_hdr;
  char* next_packet;
};

/******************************************************************************/
#define s_check(s) ((s)->p <= (s)->end)

/******************************************************************************/
#define s_check_rem(s, n) ((s)->p + (n) <= (s)->end)

/******************************************************************************/
#define s_check_end(s) ((s)->p == (s)->end)

/******************************************************************************/
#define make_stream(s) \
{ \
  (s) = (struct stream*)g_malloc(sizeof(struct stream), 1); \
}

/******************************************************************************/
#define init_stream(s, v) \
{ \
  if ((v) > (s)->size) \
  { \
    g_free((s)->data); \
    (s)->data = (char*)g_malloc((v), 0); \
    (s)->size = (v); \
  } \
  (s)->p = (s)->data; \
  (s)->end = (s)->data; \
  (s)->next_packet = 0; \
}

/******************************************************************************/
#define free_stream(s) \
{ \
  if ((s) != 0) \
  { \
    g_free((s)->data); \
  } \
  g_free((s)); \
} \

/******************************************************************************/
#define s_push_layer(s, h, n) \
{ \
  (s)->h = (s)->p; \
  (s)->p += (n); \
}

/******************************************************************************/
#define s_pop_layer(s, h) \
{ \
  (s)->p = (s)->h; \
}

/******************************************************************************/
#define s_mark_end(s) \
{ \
  (s)->end = (s)->p; \
}

/******************************************************************************/
#define in_sint8(s, v) \
{ \
  (v) = *((signed char*)((s)->p)); \
  (s)->p++; \
}

/******************************************************************************/
#define in_uint8(s, v) \
{ \
  (v) = *((unsigned char*)((s)->p)); \
  (s)->p++; \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_sint16_le(s, v) \
{ \
  (v) = (signed short) \
    ( \
      (*((unsigned char*)((s)->p + 0)) << 0) | \
      (*((unsigned char*)((s)->p + 1)) << 8) \
    ); \
  (s)->p += 2; \
}
#else
#define in_sint16_le(s, v) \
{ \
  (v) = *((signed short*)((s)->p)); \
  (s)->p += 2; \
}
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint16_le(s, v) \
{ \
  (v) = (unsigned short) \
    ( \
      (*((unsigned char*)((s)->p + 0)) << 0) | \
      (*((unsigned char*)((s)->p + 1)) << 8) \
    ); \
  (s)->p += 2; \
}
#else
#define in_uint16_le(s, v) \
{ \
  (v) = *((unsigned short*)((s)->p)); \
  (s)->p += 2; \
}
#endif

/******************************************************************************/
#define in_uint16_be(s, v) \
{ \
  (v) = *((unsigned char*)((s)->p)); \
  (s)->p++; \
  (v) <<= 8; \
  (v) |= *((unsigned char*)((s)->p)); \
  (s)->p++; \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint32_le(s, v) \
{ \
  (v) = (unsigned int) \
    ( \
      (*((unsigned char*)((s)->p + 0)) << 0) | \
      (*((unsigned char*)((s)->p + 1)) << 8) | \
      (*((unsigned char*)((s)->p + 2)) << 16) | \
      (*((unsigned char*)((s)->p + 3)) << 24) \
    ); \
  (s)->p += 4; \
}
#else
#define in_uint32_le(s, v) \
{ \
  (v) = *((unsigned int*)((s)->p)); \
  (s)->p += 4; \
}
#endif

/******************************************************************************/
#define in_uint32_be(s, v) \
{ \
  (v) = *((unsigned char*)((s)->p)); \
  (s)->p++; \
  (v) <<= 8; \
  (v) |= *((unsigned char*)((s)->p)); \
  (s)->p++; \
  (v) <<= 8; \
  (v) |= *((unsigned char*)((s)->p)); \
  (s)->p++; \
  (v) <<= 8; \
  (v) |= *((unsigned char*)((s)->p)); \
  (s)->p++; \
}

/******************************************************************************/
#define out_uint8(s, v) \
{ \
  *((s)->p) = (unsigned char)(v); \
  (s)->p++; \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint16_le(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 0); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 8); \
  (s)->p++; \
}
#else
#define out_uint16_le(s, v) \
{ \
  *((unsigned short*)((s)->p)) = (unsigned short)(v); \
  (s)->p += 2; \
}
#endif

/******************************************************************************/
#define out_uint16_be(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 8); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 0); \
  (s)->p++; \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint32_le(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 0); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 8); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 16); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 24); \
  (s)->p++; \
}
#else
#define out_uint32_le(s, v) \
{ \
  *((unsigned int*)((s)->p)) = (v); \
  (s)->p += 4; \
}
#endif

/******************************************************************************/
#define out_uint32_be(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 24); \
  s->p++; \
  *((s)->p) = (unsigned char)((v) >> 16); \
  s->p++; \
  *((s)->p) = (unsigned char)((v) >> 8); \
  s->p++; \
  *((s)->p) = (unsigned char)(v); \
  (s)->p++; \
}

/******************************************************************************/
#define in_uint8p(s, v, n) \
{ \
  (v) = (s)->p; \
  (s)->p += (n); \
}

/******************************************************************************/
#define in_uint8a(s, v, n) \
{ \
  g_memcpy((v), (s)->p, (n)); \
  (s)->p += (n); \
}

/******************************************************************************/
#define in_uint8s(s, n) \
{ \
  (s)->p += (n); \
}

/******************************************************************************/
#define out_uint8p(s, v, n) \
{ \
  g_memcpy((s)->p, (v), (n)); \
  (s)->p += (n); \
}

/******************************************************************************/
#define out_uint8a(s, v, n) \
{ \
  out_uint8p((s), (v), (n)); \
}

/******************************************************************************/
#define out_uint8s(s, n) \
{ \
  g_memset((s)->p, 0, (n)); \
  (s)->p += (n); \
}

#endif
