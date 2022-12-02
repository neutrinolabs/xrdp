#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "libxrdp.h"
#include "os_calls.h"

#include "test_libxrdp.h"

START_TEST(test_libxrdp_process_monitor_stream__when_description_is_null__fail)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 4);

    //Dummy data.
    out_uint32_le(s, 0);
    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    int error = libxrdp_process_monitor_stream(s, NULL, 1);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR);

    free_stream(s);
}
END_TEST

START_TEST(test_libxrdp_process_monitor_stream__when_stream_is_too_small__fail)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 2);

    //Dummy data.
    out_uint16_le(s, 0);
    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    struct display_size_description *description =
        (struct display_size_description *)
        g_malloc(sizeof(struct display_size_description), 1);

    int error = libxrdp_process_monitor_stream(s, description, 1);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR);

    free(description);
    free_stream(s);
}
END_TEST

START_TEST(test_libxrdp_process_monitor_stream__when_monitor_count_is_greater_than_sixteen__fail)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 4);

    //Dummy data.
    out_uint32_le(s, 17);
    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    struct display_size_description *description =
        (struct display_size_description *)
        g_malloc(sizeof(struct display_size_description), 1);

    int error = libxrdp_process_monitor_stream(s, description, 1);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_TOO_MANY_MONITORS);

    free(description);
    free_stream(s);
}
END_TEST

START_TEST(test_libxrdp_process_monitor_stream__with_single_monitor_happy_path)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 44);

    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, TS_MONITOR_PRIMARY); //flags
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 3840); //monitor width
    out_uint32_le(s, 2160); //monitor height
    out_uint32_le(s, 2000); //physical width
    out_uint32_le(s, 2000); //physical height
    out_uint32_le(s, 0); //orientation
    out_uint32_le(s, 100); //desktop scale factor
    out_uint32_le(s, 100); //device scale factor

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    struct display_size_description *description =
        (struct display_size_description *)
        g_malloc(sizeof(struct display_size_description), 1);

    int error = libxrdp_process_monitor_stream(s, description, 1);

    //Verify function call passed.
    ck_assert_int_eq(error, 0);

    ck_assert_int_eq(description->monitorCount, 1);

    // Verify normal monitor
    ck_assert_int_eq(description->minfo[0].left, 0);
    ck_assert_int_eq(description->minfo[0].top, 0);
    ck_assert_int_eq(description->minfo[0].right, 3839);
    ck_assert_int_eq(description->minfo[0].bottom, 2159);
    ck_assert_int_eq(description->minfo[0].physical_width, 2000);
    ck_assert_int_eq(description->minfo[0].physical_height, 2000);
    ck_assert_int_eq(description->minfo[0].orientation, 0);
    ck_assert_int_eq(description->minfo[0].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[0].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[0].is_primary, 1);

    // Verify normalized monitor
    ck_assert_int_eq(description->minfo_wm[0].left, 0);
    ck_assert_int_eq(description->minfo_wm[0].top, 0);
    ck_assert_int_eq(description->minfo_wm[0].right, 3839);
    ck_assert_int_eq(description->minfo_wm[0].bottom, 2159);
    ck_assert_int_eq(description->minfo_wm[0].physical_width, 2000);
    ck_assert_int_eq(description->minfo_wm[0].physical_height, 2000);
    ck_assert_int_eq(description->minfo_wm[0].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[0].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[0].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[0].is_primary, 1);

    // Verify geometry (+1 greater than )
    ck_assert_int_eq(description->session_width, 3840);
    ck_assert_int_eq(description->session_height, 2160);

    free(description);
    free_stream(s);
}
END_TEST

