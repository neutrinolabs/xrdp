#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "env.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"
#include "login_info.h"
#include "sesman_auth.h"

#include "display_server_util.h"

#include <stdio.h>
#include <string.h>

/******************************************************************************/
static void
process_helper_messages(FILE *dp,
                        const char *prog,
                        void (*setenvvar)(const char *name,
                                const char *value,
                                void *closure),
                        void *closure)
{
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), dp))
    {
        enum logLevels level = LOG_LEVEL_ERROR;

        g_strtrim(buffer, 2);

        // Has the message got a class at the start?
        if (strlen(buffer) > 3 && buffer[0] == '<' && buffer[2] == '>')
        {
            if (buffer[1] == 'V')
            {
                // Environment variable
                const char *var = buffer + 3;
                char *val = g_strchr(var, '=');
                if (val == NULL)
                {
                    LOG(LOG_LEVEL_ERROR, "%s sent over bad message '%s'",
                        prog, buffer);
                }
                else
                {
                    *val++ = '\0'; // Terminate 'var', and move to the value
                    if (setenvvar == NULL)
                    {
                        LOG(LOG_LEVEL_WARNING, "%s can't set '%s'",
                            prog, buffer);
                    }
                    else
                    {
                        (*setenvvar)(var, val, closure);
                    }
                }
            }
            else
            {
                // It's a log message
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
                const char *msg = buffer + 3;

                if (strlen(msg) > 0)
                {
                    LOG(level, "%s: %s", prog, msg);
                }
            }
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "%s sent over bad message '%s'",
                prog, buffer);
        }
    }
}

/******************************************************************************/
enum display_server_status
wait_for_display_server(struct login_info *login_info,
                        const struct list *env_names,
                        const struct list *env_values,
                        struct list *cmd,
                        void (*setenvvar)(const char *name,
                                const char *value,
                                void *closure),
                        void *closure)
{
    enum display_server_status rv = DS_STATUS_MISC_ERROR;
    int fd[2] = {-1, -1};
    const char *helper = (const char *)cmd->items[0];

    // Get the unqualified name of the helper
    const char *prog = g_strrchr(helper, '/');
    if (prog != NULL)
    {
        ++prog;
    }
    else
    {
        prog = helper;
    }

    if (g_pipe(fd) != 0)
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
            env_set_user(login_info->uid,
                         0,
                         env_names,
                         env_values);
            auth_set_env(login_info->auth_info);

            /* ...and run the program */
            g_execvp_list(helper, cmd);
            LOG(LOG_LEVEL_ERROR, "Can't run %s - %s",
                helper, g_get_strerror());
            g_exit(rv);
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG,
                "Waiting for display server to start");

            g_file_close(fd[1]);
            fd[1] = -1;
            FILE *dp = fdopen(fd[0], "r");
            if (dp == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Unable to launch %s", prog);
            }
            else
            {
                struct proc_exit_status e;

                fd[0] = -1; // File descriptor closed by fclose()
                process_helper_messages(dp, prog, setenvvar, closure);
                fclose(dp);
                e = g_waitpid_status(pid);
                switch (e.reason)
                {
                    case E_PXR_STATUS_CODE:
                        rv = (enum display_server_status)e.val;
                        break;

                    case E_PXR_SIGNAL:
                    {
                        char sigstr[MAXSTRSIGLEN];
                        LOG(LOG_LEVEL_ERROR,
                            "%s failed with unexpected signal %s",
                            prog, g_sig2text(e.val, sigstr));
                    }
                    break;

                    default:
                        LOG(LOG_LEVEL_ERROR,
                            "%s failed with unknown reason", prog);
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

    return rv;
}
