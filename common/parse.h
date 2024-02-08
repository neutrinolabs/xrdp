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
 * Parsing structs and macros
 *
 * based on parse.h from rdesktop
 * this is a super fast stream method, you bet
 * needed functions g_malloc, g_free, g_memset, g_memcpy
 */

#if !defined(PARSE_H)
#define PARSE_H

#include "arch.h"
#include "log.h"

/* Check the config_ac.h file is included so we know whether to enable the
 * development macros
 */
#ifndef CONFIG_AC_H
#   error config_ac.h not visible in parse.h
#endif

#if defined(L_ENDIAN)
#elif defined(B_ENDIAN)
#else
#error Unknown endianness.
#endif

/* parser state */
struct stream
{
    char *p;
    char *end;
    char *data;
    int size;
    int pad0;
    /* offsets of various headers */
    char *iso_hdr;
    char *mcs_hdr;
    char *sec_hdr;
    char *rdp_hdr;
    char *channel_hdr;
    /* other */
    char *next_packet;
    struct stream *next;
    int *source;
};

/** Check arguments to stream primitives
 *
 * This adds a function call overhead to every stream primitive and is
 * intended for development only
 *
 * @param s stream
 * @param n Bytes being requested for input/output
 * @param is_out (0=input, !0=output)
 * @param file __file__for caller
 * @param line __line__ for caller
 *
 * On any kind of violation a message is output and the program is
 * aborted.
 */
void
parser_stream_overflow_check(const struct stream *s, int n, int is_out,
                             const char *file, int line);

#ifdef USE_DEVEL_STREAMCHECK
#   define S_CHECK_REM(s,n) \
    parser_stream_overflow_check((s), (n), 0, __FILE__, __LINE__)
#   define S_CHECK_REM_OUT(s,n) \
    parser_stream_overflow_check((s), (n), 1, __FILE__, __LINE__)
#else
#   define S_CHECK_REM(s,n)
#   define S_CHECK_REM_OUT(s,n)
#endif

/******************************************************************************/
/**
 * Copies a UTF-8 string to a stream as little-endian UTF-16
 *
 * @param s Stream
 * @param v UTF-8 string
 * @param vn Length of UTF-8 string.
 * @param file Caller location (from __FILE__)
 * @param line Caller location (from __LINE__)
 *
 * Caller is expected to check there is room for the result in s
 */
void
out_utf8_as_utf16_le_proc(struct stream *s, const char *v,
                          unsigned int vn,
                          const char *file, int line);

#define out_utf8_as_utf16_le(s,v,vn) \
    out_utf8_as_utf16_le_proc((s), (v), (vn), __FILE__, __LINE__)


/******************************************************************************/
/**
 * Copies a fixed-size little-endian UTF-16 string from a stream as UTF-8
 *
 * @param s Stream
 * @param n Number of 16-bit words to copy
 * @param v Pointer to result buffer
 * @param vn Max size of result buffer
 *
 * @return number of characters which would be written to v, INCLUDING
 *         an additional terminator. This can be used to check for a buffer
 *         overflow. A terminator is added whether or not the input
 *         includes one.
 *
 * Output is unconditionally NULL-terminated.
 * Input is not checked for NULLs - these are copied verbatim
 */
unsigned int
in_utf16_le_fixed_as_utf8_proc(struct stream *s, unsigned int n,
                               char *v, unsigned int vn,
                               const char *file, int line);

#define in_utf16_le_fixed_as_utf8(s,n,v,vn) \
    in_utf16_le_fixed_as_utf8_proc((s), (n), (v), (vn), __FILE__, __LINE__)

/******************************************************************************/
/**
 * Returns the size of the buffer needed to store a fixed-size
 * little-endian UTF-16 string in a stream as a UTF-8 string
 *
 * @param s Stream
 * @param n Number of 16-bit words to consider
 * @return number of characters needed to store the UTF-8 string. This
 *         includes a terminator, which is written whether the parsed
 *         string includes one or not.
 * @post Stream position is not moved between start and end of this call
 */
