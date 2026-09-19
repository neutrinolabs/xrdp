#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>

#include "xrdp.h"

#include "test_xrdp.h"

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

/*
 * Scaling bitmaps properly will introduce color changes with dithering.
 * Also some filetypes use compression, and these do not represent colors
 * perfectly.
 *
 * This is the Pythagorean distance we allow between two colors for them to
 * be considered close enough to each other */
#define MAX_SIMILAR_COLOR_DISTANCE 3

/* Time to allow some large bitmap test suites to run on slower platforms */
#define LARGE_BM_TEST_SUITE_TIMEOUT 10

static void setup(void)
{
}

static void teardown(void)
{
}

// Name of last bitmap saved
static char save_bitmap_name[128];

// Puts a little-endian integer to a file.
static void
fput_int(unsigned int v, unsigned int bytes, FILE *fd)
{
    while (bytes > 0)
    {
        fputc(v % 256, fd);
        v /= 256;
        --bytes;
    }
}

static void
save_bitmap(struct xrdp_bitmap *bm)
{
    int i;
    // Find a spare filename. We can't use a static variable for this, as if
    // there are several failures, the static variable will always have
    // the same value
    for (i = 1 ; i < 100 ; ++i)
    {
        snprintf(save_bitmap_name, sizeof(save_bitmap_name),
                 "/tmp/fail%d.bmp", i);
        if (!g_file_exist(save_bitmap_name))
        {
            break;
        }
    }
    int stride = bm->width * 3; // 24 bpp
    int pad = 0;
    // Round stride up to 4 bytes
    if (stride % 4 != 0)
    {
        pad = 4 - (stride % 4);
        stride += pad;
    }
    int data_size = stride * bm->height;
    // Need BITMAPFILEHEADER and BITMAPINFOHEADER, in addition
    // to the data
    int file_size = 14 + 40 + data_size;
    FILE *fd = fopen(save_bitmap_name, "w");
    if (fd == NULL)
    {
        strlcpy(save_bitmap_name, "<none>", sizeof(save_bitmap_name));
    }
    else
    {
        int x;
        int y;
        /* BITMAPFILEHEADER (14 bytes) */
        fputc('B', fd); // bfType
        fputc('M', fd); // bfType
        fput_int(file_size, 4, fd); // bfSize
        fput_int(0, 4, fd); // bfReserved1 + bfReserved2
        fput_int(14 + 40, 4, fd); // bfOffbits

        /* BITMAPINFOHEADER (40 bytes) */
        fput_int(40, 4, fd); // biSize
        fput_int(bm->width, 4, fd); // biWidth
        fput_int(bm->height, 4, fd); // biHeight
        fput_int(1, 2, fd); // biPlanes
        fput_int(24, 2, fd); // biBitCount
        fput_int(0, 4, fd); // biCompression
        fput_int(data_size, 4, fd); // biSizeImage
        fput_int(0, 4, fd); // biXPelsPerMeter
        fput_int(0, 4, fd); // biYPelsPerMeter
        fput_int(0, 4, fd); // biClrUsed
        fput_int(0, 4, fd); // biClrImportant
        for (y = bm->height - 1 ; y >= 0 ; --y)
        {
            for (x = 0; x < bm->width ; ++x)
            {
                int pixel = xrdp_bitmap_get_pixel(bm, x, y);
                fput_int(pixel, 3, fd);
            }
            fput_int(0, pad, fd);
        }
        fclose(fd);
    }
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
        save_bitmap(bm);
        ck_abort_msg("Pixmap (%d,%d) expected 0x%06x, got 0x%06x."
                     " Bitmap %s saved",
                     i, j, expected, pixel, save_bitmap_name);
    }
}

/* Alpha output is explicit; ordinary bitmap loads keep their old semantics. */
START_TEST(test_bitmap_load_preserve_alpha)
{
    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, NULL);
    ck_assert_int_eq(xrdp_bitmap_load(bm, IMAGEDIR "/test_24bit.bmp", NULL,
                                      XRDP_BITMAP_BACKGROUND_TRANSPARENT,
                                      XBLT_SCALE, 64, 64), 0);
    check_pixel(bm, 0, 0, RED | 0xff000000);
    check_pixel(bm, 63, 63, WHITE | 0xff000000);
