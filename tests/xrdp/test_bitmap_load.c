#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "test_xrdp.h"
#include "xrdp.h"

/* Where the image files are located */
#ifndef IMAGEDIR
#define IMAGEDIR "."
#endif

/* Handy color definitions. These are variables so they are displayed
 * in assert messages if tests fail */
static int RED = COLOR24RGB(255, 0, 0);
static int GREEN = COLOR24RGB(0, 255, 0);
static int BLUE = COLOR24RGB(0, 0, 255);
static int WHITE = COLOR24RGB(255, 255, 255);

/* Virtual desktop maxima and minima [MS-RDPBCGR] 2.2.1.3.6 */
#define MAX_VDESKTOP_WIDTH 32766
#define MAX_VDESKTOP_HEIGHT 32766
#define MIN_VDESKTOP_WIDTH 200
#define MIN_VDESKTOP_HEIGHT 200

/* Characteristics of the test bitmap(s) */
#define TEST_BM_WIDTH 256
#define TEST_BM_HEIGHT 256

#define TEST_NOT4_BM_WIDTH 62
#define TEST_NOT4_BM_HEIGHT 62

#define TEST_BM_TOP_LEFT_PIXEL RED
#define TEST_BM_TOP_RIGHT_PIXEL GREEN
#define TEST_BM_BOTTOM_LEFT_PIXEL BLUE
#define TEST_BM_BOTTOM_RIGHT_PIXEL WHITE

void setup(void)
{
}

void teardown(void)
{
}

/* Tests an error is returned for a non-existent file */
START_TEST(test_bitmap_load__with_invalid_image__fail)
{
    struct xrdp_wm *wm = NULL;
    int result;

    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, wm);

    ck_assert_ptr_ne(bm, NULL);

    result = xrdp_bitmap_load(bm, "/dev/null", NULL, 0, XBLT_NONE, 0, 0);

    ck_assert_int_ne(result, 0);

    xrdp_bitmap_delete(bm);
}
END_TEST

/* Checks a particular pixmap value is expected */
static void
check_pixel(struct xrdp_bitmap *bm, int i, int j, int expected)
{
    int pixel = xrdp_bitmap_get_pixel(bm, i, j);
    if (pixel != expected)
    {
        ck_abort_msg("Pixmap (%d,%d) expected %06x, got %06x",
                     i, j, expected, pixel);
    }
}

/* Check we can load bitmaps of various depths with various transforms */
static void
load_and_transform_bm(int depth,
                      enum xrdp_bitmap_load_transform transform,
                      int twidth, int theight)
{
    struct xrdp_wm *wm = NULL;
    int result;

    int width;
    int height;

    char name[256];
    int top_left_pixel;
    int top_right_pixel;
    int bottom_left_pixel;
    int bottom_right_pixel;

    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, wm);

    ck_assert_ptr_ne(bm, NULL);

    g_snprintf(name, sizeof(name), IMAGEDIR "/test_%dbit.bmp", depth);
    result = xrdp_bitmap_load(bm, name, NULL, 0, transform, twidth, theight);

    ck_assert_int_eq(result, 0);

    /* Bitmap right size? */
    if (transform == XBLT_NONE)
    {
        width = TEST_BM_WIDTH;
        height = TEST_BM_HEIGHT;
        ck_assert_int_eq(bm->width, TEST_BM_WIDTH);
        ck_assert_int_eq(bm->height, TEST_BM_HEIGHT);
    }
    else
    {
        width = twidth;
        height = theight;
        ck_assert_int_eq(bm->width, twidth);
        ck_assert_int_eq(bm->height, theight);
    }

    /* Corners OK? */
    top_left_pixel = xrdp_bitmap_get_pixel(bm, 0, 0);
    top_right_pixel = xrdp_bitmap_get_pixel(bm, width - 1, 0);
    bottom_left_pixel = xrdp_bitmap_get_pixel(bm, 0, height - 1);
    bottom_right_pixel = xrdp_bitmap_get_pixel(bm, width - 1, height - 1);

    ck_assert_int_eq(top_left_pixel, TEST_BM_TOP_LEFT_PIXEL);
    ck_assert_int_eq(top_right_pixel, TEST_BM_TOP_RIGHT_PIXEL);
    ck_assert_int_eq(bottom_left_pixel, TEST_BM_BOTTOM_LEFT_PIXEL);
    ck_assert_int_eq(bottom_right_pixel, TEST_BM_BOTTOM_RIGHT_PIXEL);

    xrdp_bitmap_delete(bm);
}