unsigned int
in_utf16_le_fixed_as_utf8_length(struct stream *s, unsigned int n);

/******************************************************************************/
/**
 * Copies a terminated little-endian UTF-16 string from a stream as UTF-8
 *
 * @param s Stream
 * @param v Pointer to result buffer
 * @param vn Max size of result buffer
 *
 * @return number of characters which would be written to v, INCLUDING
 *         the terminator. This can be used to check for a buffer overflow.
 *
 * Output is unconditionally NULL-terminated.
 * Input processing stops when a NULL is encountered, or the end of the buffer
 * is reached.
 */
unsigned int
in_utf16_le_terminated_as_utf8(struct stream *s,
                               char *v, unsigned int vn);

/******************************************************************************/
/**
 * Returns the size of the buffer needed to store a terminated
 * little-endian UTF-16 string in a stream as a terminated UTF-8 string
 *
 * @param s Stream
 * @return number of characters needed to store the UTF-8 string,
 *         including the terminator
 * @post Stream position is not moved between start and end of this call
 *
 * Input processing stops when a NULL is encountered, or the end of the buffer
 * is reached.
 */
unsigned int
in_utf16_le_terminated_as_utf8_length(struct stream *s);

/******************************************************************************/
#define s_check_rem(s, n) ((s)->p + (n) <= (s)->end)

/******************************************************************************/
/**
 * @returns true if there are at least n bytes remaining in the stream,
 *          else false and logs an error message
 */
#define s_check_rem_and_log(s, n, msg_prefix) \
    ( s_check_rem((s), (n)) ? \
      1 : \
      LOG(LOG_LEVEL_ERROR, \
          "%s Not enough bytes in the stream: expected %d, remaining %d", \
          (msg_prefix), (n), s_rem(s)) \
      && 0 )

/******************************************************************************/
#define s_check_rem_out(s, n) ((s)->p + (n) <= (s)->data + (s)->size)

/******************************************************************************/
/**
 * @returns true if there are at least n bytes remaining in the stream,
 *          else false and logs an error message
 */
#define s_check_rem_out_and_log(s, n, msg_prefix) \
    ( s_check_rem_out((s), (n)) ? \
      1 : \
      LOG(LOG_LEVEL_ERROR, \
          "%s Not enough bytes in the stream: expected %d, remaining %d", \
          (msg_prefix), (n), s_rem_out(s)) \
      && 0 )

/******************************************************************************/
#define s_check_end(s) ((s)->p == (s)->end)

/******************************************************************************/
/**
 * @returns true if there are exactly 0 bytes remaining in the stream,
 *          else false and logs an error message
 */
#define s_check_end_and_log(s, msg_prefix) \
    ( s_check_end((s)) ? \
      1 : \
      LOG(LOG_LEVEL_ERROR, \
          "%s Expected to be at the end of the stream, " \
          "but there are %d bytes remaining", \
          (msg_prefix), s_rem(s)) \
      && 0 )

/******************************************************************************/
#define s_rem(s) ((int) ((s)->end - (s)->p))

/******************************************************************************/
#define s_rem_out(s) ((int) ((s)->data + (s)->size - (s)->p))

/******************************************************************************/
#define make_stream(s) \
    (s) = (struct stream*)g_malloc(sizeof(struct stream), 1)

/******************************************************************************/
#define init_stream(s, v) do \
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
    } while (0)

/******************************************************************************/
#define free_stream(s) do \
    { \
        if ((s) != 0) \
        { \
            g_free((s)->data); \
        } \
        g_free((s)); \
    } while (0)

/******************************************************************************/
#define s_push_layer(s, h, n) do \
    { \
        (s)->h = (s)->p; \
        (s)->p += (n); \
    } while (0)

