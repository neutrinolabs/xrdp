
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "guid.h"
#include "ms-rdpbcgr.h"
#include "os_calls.h"

#include "test_common.h"

/******************************************************************************/

START_TEST(test_guid_to_str_remotefx)
{
    /* setup */
    char dest[GUID_STR_SIZE];
    struct guid guid;
    g_memcpy(guid.g, XR_CODEC_GUID_REMOTEFX, GUID_SIZE);

    /* test */
    guid_to_str(&guid, dest);

    /* verify */
    ck_assert_str_eq(dest, "76772F12-BD72-4463-AFB3-B73C9C6F7886");
}
END_TEST

START_TEST(test_guid_to_str_nscodec)
{

    /* setup */
    char dest[GUID_STR_SIZE];
    struct guid guid;
    g_memcpy(guid.g, XR_CODEC_GUID_NSCODEC, GUID_SIZE);

    /* test */
    guid_to_str(&guid, dest);

    /* verify */
    ck_assert_str_eq(dest, "CA8D1BB9-000F-154F-589F-AE2D1A87E2D6");
}
END_TEST

START_TEST(test_guid_to_str_ignore)
{
    /* setup */
    char dest[GUID_STR_SIZE];
    struct guid guid;
    g_memcpy(guid.g, XR_CODEC_GUID_IGNORE, GUID_SIZE);

    /* test */
    guid_to_str(&guid, dest);

    /* verify */
    ck_assert_str_eq(dest, "0C4351A6-3535-42AE-910C-CDFCE5760B58");
}
END_TEST

START_TEST(test_guid_to_str_random)
{
    /* setup */
    char dest[GUID_STR_SIZE];
    struct guid guid;
    unsigned int i;
    for (i = 0 ; i < 100; ++i)
    {
        guid = guid_new();
        /* test */
        guid_to_str(&guid, dest);

        /* Check all the '-' signs are in the right places */
        ck_assert_int_eq(dest[8], '-');
        ck_assert_int_eq(dest[13], '-');
        ck_assert_int_eq(dest[18], '-');
        ck_assert_int_eq(dest[23], '-');

        /* Check the variant is RFC4122 */
        char c = dest[18 + 1]; /* char after 3rd dash */
        if (c != '8' && c != '9' && c != 'A' && c != 'B')
        {
            ck_abort_msg("Generated UUID is not RFC4122 compliant");
        }

        /* Check the version is 'randomly generated' */
        c = dest[13 + 1]; /* char after 2nd dash */
        if (c != '4')
        {
            ck_abort_msg("Generated UUID is not RFC4122 randomly-generated");
        }

    }
}
END_TEST

/******************************************************************************/

Suite *
make_suite_test_guid(void)
{
    Suite *s;
    TCase *tc_guid;

    s = suite_create("GUID");

    tc_guid = tcase_create("guid_to_str");
    suite_add_tcase(s, tc_guid);
    tcase_add_test(tc_guid, test_guid_to_str_remotefx);
    tcase_add_test(tc_guid, test_guid_to_str_nscodec);
    tcase_add_test(tc_guid, test_guid_to_str_ignore);
    tcase_add_test(tc_guid, test_guid_to_str_random);

    return s;
}
