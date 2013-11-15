/*
Copyright 2005-2013 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

the rest

*/

#ifndef __RDPMISC_H
#define __RDPMISC_H

#include <X11/Xos.h>

int
rdpBitsPerPixel(int depth);
int
g_sck_recv(int sck, void *ptr, int len, int flags);
void
g_sck_close(int sck);
int
g_sck_last_error_would_block(int sck);
void
g_sleep(int msecs);
int
g_sck_send(int sck, void *ptr, int len, int flags);
void *
g_malloc(int size, int zero);
void
g_free(void *ptr);
void
g_sprintf(char *dest, char *format, ...);
int
g_sck_tcp_socket(void);
int
g_sck_local_socket_dgram(void);
int
g_sck_local_socket_stream(void);
void
g_memcpy(void *d_ptr, const void *s_ptr, int size);
void
g_memset(void *d_ptr, const unsigned char chr, int size);
int
g_sck_tcp_set_no_delay(int sck);
int
g_sck_set_non_blocking(int sck);
int
g_sck_accept(int sck);
int
g_sck_select(int sck1, int sck2, int sck3);
int
g_sck_tcp_bind(int sck, char *port);
int
g_sck_local_bind(int sck, char *port);
int
g_sck_listen(int sck);
int
g_create_dir(const char *dirname);
int
g_directory_exist(const char *dirname);
int
g_chmod_hex(const char *filename, int flags);
void
g_hexdump(unsigned char *p, unsigned int len);

#if defined(X_BYTE_ORDER)
#  if X_BYTE_ORDER == X_LITTLE_ENDIAN
#    define L_ENDIAN
#  else
#    define B_ENDIAN
#  endif
#else
#  error Unknown endianness in rdp.h
#endif
/* check if we need to align data */
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__) || defined(__ppc__) || defined(__arm__)
#define NEED_ALIGN
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
#define s_mark_end(s) \
{ \
  (s)->end = (s)->p; \
}

/******************************************************************************/
#define make_stream(s) \
{ \
  (s) = (struct stream*)g_malloc(sizeof(struct stream), 1); \
}

/******************************************************************************/
#define free_stream(s) do \
{ \
  if ((s) != 0) \
  { \
    g_free((s)->data); \
  } \
  g_free((s)); \
} while (0)

#endif
