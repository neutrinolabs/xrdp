#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <X11/X.h> // For NoSymbol
#include <X11/keysym.h>

#include "test_xrdp.h"

#include "xrdp.h"

START_TEST(test_keymap_load__basic_load)
{
    struct xrdp_keymap km;
    const struct xrdp_key_info  *ki;

    // Set the memory area to non-zero values
    memset(&km, 0x7f, sizeof(km));

    // Japanese qwerty layout (see https://kbdlayout.info/kbdjpn)
    // for OADG 109A
    //
    // The Kanji shift-state is not supported by the file format
    km_load_file(XRDP_TOP_SRCDIR "/instfiles/km-00000411.toml", &km);

    ki = &km.keys_noshift[0x59]; // Unmapped scancode
    ck_assert_int_eq(ki->sym, NoSymbol);
    ck_assert_int_eq(ki->chr, 0);

    /* qwerty */
    ki = &km.keys_noshift[0x10];
    ck_assert_int_eq(ki->sym, XK_q);
    ck_assert_int_eq(ki->chr, 'q');
    ki = &km.keys_noshift[0x11];
    ck_assert_int_eq(ki->sym, XK_w);
    ck_assert_int_eq(ki->chr, 'w');
    ki = &km.keys_noshift[0x12];
    ck_assert_int_eq(ki->sym, XK_e);
    ck_assert_int_eq(ki->chr, 'e');
    ki = &km.keys_noshift[0x13];
    ck_assert_int_eq(ki->sym, XK_r);
    ck_assert_int_eq(ki->chr, 'r');
    ki = &km.keys_noshift[0x14];
    ck_assert_int_eq(ki->sym, XK_t);
    ck_assert_int_eq(ki->chr, 't');
    ki = &km.keys_noshift[0x15];
    ck_assert_int_eq(ki->sym, XK_y);
    ck_assert_int_eq(ki->chr, 'y');

    /* QWERTY */
    ki = &km.keys_shift[0x10];
    ck_assert_int_eq(ki->sym, XK_Q);
    ck_assert_int_eq(ki->chr, 'Q');
    ki = &km.keys_shift[0x11];
    ck_assert_int_eq(ki->sym, XK_W);
    ck_assert_int_eq(ki->chr, 'W');
    ki = &km.keys_shift[0x12];
    ck_assert_int_eq(ki->sym, XK_E);
    ck_assert_int_eq(ki->chr, 'E');
    ki = &km.keys_shift[0x13];
    ck_assert_int_eq(ki->sym, XK_R);
    ck_assert_int_eq(ki->chr, 'R');
    ki = &km.keys_shift[0x14];
    ck_assert_int_eq(ki->sym, XK_T);
    ck_assert_int_eq(ki->chr, 'T');
    ki = &km.keys_shift[0x15];
    ck_assert_int_eq(ki->sym, XK_Y);
    ck_assert_int_eq(ki->chr, 'Y');

    /* right-shift and 4 keys to left */
    ki = &km.keys_noshift[0x33]; // OEM Comma
    ck_assert_int_eq(ki->sym, XK_comma);
    ck_assert_int_eq(ki->chr, ',');
    ki = &km.keys_noshift[0x34]; // OEM Period
    ck_assert_int_eq(ki->sym, XK_period);
    ck_assert_int_eq(ki->chr, '.');
    ki = &km.keys_noshift[0x35]; // OEM 2
    ck_assert_int_eq(ki->sym, XK_slash);
    ck_assert_int_eq(ki->chr, '/');
    ki = &km.keys_noshift[0x73]; // ABNT C
    ck_assert_int_eq(ki->sym, XK_backslash);
    ck_assert_int_eq(ki->chr, '\\');
    ki = &km.keys_noshift[0x36]; // Right shift
    ck_assert_int_eq(ki->sym, XK_Shift_R);
    ck_assert_int_eq(ki->chr, 0);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_keymap_load(void)
{
    Suite *s;
    TCase *tc_keymap_load;

    s = suite_create("KeymapLoad");

    tc_keymap_load = tcase_create("xrdp_keymap_load");
    tcase_add_test(tc_keymap_load, test_keymap_load__basic_load);

    suite_add_tcase(s, tc_keymap_load);
    return s;
}
