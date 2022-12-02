#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "libxrdp.h"
#include "os_calls.h"

#include "test_libxrdp.h"

struct xrdp_sec *sec_layer;
struct xrdp_rdp *rdp_layer;
struct xrdp_session *session;

void setup(void)
{
    rdp_layer = (struct xrdp_rdp *)g_malloc(sizeof(struct xrdp_rdp), 1);
    session = (struct xrdp_session *)g_malloc(sizeof(struct xrdp_session), 1);
    session->rdp = rdp_layer;
    session->client_info = &(((struct xrdp_rdp *)session->rdp)->client_info);
    session->client_info->multimon = 1;
    sec_layer = (struct xrdp_sec *) g_malloc(sizeof(struct xrdp_sec), 1);
    sec_layer->rdp_layer = rdp_layer;
}

void teardown(void)
{
    g_free(sec_layer);
    g_free(session);
    g_free(rdp_layer);
}

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_flags_is_not_zero__fail)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 8);

    out_uint32_le(s, 1);
    out_uint32_le(s, 0);
    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_monitor_count_is_greater_than_sixteen__fail)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 8);

    out_uint32_le(s, 0);
    out_uint32_le(s, 17);
    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_TOO_MANY_MONITORS);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__with_single_monitor_happy_path)
{
    struct xrdp_client_info *client_info = &(rdp_layer->client_info);
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 3840); //monitor right
    out_uint32_le(s, 2160); //monitor bottom
    out_uint32_le(s, 1); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, 0);

    ck_assert_int_eq(client_info->display_sizes.monitorCount, 1);

    // Verify normal monitor
    ck_assert_int_eq(client_info->display_sizes.minfo[0].left, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].top, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].right, 3840);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].bottom, 2160);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].is_primary, 1);

    // Verify normalized monitor
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].left, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].top, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].right, 3840);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].bottom, 2160);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].is_primary, 1);

    // Verify geometry (+1 greater than )
    ck_assert_int_eq(client_info->display_sizes.session_width, 3841);
    ck_assert_int_eq(client_info->display_sizes.session_height, 2161);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_no_primary_monitor_is_specified_one_is_selected)
{
    struct xrdp_client_info *client_info = &(rdp_layer->client_info);
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 3840); //monitor right
    out_uint32_le(s, 2160); //monitor bottom
    out_uint32_le(s, 0); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, 0);

    ck_assert_int_eq(client_info->display_sizes.monitorCount, 1);

    // Verify normal monitor
    ck_assert_int_eq(client_info->display_sizes.minfo[0].left, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].top, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].right, 3840);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].bottom, 2160);
    ck_assert_int_eq(client_info->display_sizes.minfo[0].is_primary, 1);

    // Verify normalized monitor
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].left, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].top, 0);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].right, 3840);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].bottom, 2160);
    ck_assert_int_eq(client_info->display_sizes.minfo_wm[0].is_primary, 1);

    // Verify geometry (+1 greater than )
    ck_assert_int_eq(client_info->display_sizes.session_width, 3841);
    ck_assert_int_eq(client_info->display_sizes.session_height, 2161);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_width_is_too_large)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 33000); //monitor right
    out_uint32_le(s, 2160); //monitor bottom
    out_uint32_le(s, 1); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_width_is_too_small)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 100); //monitor right
    out_uint32_le(s, 2160); //monitor bottom
    out_uint32_le(s, 1); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_height_is_too_large)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 3840); //monitor right
    out_uint32_le(s, 33000); //monitor bottom
    out_uint32_le(s, 1); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP);

    free_stream(s);
}
END_TEST

START_TEST(test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_height_is_too_small)
{
    struct stream *s = (struct stream *)NULL;
    make_stream(s);
    init_stream(s, 28);

    out_uint32_le(s, 0); //flags
    out_uint32_le(s, 1); //monitorCount

    // Pretend we have a 4k monitor
    out_uint32_le(s, 0); //monitor left
    out_uint32_le(s, 0); //monitor top
    out_uint32_le(s, 3840); //monitor right
    out_uint32_le(s, 100); //monitor bottom
    out_uint32_le(s, 1); //is primary

    s_mark_end(s);
    //Reset the read counter of the stream so the processing function handles it properly.
    s->p = s->data;

    //Verify function call passed.
    int error = xrdp_sec_process_mcs_data_monitors(sec_layer, s);
    ck_assert_int_eq(error, SEC_PROCESS_MONITORS_ERR_INVALID_DESKTOP);

    free_stream(s);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_xrdp_sec_process_mcs_data_monitors(void)
{
    Suite *s;
    TCase *tc_process_monitors;

    s = suite_create("test_xrdp_sec_process_mcs_data_monitors");

    tc_process_monitors = tcase_create("xrdp_sec_process_mcs_data_monitors");
    tcase_add_checked_fixture(tc_process_monitors, setup, teardown);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_flags_is_not_zero__fail);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_monitor_count_is_greater_than_sixteen__fail);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__with_single_monitor_happy_path);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_no_primary_monitor_is_specified_one_is_selected);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_width_is_too_large);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_width_is_too_small);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_height_is_too_large);
    tcase_add_test(tc_process_monitors, test_xrdp_sec_process_mcs_data_monitors__when_virtual_desktop_height_is_too_small);

    suite_add_tcase(s, tc_process_monitors);

    return s;
}
