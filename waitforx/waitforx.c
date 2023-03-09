#include <X11/extensions/Xrandr.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <unistd.h>

#include "config_ac.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xwait.h" // For return status codes

#define ATTEMPTS 10
#define ALARM_WAIT 30

/*****************************************************************************/
static void
alarm_handler(int signal_num)
{
    /* Avoid printf() in signal handler (see signal-safety(7))
     *
     * Prefix the message with a newline in case another message
     * has been partly output */
    const char msg[] = "\n<E>Timed out waiting for RandR outputs\n";
    g_file_write(1, msg, g_strlen(msg));
    exit(XW_STATUS_TIMED_OUT);
}

/*****************************************************************************/
static Display *
open_display(const char *display)
{
    Display *dpy = NULL;
    unsigned int wait = ATTEMPTS;
    unsigned int n;

    for (n = 1; n <= ATTEMPTS; ++n)
    {
        printf("<D>Opening display %s. Attempt %u of %u\n", display, n, wait);
        dpy = XOpenDisplay(display);
        if (dpy != NULL)
        {
            printf("<D>Opened display %s\n", display);
            break;
        }
        g_sleep(1000);
    }

    return dpy;
}

/*****************************************************************************/
/**
 * Wait for the RandR extension (if in use) to be available
 *
 * @param dpy Display
 * @return 0 if/when outputs are available, 1 otherwise
 */
static int
wait_for_r_and_r(Display *dpy)
{
    int error_base = 0;
    int event_base = 0;
    unsigned int outputs = 0;
    unsigned int wait = ATTEMPTS;
    unsigned int n;

    XRRScreenResources *res = NULL;

    if (!XRRQueryExtension(dpy, &event_base, &error_base))
    {
        printf("<I>RandR not supported on display %s\n",
               DisplayString(dpy));
        return 0;
    }

    for (n = 1; n <= wait; ++n)
    {
        printf("<D>Waiting for outputs. Attempt %u of %u\n", n, wait);
        res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
        if (res != NULL)
        {
            if (res->noutput > 0)
            {
                outputs = res->noutput;
            }
            XRRFreeScreenResources(res);
        }

        if (outputs > 0)
        {
            printf("<D>Display %s ready with %u RandR outputs\n",
                   DisplayString(dpy), outputs);
            return 0;
        }
        g_sleep(1000);
    }

    printf("<E>Unable to find any RandR outputs\n");
    return 1;
}

/*****************************************************************************/
static void
usage(const char *argv0, int status)
{
    printf("Usage: %s -d display\n", argv0);
    exit(status);
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    const char *display_name = NULL;
    int opt;
    int status = XW_STATUS_MISC_ERROR;
    Display *dpy = NULL;

    /* Disable stdout buffering so any messages are passed immediately
     * to sesman */
    setvbuf(stdout, NULL, _IONBF, 0);

    while ((opt = getopt(argc, argv, "d:")) != -1)
    {
        switch (opt)
        {
            case 'd':
                display_name = optarg;
                break;
            default: /* '?' */
                usage(argv[0], status);
        }
    }

    if (!display_name)
    {
        usage(argv[0], status);
    }

    g_set_alarm(alarm_handler, ALARM_WAIT);

    dpy = open_display(display_name);
    if (!dpy)
    {
        printf("<E>Unable to open display %s\n", display_name);
        status = XW_STATUS_FAILED_TO_START;
    }
    else
    {
        if (wait_for_r_and_r(dpy) == 0)
        {
            status = XW_STATUS_OK;
        }
        XCloseDisplay(dpy);
    }

    return status;
}
