/*
   rdesktop: A Remote Desktop Protocol client.
   Parsing primitives
   Copyright (C) Matthew Chapman 1999-2002

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
*/

/* modified for xrdp */
/* this is a super fast stream method, you bet */
/* all little endian for now */

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
};

#define s_check(s) (s->p <= s->end)

#define s_check_rem(s, n) (s->p + n <= s->end)

#define s_check_end(s) (s->p == s->end)

#define init_stream(s, v) \
{ \
  if (v > s->size) \
  { \
    g_free(s->data); \
    s->data = (char*)g_malloc(v, 0); \
  } \
  s->p = s->data; \
  s->end = s->data; \
  s->size = v; \
}

#define s_push_layer(s, h, n) \
{ \
  s->h = s->p; \
  s->p += n; \
}

#define s_pop_layer(s, h) \
{ \
  s->p = s->h; \
}

#define s_mark_end(s) \
{ \
  s->end = s->p; \
}

#define in_uint8(s, v) \
{ \
  v = *((unsigned char*)(s->p)); \
  s->p++; \
}

#define in_uint16_le(s, v) \
{ \
  v = *((unsigned short*)(s->p)); \
  s->p += 2; \
}

#define in_uint16_be(s, v) \
{ \
  v = *((unsigned char*)(s->p)); \
  s->p++; \
  v = v << 8; \
  v = v | *((unsigned char*)(s->p)); \
  s->p++; \
}

#define in_uint32_le(s, v) \
{ \
  v = *(unsigned long*)(s->p); \
  s->p += 4; \
}

#define in_uint32_be(s, v) \
{ \
  v = *((unsigned char*)(s->p)); \
  s->p++; \
  v = v << 8; \
  v = v | *((unsigned char*)(s->p)); \
  s->p++; \
  v = v << 8; \
  v = v | *((unsigned char*)(s->p)); \
  s->p++; \
  v = v << 8; \
  v = v | *((unsigned char*)(s->p)); \
  s->p++; \
}

#define out_uint8(s, v) \
{ \
  *((unsigned char*)(s->p)) = (unsigned char)(v); \
  s->p++; \
}

#define out_uint16_le(s, v) \
{ \
  *((unsigned short*)(s->p)) = (unsigned short)(v); \
  s->p += 2; \
}

#define out_uint16_be(s, v) \
{ \
  *((unsigned char*)(s->p)) = (unsigned char)((v) >> 8); \
  s->p++; \
  *((unsigned char*)(s->p)) = (unsigned char)(v); \
  s->p++; \
}

#define out_uint32_le(s, v) \
{ \
  *((unsigned long*)(s->p)) = (v); \
  s->p += 4; \
}

#define out_uint32_be(s, v) \
{ \
  *((unsigned char*)(s->p)) = (unsigned char)((v) >> 24); \
  s->p++; \
  *((unsigned char*)(s->p)) = (unsigned char)((v) >> 16); \
  s->p++; \
  *((unsigned char*)(s->p)) = (unsigned char)((v) >> 8); \
  s->p++; \
  *((unsigned char*)(s->p)) = (unsigned char)(v); \
  s->p++; \
}

#define in_uint8p(s, v, n) \
{ \
  v = s->p; \
  s->p += n; \
}

#define in_uint8a(s, v, n) \
{ \
  g_memcpy(v, s->p, n); \
  s->p += n; \
}

#define in_uint8s(s, n) \
{ \
  s->p += n; \
}

#define out_uint8p(s, v, n) \
{ \
  g_memcpy(s->p, v, n); \
  s->p += n; \
}

#define out_uint8a(s, v, n) \
{ \
  out_uint8p(s, v, n); \
}

#define out_uint8s(s, n) \
{ \
  g_memset(s->p, 0, n); \
  s->p += n; \
}
