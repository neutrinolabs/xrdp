/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2007
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
extern struct config_sesman g_cfg; 

/******************************************************************************/
int DEFAULT_CC
env_check_password_file(char* filename, char* password)
{
  char encryptedPasswd[16];
  int fd;

  g_memset(encryptedPasswd, 0, 16);
  g_strncpy(encryptedPasswd, password, 8);
  rfbDesKey(g_fixedkey, 0);
  rfbDes((unsigned char*)encryptedPasswd, (unsigned char*)encryptedPasswd);
  fd = g_file_open(filename);
  if (fd == -1)
  {
    log_message(LOG_LEVEL_WARNING, "can't read vnc password file - %s",
                filename);
    return 1;
  }
  g_file_write(fd, encryptedPasswd, 8);
  g_file_close(fd);
  g_set_file_rights(filename, 1, 1); /* set read and write flags */
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
env_set_user(char* username, char* passwd_file, int display)
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
        if (0==g_cfg.auth_file_path)
        {
          /* if no auth_file_path is set, then we go for $HOME/.vnc/sesman_passwd */
          g_mkdir(".vnc");
          g_sprintf(passwd_file, "%s/.vnc/sesman_passwd", pw_dir);
        }
	else
	{
          /* we use auth_file_path as requested */
          g_sprintf(passwd_file, g_cfg.auth_file_path, username);
        }
	LOG_DBG("pass file: %s", passwd_file);
      }
    }
  }
  else
  {
    log_message(LOG_LEVEL_ERROR, "error getting user info for user %s", username);
  }
  return error;
}