/******************************************************************************/
#define s_pop_layer(s, h) \
    (s)->p = (s)->h

/******************************************************************************/
#define s_mark_end(s) \
    (s)->end = (s)->p

#define in_sint8(s, v) do \
    { \
        S_CHECK_REM((s), 1); \
        (v) = *((signed char*)((s)->p)); \
        (s)->p++; \
    } while (0)

/******************************************************************************/
#define in_uint8(s, v) do \
    { \
        S_CHECK_REM((s), 1); \
        (v) = *((unsigned char*)((s)->p)); \
        (s)->p++; \
    } while (0)
/******************************************************************************/
#define in_uint8_peek(s, v) do \
    { \
        S_CHECK_REM((s), 1); \
        v = *s->p; \
    } while (0)
/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_sint16_le(s, v) do \
    { \
        S_CHECK_REM((s), 2); \
        (v) = (signed short) \
              ( \
                (*((unsigned char*)((s)->p + 0)) << 0) | \
                (*((unsigned char*)((s)->p + 1)) << 8) \
              ); \
        (s)->p += 2; \
    } while (0)
#else
#define in_sint16_le(s, v) do \
    { \
        S_CHECK_REM((s), 2); \
        (v) = *((signed short*)((s)->p)); \
        (s)->p += 2; \
    } while (0)
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint16_le(s, v) do \
    { \
        S_CHECK_REM((s), 2); \
        (v) = (unsigned short) \
              ( \
                (*((unsigned char*)((s)->p + 0)) << 0) | \
                (*((unsigned char*)((s)->p + 1)) << 8) \
              ); \
        (s)->p += 2; \
    } while (0)
#else
#define in_uint16_le(s, v) do \
    { \
        S_CHECK_REM((s), 2); \
        (v) = *((unsigned short*)((s)->p)); \
        (s)->p += 2; \
    } while (0)
#endif

/******************************************************************************/
#define in_uint16_be(s, v) do \
    { \
        S_CHECK_REM((s), 2); \
        (v) = *((unsigned char*)((s)->p)); \
        (s)->p++; \
        (v) <<= 8; \
        (v) |= *((unsigned char*)((s)->p)); \
        (s)->p++; \
    } while (0)

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint32_le(s, v) do \
    { \
        S_CHECK_REM((s), 4); \
        (v) = (unsigned int) \
              ( \
                (*((unsigned char*)((s)->p + 0)) << 0) | \
                (*((unsigned char*)((s)->p + 1)) << 8) | \
                (*((unsigned char*)((s)->p + 2)) << 16) | \
                (*((unsigned char*)((s)->p + 3)) << 24) \
              ); \
        (s)->p += 4; \
    } while (0)
#else
#define in_uint32_le(s, v) do \
    { \
        S_CHECK_REM((s), 4); \
        (v) = *((unsigned int*)((s)->p)); \
        (s)->p += 4; \
    } while (0)
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint64_le(s, v) do \
    { \
        S_CHECK_REM((s), 8); \
        (v) = (tui64) \
              ( \
                (((tui64)(*((unsigned char*)((s)->p + 0)))) << 0) | \
                (((tui64)(*((unsigned char*)((s)->p + 1)))) << 8) | \
                (((tui64)(*((unsigned char*)((s)->p + 2)))) << 16) | \
                (((tui64)(*((unsigned char*)((s)->p + 3)))) << 24) | \
                (((tui64)(*((unsigned char*)((s)->p + 4)))) << 32) | \
                (((tui64)(*((unsigned char*)((s)->p + 5)))) << 40) | \
                (((tui64)(*((unsigned char*)((s)->p + 6)))) << 48) | \
                (((tui64)(*((unsigned char*)((s)->p + 7)))) << 56) \
              ); \
        (s)->p += 8; \
    } while (0)
#else
#define in_uint64_le(s, v) do \
    { \
        S_CHECK_REM((s), 8); \
        (v) = *((tui64*)((s)->p)); \
        (s)->p += 8; \
    } while (0)