/* Check we can load bitmaps that aren't a multiple of 4 pixels wide */
static void
load_not4_bm(int depth)
{
    struct xrdp_wm *wm = NULL;
    int result;

    const int width = TEST_NOT4_BM_WIDTH;
    const int height = TEST_NOT4_BM_HEIGHT;

    char name[256];
    int i;
    int j;

    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, wm);

    ck_assert_ptr_ne(bm, NULL);

    g_snprintf(name, sizeof(name), IMAGEDIR "/test_not4_%dbit.bmp", depth);
    result = xrdp_bitmap_load(bm, name, NULL, 0, XBLT_NONE, 0, 0);

    ck_assert_int_eq(result, 0);

    /* Bitmap right size? */
    ck_assert_int_eq(bm->width, TEST_NOT4_BM_WIDTH);
    ck_assert_int_eq(bm->height, TEST_NOT4_BM_HEIGHT);

    /* Check all data */
    for (i = 0 ; i < width / 2 ; ++i)
    {
        for (j = 0 ; j < height / 2 ; ++j)
        {
            check_pixel(bm, i, j, TEST_BM_TOP_LEFT_PIXEL);
            check_pixel(bm, i + width / 2, j, TEST_BM_TOP_RIGHT_PIXEL);
            check_pixel(bm, i, j + height / 2, TEST_BM_BOTTOM_LEFT_PIXEL);
            check_pixel(bm, i + width / 2, j + height / 2,
                        TEST_BM_BOTTOM_RIGHT_PIXEL);
        }
    }

    xrdp_bitmap_delete(bm);
}

START_TEST(test_bitmap_load__4_bit__ok)
{
    load_and_transform_bm(4, XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__8_bit__ok)
{
    load_and_transform_bm(8, XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__24_bit__ok)
{
    load_and_transform_bm(24, XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__max_width_zoom__ok)
{
    load_and_transform_bm(24,
                          XBLT_ZOOM, MAX_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_height_zoom__ok)
{
    load_and_transform_bm(24,
                          XBLT_ZOOM, MIN_VDESKTOP_WIDTH, MAX_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__min_zoom__ok)
{
    load_and_transform_bm(24,
                          XBLT_ZOOM, MIN_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_width_scale__ok)
{
    load_and_transform_bm(24,
                          XBLT_SCALE, MAX_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_height_scale__ok)
{
    load_and_transform_bm(24,
                          XBLT_SCALE, MIN_VDESKTOP_WIDTH, MAX_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__min_scale__ok)
{
    load_and_transform_bm(24,
                          XBLT_SCALE, MIN_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_4_bit__ok)
{
    load_not4_bm(4);
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_8_bit__ok)
{
    load_not4_bm(8);
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_24_bit__ok)
{
    load_not4_bm(24);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_bitmap_load(void)
{
    Suite *s;
    TCase *tc_bitmap_load;

    s = suite_create("BitmapLoad");

    tc_bitmap_load = tcase_create("xrdp_bitmap_load");
    tcase_add_checked_fixture(tc_bitmap_load, setup, teardown);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__with_invalid_image__fail);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__4_bit__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__8_bit__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__24_bit__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__max_width_zoom__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__max_height_zoom__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__min_zoom__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__max_width_scale__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__max_height_scale__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__min_scale__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__not_4_pixels_wide_4_bit__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__not_4_pixels_wide_8_bit__ok);
    tcase_add_test(tc_bitmap_load, test_bitmap_load__not_4_pixels_wide_24_bit__ok);

    suite_add_tcase(s, tc_bitmap_load);

    return s;
}
