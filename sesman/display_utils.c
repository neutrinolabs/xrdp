/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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

/**
 *
 * @file display_utils.c
 * @brief Definition of utility calls related to display handling
 * @author Matt Burt
 */


#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <ctype.h>

#include "arch.h"
#include "display_utils.h"
#include "log.h"
#include "os_calls.h"
#include "sesman.h"
#include "sesman_config.h"
#include "set_int.h"
#include "xrdp_sockets.h"

static int
check_process_running(int display)
{
    DIR *dir;
    struct dirent *ent;
    char path[256];
    char buf[4096];
    char display_param[32];
    int found = 0;

    g_snprintf(display_param, sizeof(display_param), ":%d", display);

    if (!(dir = opendir("/proc")))
    {
        return 0;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        if (!isdigit(ent->d_name[0]))
        {
            continue;
        }

        g_snprintf(path, sizeof(path), "/proc/%s/cmdline", ent->d_name);
        
        int fd = g_file_open_ro(path);
        if (fd == -1)
        {
            continue;
        }

        int len = g_file_read(fd, buf, sizeof(buf) - 1);
        g_file_close(fd);

        if (len > 0)
        {
            int i;
            buf[len] = '\0';
            for (i = 0; i < len; ++i)
            {
                if (buf[i] == '\0')
                {
                    buf[i] = ' ';
                }
            }

            if ((strstr(buf, "Xorg") != NULL ||
                 strstr(buf, "Xvnc") != NULL ||
                 strstr(buf, "Xxrdp") != NULL) &&
                strstr(buf, display_param) != NULL)
            {
                found = 1;
                break;
            }
        }
    }

    closedir(dir);
    return found;
}

/******************************************************************************/
/**
 *
 * @brief checks if there's a server running on a display
 * @param display the display to check
 * @return 0 if there isn't a display running, nonzero otherwise
 *
 */
static int
x_server_running_check_ports(int display)
{
    char text[256];
    int x_running;
    int sck;

    x_running = check_process_running(display);

    if (!x_running)
    {
        (void)snprintf(text, sizeof(text), X11_UNIX_SOCKET_STR, display);
        x_running = g_file_exist(text);
    }

    if (!x_running)
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        (void)snprintf(text, sizeof(text), "/tmp/.X%d-lock", display);
        x_running = g_file_exist(text);
    }

    if (!x_running) /* check 59xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            (void)snprintf(text, sizeof(text), "59%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 60xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            (void)snprintf(text, sizeof(text), "60%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (!x_running) /* check 62xx */
    {
        LOG(LOG_LEVEL_DEBUG, "Did not find a running X server at %s", text);
        if ((sck = g_tcp_socket()) != -1)
        {
            (void)snprintf(text, sizeof(text), "62%2.2d", display);
            x_running = g_tcp_bind(sck, text);
            g_tcp_close(sck);
        }
    }

    if (x_running)
    {
        LOG(LOG_LEVEL_INFO, "Found X server running for display %d", display);
    }

    return x_running;
}

/******************************************************************************/
int
display_utils_get_free_display(const struct set_int *alloc_displays)
{
    int display;

    for (display = g_cfg->sess.x11_display_offset;
            (unsigned int)display <= g_cfg->sess.max_display_number;
            ++display)
    {
        // Have we already allocated this one?
        if (!set_int_contains(alloc_displays, display))
        {
            if (!x_server_running_check_ports(display))
            {
                return display;
            }
        }
    }

    LOG(LOG_LEVEL_ERROR,
        "X server -- no display in range (%d to %d) is available",
        g_cfg->sess.x11_display_offset,
        g_cfg->sess.max_display_number);

    return -1;
}
