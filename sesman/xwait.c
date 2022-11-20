#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xwait.h"

#include <stdio.h>
#include <string.h>

/******************************************************************************/
int
wait_for_xserver(int display)
{
    FILE *dp = NULL;
    int ret = 0;
    char buffer[100];
    char exe_cmd[262];

    LOG(LOG_LEVEL_DEBUG, "Waiting for X server to start on display %d", display);

    g_snprintf(exe_cmd, sizeof(exe_cmd), "%s/xrdp-waitforx", XRDP_BIN_PATH);
    dp = popen(exe_cmd, "r");
    if (dp == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Unable to launch xrdp-waitforx");
        return 1;
    }

    while (fgets(buffer, 100, dp))
    {
        g_strtrim(buffer, 2);
        LOG(LOG_LEVEL_DEBUG, "%s", buffer);
    }

    ret = pclose(dp);
    if (ret != 0)
    {
        LOG(LOG_LEVEL_ERROR, "An error occurred while running xrdp-waitforx");
        return 0;
    }


    return 1;
}
