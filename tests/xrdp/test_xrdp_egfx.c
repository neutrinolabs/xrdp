/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2021
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
 * Test driver for XRDP routines
 *
 * If you want to run this driver under valgrind to check for memory leaks,
 * use the following command line:-
 *
 * CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all \
 *     .libs/test_xrdp
 *
 * without the 'CK_FORK=no', memory still allocated by the test driver will
 * be logged
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "log.h"
#include "os_calls.h"
#include <stdlib.h>
#include "xrdp_egfx.h"
#include "test_xrdp.h"

/*
 * The expected byte sequences in this file are written out by hand from
 * the PDU layouts in [MS-RDPEGFX], so that a change to a builder which
 * alters what goes on the wire has to be made deliberately.
 *
 * All the single segment PDUs start with the same two bytes:-
 *     0xE0   RDP_SEGMENTED_DATA descriptor (SINGLE)
 *     0x04   RDP8_BULK_ENCODED_DATA header (PACKET_COMPR_TYPE_RDP8)
 * These are followed by the RDPGFX_HEADER (cmdId, flags, pduLength) where
 * the pduLength includes the 8 bytes of the RDPGFX_HEADER.
 */

/******************************************************************************/
/* Compares everything a builder wrote with the expected wire bytes,
   and frees the stream */
static void
check_stream(struct stream *s, const unsigned char *expected, int expected_len)
{
    int len;
    int i;

    ck_assert_ptr_nonnull(s);
    len = (int)(s->end - s->data);
    ck_assert_int_eq(len, expected_len);
    for (i = 0; i < len; ++i)
    {
        ck_assert_msg((unsigned char)s->data[i] == expected[i],
                      "byte %d: expected 0x%02x, got 0x%02x",
                      i, expected[i], (unsigned char)s->data[i]);
    }
    free_stream(s);
}

#define CHECK_STREAM(s, expected) \
    check_stream((s), (expected), (int)sizeof(expected))

/******************************************************************************/
/* Sets up a stream for reading a PDU from the client */
static void
set_in_stream(struct stream *s, const unsigned char *data, int len)
{
    g_memset(s, 0, sizeof(*s));
    s->data = (char *)data;
    s->p = s->data;
    s->end = s->data + len;
    s->size = len;
}

/******************************************************************************/
struct caps_result
{
    int calls;
    int num_caps;
    int versions[8];
    int flags[8];
};

static int
test_caps_advertise(void *user, int num_caps, int *version, int *flags)
{
    struct caps_result *result = (struct caps_result *)user;
    int i;

    ++result->calls;
    result->num_caps = num_caps;
    for (i = 0; i < num_caps && i < 8; ++i)
    {
        result->versions[i] = version[i];
        result->flags[i] = flags[i];
    }
    return 0;
}

/******************************************************************************/
struct ack_result
{
    int calls;
    uint32_t queue_depth;
    int frame_id;
    int frames_decoded;
};

static int
test_frame_ack(void *user, uint32_t queue_depth, int frame_id,
               int frames_decoded)
{
    struct ack_result *result = (struct ack_result *)user;

    ++result->calls;
    result->queue_depth = queue_depth;
    result->frame_id = frame_id;
    result->frames_decoded = frames_decoded;
    return 0;
}

