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

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "test_xrdp.h"
#include "xrdp_encoder_openh264.h"

#define MAX_UV_WIDTH 3840
#define OUTPUT_SIZE (MAX_UV_WIDTH / 2 + 32)

/*****************************************************************************/
static void
check_split_uv_row(int width)
{
    unsigned char *src;
    char dst_u[OUTPUT_SIZE];
    char dst_v[OUTPUT_SIZE];
    char expected_u[OUTPUT_SIZE];
    char expected_v[OUTPUT_SIZE];
    int samples = (width + 1) / 2;
    int src_offset;
    int u_offset;
    int v_offset;
    int src_size;
    int i;

    for (src_offset = 0; src_offset < 16; ++src_offset)
    {
        /* End the allocation at the last sample so ASan detects overreads. */
        src_size = src_offset + 2 * samples;
        src = (unsigned char *) malloc(src_size > 0 ? src_size : 1);
        ck_assert_ptr_nonnull(src);
        for (i = 0; i < src_size; ++i)
        {
            /* Include high-bit values and distinguish U from V. */
            src[i] = (unsigned char) (73 * i + 19);
        }
        for (u_offset = 0; u_offset < 16; ++u_offset)
        {
            /* Exercise every pair of source, U and V alignments. */
            v_offset = (src_offset + 3 * u_offset + 7) & 15;
            memset(dst_u, 0xa5, sizeof(dst_u));
            memset(dst_v, 0xa5, sizeof(dst_v));
            memset(expected_u, 0xa5, sizeof(expected_u));
            memset(expected_v, 0xa5, sizeof(expected_v));
            for (i = 0; i < samples; ++i)
            {
                expected_u[u_offset + i] = (char) src[src_offset + 2 * i];
                expected_v[v_offset + i] = (char) src[src_offset + 2 * i + 1];
            }

            xrdp_encoder_openh264_split_uv((const char *) src + src_offset,
                                           dst_u + u_offset, dst_v + v_offset,
                                           width);

            /* Compare the whole buffers, including untouched padding. */
            ck_assert_mem_eq(dst_u, expected_u, sizeof(dst_u));
            ck_assert_mem_eq(dst_v, expected_v, sizeof(dst_v));
        }
        free(src);
    }
}

/*****************************************************************************/
START_TEST(test_split_uv_widths_and_alignment)
{
    int width;

    for (width = 0; width <= 129; ++width)
    {
        check_split_uv_row(width);
    }
    check_split_uv_row(1920);
    check_split_uv_row(MAX_UV_WIDTH);
}
END_TEST

