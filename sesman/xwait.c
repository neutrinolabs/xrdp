#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xwait.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

/******************************************************************************/
enum xwait_status
wait_for_xserver(int display)
{
    FILE *dp = NULL;
    enum xwait_status rv = XW_STATUS_MISC_ERROR;
    int ret;

    char buffer[100];
    const char exe[] = XRDP_LIBEXEC_PATH "/waitforx";
    char cmd[sizeof(exe) + 64];

    if (!g_file_exist(exe))
    {
        LOG(LOG_LEVEL_ERROR, "Unable to find %s", exe);
        return rv;
    }

    g_snprintf(cmd, sizeof(cmd), "%s -d :%d", exe, display);
    LOG(LOG_LEVEL_DEBUG, "Waiting for X server to start on display :%d",
        display);

    dp = popen(cmd, "r");
    if (dp == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Unable to launch waitforx");
        return rv;
    }

    while (fgets(buffer, 100, dp))
    {
        const char *msg = buffer;
        enum logLevels level = LOG_LEVEL_ERROR;

        g_strtrim(buffer, 2);

        // Has the message got a class at the start?
        if (strlen(buffer) > 3 && buffer[0] == '<' && buffer[2] == '>')
        {
            switch (buffer[1])
            {
                case 'D':
                    level = LOG_LEVEL_DEBUG;
                    break;
                case 'I':
                    level = LOG_LEVEL_INFO;
                    break;
                case 'W':
                    level = LOG_LEVEL_WARNING;
                    break;
                default:
                    level = LOG_LEVEL_ERROR;
                    break;
            }
            msg = buffer + 3;
        }

        if (strlen(msg) > 0)
        {
            LOG(level, "waitforx: %s", msg);
        }
    }

    ret = pclose(dp);
    if (WIFEXITED(ret))
    {
        rv = (enum xwait_status)WEXITSTATUS(ret);
    }
    else if (WIFSIGNALED(ret))
    {
        int sig = WTERMSIG(ret);
        LOG(LOG_LEVEL_ERROR, "waitforx failed with unexpected signal %d",
            sig);
    }

    return rv;
}