/******************************************************************************/
/* RDPGFX_CREATE_SURFACE_PDU */
START_TEST(test_xrdp_egfx_create_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x09, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
        0xFF, 0x00,             /* surfaceId */
        0x80, 0x02,             /* width */
        0xE0, 0x01,             /* height */
        0x20                    /* pixelFormat */
    };

    CHECK_STREAM(xrdp_egfx_create_surface(NULL, 0xFF, 640, 480,
                                          XR_PIXEL_FORMAT_XRGB_8888),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_DELETE_SURFACE_PDU */
START_TEST(test_xrdp_egfx_delete_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0A, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
        0x34, 0x12              /* surfaceId */
    };

    CHECK_STREAM(xrdp_egfx_delete_surface(NULL, 0x1234), expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU */
START_TEST(test_xrdp_egfx_map_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0F, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x07, 0x00,             /* surfaceId */
        0x00, 0x00,             /* reserved */
        0x00, 0x01, 0x00, 0x00, /* outputOriginX */
        0x00, 0x02, 0x00, 0x00  /* outputOriginY */
    };

    CHECK_STREAM(xrdp_egfx_map_surface(NULL, 7, 0x100, 0x200), expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_SOLIDFILL_PDU */
START_TEST(test_xrdp_egfx_fill_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x01, 0x00,             /* surfaceId */
        0xCC, 0xBB, 0xAA, 0x00, /* fillPixel (B, G, R, XA) */
        0x02, 0x00,             /* fillRectCount */
        0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00
    };
    static const struct xrdp_egfx_rect rects[] =
    {
        {1, 2, 3, 4}, {5, 6, 7, 8}
    };

    CHECK_STREAM(xrdp_egfx_fill_surface(NULL, 1, 0x00AABBCC, 2, rects),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_SURFACE_TO_SURFACE_PDU */
START_TEST(test_xrdp_egfx_surface_to_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x05, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
        0x01, 0x00,             /* surfaceIdSrc */
        0x02, 0x00,             /* surfaceIdDest */
        0x0A, 0x00, 0x14, 0x00, 0x1E, 0x00, 0x28, 0x00, /* rectSrc */
        0x02, 0x00,             /* destPtsCount */
        0x64, 0x00, 0xC8, 0x00,
        0x2C, 0x01, 0x90, 0x01
    };
    static const struct xrdp_egfx_rect src_rect = {10, 20, 30, 40};
    static const struct xrdp_egfx_point pts[] =
    {
        {100, 200}, {300, 400}
    };

    CHECK_STREAM(xrdp_egfx_surface_to_surface(NULL, 1, 2, &src_rect, 2, pts),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_START_FRAME_PDU */
START_TEST(test_xrdp_egfx_frame_start)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0B, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x88, 0x77, 0x66, 0x55, /* timestamp */
        0x44, 0x33, 0x22, 0x11  /* frameId */
    };

    CHECK_STREAM(xrdp_egfx_frame_start(NULL, 0x11223344, 0x55667788),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_END_FRAME_PDU */
START_TEST(test_xrdp_egfx_frame_end)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0C, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
        0x44, 0x33, 0x22, 0x11  /* frameId */
    };

    CHECK_STREAM(xrdp_egfx_frame_end(NULL, 0x11223344), expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CAPS_CONFIRM_PDU */
START_TEST(test_xrdp_egfx_capsconfirm)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x13, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x01, 0x07, 0x0A, 0x00, /* version */
        0x04, 0x00, 0x00, 0x00, /* capsDataLength */
        0x20, 0x00, 0x00, 0x00  /* flags */
    };

    CHECK_STREAM(xrdp_egfx_capsconfirm(NULL, XR_RDPGFX_CAPVERSION_107,
                                       XR_RDPGFX_CAPS_FLAG_AVC_DISABLED),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CAPS_CONFIRM_PDU containing RDPGFX_CAPSET_VERSION101 */
START_TEST(test_xrdp_egfx_capsconfirm_version101)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x13, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x0A, 0x00, /* version */
        0x10, 0x00, 0x00, 0x00, /* capsDataLength */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    /* The flags are ignored for this version */
    CHECK_STREAM(xrdp_egfx_capsconfirm(NULL, XR_RDPGFX_CAPVERSION_101,
                                       0x12345678),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_WIRE_TO_SURFACE_PDU_1, bitmap data in a single segment */
START_TEST(test_xrdp_egfx_wire_to_surface1)
{
    static const unsigned char expected[] =
    {
        0xE1,                   /* descriptor (MULTIPART) */
        0x02, 0x00,             /* segmentCount */
        0x1E, 0x00, 0x00, 0x00, /* uncompressedSize */
        /* first segment - PDU up to the bitmap data */
        0x1A, 0x00, 0x00, 0x00, /* segment size */
        0x04,
        0x01, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
        0x03, 0x00,             /* surfaceId */
        0x0A, 0x00,             /* codecId */
        0x20,                   /* pixelFormat */
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, /* destRect */
        0x05, 0x00, 0x00, 0x00, /* bitmapDataLength */
        /* second segment - the bitmap data */
        0x06, 0x00, 0x00, 0x00, /* segment size */
        0x04,
        0x01, 0x02, 0x03, 0x04, 0x05
    };
    char bitmap_data[] = {1, 2, 3, 4, 5};
    struct xrdp_egfx_rect dest_rect = {0, 0, 4, 2};

    CHECK_STREAM(xrdp_egfx_wire_to_surface1(NULL, 3,
                                            XR_RDPGFX_CODECID_PLANAR,
                                            XR_PIXEL_FORMAT_XRGB_8888,
                                            &dest_rect, bitmap_data,
                                            sizeof(bitmap_data)),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_WIRE_TO_SURFACE_PDU_1 where the bitmap data doesn't fit in
   one segment. A segment can hold at most 65535 bytes */
START_TEST(test_xrdp_egfx_wire_to_surface1_multi_segment)
{
    const int bitmap_data_length = 0xFFFF + 1;
    /* descriptor, segmentCount and uncompressedSize */
    const int hdr_len = 1 + 2 + 4;
    /* each segment has a size and a RDP8_BULK_ENCODED_DATA header */
    const int seg1_len = 4 + 1 + 25;
    const int seg2_len = 4 + 1 + 0xFFFF;
    const int seg3_len = 4 + 1 + 1;
    unsigned char *p;
    char *bitmap_data;
    struct stream *s;
    struct xrdp_egfx_rect dest_rect = {0, 0, 256, 64};
    int i;

    bitmap_data = g_new(char, bitmap_data_length);
    ck_assert_ptr_nonnull(bitmap_data);
    for (i = 0; i < bitmap_data_length; ++i)
    {
        bitmap_data[i] = (char)(i % 251);
    }

    s = xrdp_egfx_wire_to_surface1(NULL, 3, XR_RDPGFX_CODECID_UNCOMPRESSED,
                                   XR_PIXEL_FORMAT_XRGB_8888,
                                   &dest_rect, bitmap_data,
                                   bitmap_data_length);
    ck_assert_ptr_nonnull(s);
    ck_assert_int_eq((int)(s->end - s->data),
                     hdr_len + seg1_len + seg2_len + seg3_len);

    p = (unsigned char *)s->data;
    ck_assert_int_eq(p[0], 0xE1);
    /* segmentCount */
    ck_assert_int_eq(p[1] | (p[2] << 8), 3);
    /* uncompressedSize is the size of the whole PDU */
    ck_assert_int_eq(p[3] | (p[4] << 8) | (p[5] << 16) | (p[6] << 24),
                     25 + bitmap_data_length);

    /* first segment */
    p += hdr_len;
    ck_assert_int_eq(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24),
                     1 + 25);
    ck_assert_int_eq(p[4], 0x04);
    /* pduLength in the RDPGFX_HEADER */
    ck_assert_int_eq(p[9] | (p[10] << 8) | (p[11] << 16) | (p[12] << 24),
                     25 + bitmap_data_length);
    /* bitmapDataLength is the last field in the first segment */
    ck_assert_int_eq(p[26] | (p[27] << 8) | (p[28] << 16) | (p[29] << 24),
                     bitmap_data_length);

    /* second segment */
    p += seg1_len;
    ck_assert_int_eq(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24),
                     1 + 0xFFFF);
    ck_assert_int_eq(p[4], 0x04);
    ck_assert_int_eq(g_memcmp(p + 5, bitmap_data, 0xFFFF), 0);

    /* third segment */
    p += seg2_len;
    ck_assert_int_eq(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24),
                     1 + 1);
    ck_assert_int_eq(p[4], 0x04);
    ck_assert_int_eq(p[5], (unsigned char)bitmap_data[0xFFFF]);

    free_stream(s);
    g_free(bitmap_data);
}
END_TEST

/******************************************************************************/
/* RDPGFX_WIRE_TO_SURFACE_PDU_2 */
START_TEST(test_xrdp_egfx_wire_to_surface2)
{
    static const unsigned char expected[] =
    {
        0xE1,                   /* descriptor (MULTIPART) */
        0x02, 0x00,             /* segmentCount */
        0x18, 0x00, 0x00, 0x00, /* uncompressedSize */
        /* first segment - PDU up to the bitmap data */
        0x16, 0x00, 0x00, 0x00, /* segment size */
        0x04,
        0x02, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x03, 0x00,             /* surfaceId */
        0x09, 0x00,             /* codecId */
        0x04, 0x03, 0x02, 0x01, /* codecContextId */
        0x20,                   /* pixelFormat */
        0x03, 0x00, 0x00, 0x00, /* bitmapDataLength */
        /* second segment - the bitmap data */
        0x04, 0x00, 0x00, 0x00, /* segment size */
        0x04,
        0xAA, 0xBB, 0xCC
    };
    char bitmap_data[] = {(char)0xAA, (char)0xBB, (char)0xCC};

    CHECK_STREAM(xrdp_egfx_wire_to_surface2(NULL, 3, 9, 0x01020304,
                                            XR_PIXEL_FORMAT_XRGB_8888,
                                            bitmap_data,
                                            sizeof(bitmap_data)),
                 expected);
}
END_TEST

/******************************************************************************/
/* Checks the fixed part of a RDPGFX_RESET_GRAPHICS_PDU. The PDU is always
   340 bytes long, whatever the number of monitors */
static void
check_reset_graphics(struct stream *s, int width, int height,
                     int monitor_count)
{
    unsigned char *p;
    int i;

    ck_assert_ptr_nonnull(s);
    ck_assert_int_eq((int)(s->end - s->data), 2 + 340);
    p = (unsigned char *)s->data;
    ck_assert_int_eq(p[0], 0xE0);
    ck_assert_int_eq(p[1], 0x04);
    p += 2;
    /* cmdId */
    ck_assert_int_eq(p[0] | (p[1] << 8), XR_RDPGFX_CMDID_RESETGRAPHICS);
    /* flags */
    ck_assert_int_eq(p[2] | (p[3] << 8), 0);
    /* pduLength */
    ck_assert_int_eq(p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24), 340);
    ck_assert_int_eq(p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24),
                     width);
    ck_assert_int_eq(p[12] | (p[13] << 8) | (p[14] << 16) | (p[15] << 24),
                     height);
    ck_assert_int_eq(p[16] | (p[17] << 8) | (p[18] << 16) | (p[19] << 24),
                     monitor_count);
    /* Everything after the monitor definitions is padding */
    for (i = 20 + monitor_count * 20; i < 340; ++i)
    {
        ck_assert_msg(p[i] == 0, "pad byte %d is 0x%02x", i, p[i]);
    }
}

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics_no_monitors)
{
    /* Monitor the builder makes up when it isn't given one */
    static const unsigned char expected_monitor[] =
    {
        0x00, 0x00, 0x00, 0x00, /* left */
        0x00, 0x00, 0x00, 0x00, /* top */
        0xFF, 0x03, 0x00, 0x00, /* right */
        0xFF, 0x02, 0x00, 0x00, /* bottom */
        0x01, 0x00, 0x00, 0x00  /* flags (primary) */
    };
    struct stream *s = xrdp_egfx_reset_graphics(NULL, 1024, 768, 0, NULL);

    /* Some clients need at least one monitor to be sent */
    check_reset_graphics(s, 1024, 768, 1);
    ck_assert_int_eq(g_memcmp(s->data + 2 + 20, expected_monitor,
                              sizeof(expected_monitor)), 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics_one_monitor)
{
    static const unsigned char expected_monitor[] =
    {
        0x00, 0x00, 0x00, 0x00, /* left */
        0x00, 0x00, 0x00, 0x00, /* top */
        0x7F, 0x07, 0x00, 0x00, /* right */
        0x37, 0x04, 0x00, 0x00, /* bottom */
        0x01, 0x00, 0x00, 0x00  /* flags (primary) */
    };
    struct monitor_info mi[1];
    struct stream *s;

    g_memset(mi, 0, sizeof(mi));
    mi[0].right = 1919;
    mi[0].bottom = 1079;
    mi[0].is_primary = 1;

    s = xrdp_egfx_reset_graphics(NULL, 1920, 1080, 1, mi);
    check_reset_graphics(s, 1920, 1080, 1);
    ck_assert_int_eq(g_memcmp(s->data + 2 + 20, expected_monitor,
                              sizeof(expected_monitor)), 0);
    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics_max_monitors)
{
    static const unsigned char expected_last_monitor[] =
    {
        0x00, 0x3C, 0x00, 0x00, /* left */
        0x00, 0x00, 0x00, 0x00, /* top */
        0xFF, 0x3F, 0x00, 0x00, /* right */
        0xFF, 0x02, 0x00, 0x00, /* bottom */
        0x00, 0x00, 0x00, 0x00  /* flags */
    };
    struct monitor_info mi[17];
    struct stream *s;
    int i;

    g_memset(mi, 0, sizeof(mi));
    for (i = 0; i < 17; ++i)
    {
        mi[i].left = i * 1024;
        mi[i].right = mi[i].left + 1023;
        mi[i].bottom = 767;
        mi[i].is_primary = (i == 0);
    }

    /* 16 monitors exactly fill the PDU */
    s = xrdp_egfx_reset_graphics(NULL, 16 * 1024, 768, 16, mi);
    check_reset_graphics(s, 16 * 1024, 768, 16);
    ck_assert_int_eq(g_memcmp(s->data + 2 + 20 + 15 * 20,
                              expected_last_monitor,
                              sizeof(expected_last_monitor)), 0);
    free_stream(s);

    /* More than that can't be sent */
    ck_assert_ptr_null(xrdp_egfx_reset_graphics(NULL, 17 * 1024, 768,
                       17, mi));
}
END_TEST

/******************************************************************************/
/* RDPGFX_DELETE_ENCODING_CONTEXT_PDU */
START_TEST(test_xrdp_egfx_delete_encoding_context)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x03, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00,
        0x05, 0x00,             /* surfaceId */
        0xD4, 0xC3, 0xB2, 0xA1  /* codecContextId */
    };

    CHECK_STREAM(xrdp_egfx_delete_encoding_context(NULL, 5, 0xA1B2C3D4),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_SURFACE_TO_CACHE_PDU */
START_TEST(test_xrdp_egfx_surface_to_cache)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x06, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00,
        0x05, 0x00,             /* surfaceId */
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, /* cacheKey */
        0x02, 0x01,             /* cacheSlot */
        0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00 /* rectSrc */
    };
    static const struct xrdp_egfx_rect src_rect = {1, 2, 3, 4};

    CHECK_STREAM(xrdp_egfx_surface_to_cache(NULL, 5, 0x1122334455667788ULL,
                                            0x0102, &src_rect),
                 expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CACHE_TO_SURFACE_PDU */
START_TEST(test_xrdp_egfx_cache_to_surface)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x07, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
        0x02, 0x01,             /* cacheSlot */
        0x05, 0x00,             /* surfaceId */
        0x02, 0x00,             /* destPtsCount */
        0x0A, 0x00, 0x14, 0x00,
        0x1E, 0x00, 0x28, 0x00
    };
    static const unsigned char expected_no_points[] =
    {
        0xE0, 0x04,
        0x07, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00,
        0x02, 0x01,             /* cacheSlot */
        0x05, 0x00,             /* surfaceId */
        0x00, 0x00              /* destPtsCount */
    };
    static const struct xrdp_egfx_point pts[] =
    {
        {10, 20}, {30, 40}
    };

    CHECK_STREAM(xrdp_egfx_cache_to_surface(NULL, 0x0102, 5, 2, pts),
                 expected);
    CHECK_STREAM(xrdp_egfx_cache_to_surface(NULL, 0x0102, 5, 0, NULL),
                 expected_no_points);

    /* destPtsCount is a 16-bit field */
    ck_assert_ptr_null(xrdp_egfx_cache_to_surface(NULL, 0x0102, 5, -1, pts));
    ck_assert_ptr_null(xrdp_egfx_cache_to_surface(NULL, 0x0102, 5,
                       0x10000, pts));
}
END_TEST

/******************************************************************************/
/* RDPGFX_EVICT_CACHE_ENTRY_PDU */
START_TEST(test_xrdp_egfx_evict_cache_entry)
{
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x08, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
        0x02, 0x01              /* cacheSlot */
    };

    CHECK_STREAM(xrdp_egfx_evict_cache_entry(NULL, 0x0102), expected);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CACHE_IMPORT_REPLY_PDU */
START_TEST(test_xrdp_egfx_cache_import_reply)
{
    static const unsigned char expected_none[] =
    {
        0xE0, 0x04,
        0x11, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00              /* importedEntriesCount */
    };
    static const unsigned char expected_two[] =
    {
        0xE0, 0x04,
        0x11, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00,
        0x02, 0x00,             /* importedEntriesCount */
        0x01, 0x00,             /* cacheSlots */
        0x03, 0x02
    };
    static const uint16_t slots[] = {1, 0x0203};

    CHECK_STREAM(xrdp_egfx_cache_import_reply(NULL, 0, NULL), expected_none);
    CHECK_STREAM(xrdp_egfx_cache_import_reply(NULL, 2, slots), expected_two);

    /* Can't import more entries than the client is allowed to offer */
    ck_assert_ptr_null(xrdp_egfx_cache_import_reply(NULL, -1, slots));
    ck_assert_ptr_null(
        xrdp_egfx_cache_import_reply(NULL,
                                     XR_RDPGFX_CACHE_IMPORT_OFFER_LIMIT,
                                     slots));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_caps_confirm_flags)
{
    const int thin = XR_RDPGFX_CAPS_FLAG_THINCLIENT;
    const int small = XR_RDPGFX_CAPS_FLAG_SMALL_CACHE;
    const int avc420 = XR_RDPGFX_CAPS_FLAG_AVC420_ENABLED;
    const int avc_off = XR_RDPGFX_CAPS_FLAG_AVC_DISABLED;
    const int avc_thin = XR_RDPGFX_CAPS_FLAG_AVC_THINCLIENT;
    const int no_scale = XR_RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE;

    /* Version 8 has no codec flags */
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_8, thin, 0),
        thin);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_8, small, 1),
        small);

    /* Version 8.1 says whether AVC420 will be used */
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_81,
                                     small | avc420, 1),
        small | avc420);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_81,
                                     small | avc420, 0),
        small);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_81, thin, 0),
        thin);

    /* Version 10.1 has no flags */
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_101, 0x1234, 0),
        0);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_101, 0x1234, 1),
        0);

    /* Version 10 and later say whether AVC is not being used */
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_10, 0, 1),
        0);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_10, 0, 0),
        avc_off);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_102,
                                     small | avc_off, 0),
        small | avc_off);
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_107,
                                     small | avc_thin | no_scale, 1),
        small | avc_thin | no_scale);
    /* AVC_THINCLIENT can't be set along with AVC_DISABLED */
    ck_assert_int_eq(
        xrdp_egfx_caps_confirm_flags(XR_RDPGFX_CAPVERSION_107,
                                     small | avc_thin | no_scale, 0),
        small | avc_off | no_scale);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_get_max_cache_slots)
{
    struct xrdp_egfx egfx;

    g_memset(&egfx, 0, sizeof(egfx));
    /* Capabilities not confirmed */
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 0);

    egfx.caps_version = XR_RDPGFX_CAPVERSION_8;
    egfx.caps_flags = 0;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 25600);
    egfx.caps_flags = XR_RDPGFX_CAPS_FLAG_THINCLIENT;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 4096);

    egfx.caps_version = XR_RDPGFX_CAPVERSION_81;
    egfx.caps_flags = XR_RDPGFX_CAPS_FLAG_SMALL_CACHE |
                      XR_RDPGFX_CAPS_FLAG_AVC420_ENABLED;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 4096);
    egfx.caps_flags = XR_RDPGFX_CAPS_FLAG_AVC420_ENABLED;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 25600);

    egfx.caps_version = XR_RDPGFX_CAPVERSION_101;
    egfx.caps_flags = 0;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 25600);

    /* Version 10.3 always has the small cache */
    egfx.caps_version = XR_RDPGFX_CAPVERSION_103;
    egfx.caps_flags = 0;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 4096);

    egfx.caps_version = XR_RDPGFX_CAPVERSION_107;
    egfx.caps_flags = XR_RDPGFX_CAPS_FLAG_AVC_DISABLED;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 25600);
    egfx.caps_flags |= XR_RDPGFX_CAPS_FLAG_SMALL_CACHE;
    ck_assert_int_eq(xrdp_egfx_get_max_cache_slots(&egfx), 4096);
}
END_TEST