#ifdef USE_IMLIB2
    ck_assert_int_eq(xrdp_bitmap_load(bm, IMAGEDIR "/test_alpha_blend.png", NULL,
                                      XRDP_BITMAP_BACKGROUND_TRANSPARENT,
                                      XBLT_SCALE, 64, 64), 0);
    check_pixel(bm, 0, 0, RED | 0xff000000);
    check_pixel(bm, 63, 0, GREEN | 0xff000000);
    check_pixel(bm, 0, 63, BLUE | 0xff000000);
    ck_assert_uint_eq((unsigned int)xrdp_bitmap_get_pixel(bm, 63, 63) >> 24, 0);
#endif
    xrdp_bitmap_delete(bm);
    bm = xrdp_bitmap_create(4, 4, 24, WND_TYPE_IMAGE, NULL);
    ck_assert_int_ne(xrdp_bitmap_load(bm, IMAGEDIR "/test_24bit.bmp", NULL,
                                      XRDP_BITMAP_BACKGROUND_TRANSPARENT,
                                      XBLT_NONE, 0, 0), 0);
    xrdp_bitmap_delete(bm);
}
END_TEST

/* Calculates whether two colors are close enough to be considered the same */
static void
check_is_close_color(struct xrdp_bitmap *bm, int i, int j, int expected)
{
    int pixel = xrdp_bitmap_get_pixel(bm, i, j);
    int r1;
    int g1;
    int b1;
    int r2;
    int g2;
    int b2;
    int variance;

    SPLITCOLOR32(r1, g1, b1, pixel);
    SPLITCOLOR32(r2, g2, b2, expected);

    variance = ((r1 - r2) * (r1 - r2) +
                (g1 - g2) * (g1 - g2) +
                (b1 - b2) * (b1 - b2));

    if (variance > MAX_SIMILAR_COLOR_DISTANCE * MAX_SIMILAR_COLOR_DISTANCE)
    {
        save_bitmap(bm);
        ck_abort_msg("Pixmap (%d,%d) expected 0x%06x, got 0x%06x"
                     " which exceeds distance of %d."
                     " Bitmap %s saved.",
                     i, j, expected, pixel, MAX_SIMILAR_COLOR_DISTANCE,
                     save_bitmap_name);
    }
}

/* Check we can load images of various depths with various transforms */
static void
load_and_transform_img(const char *name,
                       enum xrdp_bitmap_load_transform transform,
                       int twidth, int theight)
{
    struct xrdp_wm *wm = NULL;
    int result;

    int width;
    int height;

    char full_name[256];

    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, wm);

    ck_assert_ptr_ne(bm, NULL);

    g_snprintf(full_name, sizeof(full_name), IMAGEDIR "/%s", name);
    result = xrdp_bitmap_load(bm, full_name, NULL, HCOLOR(bm->bpp, WHITE),
                              transform, twidth, theight);

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

    /* Corners OK?  Allow for dithering, and avoid a 1-pixel edge */
    check_is_close_color(bm, 1, 1, TEST_BM_TOP_LEFT_PIXEL);
    check_is_close_color(bm, width - 2, 1, TEST_BM_TOP_RIGHT_PIXEL);
    check_is_close_color(bm, 1, height - 2, TEST_BM_BOTTOM_LEFT_PIXEL);
    check_is_close_color(bm, width - 2, height - 2, TEST_BM_BOTTOM_RIGHT_PIXEL);

    xrdp_bitmap_delete(bm);
}

