
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <signal.h>

#include "os_calls.h"

#include "test_common.h"

static tintptr g_wobj1 = 0;

/******************************************************************************/
void
os_calls_signals_init(void)
{
    g_wobj1 = g_create_wait_obj("");
}

/******************************************************************************/
void
os_calls_signals_deinit(void)
{
    g_delete_wait_obj(g_wobj1);
    g_wobj1 = 0;
}

/******************************************************************************/
/**
 * Set the global wait object g_wobj1
 */
static void
set_wobj1(int signum)
{
    g_set_wait_obj(g_wobj1);
}

/******************************************************************************/
/**
 * Sends a number of signals to the process and checks they are all delivered
 *
 * @param sig Signal number
 * @param count Number of signals to send
 *
 * The caller is expected to establish a signal handler before this call
 * which sets the global g_wobj1 on signal delivery */

static
void send_multiple_signals(int sig, unsigned int count)
{
    while (count-- > 0)
    {
        g_reset_wait_obj(g_wobj1);
        ck_assert_int_eq(g_is_wait_obj_set(g_wobj1), 0);
        // Expect the signal to be delivered synchronously
        raise(sig);
        ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);
    }
}

/******************************************************************************/
START_TEST(test_g_set_alarm)
{
    g_reset_wait_obj(g_wobj1);
    ck_assert_int_eq(g_is_wait_obj_set(g_wobj1), 0);

    g_set_alarm(set_wobj1, 1);

    g_obj_wait(&g_wobj1, 1, NULL, 0, 2000);

    ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);

    // Clean up
    g_set_alarm(NULL, 0);
}
END_TEST

/******************************************************************************/
START_TEST(test_g_signal_child_stop_1)
{
    struct proc_exit_status e;

    g_reset_wait_obj(g_wobj1);
    ck_assert_int_eq(g_is_wait_obj_set(g_wobj1), 0);

    g_signal_child_stop(set_wobj1);

    int pid = g_fork();

    if (pid == 0)
    {
        g_exit(45);
    }
    ck_assert_int_ne(pid, 0);
    g_obj_wait(&g_wobj1, 1, NULL, 0, 2000);
    ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);

    e = g_waitpid_status(pid);

    ck_assert_int_eq(e.reason, E_PXR_STATUS_CODE);
    ck_assert_int_eq(e.val, 45);

    // Try another one to make sure the signal handler is still in place.
    // This one can generate a signal
    g_reset_wait_obj(g_wobj1);

    pid = g_fork();
    if (pid == 0)
    {
        // Before raising the signal, change directory to a non-writeable
        // one to avoid generating a corefile.
        g_set_current_dir("/");
        raise(SIGUSR2);
    }
    ck_assert_int_ne(pid, 0);
    g_obj_wait(&g_wobj1, 1, NULL, 0, 2000);
    ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);

    e = g_waitpid_status(pid);

    ck_assert_int_eq(e.reason, E_PXR_SIGNAL);
    ck_assert_int_eq(e.val, SIGUSR2);

    // Clean up
    g_signal_child_stop(NULL);
}
END_TEST

/******************************************************************************/
/* Checks that multiple children finishing do not interrupt
 * g_waitpid_status() */
START_TEST(test_g_signal_child_stop_2)
{
#define CHILD_COUNT 20
    int pids[CHILD_COUNT];
    unsigned int i;

    struct proc_exit_status e;

    g_reset_wait_obj(g_wobj1);
    ck_assert_int_eq(g_is_wait_obj_set(g_wobj1), 0);

    g_signal_child_stop(set_wobj1);

    for (i = 0 ; i < CHILD_COUNT; ++i)
    {
        int pid = g_fork();
        if (pid == 0)
        {
            g_sleep((i + 1) * 100);
            g_exit(i + 1);
        }
        ck_assert_int_ne(pid, 0);
        pids[i] = pid;
    }
    g_obj_wait(&g_wobj1, 1, NULL, 0, 2000);
    ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);

    for (i = 0 ; i < CHILD_COUNT; ++i)
    {
        e = g_waitpid_status(pids[i]);
        ck_assert_int_eq(e.reason, E_PXR_STATUS_CODE);
        ck_assert_int_eq(e.val, (i + 1));
    }

    // Clean up
    g_signal_child_stop(NULL);
#undef CHILD_COUNT
}
END_TEST


/******************************************************************************/
START_TEST(test_g_signal_segfault)
{
    g_signal_segfault(set_wobj1);

    // Only one signal can be received in this way. After handling
    // the signal the handler should be automatically reset.
    send_multiple_signals(SIGSEGV, 1);

    g_signal_segfault(NULL);
}
END_TEST

/******************************************************************************/
START_TEST(test_g_signal_hang_up)
{
    g_signal_hang_up(set_wobj1);

    send_multiple_signals(SIGHUP, 5);

    g_signal_hang_up(NULL);
}

/******************************************************************************/
START_TEST(test_g_signal_user_interrupt)
{
    g_signal_user_interrupt(set_wobj1);

    send_multiple_signals(SIGINT, 5);

    g_signal_user_interrupt(NULL);
}

/******************************************************************************/
START_TEST(test_g_signal_terminate)
{
    g_signal_terminate(set_wobj1);

    send_multiple_signals(SIGTERM, 5);

    g_signal_terminate(NULL);
}

/******************************************************************************/
START_TEST(test_g_signal_pipe)
{
    g_signal_pipe(set_wobj1);

    send_multiple_signals(SIGPIPE, 5);

    g_signal_pipe(NULL);
}

/******************************************************************************/
START_TEST(test_g_signal_usr1)
{
    g_signal_usr1(set_wobj1);

    send_multiple_signals(SIGUSR1, 5);

    g_signal_usr1(NULL);
}

/******************************************************************************/
START_TEST(test_waitpid_not_interrupted_by_sig)
{
    /* Start a child which waits 3 seconds and exits */
    int child_pid = g_fork();
    if (child_pid == 0)
    {
        g_sleep(3000);
        g_exit(42);
    }

    /* Set an alarm for 1 second's time */
    g_reset_wait_obj(g_wobj1);
    g_set_alarm(set_wobj1, 1);

    struct proc_exit_status e = g_waitpid_status(child_pid);
    // We should have had the alarm...
    ck_assert_int_ne(g_is_wait_obj_set(g_wobj1), 0);

    // ..and got the status of the child
    ck_assert_int_eq(e.reason, E_PXR_STATUS_CODE);
    ck_assert_int_eq(e.val, 42);

    // Clean up
    g_set_alarm(NULL, 0);
}

/******************************************************************************/
TCase *
make_tcase_test_os_calls_signals(void)
{
    TCase *tc = tcase_create("oscalls-signals");

    tcase_add_test(tc, test_g_set_alarm);
    tcase_add_test(tc, test_g_signal_child_stop_1);
    tcase_add_test(tc, test_g_signal_child_stop_2);
    tcase_add_test(tc, test_g_signal_segfault);
    tcase_add_test(tc, test_g_signal_hang_up);
    tcase_add_test(tc, test_g_signal_user_interrupt);
    tcase_add_test(tc, test_g_signal_terminate);
    tcase_add_test(tc, test_g_signal_pipe);
    tcase_add_test(tc, test_g_signal_usr1);
    tcase_add_test(tc, test_waitpid_not_interrupted_by_sig);

    return tc;
}
