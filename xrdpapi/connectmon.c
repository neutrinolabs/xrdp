/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
 * Copyright (C) Laxmikant Rashinkar 2012-2013
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
 * sample program to demonstrate use of xrdpapi connection monitoring
 *
 */

/*
 * build instructions:
 *     gcc connectmon.c -o connectmon \
 *         -I.. -I../common -L./.libs -L../common/.libs \
 *         -DHAVE_CONFIG_H -lxrdpapi -lcommon
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdpapi.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <errno.h>

/******************************************************************************/
static void
print_current_connection_state(void)
{
    WTS_CONNECTSTATE_CLASS connect_state;
    char buff[64];
    const char *statestr = "unavailable";
    if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION,
                                    WTSConnectState,
                                    &connect_state, NULL, NULL))
    {
        if (connect_state == WTSConnected)
        {
            statestr = "connected";
        }
        else if (connect_state == WTSDisconnected)
        {
            statestr = "disconnected";
        }
        else
        {
            snprintf(buff, sizeof(buff),
                     "unrecognised val %d", (int)connect_state);
            statestr = buff;
        }
    }
    printf("Current connect state is %s\n", statestr);
}

/******************************************************************************/
static LRESULT
callback (void *cbdata, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 1;
    int *countptr = (int *)cbdata;

    switch (wParam)
    {
        case WTS_REMOTE_CONNECT:
            ++*countptr;
            printf("State change %d : Connect to session\n", *countptr);
            print_current_connection_state();
            break;

        case WTS_REMOTE_DISCONNECT:
            ++*countptr;
            printf("State change %d : Disconnect from session\n", *countptr);
            print_current_connection_state();
            break;

        default:
            printf("** Unexpected callback reason %ld\n", (long)wParam);
            result = 0;
    }

    return result;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int result = 0;
    int changes = 10;
    int fd;
    struct log_config *lc;

    if ((lc = log_config_init_for_console(LOG_LEVEL_DEBUG, NULL)) != NULL)
    {
        log_start_from_param(lc);
    }

    if (argc > 1 && atoi(argv[1]) >= 0)
    {
        changes = atoi(argv[1]);
        if (changes > 1000)
        {
            changes = 1000; // Use 0 for more than this
        }
    }
    if (changes > 0)
    {
        printf("Connection monitor : exiting after %d state changes\n", changes);
    }
    else
    {
        printf("Connection monitor\n");
    }

    print_current_connection_state();


    if (!WTSRegisterSessionNotificationEx(WTS_CURRENT_SERVER_HANDLE, &fd, 0, NULL))
    {
        result = 1; // Error occurred (should be logged)
    }
    else
    {
        int count = 0;
        while (result == 0)
        {
            struct pollfd pollarg =
            {
                .fd = fd,
                .events = (POLLIN | POLLERR),
                .revents = 0
            };
            int pollstat;
            LRESULT cbresult;

            if (changes > 0 && count >= changes)
            {
                break;
            }

            if ((pollstat = poll(&pollarg, 1, -1)) < 0)
            {
                printf("** Error from poll() : %s\n", strerror(errno));
                result = 1;
            }
            else if (pollstat == 0)
            {
                printf("** Timeout from poll() !?\n");
                result = 1;
            }
            else if ((pollarg.revents & POLLERR))
            {
                printf("** File descriptor was closed\n");
                result = 1;
            }
            else if (!WTSGetDispatchMessage(&count, callback, &cbresult))
            {
                printf("** Unexpected faiure to dispatch message\n");
                result = 1;
            }
            else if (!cbresult)
            {
                printf("** Callback failed\n");
                result = 1;
            }
        }

        (void)WTSUnRegisterSessionNotificationEx(WTS_CURRENT_SERVER_HANDLE,
                fd, NULL);
    }

    if (lc != NULL)
    {
        log_config_free(lc);
        log_end();
    }

    return result;
}
