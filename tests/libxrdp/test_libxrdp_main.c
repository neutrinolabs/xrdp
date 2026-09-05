/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Idan Freiberg 2013-2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
#if defined(XRDP_NLA)
    srunner_add_suite(sr, make_suite_test_xrdp_nla());
#endif

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