#endif

/******************************************************************************/
#define in_uint32_be(s, v) do \
    { \
        S_CHECK_REM((s), 4); \
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
    } while (0)

/******************************************************************************/
#define out_uint8(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 1); \
        *((s)->p) = (unsigned char)(v); \
        (s)->p++; \
    } while (0)

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint16_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 2); \
        *((s)->p) = (unsigned char)((v) >> 0); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 8); \
        (s)->p++; \
    } while (0)
#else
#define out_uint16_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 2); \
        *((unsigned short*)((s)->p)) = (unsigned short)(v); \
        (s)->p += 2; \
    } while (0)
#endif

/******************************************************************************/
#define out_uint16_be(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 2); \
        *((s)->p) = (unsigned char)((v) >> 8); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 0); \
        (s)->p++; \
    } while (0)

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint32_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 4); \
        *((s)->p) = (unsigned char)((v) >> 0); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 8); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 16); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 24); \
        (s)->p++; \
    } while (0)
#else
#define out_uint32_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 4); \
        *((unsigned int*)((s)->p)) = (v); \
        (s)->p += 4; \
    } while (0)
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint64_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 8); \
        *((s)->p) = (unsigned char)((v) >> 0); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 8); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 16); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 24); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 32); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 40); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 48); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 56); \
        (s)->p++; \
    } while (0)
#else
#define out_uint64_le(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 8); \
        *((tui64*)((s)->p)) = (v); \
        (s)->p += 8; \
    } while (0)
#endif

/******************************************************************************/
#define out_uint32_be(s, v) do \
    { \
        S_CHECK_REM_OUT((s), 4); \
        *((s)->p) = (unsigned char)((v) >> 24); \
        s->p++; \
        *((s)->p) = (unsigned char)((v) >> 16); \
        s->p++; \
        *((s)->p) = (unsigned char)((v) >> 8); \
        s->p++; \
        *((s)->p) = (unsigned char)(v); \
        (s)->p++; \
    } while (0)

/******************************************************************************/
#define in_uint8p(s, v, n) do \
    { \
        S_CHECK_REM((s), (n)); \
        (v) = (s)->p; \
        (s)->p += (n); \
    } while (0)

/******************************************************************************/
#define in_uint8a(s, v, n) do \
    { \
        S_CHECK_REM((s), (n)); \
        g_memcpy((v), (s)->p, (n)); \
        (s)->p += (n); \
    } while (0)

/******************************************************************************/
#define in_uint8s(s, n) do \
    { \
        S_CHECK_REM((s), (n)); \
        (s)->p += (n); \
    } while (0);

/******************************************************************************/
#define out_uint8p(s, v, n) do \
    { \
        S_CHECK_REM_OUT((s), (n)); \
        g_memcpy((s)->p, (v), (n)); \
        (s)->p += (n); \
    } while (0)

/******************************************************************************/
#define out_uint8a(s, v, n) \
    out_uint8p((s), (v), (n))

/******************************************************************************/
#define out_uint8s(s, n) do \
    { \
        S_CHECK_REM_OUT((s), (n)); \
        g_memset((s)->p, 0, (n)); \
        (s)->p += (n); \
    } while (0)

/*
 * @brief allocate a new stream
 *
 * @param _s opaque handle to the new stream
 * @param _l length of new stream
 ******************************************************************************/
#define xstream_new(_s, _l)   \
    do                           \
    {                            \
        make_stream((_s));       \
        init_stream((_s), (_l)); \
    } while (0)

/**
 * @brief release a previously allocated stream
 *
 * @param _s opaque handle returned by stream_new()
 *****************************************************************************/
#define xstream_free(_s) free_stream(_s)

#define xstream_skip_u8(_s, _n)       in_uint8s(_s, _n)