/* Check we can load bitmaps that aren't a multiple of 4 pixels wide */
static void
load_not4_img(const char *name)
{
    struct xrdp_wm *wm = NULL;
    int result;

    const int width = TEST_NOT4_BM_WIDTH;
    const int height = TEST_NOT4_BM_HEIGHT;

    char full_name[256];
    int i;
    int j;

    struct xrdp_bitmap *bm = xrdp_bitmap_create(4, 4, 32, WND_TYPE_IMAGE, wm);

    ck_assert_ptr_ne(bm, NULL);

    g_snprintf(full_name, sizeof(full_name), IMAGEDIR "/%s", name);
    result = xrdp_bitmap_load(bm, full_name, NULL, HCOLOR(bm->bpp, WHITE),
                              XBLT_NONE, 0, 0);

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
    load_and_transform_img("test_4bit.bmp", XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__8_bit__ok)
{
    load_and_transform_img("test_8bit.bmp", XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__24_bit__ok)
{
    load_and_transform_img("test_24bit.bmp", XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_bitmap_load__max_width_zoom__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_ZOOM, MAX_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_height_zoom__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_ZOOM, MIN_VDESKTOP_WIDTH, MAX_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__min_zoom__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_ZOOM, MIN_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_width_scale__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_SCALE, MAX_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__max_height_scale__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_SCALE, MIN_VDESKTOP_WIDTH, MAX_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__min_scale__ok)
{
    load_and_transform_img("test_24bit.bmp",
                           XBLT_SCALE, MIN_VDESKTOP_WIDTH, MIN_VDESKTOP_HEIGHT);
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_4_bit__ok)
{
    load_not4_img("test_not4_4bit.bmp");
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_8_bit__ok)
{
    load_not4_img("test_not4_8bit.bmp");
}
END_TEST

START_TEST(test_bitmap_load__not_4_pixels_wide_24_bit__ok)
{
    load_not4_img("test_not4_24bit.bmp");
}
END_TEST

#ifdef USE_IMLIB2
START_TEST(test_png_load__blend_ok)
{
    load_and_transform_img("test_alpha_blend.png", XBLT_NONE, 0, 0);
}
END_TEST

START_TEST(test_jpg_load__ok)
{
    load_and_transform_img("test1.jpg", XBLT_NONE, 0, 0);
}
END_TEST
#endif

/******************************************************************************/
Suite *
make_suite_test_bitmap_load(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("BitmapLoad");

    tc = tcase_create("xrdp_bitmap_load_basic");
    suite_add_tcase(s, tc);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_bitmap_load__with_invalid_image__fail);
    tcase_add_test(tc, test_bitmap_load_preserve_alpha);
    tcase_add_test(tc, test_bitmap_load__4_bit__ok);
    tcase_add_test(tc, test_bitmap_load__8_bit__ok);
    tcase_add_test(tc, test_bitmap_load__24_bit__ok);

    tc = tcase_create("xrdp_bitmap_load_zoom");
    suite_add_tcase(s, tc);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_set_timeout(tc, LARGE_BM_TEST_SUITE_TIMEOUT);
    tcase_add_test(tc, test_bitmap_load__max_width_zoom__ok);
    tcase_add_test(tc, test_bitmap_load__max_height_zoom__ok);
    tcase_add_test(tc, test_bitmap_load__min_zoom__ok);

    tc = tcase_create("xrdp_bitmap_load_scale");
    suite_add_tcase(s, tc);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_set_timeout(tc, LARGE_BM_TEST_SUITE_TIMEOUT);
    tcase_add_test(tc, test_bitmap_load__max_width_scale__ok);
    tcase_add_test(tc, test_bitmap_load__max_height_scale__ok);
    tcase_add_test(tc, test_bitmap_load__min_scale__ok);

    tc = tcase_create("xrdp_bitmap_load_not_mod_4");
    suite_add_tcase(s, tc);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_bitmap_load__not_4_pixels_wide_4_bit__ok);
    tcase_add_test(tc, test_bitmap_load__not_4_pixels_wide_8_bit__ok);
    tcase_add_test(tc, test_bitmap_load__not_4_pixels_wide_24_bit__ok);


#ifdef USE_IMLIB2
    tc = tcase_create("xrdp_other_load");
    suite_add_tcase(s, tc);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_png_load__blend_ok);
    tcase_add_test(tc, test_jpg_load__ok);
#endif

    return s;
}
