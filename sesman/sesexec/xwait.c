#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "env.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "xwait.h"

#include <stdio.h>
#include <string.h>

/******************************************************************************/
static void
log_waitforx_messages(FILE *dp)
{
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), dp))
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
}

/******************************************************************************/
/**
 * Contruct the command to run to check the X server
 */
static struct list *
make_xwait_command(const char *display)
{
    const char exe[] = XRDP_LIBEXEC_PATH "/waitforx";

    struct list *cmd = list_create();
    if (cmd != NULL)
    {
        cmd->auto_free = 1;

        if (!list_add_strdup_multi(cmd, exe, NULL))
        {
            goto fail;
        }

        if (display != NULL)
        {
            if (!list_add_strdup_multi(cmd, "-d", display, NULL))
            {
                goto fail;
            }
        }
    }

    return cmd;

fail:
    list_delete(cmd);
    return NULL;

}

/******************************************************************************/
/*
 * Gets the display from the environment */
static const char *
get_display(struct list *env_names,
            struct list *env_values)
{
    int i;
    for (i = 0; i < env_names->count; ++i)
    {
        if (strcmp((const char *)list_get_item(env_names, i), "DISPLAY") == 0)
        {
            return (const char *)list_get_item(env_values, i);
        }
    }

    return NULL;
}

/******************************************************************************/
enum xwait_status
wait_for_xserver(uid_t uid,
                 struct list *env_names,
                 struct list *env_values)
{
    enum xwait_status rv = XW_STATUS_MISC_ERROR;
    int fd[2] = {-1, -1};

    // Construct the command to execute to check the display
    const char *display = get_display(env_names, env_values);
    struct list *cmd = make_xwait_command(display);
    if (cmd == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't create xwait command list - no mem");
    }
    else if (g_pipe(fd) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't create pipe : %s", g_get_strerror());
    }
    else
    {
        pid_t pid = g_fork();
        if (pid < 0)
        {
            // Error already logged
        }
        else if (pid == 0)
        {
            /* Child process */

            /* Send stdout and stderr up the pipe */
            g_file_close(fd[0]);
            g_file_duplicate_on(fd[1], 1);
            g_file_duplicate_on(fd[1], 2);

            /* Move to the user context... */
            env_set_user(uid,
                         0,
                         env_names,
                         env_values);

            /* ...and run the program */
            g_execvp_list((const char *)cmd->items[0], cmd);
            LOG(LOG_LEVEL_ERROR, "Can't run %s - %s",
                (const char *)cmd->items[0], g_get_strerror());
            g_exit(rv);
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG,
                "Waiting for X server to start on display %s",
                (display != NULL) ? display : "???");

            g_file_close(fd[1]);
            fd[1] = -1;
            FILE *dp = fdopen(fd[0], "r");
            if (dp == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Unable to launch waitforx");
            }
            else
            {
                struct proc_exit_status e;

                fd[0] = -1; // File descriptor closed by fclose()
                log_waitforx_messages(dp);
                fclose(dp);
                e = g_waitpid_status(pid);
                switch (e.reason)
                {
                    case E_PXR_STATUS_CODE:
                        rv = (enum xwait_status)e.val;
                        break;

                    case E_PXR_SIGNAL:
                    {
                        char sigstr[MAXSTRSIGLEN];
                        LOG(LOG_LEVEL_ERROR,
                            "waitforx failed with unexpected signal %s",
                            g_sig2text(e.val, sigstr));
                    }
                    break;

                    default:
                        LOG(LOG_LEVEL_ERROR,
                            "waitforx failed with unknown reason");
                }
            }
        }
        if (fd[0] >= 0)
        {
            g_file_close(fd[0]);
        }

        if (fd[1] >= 0)
        {
            g_file_close(fd[1]);
        }
    }

    list_delete(cmd);

    return rv;
}