#define xstream_rd_u8(_s, _var)       in_uint8(_s, _var)
#define xstream_rd_u16_le(_s, _var)   in_uint16_le(_s, _var)
#define xstream_rd_u32_le(_s, _var)   in_uint32_le(_s, _var)

#define xstream_rd_s8_le(_s, _var)    in_sint8(_s, _var)
#define xstream_rd_s16_le(_s, _var)   in_sint16_le(_s, _var)
#define xstream_rd_s32_le(_s, _var)   TODO

#define xstream_wr_u8(_s, _var)       out_uint8(_s, _var)
#define xstream_wr_u16_le(_s, _var)   out_uint16_le(_s, _var)
#define xstream_wr_u32_le(_s, _var)   out_uint32_le(_s, _var)

#define xstream_wr_s8(_s, _var)       TODO
#define xstream_wr_s16_le(_s, _var)   TODO
#define xstream_wr_s32_le(_s, _var)   TODO

#define xstream_rd_u64_le(_s, _v)                             \
    do                                                            \
    {                                                             \
        _v =                                                      \
            (tui64)(*((unsigned char *)_s->p)) |                      \
            (((tui64) (*(((unsigned char *)_s->p) + 1))) << 8)  |     \
            (((tui64) (*(((unsigned char *)_s->p) + 2))) << 16) |     \
            (((tui64) (*(((unsigned char *)_s->p) + 3))) << 24) |     \
            (((tui64) (*(((unsigned char *)_s->p) + 4))) << 32) |     \
            (((tui64) (*(((unsigned char *)_s->p) + 5))) << 40) |     \
            (((tui64) (*(((unsigned char *)_s->p) + 6))) << 48) |     \
            (((tui64) (*(((unsigned char *)_s->p) + 7))) << 56);      \
        _s->p += 8;                                               \
    } while (0)

#define xstream_wr_u64_le(_s, _v)                                           \
    do                                                                          \
    {                                                                           \
        *(((unsigned char *) _s->p) + 0) = (unsigned char) ((_v >>  0) & 0xff); \
        *(((unsigned char *) _s->p) + 1) = (unsigned char) ((_v >>  8) & 0xff); \
        *(((unsigned char *) _s->p) + 2) = (unsigned char) ((_v >> 16) & 0xff); \
        *(((unsigned char *) _s->p) + 3) = (unsigned char) ((_v >> 24) & 0xff); \
        *(((unsigned char *) _s->p) + 4) = (unsigned char) ((_v >> 32) & 0xff); \
        *(((unsigned char *) _s->p) + 5) = (unsigned char) ((_v >> 40) & 0xff); \
        *(((unsigned char *) _s->p) + 6) = (unsigned char) ((_v >> 48) & 0xff); \
        *(((unsigned char *) _s->p) + 7) = (unsigned char) ((_v >> 56) & 0xff); \
        _s->p += 8;                                                             \
    } while (0)

/* copy data into stream */
#define xstream_copyin(_s, _dest, _len)   \
    do                                        \
    {                                         \
        g_memcpy((_s)->p, (_dest), (_len));   \
        (_s)->p += (_len);                    \
    } while (0)

/* copy data out of stream */
#define xstream_copyout(_dest, _s, _len)  \
    do                                        \
    {                                         \
        g_memcpy((_dest), (_s)->p, (_len));   \
        (_s)->p += (_len);                    \
    } while (0)

#define xstream_rd_string(_dest, _s, _len) \
    do                                         \
    {                                          \
        g_memcpy((_dest), (_s)->p, (_len));    \
        (_s)->p += (_len);                     \
    } while (0)

#define xstream_wr_string(_s, _src, _len) \
    do                                        \
    {                                         \
        g_memcpy((_s)->p, (_src), (_len));    \
        (_s)->p += (_len);                    \
    } while (0)

#define xstream_len(_s)               (int) ((_s)->p - (_s)->data)
#define xstream_seek(_s, _len)        (_s)->p += (_len)

#endif
