#include <X11/extensions/Xrandr.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <unistd.h>

#include "config_ac.h"
#include "os_calls.h"
#include "string_calls.h"

#define ATTEMPTS 10
#define ALARM_WAIT 30

void
alarm_handler(int signal_num)
{
    /* Avoid printf() in signal handler (see signal-safety(7)) */
    const char msg[] = "Timed out waiting for RandR outputs\n";
    g_file_write(1, msg, g_strlen(msg));
    exit(1);
}

int
main(int argc, char **argv)
{
    char *display = NULL;
    int error_base = 0;
    int event_base = 0;
    int n = 0;
    int outputs = 0;
    int wait = ATTEMPTS;

    Display *dpy = NULL;
    XRRScreenResources *res = NULL;

    display = getenv("DISPLAY");

    g_set_alarm(alarm_handler, ALARM_WAIT);

    if (!display)
    {
        printf("DISPLAY is null");
        exit(1);
    }

    for (n = 1; n <= wait; ++n)
    {
        dpy = XOpenDisplay(display);
        printf("Opening display %s. Attempt %d of %d\n", display, n, wait);
        if (dpy != NULL)
        {
            printf("Opened display %s\n", display);
            break;
        }
        g_sleep(1000);
    }

    if (!dpy)
    {
        printf("Unable to open display %s\n", display);
        exit(1);
    }

    if (!XRRQueryExtension(dpy, &event_base, &error_base))
    {
        printf("RandR not supported on display %s", display);
    }
    else
    {
        for (n = 1; n <= wait; ++n)
        {
            res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
            printf("Waiting for outputs. Attempt %d of %d\n", n, wait);
            if (res != NULL)
            {
                if (res->noutput > 0)
                {
                    outputs = res->noutput;
                    XRRFreeScreenResources(res);
                    printf("Found %d output[s]\n", outputs);
                    break;
                }
                XRRFreeScreenResources(res);
            }
            g_sleep(1000);
        }

        if (outputs > 0)
        {
            printf("display %s ready with %d outputs\n", display, res->noutput);
        }
        else
        {
            printf("Unable to find any outputs\n");
            exit(1);
        }
    }

    exit(0);
}
