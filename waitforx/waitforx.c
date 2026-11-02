#include <X11/extensions/Xrandr.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "config_ac.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xrdp_sockets.h"
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
    const char msg[] = "\n<E>Timed out waiting for X display\n";
    g_file_write(1, msg, g_strlen(msg));
    exit(XW_STATUS_TIMED_OUT);
}

/*****************************************************************************/
/***
 * Checks whether display can be reached via a Unix Domain Socket socket.
 *
 * Local displays can be reached by a Unix Domain socket. The display
 * string will be of the form ':n' or ':n.m' where 'n' and 'm'
 * are unsigned numbers
 *
 * @param display Display string
 * @param[out] sock_name sock_name or ""
 * @param sock_name_len Length of sock_name
 * @return !=0 if sock_name is not NULL
 */
static int
get_display_sock_name(const char *display, char *sock_name,
                      size_t sock_name_len)
{
    int local = 0;
    int dnum = 0;
    if (display != NULL && *display++ == ':' && isdigit(*display))
    {
        do
        {
            if (dnum > (INT_MAX / 10 - 1))
            {
                break; // Avoid signed integer overflow
            }
            dnum = (dnum * 10) + (*display - '0');
            ++display;
        }
        while (isdigit(*display));

        // Skip the optional screen identifier
        if (*display == '.' && isdigit(*(display + 1)))
        {
            do
            {
                ++display;
            }
            while (isdigit(*display));
        }

        local = (*display == '\0');
    }

    if (local)
    {
        snprintf(sock_name, sock_name_len, X11_UNIX_SOCKET_STR, dnum);
    }
    else
    {
        sock_name[0] = '\0';
    }


    return (sock_name[0] != '\0');
}

/*****************************************************************************/
static Display *
open_display(const char *display)
{
    char sock_name[XRDP_SOCKETS_MAXPATH];
    int local_fd = -1;
    Display *dpy = NULL;
    unsigned int wait = ATTEMPTS;
    unsigned int n;

    // If the display is local, we try to connect to the X11 socket for
    // the display first. If we can't do this, we don't attempt to open
    // the display.
    //
    // This is to ensure the display open code in libxcb doesn't attempt
    // to connect to the X server over TCP. This can block if the network
    // is configured in an unexpected way, which leads to use failing
    // to detect the X server starting up shortly after.
    //
    // Some versions of libxcb support a 'unix:' prefix to the display
    // string to allow a connection to be restricted to a local socket.
    // This is not documented, and varies significantly between versions
    // of libxcb. We can't use it here.
    if (get_display_sock_name(display, sock_name, sizeof(sock_name)) != 0)
    {
        for (n = 1; n <= wait ; ++n)
        {
            printf("<D>Opening socket %s. Attempt %u of %u\n",
                   sock_name, n, wait);
            if ((local_fd = g_sck_local_socket()) >= 0)
            {
                if  (g_sck_local_connect(&local_fd, sock_name, NULL, 0) == 0)
                {
                    printf("<D>Socket '%s' open succeeded.\n", sock_name);
                    break;
                }
                else
                {
                    printf("<D>Socket '%s' open failed [%s].\n",
                           sock_name, g_get_strerror());
                    g_file_close(local_fd);
                    local_fd = -1;
                }
            }
            g_sleep(1000);
        }

        // Subtract the wait time for this stage from the overall wait time
        wait -= (n - 1);
    }

    for (n = 1; n <= wait; ++n)
    {
        printf("<D>Opening display '%s'. Attempt %u of %u\n", display, n, wait);
        if ((dpy = XOpenDisplay(display)) != NULL)
        {
            printf("<D>Opened display %s\n", display);
            break;
        }
        g_sleep(1000);
    }

    // Close the file after we try the display open, to prevent
    // a display reset if our connect was the last client.
    if (local_fd >= 0)
    {
        g_file_close(local_fd);
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