/******************************************************************************/
/* Cache slots the client doesn't have are rejected before anything
   is sent */
START_TEST(test_xrdp_egfx_send_cache_checks)
{
    static const struct xrdp_egfx_rect src_rect = {1, 2, 3, 4};
    static const struct xrdp_egfx_point pt = {10, 20};
    struct xrdp_egfx egfx;

    /* There's no session in this structure, so anything which
       gets past the checks will fail the test by crashing */
    g_memset(&egfx, 0, sizeof(egfx));

    /* Cache slots are one-based */
    egfx.caps_version = XR_RDPGFX_CAPVERSION_107;
    egfx.caps_flags = XR_RDPGFX_CAPS_FLAG_SMALL_CACHE;
    ck_assert_int_ne(xrdp_egfx_send_surface_to_cache(&egfx, 1, 99, 0,
                     &src_rect), 0);
    ck_assert_int_ne(xrdp_egfx_send_surface_to_cache(&egfx, 1, 99, 4097,
                     &src_rect), 0);
    ck_assert_int_ne(xrdp_egfx_send_cache_to_surface(&egfx, 0, 1, 1, &pt), 0);
    ck_assert_int_ne(xrdp_egfx_send_cache_to_surface(&egfx, 4097, 1, 1, &pt),
                     0);
    ck_assert_int_ne(xrdp_egfx_send_evict_cache_entry(&egfx, 0), 0);
    ck_assert_int_ne(xrdp_egfx_send_evict_cache_entry(&egfx, 4097), 0);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CAPS_ADVERTISE_PDU */
START_TEST(test_xrdp_egfx_process_caps_advertise)
{
    static const unsigned char pdu[] =
    {
        0x12, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00,
        0x05, 0x00,             /* capsSetCount */
        /* RDPGFX_CAPSET_VERSION8 */
        0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        /* RDPGFX_CAPSET_VERSION101 */
        0x00, 0x01, 0x0A, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* A capability set we don't know about, with more data
           than we are able to make use of */
        0x00, 0x00, 0x0B, 0x00, 0x08, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        /* RDPGFX_CAPSET_VERSION101 of the wrong length */
        0x00, 0x01, 0x0A, 0x00, 0x04, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF,
        /* RDPGFX_CAPSET_VERSION107 */
        0x01, 0x07, 0x0A, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00
    };
    struct xrdp_egfx egfx;
    struct caps_result result;
    struct stream s;

    ck_assert_int_eq((int)sizeof(pdu), 0x56);

    g_memset(&egfx, 0, sizeof(egfx));
    g_memset(&result, 0, sizeof(result));
    egfx.user = &result;
    egfx.caps_advertise = test_caps_advertise;

    set_in_stream(&s, pdu, sizeof(pdu));
    ck_assert_int_eq(xrdp_egfx_process(&egfx, &s), 0);

    ck_assert_int_eq(result.calls, 1);
    ck_assert_int_eq(result.num_caps, 3);
    ck_assert_int_eq(result.versions[0], XR_RDPGFX_CAPVERSION_8);
    ck_assert_int_eq(result.flags[0], XR_RDPGFX_CAPS_FLAG_SMALL_CACHE);
    ck_assert_int_eq(result.versions[1], XR_RDPGFX_CAPVERSION_101);
    ck_assert_int_eq(result.flags[1], 0);
    ck_assert_int_eq(result.versions[2], XR_RDPGFX_CAPVERSION_107);
    ck_assert_int_eq(result.flags[2], XR_RDPGFX_CAPS_FLAG_AVC_DISABLED);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_process_caps_advertise_bad)
{
    /* Two capability sets promised, but only one present */
    static const unsigned char pdu_short[] =
    {
        0x12, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
        0x02, 0x00,             /* capsSetCount */
        0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };
    /* capsDataLength runs off the end of the PDU */
    static const unsigned char pdu_bad_length[] =
    {
        0x12, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
        0x01, 0x00,             /* capsSetCount */
        0x04, 0x00, 0x08, 0x00, 0x05, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };
    static const unsigned char pdu_no_sets[] =
    {
        0x12, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00              /* capsSetCount */
    };
    static const unsigned char pdu_no_count[] =
    {
        0x12, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00
    };
    struct xrdp_egfx egfx;
    struct caps_result result;
    struct stream s;

    g_memset(&egfx, 0, sizeof(egfx));
    g_memset(&result, 0, sizeof(result));
    egfx.user = &result;
    egfx.caps_advertise = test_caps_advertise;

    set_in_stream(&s, pdu_short, sizeof(pdu_short));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_bad_length, sizeof(pdu_bad_length));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_no_sets, sizeof(pdu_no_sets));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_no_count, sizeof(pdu_no_count));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);

    ck_assert_int_eq(result.calls, 0);
}
END_TEST

