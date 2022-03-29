#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <check.h>
#include "test_libxrdp.h"

int main (void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create(make_suite_test_xrdp_sec_process_mcs_data_monitors());
    srunner_add_suite(sr, make_suite_test_monitor_processing());

    srunner_set_tap(sr, "-");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
