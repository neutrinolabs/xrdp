/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 * @file env.c
 * @brief User environment handling code
 * @author Jay Sorg
 *
 */

#include "sesman.h"

#include "sys/types.h"
#include "grp.h"

extern unsigned char g_fixedkey[8]; /* in sesman.c */
extern struct config_sesman *g_cfg;  /* in sesman.c */

/******************************************************************************/
int DEFAULT_CC
env_check_password_file(char *filename, char *password)
{
    char encryptedPasswd[16];
    int fd;

    g_memset(encryptedPasswd, 0, 16);
    g_strncpy(encryptedPasswd, password, 8);
    rfbDesKey(g_fixedkey, 0);
    rfbDes((unsigned char *)encryptedPasswd, (unsigned char *)encryptedPasswd);
    fd = g_file_open(filename);

    if (fd == -1)
    {
        log_message(LOG_LEVEL_WARNING,
                    "can't read vnc password file - %s",
                    filename);
        return 1;
    }

    g_file_write(fd, encryptedPasswd, 8);
    g_file_close(fd);
    return 0;
}

/******************************************************************************/
int DEFAULT_CC
env_set_user(char *username, char *passwd_file, int display)
{
    int error;
    int pw_uid;
    int pw_gid;
    int uid;
    char pw_shell[256];
    char pw_dir[256];
    char pw_gecos[256];
    char text[256];

    error = g_getuser_info(username, &pw_gid, &pw_uid, pw_shell, pw_dir,
                           pw_gecos);

    if (error == 0)
    {
        g_rm_temp_dir();
        error = g_setgid(pw_gid);

        if (error == 0)
        {
            error = g_initgroups(username, pw_gid);
        }

        if (error == 0)
        {
            uid = pw_uid;
            error = g_setuid(uid);
        }

        g_mk_temp_dir(0);

        if (error == 0)
        {
            g_clearenv();
            g_setenv("SHELL", pw_shell, 1);
            g_setenv("PATH", "/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin", 1);
            g_setenv("USER", username, 1);
            g_sprintf(text, "%d", uid);
            g_setenv("UID", text, 1);
            g_setenv("HOME", pw_dir, 1);
            g_set_current_dir(pw_dir);
            g_sprintf(text, ":%d.0", display);
            g_setenv("DISPLAY", text, 1);

            if (passwd_file != 0)
            {
                if (0 == g_cfg->auth_file_path)
                {
                    /* if no auth_file_path is set, then we go for
                       $HOME/.vnc/sesman_username_passwd */
                    g_mkdir(".vnc");
                    g_sprintf(passwd_file, "%s/.vnc/sesman_%s_passwd", pw_dir, username);
                }
                else
                {
                    /* we use auth_file_path as requested */
                    g_sprintf(passwd_file, g_cfg->auth_file_path, username);
                }

                LOG_DBG("pass file: %s", passwd_file);
            }
        }
    }
    else
    {
        log_message(LOG_LEVEL_ERROR,
                    "error getting user info for user %s", username);
    }

    return error;
}
