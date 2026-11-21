/**
 * xrdp: A Remote Desktop Protocol server.
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

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#include "bench_openh264_ref.h"

/* Keep the original loop and the first SSE2 implementation fixed, in a
 * separate translation unit from the benchmark driver. Compile these with
 * the same optimization flags as the production implementation. */
void
bench_openh264_scalar(const char *src, char *dst_u, char *dst_v, int width)
{
    for (; width > 0; width -= 2)
    {
        *(dst_u++) = *(src++);
        *(dst_v++) = *(src++);
    }
}

void
bench_openh264_previous(const char *src, char *dst_u, char *dst_v, int width)
{
#if defined(__SSE2__)
    const __m128i mask = _mm_set1_epi16(0x00ff);
    __m128i uv0;
    __m128i uv1;
    __m128i u;
    __m128i v;

    for (; width >= 32; width -= 32)
    {
        uv0 = _mm_loadu_si128((const __m128i *) src);
        uv1 = _mm_loadu_si128((const __m128i *) (src + 16));
        u = _mm_packus_epi16(_mm_and_si128(uv0, mask),
                             _mm_and_si128(uv1, mask));
        v = _mm_packus_epi16(_mm_srli_epi16(uv0, 8),
                             _mm_srli_epi16(uv1, 8));
        _mm_storeu_si128((__m128i *) dst_u, u);
        _mm_storeu_si128((__m128i *) dst_v, v);
        src += 32;
        dst_u += 16;
        dst_v += 16;
    }
#endif
    for (; width > 0; width -= 2)
    {
        *(dst_u++) = *(src++);
        *(dst_v++) = *(src++);
    }
}
