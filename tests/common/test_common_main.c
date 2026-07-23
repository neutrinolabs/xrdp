
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "ssl_calls.h"

#include "test_common.h"

/**
 * Converts a binary buffer to a hexadecimal representation
 *
 * Result must be free'd after use
 */
char *
bin_to_hex(const char *input, int length)
{
    int i;
    char *result = (char *)malloc(length * 2 + 1);
    if (result != NULL)
    {
        char *p = result;
        const char *hexdigit = "0123456789abcdef";
        for (i = 0 ; i < length ; ++i)
        {
            int c = input[i];
            if (c < 0)
            {
                c += 256;
            }
            *p++ = hexdigit[ c / 16];
            *p++ = hexdigit[ c % 16];
        }
        *p = '\0';
    }

    return result;
}

static int
run_suite(const char *test_name)
{
    const char *env = getenv("TEST_NAME");
    return (env == NULL || strcmp(env, test_name) == 0);
}

int main (void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create (NULL);
    if (run_suite("dechunker"))
    {
        srunner_add_suite(sr, make_suite_test_dechunker());
    }
    if (run_suite("fifo"))
    {
        srunner_add_suite(sr, make_suite_test_fifo());
    }
    if (run_suite("list"))
    {
        srunner_add_suite(sr, make_suite_test_list());
    }
    if (run_suite("list16"))
    {
        srunner_add_suite(sr, make_suite_test_list16());
    }
    if (run_suite("parse"))
    {
        srunner_add_suite(sr, make_suite_test_parse());
    }
    if (run_suite("set_int"))
    {
        srunner_add_suite(sr, make_suite_test_set_int());
    }
    if (run_suite("string"))
    {
        srunner_add_suite(sr, make_suite_test_string());
    }
    if (run_suite("unicode"))
    {
        srunner_add_suite(sr, make_suite_test_string_unicode());
    }
    if (run_suite("os_calls"))
    {
        srunner_add_suite(sr, make_suite_test_os_calls());
    }
    if (run_suite("ssl_calls"))
    {
        srunner_add_suite(sr, make_suite_test_ssl_calls());
    }
    if (run_suite("base64"))
    {
        srunner_add_suite(sr, make_suite_test_base64());
    }
    if (run_suite("guid"))
    {
        srunner_add_suite(sr, make_suite_test_guid());
    }
    if (run_suite("scancode"))
    {
        srunner_add_suite(sr, make_suite_test_scancode());
    }
    if (run_suite("timers"))
    {
        srunner_add_suite(sr, make_suite_test_timers());
    }

    srunner_set_tap(sr, "-");
    /*
     * Set up console logging */
    struct log_config *lc = log_config_init_for_console(LOG_LEVEL_INFO, NULL);
    log_start_from_param(lc);
    log_config_free(lc);
    /* Disable stdout buffering, as this can confuse the error
     * reporting when running in libcheck fork mode */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Initialise modules */
    ssl_init();
    os_calls_signals_init();

    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ssl_finish();
    os_calls_signals_deinit();

    log_end();
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
