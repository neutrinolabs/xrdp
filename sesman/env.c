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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <grp.h>

#include "list.h"
#include "sesman.h"
#include "ssl_calls.h"

extern unsigned char g_fixedkey[8]; /* in sesman.c */
extern struct config_sesman *g_cfg;  /* in sesman.c */

/******************************************************************************/
int
env_check_password_file(const char *filename, const char *passwd)
{
    char encryptedPasswd[16];
    char key[24];
    char passwd_hash[20];
    char passwd_hash_text[40];
    int fd;
    int passwd_bytes;
    void *des;
    void *sha1;

    /* create password hash from password */
    passwd_bytes = g_strlen(passwd);
    sha1 = ssl_sha1_info_create();
    ssl_sha1_transform(sha1, "xrdp_vnc", 8);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_complete(sha1, passwd_hash);
    ssl_sha1_info_delete(sha1);
    g_snprintf(passwd_hash_text, 39, "%2.2x%2.2x%2.2x%2.2x",
               (tui8)passwd_hash[0], (tui8)passwd_hash[1],
               (tui8)passwd_hash[2], (tui8)passwd_hash[3]);
    passwd_hash_text[39] = 0;
    passwd = passwd_hash_text;

    /* create file from password */
    g_memset(encryptedPasswd, 0, sizeof(encryptedPasswd));
    g_strncpy(encryptedPasswd, passwd, 8);
    g_memset(key, 0, sizeof(key));
    g_mirror_memcpy(key, g_fixedkey, 8);
    des = ssl_des3_encrypt_info_create(key, 0);
    ssl_des3_encrypt(des, 8, encryptedPasswd, encryptedPasswd);
    ssl_des3_info_delete(des);
    fd = g_file_open_ex(filename, 0, 1, 1, 1);
    if (fd == -1)
    {
        log_message(LOG_LEVEL_WARNING,
                    "Cannot write VNC password hash to file %s: %s",
                    filename, g_get_strerror());
        return 1;
    }
    g_file_write(fd, encryptedPasswd, 8);
    g_file_close(fd);
    return 0;
}

/******************************************************************************/
/*  its the responsibility of the caller to free passwd_file                  */
int
env_set_user(const char *username, char **passwd_file, int display,
             const struct list *env_names, const struct list *env_values)
{
    int error;
    int pw_uid;
    int pw_gid;
    int uid;
    int index;
    int len;
    char *name;
    char *value;
    char *pw_shell;
    char *pw_dir;
    char text[256];
    char hostname[256];

    pw_shell = 0;
    pw_dir = 0;

    error = g_getuser_info(username, &pw_gid, &pw_uid, &pw_shell, &pw_dir, 0);

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

        g_mk_socket_path(0);

        if (error == 0)
        {
            g_clearenv();
            g_setenv("SHELL", pw_shell, 1);
            g_setenv("PATH", "/sbin:/bin:/usr/bin:/usr/local/bin", 1);
            g_setenv("USER", username, 1);
            g_setenv("LOGNAME", username, 1);
            g_sprintf(text, "%d", uid);
            g_setenv("UID", text, 1);
            g_setenv("HOME", pw_dir, 1);
            g_set_current_dir(pw_dir);
            g_sprintf(text, ":%d.0", display);
            g_setenv("DISPLAY", text, 1);
            g_setenv("XRDP_SESSION", "1", 1);
            /* XRDP_SOCKET_PATH should be set even here, chansrv uses this */
            g_setenv("XRDP_SOCKET_PATH", XRDP_SOCKET_PATH, 1);
            if ((env_names != 0) && (env_values != 0) &&
                (env_names->count == env_values->count))
            {
                for (index = 0; index < env_names->count; index++)
                {
                    name = (char *) list_get_item(env_names, index),
                    value = (char *) list_get_item(env_values, index),
                    g_setenv(name, value, 1);
                }
            }
            g_gethostname(hostname, 255);
            hostname[255] = 0;
            if (passwd_file != 0)
            {
                if (0 == g_cfg->auth_file_path)
                {
                    /* if no auth_file_path is set, then we go for
                     $HOME/.vnc/sesman_passwd-USERNAME@HOSTNAME:DISPLAY */
                    if (!g_directory_exist(".vnc"))
                    {
                        if (g_mkdir(".vnc") < 0)
                        {
                            log_message(LOG_LEVEL_ERROR,
                                        "Error creating .vnc directory: %s",
                                        g_get_strerror());
                        }
                    }

                    len = g_snprintf(NULL, 0, "%s/.vnc/sesman_passwd-%s@%s:%d",
                                     pw_dir, username, hostname, display);

                    *passwd_file = (char *) g_malloc(len + 1, 1);
                    if (*passwd_file != NULL)
                    {
                        /* Try legacy names first, remove if found */
                        g_sprintf(*passwd_file, "%s/.vnc/sesman_%s_passwd:%d",
                                  pw_dir, username, display);
                        if (g_file_exist(*passwd_file))
                        {
                            log_message(LOG_LEVEL_WARNING, "Removing old "
                                        "password file %s", *passwd_file);
                            g_file_delete(*passwd_file);
                        }
                        g_sprintf(*passwd_file, "%s/.vnc/sesman_%s_passwd",
                                  pw_dir, username);
                        if (g_file_exist(*passwd_file))
                        {
                            log_message(LOG_LEVEL_WARNING, "Removing insecure "
                                        "password file %s", *passwd_file);
                            g_file_delete(*passwd_file);
                        }
                        g_sprintf(*passwd_file, "%s/.vnc/sesman_passwd-%s@%s:%d",
                                  pw_dir, username, hostname, display);
                    }
                }
                else
                {
                    /* we use auth_file_path as requested */
                    len = g_snprintf(NULL, 0, g_cfg->auth_file_path, username);

                    *passwd_file = (char *) g_malloc(len + 1, 1);
                    if (*passwd_file != NULL)
                    {
                        g_sprintf(*passwd_file, g_cfg->auth_file_path, username);
                    }
                }

                if (*passwd_file != NULL)
                {
                    LOG_DBG("pass file: %s", *passwd_file);
                }
            }

            g_free(pw_dir);
            g_free(pw_shell);
        }
    }
    else
    {
        log_message(LOG_LEVEL_ERROR,
                    "error getting user info for user %s",
                    username);
    }

    return error;
}
