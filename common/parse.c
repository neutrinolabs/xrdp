/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2021 Matt Burt
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
 * Enforce stream primitive checking
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>

#include "arch.h"
#include "parse.h"
#include "log.h"
#include "string_calls.h"
#include "unicode_defines.h"

/******************************************************************************/

#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint16_le_unchecked(s, v) do \
    { \
        *((s)->p) = (unsigned char)((v) >> 0); \
        (s)->p++; \
        *((s)->p) = (unsigned char)((v) >> 8); \
        (s)->p++; \
    } while (0)
#else
#define out_uint16_le_unchecked(s, v) do \
    { \
        *((unsigned short*)((s)->p)) = (unsigned short)(v); \
        (s)->p += 2; \
    } while (0)
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint16_le_unchecked(s, v) do \
    { \
        (v) = (unsigned short) \
              ( \
                (*((unsigned char*)((s)->p + 0)) << 0) | \
                (*((unsigned char*)((s)->p + 1)) << 8) \
              ); \
        (s)->p += 2; \
    } while (0)
#else
#define in_uint16_le_unchecked(s, v) do \
    { \
        (v) = *((unsigned short*)((s)->p)); \
        (s)->p += 2; \
    } while (0)
#endif

/******************************************************************************/
void
parser_stream_overflow_check(const struct stream *s, int n, int is_out,
                             const char *file, int line)
{
    /* Sanity checks */
    if (n < 0)
    {
        LOG(LOG_LEVEL_ALWAYS, "%s:%d "
            "stream primitive called with negative n=%d",
            file, line, n);
        abort();
    }

    if (is_out)
    {
        /* Output overflow */
        if (!s_check_rem_out(s, n))
        {
            LOG(LOG_LEVEL_ALWAYS, "%s:%d Stream output buffer overflow. "
                "Size=%d, pos=%d, requested=%d", file, line,
                s->size, (int)(s->p - s->data), n);
            abort();
        }
    }
    else
    {
        /* Input overflow */
        if (!s_check_rem(s, n))
        {
            LOG(LOG_LEVEL_ALWAYS, "%s:%d Stream input buffer overflow. "
                "Max=%d, pos=%d, requested=%d", file, line,
                (int)(s->end - s->data), (int)(s->p - s->data), n);
            abort();
        }
    }
}

/******************************************************************************/
void
out_utf8_as_utf16_le_proc(struct stream *s, const char *v,
                          unsigned int vn,
                          const char *file, int line)
{
    // Expansion of S_CHECK_REM_OUT(s, <octet_count>) using passed-in
    // file and line
#ifdef USE_DEVEL_STREAMCHECK
    int octet_cnt = utf8_as_utf16_word_count(v, vn) * 2;
    parser_stream_overflow_check(s, octet_cnt, 1, file, line);
#endif

    while (vn > 0)
    {
        char32_t c32 = utf8_get_next_char(&v, &vn);
        char16_t low;
        if (c32 < 0x10000)
        {
            low = (char16_t)c32;
        }
        else
        {
            /* Need a surrogate pair */
            low = LOW_SURROGATE_FROM_C32(c32);
            char16_t high = HIGH_SURROGATE_FROM_C32(c32);
            out_uint16_le_unchecked(s, high);
        }
        out_uint16_le_unchecked(s, low);
    }
}

/******************************************************************************/
/**
 * Gets the next Unicode character from a code stream
 * @param s Stream
 * @return Unicode character
 *
 * Non-characters and illegally coded characters are mapped to
 * UCS_REPLACEMENT_CHARACTER
 *
 * @pre Two bytes are assumed to be available on the stram on entry
 */
