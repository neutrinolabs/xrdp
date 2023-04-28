#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "test_libxrdp.h"

int main (void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create(make_suite_test_xrdp_sec_process_mcs_data_monitors());
    srunner_add_suite(sr, make_suite_test_monitor_processing());

    srunner_set_tap(sr, "-");

    /*
     * Set up console logging */
    struct log_config *lc = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(lc);
    log_config_free(lc);
    /* Disable stdout buffering, as this can confuse the error
     * reporting when running in libcheck fork mode */
    setvbuf(stdout, NULL, _IONBF, 0);

    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    log_end();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
