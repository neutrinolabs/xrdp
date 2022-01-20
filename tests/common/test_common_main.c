
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>

#include "log.h"
#include "ssl_calls.h"

#include "test_common.h"

int main (void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create (make_suite_test_string());
    srunner_add_suite(sr, make_suite_test_os_calls());
    srunner_add_suite(sr, make_suite_test_ssl_calls());
    //   srunner_add_suite(sr, make_list_suite());

    srunner_set_tap(sr, "-");
    /*
     * Set up console logging */
    struct log_config *lc = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(lc);
    log_config_free(lc);

    /* Initialise the ssl module */
    ssl_init();

    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ssl_finish();
    log_end();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
