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

START_TEST(test_xrdp_egfx_send_create_surface__happy_path)
{
    struct xrdp_egfx_bulk *bulk = g_new0(struct xrdp_egfx_bulk, 1);

    const int surface_id = 0xFF;
    const int width = 640;
    const int height = 480;
    const int pixel_format = 32;

    struct stream *s = xrdp_egfx_create_surface(
                           bulk, surface_id, width, height, pixel_format);
    s->p = s->data;

    unsigned char descriptor;
    in_uint8(s, descriptor);
    ck_assert_int_eq(0xE0, descriptor);

    free_stream(s);
    g_free(bulk);
}
END_TEST

/******************************************************************************/
/**
 * Check a built PDU against the expected bytes, then free it
 *
 * The expected bytes are laid out as in MS-RDPEGFX section 2.2 :
 * RDP_SEGMENTED_DATA descriptor, RDP8_BULK_ENCODED_DATA header,
 * then the RDPGFX_HEADER and the command body, all little-endian.
 */
static void
check_pdu(struct stream *s, const unsigned char *expected, size_t len)
{
    ck_assert_ptr_ne(s, NULL);
    ck_assert_uint_eq((size_t)(s->end - s->data), len);
    ck_assert_mem_eq(s->data, expected, len);
    free_stream(s);
}