/******************************************************************************/
/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */
START_TEST(test_xrdp_egfx_process_frame_ack)
{
    /* Two acks in the same message. The second one is asking
       for acks to be suspended */
    static const unsigned char pdu[] =
    {
        0x0D, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, /* queueDepth */
        0x44, 0x33, 0x22, 0x11, /* frameId */
        0x10, 0x00, 0x00, 0x00, /* totalFramesDecoded */

        0x0D, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, /* queueDepth */
        0x45, 0x33, 0x22, 0x11, /* frameId */
        0x11, 0x00, 0x00, 0x00  /* totalFramesDecoded */
    };
    struct xrdp_egfx egfx;
    struct ack_result result;
    struct stream s;

    g_memset(&egfx, 0, sizeof(egfx));
    g_memset(&result, 0, sizeof(result));
    egfx.user = &result;
    egfx.frame_ack = test_frame_ack;

    /* First PDU only */
    set_in_stream(&s, pdu, 20);
    ck_assert_int_eq(xrdp_egfx_process(&egfx, &s), 0);
    ck_assert_int_eq(result.calls, 1);
    ck_assert_int_eq(result.queue_depth, 2);
    ck_assert_int_eq(result.frame_id, 0x11223344);
    ck_assert_int_eq(result.frames_decoded, 16);

    /* Both PDUs */
    g_memset(&result, 0, sizeof(result));
    set_in_stream(&s, pdu, sizeof(pdu));
    ck_assert_int_eq(xrdp_egfx_process(&egfx, &s), 0);
    ck_assert_int_eq(result.calls, 2);
    ck_assert_int_eq(result.queue_depth == XR_SUSPEND_FRAME_ACKNOWLEDGEMENT,
                     1);
    ck_assert_int_eq(result.frame_id, 0x11223345);
    ck_assert_int_eq(result.frames_decoded, 17);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_process_bad_pdu)
{
    /* frame ack which is too short */
    static const unsigned char pdu_short_ack[] =
    {
        0x0D, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };
    /* pduLength smaller than the RDPGFX_HEADER */
    static const unsigned char pdu_small_length[] =
    {
        0x0D, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };
    /* pduLength bigger than the data received */
    static const unsigned char pdu_big_length[] =
    {
        0x0D, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00
    };
    struct xrdp_egfx egfx;
    struct ack_result result;
    struct stream s;

    g_memset(&egfx, 0, sizeof(egfx));
    g_memset(&result, 0, sizeof(result));
    egfx.user = &result;
    egfx.frame_ack = test_frame_ack;

    set_in_stream(&s, pdu_short_ack, sizeof(pdu_short_ack));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_small_length, sizeof(pdu_small_length));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_big_length, sizeof(pdu_big_length));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);

    ck_assert_int_eq(result.calls, 0);
}
END_TEST

