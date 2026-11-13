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

#define _POSIX_C_SOURCE 200809L

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bench_openh264_ref.h"
#include "xrdp_encoder_openh264.h"

#define BATCHES 11
#define MIN_BATCH_SECONDS 0.020
#define GUARD 32
#define IMPLEMENTATIONS 3

typedef void (*split_uv_fn)(const char *, char *, char *, int);

struct bench_case
{
    int width;
    int rows;
    int offset;
    int padding;
};

struct buffers
{
    char *src;
    char *u;
    char *v;
    char *expected_u;
    char *expected_v;
    size_t dst_bytes;
    int src_stride;
    int dst_stride;
    int start;
};

static const struct bench_case cases[] =
{
    {16, 16, 0, 0},
    {31, 16, 3, 7},
    {32, 16, 0, 0},
    {48, 32, 1, 5},
    {64, 32, 0, 0},
    {256, 128, 3, 7},
    {1918, 540, 3, 7},
    {1919, 540, 1, 5},
    {1920, 540, 0, 0},
    {3838, 1080, 3, 7},
    {3840, 1080, 0, 0}
};

static const split_uv_fn implementations[IMPLEMENTATIONS] =
{
    bench_openh264_scalar,
    bench_openh264_previous,
    xrdp_encoder_openh264_split_uv
};

static const char *const names[IMPLEMENTATIONS] =
{
    "original", "previous", "current"
};

static double
now_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1000000000.0;
}

static char *
allocate(size_t bytes)
{
    char *p = (char *) malloc(bytes);
    if (p == NULL)
    {
        fprintf(stderr, "Unable to allocate benchmark buffers\n");
        exit(EXIT_FAILURE);
    }
    memset(p, 0xa5, bytes);
    return p;
}

static void
init_buffers(const struct bench_case *c, struct buffers *b)
{
    int samples = (c->width + 1) / 2;
    int row;
    int x;
    size_t src_bytes;
    b->src_stride = 2 * samples + c->padding;
    b->dst_stride = samples + c->padding;
    b->start = GUARD + c->offset;
    src_bytes = (size_t) b->src_stride * c->rows + b->start + GUARD;
    b->dst_bytes = (size_t) b->dst_stride * c->rows + b->start + GUARD;
    b->src = allocate(src_bytes);
    b->u = allocate(b->dst_bytes);
    b->v = allocate(b->dst_bytes);
    b->expected_u = allocate(b->dst_bytes);
    b->expected_v = allocate(b->dst_bytes);
    for (row = 0; row < c->rows; ++row)
    {
        for (x = 0; x < samples; ++x)
        {
            char u = (char) (73 * x + 19 * row);
            char v = (char) (37 * x + 53 * row + 129);
            b->src[b->start + row * b->src_stride + 2 * x] = u;
            b->src[b->start + row * b->src_stride + 2 * x + 1] = v;
            b->expected_u[b->start + row * b->dst_stride + x] = u;
            b->expected_v[b->start + row * b->dst_stride + x] = v;
        }
    }
}

static void
check_buffers(const struct bench_case *c, const struct buffers *b, int impl)
{
    /* Comparing the complete allocation also checks row padding and guards. */
    if (memcmp(b->u, b->expected_u, b->dst_bytes) != 0 ||
            memcmp(b->v, b->expected_v, b->dst_bytes) != 0)
    {
        fprintf(stderr, "%s failed correctness/guards for width %d\n",
                names[impl], c->width);
        exit(EXIT_FAILURE);
    }
}

static double
run_batch(const struct bench_case *c, struct buffers *b,
          split_uv_fn split, size_t iterations)
{
    size_t i;
    int row;
    double start = now_seconds();
    for (i = 0; i < iterations; ++i)
    {
        const char *src = b->src + b->start;
        char *u = b->u + b->start;
        char *v = b->v + b->start;
        for (row = 0; row < c->rows; ++row)
        {
            split(src, u, v, c->width);
            src += b->src_stride;
            u += b->dst_stride;
            v += b->dst_stride;
        }
    }
    return now_seconds() - start;
}

