/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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
 */

/**
 * @file    common/base64.c
 * @brief   Base64 encoder / decoder
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "string_calls.h"

#include "base64.h"

/*
 * Values for invalid and padding characters, used in the charmap
 * for converting base64 to binary
 *
 * These values are specially chosen to make it easy to detect padding or
 * invalid characters by or-ing together the values looked up in
 * a base64 quantum */
#define E_INVALID 0x40
#define E_PAD 0x80

/* Determine the character set on this platform */
#if ('a' == 0x61 && 'z' == 0x7a ) && \
    ('A' == 0x41 && 'Z' == 0x5a ) && \
    ('0' == 0x30 && '9' == 0x39 )
# define PLATFORM_IS_ASCII 1
#else
#   error "Unrecognised character set on this platform"
#endif /* character set check */


/*
 * Define a table to map the base64 character values to bit values.
 */
#ifdef PLATFORM_IS_ASCII
#define CHARMAP_BASE 0x28
#define E_IV E_INVALID  /* For table alignment */
const unsigned char charmap[] =
{
    /* 0x28 */ E_IV, E_IV, E_IV, 0x3e, E_IV, E_IV, E_IV, 0x3f,
    /* 0x30 */ 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    /* 0x38 */ 0x3c, 0x3d, E_IV, E_IV, E_IV, E_PAD, E_IV, E_IV,
    /* 0x40 */ E_IV, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    /* 0x48 */ 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    /* 0x50 */ 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    /* 0x58 */ 0x17, 0x18, 0x19, E_IV, E_IV, E_IV, E_IV, E_IV,
    /* 0x60 */ E_IV, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    /* 0x68 */ 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    /* 0x70 */ 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    /* 0x78 */ 0x31, 0x32, 0x33
};
#undef E_IV
#endif /* PLATFORM_IS_ASCII */


/**
 * Lookup a value in the charmap
 *
 * @param x - byte to lookup. Only referenced once so can safely have
 *            side effects.
 * @param dest - destination to assign result to.
 */
#define CM_LOOKUP(x,dest) \
    { \
        unsigned int t = (unsigned int)(x) - CHARMAP_BASE;\
        dest = (t < sizeof(charmap)) ? charmap[t] : E_INVALID; \
    }

/*****************************************************************************/

int
base64_decode(const char *src, char *dst, size_t dst_len, size_t *actual_len)
{
    *actual_len = 0;
    size_t src_len;
    size_t src_i = 0;
    size_t dst_i = 0;
    unsigned int a;  /* Four characters of base64 quantum */
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int v;

#define OUTPUT_CHAR(x) \
    { \
        if (dst_i < dst_len) \
        { \
            dst[dst_i] = (x);\
        } \
        ++dst_i; \
    }

    src_len = g_strlen(src);

    while (src_i < src_len)
    {
        if ((src_len - src_i) >= 4)
        {
            /* Usual case  - full quantum */
            CM_LOOKUP(src[src_i++], a);
            CM_LOOKUP(src[src_i++], b);
            CM_LOOKUP(src[src_i++], c);
            CM_LOOKUP(src[src_i++], d);
        }
        else
        {
            /* Add padding on the end to make up the full quantum */
            CM_LOOKUP(src[src_i++], a);
            b = E_PAD;
            c = E_PAD;
            d = E_PAD;
            if ((src_len - src_i) > 0)
            {
                CM_LOOKUP(src[src_i++], b);
            }
            if ((src_len - src_i) > 0)
            {
                CM_LOOKUP(src[src_i++], c);
            }
        }

        /*
         * Bitwise-or the translated quantum values together, so that
         * any invalid or padding characters can be detected with a
         * single test */
        v = a | b | c | d;

        if ((v & E_INVALID) != 0)
        {
            return -1;  /* At least one invalid character */
        }

        if ((v & E_PAD) == 0)
        {
            /* No padding - a full quantum */
            v = (a << 18) | (b << 12) | (c << 6) | d;
            OUTPUT_CHAR(v >> 16);
            OUTPUT_CHAR((v >> 8) & 0xff);
            OUTPUT_CHAR(v & 0xff);
        }
        else if (((a | b | c) & E_PAD) == 0)
        {
            /* No padding in the first 3 chars, so the padding must
             * be at the end */
            v = (a << 10) | (b << 4) | (c >> 2);
            OUTPUT_CHAR(v >> 8);
            OUTPUT_CHAR(v & 0xff);
        }
        else if (((a | b) & E_PAD) == 0 && c == d)
        {
            /* No padding in first two chars, so if the last two chars are
             * equal, they must both be padding */
            v = (a << 2) | (b >> 4);
            OUTPUT_CHAR(v);
        }
        else
        {
            /* Illegal padding */
            return -1;
        }
    }

    *actual_len = dst_i;
    return 0;

#undef OUTPUT_CHAR
}

/*****************************************************************************/
size_t
base64_encode(const char *src, size_t src_len, char *dst, size_t dst_len)
{
    char *p = dst;
    size_t src_i = 0;
    size_t max_src_len;
    unsigned int v;
    static const char *b64chr =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/=";

    /* Each three octets of the source results in four bytes at the output,
     * plus we need a terminator. So we can work out the maximum number of
     * source octets we can process */
    if (dst_len == 0)
    {
        max_src_len = 0;
    }
    else
    {
        max_src_len = (dst_len - 1) / 4 * 3;
    }

    if (src_len > max_src_len)
    {
        src_len = max_src_len;
    }

    while (src_i < src_len)
    {
        switch (src_len - src_i)
        {
            case 1:
                v = (unsigned int)(unsigned char)src[src_i++] << 4;
                *p++ = b64chr[v >> 6];
                *p++ = b64chr[v & 0x3f];
                *p++ = b64chr[64];
                *p++ = b64chr[64];
                break;

            case 2:
                v = (unsigned int)(unsigned char)src[src_i++] << 10;
                v |= (unsigned int)(unsigned char)src[src_i++] << 2;
                *p++ = b64chr[v >> 12];
                *p++ = b64chr[(v >> 6) & 0x3f];
                *p++ = b64chr[v & 0x3f];
                *p++ = b64chr[64];
                break;

            default:
                v = (unsigned int)(unsigned char)src[src_i++] << 16;
                v |= (unsigned int)(unsigned char)src[src_i++] << 8;
                v |= (unsigned int)(unsigned char)src[src_i++];
                *p++ = b64chr[v >> 18];
                *p++ = b64chr[(v >> 12) & 0x3f];
                *p++ = b64chr[(v >> 6) & 0x3f];
                *p++ = b64chr[v & 0x3f];
                break;
        }
    }

    *p = '\0';

    return src_len;
}
