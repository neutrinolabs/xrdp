#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif


#include "test_xrdp.h"

#include "xrdp.h"

START_TEST(test_keymap_load__ini_vs_toml)
{
    struct xrdp_keymap *keymap_ini;
    struct xrdp_keymap *keymap_toml;
    struct xrdp_keymap *keymap_zero;

    keymap_ini = calloc(1, sizeof(struct xrdp_keymap));
    keymap_toml = calloc(1, sizeof(struct xrdp_keymap));
    keymap_zero = calloc(1, sizeof(struct xrdp_keymap));

    /* check if memory allocated */
    ck_assert_ptr_nonnull(keymap_ini);
    ck_assert_ptr_nonnull(keymap_toml);
    ck_assert_ptr_nonnull(keymap_zero);

    km_load_file(XRDP_TOP_SRCDIR "/instfiles/km-00000411.ini", keymap_ini);
    km_load_file_toml(XRDP_TOP_SRCDIR "/instfiles/km-00000411.toml", keymap_toml);

    /* check if keymap is loaded */
    ck_assert_mem_ne(keymap_zero, keymap_ini, sizeof(struct xrdp_keymap));
    ck_assert_mem_ne(keymap_zero, keymap_toml, sizeof(struct xrdp_keymap));

    /* check TOML loader returns the identical result to INI loader */
    /* NOTE: For strict comparsion, we should create a compare function */
    ck_assert_mem_eq(keymap_ini, keymap_toml, sizeof(struct xrdp_keymap));

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
    tcase_add_test(tc_keymap_load, test_keymap_load__ini_vs_toml);

    suite_add_tcase(s, tc_keymap_load);
    return s;
}
