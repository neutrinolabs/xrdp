
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "scancode.h"

#include "test_common.h"

// Max supported scancode value
#define MAX_SUPPORTED_SCANCODE 0x2ff

// Checks conversions to-and-from scancode indexes
START_TEST(test_scancode__scancode_to_index)
{
    int i;
    // In the range 0x00 - 0x7f, scancodes are the same as indexes
    for (i = 0x0; i <= 0x7f; ++i)
    {
        ck_assert_int_eq(scancode_to_index(i), i);
        ck_assert_int_eq(scancode_from_index(i), i);
    }

    // Scancodes from 0x80 - 0xff are not supported
    for (i = 0x80; i <= 0xff; ++i)
    {
        ck_assert_int_eq(scancode_to_index(i), -1);
    }

    // 0x100 - 0x177 map to 0x80 - 0xf7
    for (i = 0x100; i <= 0x177; ++i)
    {
        ck_assert_int_eq(scancode_to_index(i), i - 0x80);
        ck_assert_int_eq(scancode_from_index(i - 0x80), i);
    }

    // Scancodes from 0x178 - 0x1ff are not supported
    for (i = 0x178; i <= 0x1ff; ++i)
    {
        ck_assert_int_eq(scancode_to_index(i), -1);
    }

    // In the range 0x200 up, only SCANCODE_PAUSE_KEY is
    // supported
    ck_assert_int_ge(SCANCODE_PAUSE_KEY, 0x200);
    ck_assert_int_le(SCANCODE_PAUSE_KEY, MAX_SUPPORTED_SCANCODE);
    for (i = 0x200; i <= MAX_SUPPORTED_SCANCODE; ++i)
    {
        if (i == SCANCODE_PAUSE_KEY)
        {
            ck_assert_int_eq(scancode_to_index(i), SCANCODE_INDEX_PAUSE_KEY);
            ck_assert_int_eq(scancode_from_index(SCANCODE_INDEX_PAUSE_KEY), i);
        }
        else
        {
            ck_assert_int_eq(scancode_to_index(i), -1);
        }
    }
}

// Checks all returned evdev scancodes are mapped to a keycode
START_TEST(test_scancode__keycode_sets)
{
    ck_assert_int_eq(scancode_set_keycode_set(NULL), 1);
    ck_assert_int_eq(scancode_set_keycode_set(""), 1);
    ck_assert_int_eq(scancode_set_keycode_set("evdev"), 0);
    ck_assert_int_eq(scancode_set_keycode_set("evdev+aliases(qwerty)"), 0);
    ck_assert_int_eq(scancode_set_keycode_set("base"), 0);
    ck_assert_int_eq(scancode_set_keycode_set("xfree86"), 0);
    ck_assert_int_eq(scancode_set_keycode_set("xfree86+aliases(qwerty)"), 0);
}
END_TEST

// Checks all returned evdev scancodes are mapped to a keycode
START_TEST(test_scancode__evdev_all_values_returned)
{
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("evdev"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        ck_assert_int_ge(scancode, 0);
        ck_assert_int_le(scancode, MAX_SUPPORTED_SCANCODE);
        unsigned short keycode = scancode_to_x11_keycode(scancode);
        ck_assert_int_ne(keycode, 0);
    }
}
END_TEST

// Checks all invalid evdev scancodes return 0
START_TEST(test_scancode__evdev_bad_values_mapped_to_0)
{
    // Store valid scancodes which are between 0 and MAX_SUPPORTED_SCANCODE
    char valid[MAX_SUPPORTED_SCANCODE + 1] = {0};
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("evdev"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        ck_assert_int_ge(scancode, 0);
        ck_assert_int_le(scancode, MAX_SUPPORTED_SCANCODE);
        valid[scancode] = 1;
    }

    for (scancode = 0 ; scancode <= MAX_SUPPORTED_SCANCODE; ++scancode)
    {
        if (!valid[scancode])
        {
            ck_assert_int_eq(scancode_to_x11_keycode(scancode), 0);
        }
    }
}
END_TEST

// Checks all returned base scancodes are mapped to a keycode
START_TEST(test_scancode__base_all_values_returned)
{
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("base"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        ck_assert_int_ge(scancode, 0);
        ck_assert_int_le(scancode, MAX_SUPPORTED_SCANCODE);
        unsigned short keycode = scancode_to_x11_keycode(scancode);
        ck_assert_int_ne(keycode, 0);
    }
}
END_TEST

// Checks all invalid base scancodes return 0
START_TEST(test_scancode__base_bad_values_mapped_to_0)
{
    // Store valid scancodes which are between 0 and MAX_SUPPORTED_SCANCODE
    char valid[MAX_SUPPORTED_SCANCODE + 1] = {0};
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("base"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        ck_assert_int_ge(scancode, 0);
        ck_assert_int_le(scancode, MAX_SUPPORTED_SCANCODE);
        valid[scancode] = 1;
    }

    for (scancode = 0 ; scancode <= MAX_SUPPORTED_SCANCODE; ++scancode)
    {
        if (!valid[scancode])
        {
            ck_assert_int_eq(scancode_to_x11_keycode(scancode), 0);
        }
    }
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_scancode(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("Scancode");

    tc = tcase_create("scancode");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_scancode__scancode_to_index);
    tcase_add_test(tc, test_scancode__keycode_sets);
    tcase_add_test(tc, test_scancode__evdev_all_values_returned);
    tcase_add_test(tc, test_scancode__evdev_bad_values_mapped_to_0);
    tcase_add_test(tc, test_scancode__base_all_values_returned);
    tcase_add_test(tc, test_scancode__base_bad_values_mapped_to_0);

    return s;
}
