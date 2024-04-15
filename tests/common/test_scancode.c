
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "scancode.h"

#include "test_common.h"

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
        unsigned short keycode = scancode_to_keycode(scancode);
        ck_assert_int_ne(keycode, 0);
    }
}
END_TEST

// Checks all invalid evdev scancodes return 0
START_TEST(test_scancode__evdev_bad_values_mapped_to_0)
{
    // Store valid scancodes which are between 0 and 0x1ff
    int valid[512] = {0};
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("evdev"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        valid[scancode] = 1;
    }

    for (scancode = 0 ; scancode < 512; ++scancode)
    {
        if (!valid[scancode])
        {
            ck_assert_int_eq(scancode_to_keycode(scancode), 0);
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
        unsigned short keycode = scancode_to_keycode(scancode);
        ck_assert_int_ne(keycode, 0);
    }
}
END_TEST

// Checks all invalid base scancodes return 0
START_TEST(test_scancode__base_bad_values_mapped_to_0)
{
    // Store valid scancodes which are between 0 and 0x1ff
    int valid[512] = {0};
    unsigned int iter;
    unsigned int scancode;

    ck_assert_int_eq(scancode_set_keycode_set("base"), 0);

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        valid[scancode] = 1;
    }

    for (scancode = 0 ; scancode < 512; ++scancode)
    {
        if (!valid[scancode])
        {
            ck_assert_int_eq(scancode_to_keycode(scancode), 0);
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
    tcase_add_test(tc, test_scancode__keycode_sets);
    tcase_add_test(tc, test_scancode__evdev_all_values_returned);
    tcase_add_test(tc, test_scancode__evdev_bad_values_mapped_to_0);
    tcase_add_test(tc, test_scancode__base_all_values_returned);
    tcase_add_test(tc, test_scancode__base_bad_values_mapped_to_0);

    return s;
}