/******************************************************************************/
/* PDUs we don't act on are skipped, and don't stop us
   processing what comes after them */
START_TEST(test_xrdp_egfx_process_ignored_pdus)
{
    static const unsigned char pdu[] =
    {
        /* RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU */
        0x16, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x44, 0x33, 0x22, 0x11, /* frameId */
        0x00, 0x10, 0x00, 0x00, /* timestamp */
        0x01, 0x00,             /* timeDiffSE */
        0x02, 0x00,             /* timeDiffEDR */
        /* Not a PDU the client can send */
        0xEE, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
        0x01, 0x02,
        /* RDPGFX_FRAME_ACKNOWLEDGE_PDU */
        0x0D, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, /* queueDepth */
        0x44, 0x33, 0x22, 0x11, /* frameId */
        0x10, 0x00, 0x00, 0x00  /* totalFramesDecoded */
    };
    struct xrdp_egfx egfx;
    struct ack_result result;
    struct stream s;

    g_memset(&egfx, 0, sizeof(egfx));
    g_memset(&result, 0, sizeof(result));
    egfx.user = &result;
    egfx.frame_ack = test_frame_ack;

    set_in_stream(&s, pdu, sizeof(pdu));
    ck_assert_int_eq(xrdp_egfx_process(&egfx, &s), 0);
    ck_assert_int_eq(result.calls, 1);
    ck_assert_int_eq(result.frame_id, 0x11223344);
}
END_TEST