START_TEST(test_libxrdp_process_monitor_stream__with_sextuple_monitor_happy_path)
{
#define MONITOR_WIDTH 3840
#define MONITOR_HEIGHT 2160

    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 8192);

    out_uint32_le(s, 6); //monitorCount

    // 4k monitor at position (0, 0)
    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 9); //physical width
    out_uint32_le(s, 9); //physical height
    out_uint32_le(s, -10); //orientation
    out_uint32_le(s, -100); //desktop scale factor
    out_uint32_le(s, 600); //device scale factor

    // 4k monitor at position (1, 0)
    out_uint32_le(s, TS_MONITOR_PRIMARY); //flags
    out_uint32_le(s, MONITOR_WIDTH); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 5); //physical width
    out_uint32_le(s, 11000); //physical height
    out_uint32_le(s, 10); //orientation (Expect to be reset to 0)
    out_uint32_le(s, 360); //desktop scale factor
    out_uint32_le(s, 720); //device scale factor

    // 4k monitor at position (2, 0)
    out_uint32_le(s, 0); //flags
    out_uint32_le(s, (2 * MONITOR_WIDTH)); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 1000); //physical width
    out_uint32_le(s, 1000); //physical height
    out_uint32_le(s, 5000); //orientation
    out_uint32_le(s, 80); //desktop scale factor
    out_uint32_le(s, 140); //device scale factor

    // 4k monitor at position (0, 1)
    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, MONITOR_HEIGHT); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 1000); //physical width
    out_uint32_le(s, 1000); //physical height
    out_uint32_le(s, 91); //orientation
    out_uint32_le(s, 180); //desktop scale factor
    out_uint32_le(s, 100); //device scale factor

    // 4k monitor at position (1, 1)
    out_uint32_le(s, 0); //flags
    out_uint32_le(s, MONITOR_WIDTH); //monitor left
    out_uint32_le(s, MONITOR_HEIGHT); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 1000); //physical width
    out_uint32_le(s, 1000); //physical height
    out_uint32_le(s, 0); //orientation
    out_uint32_le(s, 20); //desktop scale factor
    out_uint32_le(s, 50); //device scale factor

    // 4k monitor at position (2, 1)
    out_uint32_le(s, 0); //flags
    out_uint32_le(s, (2 * MONITOR_WIDTH)); //monitor left
    out_uint32_le(s, MONITOR_HEIGHT); //monitor top
    out_uint32_le(s, MONITOR_WIDTH); //monitor width
    out_uint32_le(s, MONITOR_HEIGHT); //monitor height
    out_uint32_le(s, 1000); //physical width
    out_uint32_le(s, 1000); //physical height
    out_uint32_le(s, 0); //orientation
    out_uint32_le(s, 300); //desktop scale factor
    out_uint32_le(s, 400); //device scale factor

    s_mark_end(s);
    // Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    struct display_size_description *description =
        (struct display_size_description *)
        g_malloc(sizeof(struct display_size_description), 1);

    int error = libxrdp_process_monitor_stream(s, description, 1);

    //Verify function call passed.
    ck_assert_int_eq(error, 0);

    ck_assert_int_eq(description->monitorCount, 6);

    /*************************************************
     * Verify standard monitors
     *************************************************/
    ck_assert_int_eq(description->minfo[0].left, 0);
    ck_assert_int_eq(description->minfo[0].top, 0);
    ck_assert_int_eq(description->minfo[0].right, MONITOR_WIDTH - 1);
    ck_assert_int_eq(description->minfo[0].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo[0].physical_width, 0);
    ck_assert_int_eq(description->minfo[0].physical_height, 0);
    ck_assert_int_eq(description->minfo[0].orientation, 0);
    ck_assert_int_eq(description->minfo[0].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[0].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[0].is_primary, 0);

    ck_assert_int_eq(description->minfo[1].left, MONITOR_WIDTH);
    ck_assert_int_eq(description->minfo[1].top, 0);
    ck_assert_int_eq(description->minfo[1].right, (2 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo[1].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo[1].physical_width, 0);
    ck_assert_int_eq(description->minfo[1].physical_height, 0);
    ck_assert_int_eq(description->minfo[1].orientation, 0);
    ck_assert_int_eq(description->minfo[1].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[1].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[1].is_primary, 1);

    ck_assert_int_eq(description->minfo[2].left, (2 * MONITOR_WIDTH));
    ck_assert_int_eq(description->minfo[2].top, 0);
    ck_assert_int_eq(description->minfo[2].right, (3 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo[2].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo[2].physical_width, 1000);
    ck_assert_int_eq(description->minfo[2].physical_height, 1000);
    ck_assert_int_eq(description->minfo[2].orientation, 0);
    ck_assert_int_eq(description->minfo[2].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[2].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[2].is_primary, 0);

    ck_assert_int_eq(description->minfo[3].left, 0);
    ck_assert_int_eq(description->minfo[3].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo[3].right, MONITOR_WIDTH - 1);
    ck_assert_int_eq(description->minfo[3].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo[3].physical_width, 1000);
    ck_assert_int_eq(description->minfo[3].physical_height, 1000);
    ck_assert_int_eq(description->minfo[3].orientation, 0);
    ck_assert_int_eq(description->minfo[3].desktop_scale_factor, 180);
    ck_assert_int_eq(description->minfo[3].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[3].is_primary, 0);

    ck_assert_int_eq(description->minfo[4].left, MONITOR_WIDTH);
    ck_assert_int_eq(description->minfo[4].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo[4].right, (2 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo[4].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo[4].physical_width, 1000);
    ck_assert_int_eq(description->minfo[4].physical_height, 1000);
    ck_assert_int_eq(description->minfo[4].orientation, 0);
    ck_assert_int_eq(description->minfo[4].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[4].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[4].is_primary, 0);

    ck_assert_int_eq(description->minfo[5].left, (2 * MONITOR_WIDTH));
    ck_assert_int_eq(description->minfo[5].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo[5].right, (3 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo[5].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo[5].physical_width, 1000);
    ck_assert_int_eq(description->minfo[5].physical_height, 1000);
    ck_assert_int_eq(description->minfo[5].orientation, 0);
    ck_assert_int_eq(description->minfo[5].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo[5].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo[5].is_primary, 0);

    /*************************************************
     * Verify normalized monitors
     *************************************************/
    ck_assert_int_eq(description->minfo_wm[0].left, 0);
    ck_assert_int_eq(description->minfo_wm[0].top, 0);
    ck_assert_int_eq(description->minfo_wm[0].right, MONITOR_WIDTH - 1);
    ck_assert_int_eq(description->minfo_wm[0].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo_wm[0].physical_width, 0);
    ck_assert_int_eq(description->minfo_wm[0].physical_height, 0);
    ck_assert_int_eq(description->minfo_wm[0].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[0].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[0].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[0].is_primary, 0);

    ck_assert_int_eq(description->minfo_wm[1].left, MONITOR_WIDTH);
    ck_assert_int_eq(description->minfo_wm[1].top, 0);
    ck_assert_int_eq(description->minfo_wm[1].right, (2 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo_wm[1].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo_wm[1].physical_width, 0);
    ck_assert_int_eq(description->minfo_wm[1].physical_height, 0);
    ck_assert_int_eq(description->minfo_wm[1].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[1].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[1].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[1].is_primary, 1);

    ck_assert_int_eq(description->minfo_wm[2].left, (2 * MONITOR_WIDTH));
    ck_assert_int_eq(description->minfo_wm[2].top, 0);
    ck_assert_int_eq(description->minfo_wm[2].right, (3 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo_wm[2].bottom, MONITOR_HEIGHT - 1);
    ck_assert_int_eq(description->minfo_wm[2].physical_width, 1000);
    ck_assert_int_eq(description->minfo_wm[2].physical_height, 1000);
    ck_assert_int_eq(description->minfo_wm[2].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[2].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[2].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[2].is_primary, 0);

    ck_assert_int_eq(description->minfo_wm[3].left, 0);
    ck_assert_int_eq(description->minfo_wm[3].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo_wm[3].right, MONITOR_WIDTH - 1);
    ck_assert_int_eq(description->minfo_wm[3].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo_wm[3].physical_width, 1000);
    ck_assert_int_eq(description->minfo_wm[3].physical_height, 1000);
    ck_assert_int_eq(description->minfo_wm[3].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[3].desktop_scale_factor, 180);
    ck_assert_int_eq(description->minfo_wm[3].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[3].is_primary, 0);

    ck_assert_int_eq(description->minfo_wm[4].left, MONITOR_WIDTH);
    ck_assert_int_eq(description->minfo_wm[4].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo_wm[4].right, (2 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo_wm[4].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo_wm[4].physical_width, 1000);
    ck_assert_int_eq(description->minfo_wm[4].physical_height, 1000);
    ck_assert_int_eq(description->minfo_wm[4].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[4].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[4].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[4].is_primary, 0);

    ck_assert_int_eq(description->minfo_wm[5].left, (2 * MONITOR_WIDTH));
    ck_assert_int_eq(description->minfo_wm[5].top, MONITOR_HEIGHT);
    ck_assert_int_eq(description->minfo_wm[5].right, (3 * MONITOR_WIDTH - 1));
    ck_assert_int_eq(description->minfo_wm[5].bottom, (2 * MONITOR_HEIGHT - 1));
    ck_assert_int_eq(description->minfo_wm[5].physical_width, 1000);
    ck_assert_int_eq(description->minfo_wm[5].physical_height, 1000);
    ck_assert_int_eq(description->minfo_wm[5].orientation, 0);
    ck_assert_int_eq(description->minfo_wm[5].desktop_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[5].device_scale_factor, 100);
    ck_assert_int_eq(description->minfo_wm[5].is_primary, 0);

    // Verify geometry
    ck_assert_int_eq(description->session_width, (3 * MONITOR_WIDTH));
    ck_assert_int_eq(description->session_height, (2 * MONITOR_HEIGHT));

    free(description);
    free_stream(s);
#undef MONITOR_WIDTH
#undef MONITOR_HEIGHT
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_monitor_processing(void)
{
    Suite *s;
    TCase *tc_process_monitors;

    s = suite_create("test_libxrdp_process_monitor_stream");

    tc_process_monitors = tcase_create("libxrdp_process_monitor_stream");
    tcase_add_test(tc_process_monitors, test_libxrdp_process_monitor_stream__when_description_is_null__fail);
    tcase_add_test(tc_process_monitors, test_libxrdp_process_monitor_stream__when_stream_is_too_small__fail);
    tcase_add_test(tc_process_monitors, test_libxrdp_process_monitor_stream__when_monitor_count_is_greater_than_sixteen__fail);
    tcase_add_test(tc_process_monitors, test_libxrdp_process_monitor_stream__with_single_monitor_happy_path);
    tcase_add_test(tc_process_monitors, test_libxrdp_process_monitor_stream__with_sextuple_monitor_happy_path);

    suite_add_tcase(s, tc_process_monitors);

    return s;
}
