
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "scancode.h"

#include "test_common.h"

// Checks all returned scancodes are mapped to a keycode
START_TEST(test_scancode__all_values_returned)
{
    unsigned int iter;
    unsigned int scancode;

    iter = 0;
    while ((scancode = scancode_get_next(&iter)) != 0)
    {
        unsigned short keycode = scancode_to_keycode(scancode);
        ck_assert_int_ne(keycode, 0);
    }
}
END_TEST

// Checks all invalid scancodes return 0
START_TEST(test_scancode__bad_values_mapped_to_0)
{
    // Store valid scancodes which are between 0 and 0x1ff
    int valid[512] = {0};
    unsigned int iter;
    unsigned int scancode;

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
    tcase_add_test(tc, test_scancode__all_values_returned);
    tcase_add_test(tc, test_scancode__bad_values_mapped_to_0);

    return s;
}
