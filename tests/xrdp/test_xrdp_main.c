/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2021
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
 *
 * Test driver for XRDP routines
 *
 * If you want to run this driver under valgrind to check for memory leaks,
 * use the following command line:-
 *
 * CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all \
 *     .libs/test_xrdp
 *
 * without the 'CK_FORK=no', memory still allocated by the test driver will
 * be logged
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "log.h"
#include "os_calls.h"
#include <stdio.h>
#include <stdlib.h>

#include "test_xrdp.h"

int main (void)
{
    int number_failed;
    SRunner *sr;
    struct log_config *logging;

    /* Configure the logging sub-system so that functions can use
     * the log functions as appropriate */
    logging = log_config_init_for_console(LOG_LEVEL_INFO,
                                          g_getenv("TEST_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);
    /* Disable stdout buffering, as this can confuse the error
     * reporting when running in libcheck fork mode */
    setvbuf(stdout, NULL, _IONBF, 0);

    sr = srunner_create (make_suite_test_bitmap_load());
    srunner_add_suite(sr, make_suite_test_keymap_load());
    srunner_add_suite(sr, make_suite_egfx_base_functions());
    srunner_add_suite(sr, make_suite_region());
    srunner_add_suite(sr, make_suite_tconfig_load_gfx());

    srunner_set_tap(sr, "-");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    log_end();

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