static int
compare_doubles(const void *a, const void *b)
{
    double x = *(const double *) a;
    double y = *(const double *) b;
    return (x > y) - (x < y);
}

static void
benchmark_case(const struct bench_case *c, int verify_only)
{
    struct buffers b;
    size_t iterations[IMPLEMENTATIONS];
    double times[IMPLEMENTATIONS][BATCHES];
    int impl;
    int batch;
    int pos;
    double elapsed;
    init_buffers(c, &b);
    for (impl = 0; impl < IMPLEMENTATIONS; ++impl)
    {
        memset(b.u, 0xa5, b.dst_bytes);
        memset(b.v, 0xa5, b.dst_bytes);
        run_batch(c, &b, implementations[impl], 1);
        check_buffers(c, &b, impl);
    }

    if (!verify_only)
    {
        for (impl = 0; impl < IMPLEMENTATIONS; ++impl)
        {
            /* Calibration also warms the reused buffers and instruction cache. */
            iterations[impl] = 1;
            while (run_batch(c, &b, implementations[impl], iterations[impl]) <
                    MIN_BATCH_SECONDS)
            {
                iterations[impl] *= 2;
            }
        }
        for (batch = 0; batch < BATCHES; ++batch)
        {
            for (pos = 0; pos < IMPLEMENTATIONS; ++pos)
            {
                /* Reverse the order each batch to reduce clock/thermal bias. */
                impl = (batch % 2 == 0) ? pos : IMPLEMENTATIONS - 1 - pos;
                elapsed = run_batch(c, &b, implementations[impl],
                                    iterations[impl]);
                times[impl][batch] = elapsed * 1000000.0 / iterations[impl];
                check_buffers(c, &b, impl);
            }
        }
        for (impl = 0; impl < IMPLEMENTATIONS; ++impl)
        {
            qsort(times[impl], BATCHES, sizeof(double), compare_doubles);
        }
        printf("%4dx%-4d %2d/%-2d %10.3f %10.3f %10.3f [%9.3f,%9.3f]"
               " %8.2fx %8.2fx\n",
               c->width, 2 * c->rows, c->offset, c->padding,
               times[0][BATCHES / 2], times[1][BATCHES / 2],
               times[2][BATCHES / 2], times[2][0], times[2][BATCHES - 1],
               times[0][BATCHES / 2] / times[2][BATCHES / 2],
               times[1][BATCHES / 2] / times[2][BATCHES / 2]);
        fflush(stdout);
    }
    free(b.src);
    free(b.u);
    free(b.v);
    free(b.expected_u);
    free(b.expected_v);
}

int
main(int argc, char **argv)
{
    size_t i;
    int verify_only = argc == 2 && strcmp(argv[1], "--verify-only") == 0;
    if (argc > 1 && !verify_only)
    {
        fprintf(stderr, "Usage: %s [--verify-only]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!verify_only)
    {
        printf("UV conversion only; reused buffers; %d alternating batches;"
               " calibration >= %.0f ms\n", BATCHES, MIN_BATCH_SECONDS * 1000);
#if defined(__SSE2__)
        puts("Previous baseline: 32-byte SSE2 loop with scalar tail");
#else
        puts("Previous baseline: scalar fallback (SSE2 disabled)");
#endif
        puts("Dimensions are luma pixels; offset/pad are bytes per chroma row.");
        puts("Times: microseconds/frame, medians; current also shows min/max.");
        puts("   frame  off/pad   original   previous    current"
             "          [min,max]       vs orig   vs prev");
    }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        benchmark_case(&cases[i], verify_only);
    }
    if (verify_only)
    {
        puts("All UV benchmark cases passed correctness and guard checks.");
    }
    return EXIT_SUCCESS;
}