static char32_t
get_c32_from_stream(struct stream *s)
{
    char32_t c32 = UCS_REPLACEMENT_CHARACTER; // Assume failure
    char16_t w;

    in_uint16_le_unchecked(s, w);

    if (IS_HIGH_SURROGATE(w))
    {
        if (s_check_rem(s, 2))
        {
            char16_t low;
            in_uint16_le_unchecked(s, low);
            if (IS_LOW_SURROGATE(low))
            {
                /* Valid surrogate pair */
                char32_t v = C32_FROM_SURROGATE_PAIR(low, w);

                /* Ignore some values which can be successfully encoded
                 * in this way */
                if (!IS_PLANE_END_NON_CHARACTER(c32))
                {
                    c32 = v;
                }
            }
            else
            {
                /* Invalid low surrogate  - pop character back */
                s->p -= 2;
            }
        }
    }
    else if (!IS_LOW_SURROGATE(w) &&
             !IS_PLANE_END_NON_CHARACTER(w) &&
             !IS_ARABIC_NON_CHARACTER(w))
    {
        /* Character from the Basic Multilingual Plane */
        c32 = (char32_t)w;
    }

    return c32;
}

/******************************************************************************/
unsigned int
in_utf16_le_fixed_as_utf8_proc(struct stream *s, unsigned int n,
                               char *v, unsigned int vn,
                               const char *file, int line)
{
    unsigned int rv = 0;
    char32_t c32;
    char u8str[MAXLEN_UTF8_CHAR];
    unsigned int u8len;
    char *saved_s_end = s->end;

    // Expansion of S_CHECK_REM(s, n*2) using passed-in file and line
#ifdef USE_DEVEL_STREAMCHECK
    parser_stream_overflow_check(s, n * 2, 0, file, line);
#endif
    // Temporarily set the stream end pointer to allow us to use
    // s_check_rem() when reading in UTF-16 words
    if (s->end - s->p > (int)(n * 2))
    {
        s->end = s->p + (int)(n * 2);
    }

    while (s_check_rem(s, 2))
    {
        c32 = get_c32_from_stream(s);

        u8len = utf_char32_to_utf8(c32, u8str);
        if (u8len + 1 <= vn)
        {
            /* Room for this character and a terminator. Add the character */
            unsigned int i;
            for (i = 0 ; i < u8len ; ++i)
            {
                v[i] = u8str[i];
            }
            vn -= u8len;
            v += u8len;
        }
        else if (vn > 1)
        {
            /* We've skipped a character, but there's more than one byte
             * remaining in the output buffer. Mark the output buffer as
             * full so we don't get a smaller character being squeezed into
             * the remaining space */
            vn = 1;
        }

        rv += u8len;
    }

    // Restore stream to full length
    s->end = saved_s_end;

    if (vn > 0)
    {
        *v = '\0';
    }
    ++rv;
    return rv;
}

/******************************************************************************/
unsigned int
in_utf16_le_fixed_as_utf8_length(struct stream *s, unsigned int n)
{
    char *saved_s_p = s->p;
    unsigned int rv = in_utf16_le_fixed_as_utf8(s, n, NULL, 0);
    s->p = saved_s_p;
    return rv;
}

/******************************************************************************/
unsigned int
in_utf16_le_terminated_as_utf8(struct stream *s,
                               char *v, unsigned int vn)
{
    unsigned int rv = 0;
    char32_t c32;
    char u8str[MAXLEN_UTF8_CHAR];
    unsigned int u8len;
    while (s_check_rem(s, 2))
    {
        c32 = get_c32_from_stream(s);
        if (c32 == 0)
        {
            break;  // Terminator encountered
        }

        u8len = utf_char32_to_utf8(c32, u8str);
        if (u8len + 1 <= vn)
        {
            /* Room for this character and a terminator. Add the character */
            unsigned int i;
            for (i = 0 ; i < u8len ; ++i)
            {
                v[i] = u8str[i];
            }
            vn -= u8len;
            v += u8len;
        }
        else if (vn > 1)
        {
            /* We've skipped a character, but there's more than one byte
             * remaining in the output buffer. Mark the output buffer as
             * full so we don't get a smaller character being squeezed into
             * the remaining space */
            vn = 1;
        }
        rv += u8len;
    }

    if (vn > 0)
    {
        *v = '\0';
    }
    ++rv;

    return rv;
}

/******************************************************************************/
unsigned int
in_utf16_le_terminated_as_utf8_length(struct stream *s)
{
    char *saved_s_p = s->p;
    unsigned int rv = in_utf16_le_terminated_as_utf8(s, NULL, 0);
    s->p = saved_s_p;
    return rv;
}
