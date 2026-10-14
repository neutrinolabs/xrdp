/**
 * xrdp: A Remote Desktop Protocol server.
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
 * wlup unit tests
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "os_calls.h"

#include "test_wlup.h"

int main(void)
{
    int number_failed;
    SRunner *sr;
    struct log_config *logging;

    logging = log_config_init_for_console(LOG_LEVEL_INFO,
                                          g_getenv("TEST_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);
    setvbuf(stdout, NULL, _IONBF, 0);

    sr = srunner_create(make_suite_wlup_core());
    srunner_set_tap(sr, "-");
    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    log_end();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