/******************************************************************************/
START_TEST(test_xrdp_egfx_create_surface__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,             /* SINGLE descriptor, RDP8 header */
        0x09, 0x00, 0x00, 0x00, /* CREATESURFACE, flags */
        0x0F, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceId */
        0x80, 0x02,             /* width 640 */
        0xE0, 0x01,             /* height 480 */
        0x20                    /* pixelFormat XRGB */
    };

    check_pdu(xrdp_egfx_create_surface(&bulk, 1, 640, 480,
                                       XR_PIXEL_FORMAT_XRGB_8888),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_delete_surface__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0A, 0x00, 0x00, 0x00, /* DELETESURFACE, flags */
        0x0A, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00              /* surfaceId */
    };

    check_pdu(xrdp_egfx_delete_surface(&bulk, 1),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_map_surface__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0F, 0x00, 0x00, 0x00, /* MAPSURFACETOOUTPUT, flags */
        0x14, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceId */
        0x00, 0x00,             /* reserved */
        0x0A, 0x00, 0x00, 0x00, /* outputOriginX */
        0x14, 0x00, 0x00, 0x00  /* outputOriginY */
    };

    check_pdu(xrdp_egfx_map_surface(&bulk, 1, 10, 20),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_fill_surface__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    const struct xrdp_egfx_rect rects[2] =
    {
        {0, 0, 10, 20},
        {5, 6, 7, 8}
    };
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x04, 0x00, 0x00, 0x00, /* SOLIDFILL, flags */
        0x20, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceId */
        0x33, 0x22, 0x11, 0xFF, /* fillPixel */
        0x02, 0x00,             /* fillRectCount */
        0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x14, 0x00,
        0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00
    };

    check_pdu(xrdp_egfx_fill_surface(&bulk, 1, 0xFF112233, 2, rects),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_fill_surface__no_rects)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x04, 0x00, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00              /* fillRectCount */
    };

    check_pdu(xrdp_egfx_fill_surface(&bulk, 1, 0, 0, NULL),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_surface_to_surface__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    const struct xrdp_egfx_rect src_rect = {1, 2, 3, 4};
    const struct xrdp_egfx_point dst_points[2] = {{10, 11}, {12, 13}};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x05, 0x00, 0x00, 0x00, /* SURFACETOSURFACE, flags */
        0x1E, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceIdSrc */
        0x02, 0x00,             /* surfaceIdDest */
        0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, /* rectSrc */
        0x02, 0x00,             /* destPtsCount */
        0x0A, 0x00, 0x0B, 0x00,
        0x0C, 0x00, 0x0D, 0x00
    };

    check_pdu(xrdp_egfx_surface_to_surface(&bulk, 1, 2, &src_rect,
                                           2, dst_points),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_frame_start__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0B, 0x00, 0x00, 0x00, /* STARTFRAME, flags */
        0x10, 0x00, 0x00, 0x00, /* pduLength */
        0x04, 0x03, 0x02, 0x01, /* timestamp */
        0x07, 0x00, 0x00, 0x00  /* frameId */
    };

    check_pdu(xrdp_egfx_frame_start(&bulk, 7, 0x01020304),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_frame_end__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x0C, 0x00, 0x00, 0x00, /* ENDFRAME, flags */
        0x0C, 0x00, 0x00, 0x00, /* pduLength */
        0x07, 0x00, 0x00, 0x00  /* frameId */
    };

    check_pdu(xrdp_egfx_frame_end(&bulk, 7), expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_capsconfirm__version)
{
    /* The flags field is deliberately not checked, see issue #3942 */
    struct xrdp_egfx_bulk bulk = {0};
    static const unsigned char expected[] =
    {
        0xE0, 0x04,
        0x13, 0x00, 0x00, 0x00, /* CAPSCONFIRM, flags */
        0x14, 0x00, 0x00, 0x00, /* pduLength */
        0x02, 0x00, 0x0A, 0x00, /* version 10.0 */
        0x04, 0x00, 0x00, 0x00  /* capsDataLength */
    };
    struct stream *s;

    s = xrdp_egfx_capsconfirm(&bulk, XR_RDPGFX_CAPVERSION_10, 0);
    ck_assert_ptr_ne(s, NULL);
    ck_assert_uint_eq((size_t)(s->end - s->data), sizeof(expected) + 4);
    ck_assert_mem_eq(s->data, expected, sizeof(expected));
    free_stream(s);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_wire_to_surface1__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    struct xrdp_egfx_rect dest_rect = {0, 0, 2, 2};
    unsigned char data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    static const unsigned char expected[] =
    {
        0xE1,                   /* MULTIPART descriptor */
        0x02, 0x00,             /* segmentCount */
        0x1D, 0x00, 0x00, 0x00, /* uncompressedSize */
        /* segment 1: header and fixed fields */
        0x1A, 0x00, 0x00, 0x00, /* segment size */
        0x04,                   /* RDP8 header */
        0x01, 0x00, 0x00, 0x00, /* WIRETOSURFACE_1, flags */
        0x1D, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceId */
        0x00, 0x00,             /* codecId UNCOMPRESSED */
        0x20,                   /* pixelFormat */
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, /* destRect */
        0x04, 0x00, 0x00, 0x00, /* bitmapDataLength */
        /* segment 2: bitmap data */
        0x05, 0x00, 0x00, 0x00,
        0x04,
        0xAA, 0xBB, 0xCC, 0xDD
    };

    check_pdu(xrdp_egfx_wire_to_surface1(&bulk, 1,
                                         XR_RDPGFX_CODECID_UNCOMPRESSED,
                                         XR_PIXEL_FORMAT_XRGB_8888,
                                         &dest_rect, data, sizeof(data)),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_wire_to_surface1__splits_large_data)
{
    /* One byte more than a part can hold forces a second data segment */
    enum { DATA_LEN = 0xFFFF + 2 };
    struct xrdp_egfx_bulk bulk = {0};
    struct xrdp_egfx_rect dest_rect = {0, 0, 1, 1};
    unsigned char *data = (unsigned char *)g_malloc(DATA_LEN, 0);
    struct stream *s;
    uint32_t v32;
    uint16_t v16;

    ck_assert_ptr_ne(data, NULL);
    memset(data, 0x5A, DATA_LEN);
    s = xrdp_egfx_wire_to_surface1(&bulk, 1, XR_RDPGFX_CODECID_PLANAR,
                                   XR_PIXEL_FORMAT_XRGB_8888,
                                   &dest_rect, data, DATA_LEN);
    ck_assert_ptr_ne(s, NULL);
    /* descriptor + count + size, then three segments (4 byte size and
     * RDP8 header each) */
    ck_assert_uint_eq((size_t)(s->end - s->data),
                      (size_t)(7 + (5 + 25) + (5 + 0xFFFF) + (5 + 2)));
    s->p = s->data + 1;
    in_uint16_le(s, v16);
    ck_assert_uint_eq(v16, 3);
    in_uint32_le(s, v32);
    ck_assert_uint_eq(v32, 25 + DATA_LEN);
    /* sizes of the second and third segments */
    s->p = s->data + 7 + 30;
    in_uint32_le(s, v32);
    ck_assert_uint_eq(v32, 1 + 0xFFFF);
    s->p = s->data + 7 + 30 + 5 + 0xFFFF;
    in_uint32_le(s, v32);
    ck_assert_uint_eq(v32, 1 + 2);
    /* the payload survives the split */
    ck_assert_uint_eq((unsigned char)s->end[-1], 0x5A);
    ck_assert_uint_eq((unsigned char)s->data[7 + 30 + 5], 0x5A);

    free_stream(s);
    g_free(data);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_wire_to_surface2__bytes)
{
    struct xrdp_egfx_bulk bulk = {0};
    unsigned char data[2] = {0xAA, 0xBB};
    static const unsigned char expected[] =
    {
        0xE1,
        0x02, 0x00,
        0x17, 0x00, 0x00, 0x00, /* uncompressedSize */
        0x16, 0x00, 0x00, 0x00, /* segment size */
        0x04,
        0x02, 0x00, 0x00, 0x00, /* WIRETOSURFACE_2, flags */
        0x17, 0x00, 0x00, 0x00, /* pduLength */
        0x01, 0x00,             /* surfaceId */
        0x03, 0x00,             /* codecId CAVIDEO */
        0x05, 0x00, 0x00, 0x00, /* codecContextId */
        0x20,                   /* pixelFormat */
        0x02, 0x00, 0x00, 0x00, /* bitmapDataLength */
        0x03, 0x00, 0x00, 0x00,
        0x04,
        0xAA, 0xBB
    };

    check_pdu(xrdp_egfx_wire_to_surface2(&bulk, 1,
                                         XR_RDPGFX_CODECID_CAVIDEO, 5,
                                         XR_PIXEL_FORMAT_XRGB_8888,
                                         data, sizeof(data)),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
/* Reset graphics PDUs are always 340 bytes long, padded with zeros */
static void
put_le32(unsigned char *buf, size_t offset, uint32_t value)
{
    buf[offset] = value & 0xFF;
    buf[offset + 1] = (value >> 8) & 0xFF;
    buf[offset + 2] = (value >> 16) & 0xFF;
    buf[offset + 3] = (value >> 24) & 0xFF;
}

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics__no_monitors)
{
    /* No monitors means one implicit monitor covering the session */
    struct xrdp_egfx_bulk bulk = {0};
    unsigned char expected[2 + 340] = {0xE0, 0x04, 0x0E};

    put_le32(expected, 6, 340);     /* pduLength */
    put_le32(expected, 10, 1920);   /* width */
    put_le32(expected, 14, 1080);   /* height */
    put_le32(expected, 18, 1);      /* monitorCount */
    put_le32(expected, 30, 1919);   /* right */
    put_le32(expected, 34, 1079);   /* bottom */
    put_le32(expected, 38, 1);      /* flags PRIMARY */

    check_pdu(xrdp_egfx_reset_graphics(&bulk, 1920, 1080, 0, NULL),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics__two_monitors)
{
    struct xrdp_egfx_bulk bulk = {0};
    struct monitor_info mi[2] = {{0}};
    unsigned char expected[2 + 340] = {0xE0, 0x04, 0x0E};

    mi[0].right = 1919;
    mi[0].bottom = 1079;
    mi[0].is_primary = 1;
    mi[1].left = 1920;
    mi[1].right = 3839;
    mi[1].bottom = 1079;

    put_le32(expected, 6, 340);
    put_le32(expected, 10, 3840);
    put_le32(expected, 14, 1080);
    put_le32(expected, 18, 2);
    put_le32(expected, 30, 1919);   /* monitor 0 right */
    put_le32(expected, 34, 1079);   /* monitor 0 bottom */
    put_le32(expected, 38, 1);      /* monitor 0 flags */
    put_le32(expected, 42, 1920);   /* monitor 1 left */
    put_le32(expected, 50, 3839);   /* monitor 1 right */
    put_le32(expected, 54, 1079);   /* monitor 1 bottom */

    check_pdu(xrdp_egfx_reset_graphics(&bulk, 3840, 1080, 2, mi),
              expected, sizeof(expected));
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_egfx_reset_graphics__too_many_monitors)
{
    struct xrdp_egfx_bulk bulk = {0};
    struct monitor_info mi[17] = {{0}};

    ck_assert_ptr_eq(xrdp_egfx_reset_graphics(&bulk, 1920, 1080, 17, mi),
                     NULL);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_egfx_base_functions(void)
{
    Suite *s;
    TCase *tc_process_monitors;

    s = suite_create("test_xrdp_egfx_base_functions");

    tc_process_monitors = tcase_create("xrdp_egfx_base_functions");
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_send_create_surface__happy_path);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_create_surface__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_delete_surface__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_map_surface__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_fill_surface__bytes);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_fill_surface__no_rects);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_surface_to_surface__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_frame_start__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_frame_end__bytes);
    tcase_add_test(tc_process_monitors, test_xrdp_egfx_capsconfirm__version);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_wire_to_surface1__bytes);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_wire_to_surface1__splits_large_data);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_wire_to_surface2__bytes);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_reset_graphics__no_monitors);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_reset_graphics__two_monitors);
    tcase_add_test(tc_process_monitors,
                   test_xrdp_egfx_reset_graphics__too_many_monitors);

    suite_add_tcase(s, tc_process_monitors);

    return s;
}