/*****************************************************************************/
START_TEST(test_split_uv_exact_buffer_sizes)
{
    static const int widths[] =
    {
        6, 7, 8, 9, 14, 15, 16, 17, 30, 31, 32, 33,
        46, 47, 48, 49, 62, 63, 64, 65,
        1918, 1919, 1920, 3838, 3839, 3840
    };
    unsigned char *src;
    char *dst_u;
    char *dst_v;
    char expected_u[OUTPUT_SIZE];
    char expected_v[OUTPUT_SIZE];
    unsigned int width_index;
    int width;
    int samples;
    int src_offset;
    int u_offset;
    int v_offset;
    int src_size;
    int u_size;
    int v_size;
    int i;

    for (width_index = 0;
            width_index < sizeof(widths) / sizeof(widths[0]); ++width_index)
    {
        width = widths[width_index];
        samples = (width + 1) / 2;
        for (src_offset = 0; src_offset < 16; ++src_offset)
        {
            src_size = src_offset + 2 * samples;
            src = (unsigned char *) malloc(src_size);
            ck_assert_ptr_nonnull(src);
            for (i = 0; i < src_size; ++i)
            {
                src[i] = (unsigned char) (73 * i + 19);
            }
            for (u_offset = 0; u_offset < 16; ++u_offset)
            {
                /* Cover every pair of source, U and V alignments. */
                v_offset = (src_offset + 3 * u_offset + 7) & 15;
                u_size = u_offset + samples;
                v_size = v_offset + samples;
                /* Exact allocation ends let ASan catch SIMD overreads and
                 * overstores, including the last pair of an odd width. */
                dst_u = (char *) malloc(u_size);
                dst_v = (char *) malloc(v_size);
                ck_assert_ptr_nonnull(dst_u);
                ck_assert_ptr_nonnull(dst_v);
                memset(dst_u, 0xa5, u_size);
                memset(dst_v, 0x5a, v_size);
                memset(expected_u, 0xa5, u_size);
                memset(expected_v, 0x5a, v_size);
                for (i = 0; i < samples; ++i)
                {
                    expected_u[u_offset + i] =
                        (char) src[src_offset + 2 * i];
                    expected_v[v_offset + i] =
                        (char) src[src_offset + 2 * i + 1];
                }

                xrdp_encoder_openh264_split_uv((const char *) src + src_offset,
                                               dst_u + u_offset,
                                               dst_v + v_offset, width);

                /* Also verify the sentinel prefixes remain untouched. */
                ck_assert_mem_eq(dst_u, expected_u, u_size);
                ck_assert_mem_eq(dst_v, expected_v, v_size);
                free(dst_u);
                free(dst_v);
            }
            free(src);
        }
    }
}
END_TEST

/*****************************************************************************/
START_TEST(test_split_uv_empty)
{
    xrdp_encoder_openh264_split_uv(NULL, NULL, NULL, 0);
    xrdp_encoder_openh264_split_uv(NULL, NULL, NULL, -1);
}
END_TEST

/*****************************************************************************/
START_TEST(test_split_uv_subrectangle)
{
    char src[96 * 5];
    char dst_u[48 * 5];
    char dst_v[56 * 5];
    char expected_u[sizeof(dst_u)];
    char expected_v[sizeof(dst_v)];
    const int width = 65;
    const int height = 5;
    int row;
    int sample;
    int i;

    for (i = 0; i < (int) sizeof(src); ++i)
    {
        src[i] = (char) (73 * i + 19);
    }
    memset(dst_u, 0xa5, sizeof(dst_u));
    memset(dst_v, 0xa5, sizeof(dst_v));
    memset(expected_u, 0xa5, sizeof(expected_u));
    memset(expected_v, 0xa5, sizeof(expected_v));

    /* An odd-sized rectangle at nonzero offsets with distinct row strides. */
    for (row = 0; row < (height + 1) / 2; ++row)
    {
        xrdp_encoder_openh264_split_uv(src + (row + 1) * 96 + 6,
                                       dst_u + (row + 1) * 48 + 3,
                                       dst_v + (row + 1) * 56 + 3, width);
        for (sample = 0; sample < (width + 1) / 2; ++sample)
        {
            expected_u[(row + 1) * 48 + 3 + sample] =
                src[(row + 1) * 96 + 6 + 2 * sample];
            expected_v[(row + 1) * 56 + 3 + sample] =
                src[(row + 1) * 96 + 6 + 2 * sample + 1];
        }
    }

    ck_assert_mem_eq(dst_u, expected_u, sizeof(dst_u));
    ck_assert_mem_eq(dst_v, expected_v, sizeof(dst_v));
}
END_TEST

/*****************************************************************************/
Suite *
make_suite_encoder_openh264(void)
{
    Suite *s = suite_create("test_xrdp_encoder_openh264");
    TCase *tc = tcase_create("split_uv");

    tcase_add_test(tc, test_split_uv_widths_and_alignment);
    tcase_add_test(tc, test_split_uv_exact_buffer_sizes);
    tcase_add_test(tc, test_split_uv_empty);
    tcase_add_test(tc, test_split_uv_subrectangle);
    suite_add_tcase(s, tc);

    return s;
}