/******************************************************************************/
/* RDPGFX_CACHE_IMPORT_OFFER_PDU */
START_TEST(test_xrdp_egfx_process_cache_import_offer)
{
    static const unsigned char pdu[] =
    {
        0x10, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
        0x02, 0x00,             /* cacheEntriesCount */
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, /* cacheKey */
        0x00, 0x10, 0x00, 0x00, /* bitmapLength */
        0x89, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, /* cacheKey */
        0x00, 0x20, 0x00, 0x00  /* bitmapLength */
    };
    /* Two entries promised, but only one present */
    static const unsigned char pdu_short[] =
    {
        0x10, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
        0x02, 0x00,             /* cacheEntriesCount */
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, /* cacheKey */
        0x00, 0x10, 0x00, 0x00  /* bitmapLength */
    };
    static const unsigned char pdu_no_count[] =
    {
        0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00
    };
    struct xrdp_egfx egfx;
    struct stream s;
    unsigned char *pdu_too_many;
    const int too_many = XR_RDPGFX_CACHE_IMPORT_OFFER_LIMIT;
    const int too_many_len = 8 + 2 + too_many * 12;

    ck_assert_int_eq((int)sizeof(pdu), 0x22);

    /* Capabilities haven't been confirmed, so the offer is checked,
       but no reply is sent. That's just as well, as there's no session
       here to send it on */
    g_memset(&egfx, 0, sizeof(egfx));

    set_in_stream(&s, pdu, sizeof(pdu));
    ck_assert_int_eq(xrdp_egfx_process(&egfx, &s), 0);

    set_in_stream(&s, pdu_short, sizeof(pdu_short));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    set_in_stream(&s, pdu_no_count, sizeof(pdu_no_count));
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);

    /* The number of entries is limited, even if they are all present */
    pdu_too_many = g_new0(unsigned char, too_many_len);
    ck_assert_ptr_nonnull(pdu_too_many);
    pdu_too_many[0] = 0x10;
    pdu_too_many[4] = too_many_len & 0xFF;
    pdu_too_many[5] = (too_many_len >> 8) & 0xFF;
    pdu_too_many[6] = (too_many_len >> 16) & 0xFF;
    pdu_too_many[8] = too_many & 0xFF;
    pdu_too_many[9] = (too_many >> 8) & 0xFF;
    set_in_stream(&s, pdu_too_many, too_many_len);
    ck_assert_int_ne(xrdp_egfx_process(&egfx, &s), 0);
    g_free(pdu_too_many);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_egfx_base_functions(void)
{
    Suite *s;
    TCase *tc_builders;
    TCase *tc_caps;
    TCase *tc_process;

    s = suite_create("test_xrdp_egfx_base_functions");

    tc_builders = tcase_create("xrdp_egfx_base_functions");
    tcase_add_test(tc_builders, test_xrdp_egfx_create_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_delete_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_map_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_fill_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_surface_to_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_frame_start);
    tcase_add_test(tc_builders, test_xrdp_egfx_frame_end);
    tcase_add_test(tc_builders, test_xrdp_egfx_capsconfirm);
    tcase_add_test(tc_builders, test_xrdp_egfx_capsconfirm_version101);
    tcase_add_test(tc_builders, test_xrdp_egfx_wire_to_surface1);
    tcase_add_test(tc_builders,
                   test_xrdp_egfx_wire_to_surface1_multi_segment);
    tcase_add_test(tc_builders, test_xrdp_egfx_wire_to_surface2);
    tcase_add_test(tc_builders, test_xrdp_egfx_reset_graphics_no_monitors);
    tcase_add_test(tc_builders, test_xrdp_egfx_reset_graphics_one_monitor);
    tcase_add_test(tc_builders, test_xrdp_egfx_reset_graphics_max_monitors);
    tcase_add_test(tc_builders, test_xrdp_egfx_delete_encoding_context);
    tcase_add_test(tc_builders, test_xrdp_egfx_surface_to_cache);
    tcase_add_test(tc_builders, test_xrdp_egfx_cache_to_surface);
    tcase_add_test(tc_builders, test_xrdp_egfx_evict_cache_entry);
    tcase_add_test(tc_builders, test_xrdp_egfx_cache_import_reply);
    suite_add_tcase(s, tc_builders);

    tc_caps = tcase_create("xrdp_egfx_capabilities");
    tcase_add_test(tc_caps, test_xrdp_egfx_caps_confirm_flags);
    tcase_add_test(tc_caps, test_xrdp_egfx_get_max_cache_slots);
    tcase_add_test(tc_caps, test_xrdp_egfx_send_cache_checks);
    suite_add_tcase(s, tc_caps);

    tc_process = tcase_create("xrdp_egfx_process");
    tcase_add_test(tc_process, test_xrdp_egfx_process_caps_advertise);
    tcase_add_test(tc_process, test_xrdp_egfx_process_caps_advertise_bad);
    tcase_add_test(tc_process, test_xrdp_egfx_process_frame_ack);
    tcase_add_test(tc_process, test_xrdp_egfx_process_bad_pdu);
    tcase_add_test(tc_process, test_xrdp_egfx_process_ignored_pdus);
    tcase_add_test(tc_process, test_xrdp_egfx_process_cache_import_offer);
    suite_add_tcase(s, tc_process);

    return s;
}
