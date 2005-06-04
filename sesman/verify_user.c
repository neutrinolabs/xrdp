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
   Copyright (C) Jay Sorg 2005

   authenticate user

*/

#ifdef USE_PAM

#include <security/pam_userpass.h>

#define SERVICE "xrdp"

/******************************************************************************/
/* returns boolean */
int auth_userpass(char* user, char* pass)
{
  pam_handle_t* pamh;
  pam_userpass_t userpass;
  struct pam_conv conv = {pam_userpass_conv, &userpass};
  const void* template1;
  int status;

  userpass.user = user;
  userpass.pass = pass;
  if (pam_start(SERVICE, user, &conv, &pamh) != PAM_SUCCESS)
  {
    return 0;
  }
  status = pam_authenticate(pamh, 0);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  status = pam_acct_mgmt(pamh, 0);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  status = pam_get_item(pamh, PAM_USER, &template1);
  if (status != PAM_SUCCESS)
  {
    pam_end(pamh, status);
    return 0;
  }
  if (pam_end(pamh, PAM_SUCCESS) != PAM_SUCCESS)
  {
    return 0;
  }
  return 1;
}

#else

#define _XOPEN_SOURCE
#include <unistd.h>
#include <string.h>
#include <shadow.h>
#include <pwd.h>

/******************************************************************************/
/* returns boolean */
int auth_userpass(char* user, char* pass)
{
  char salt[13] = "$1$";
  char hash[35] = "";
  char* encr = 0;
  struct passwd* spw;
  struct spwd* stp;
  int saltcnt = 0;

  spw = getpwnam(user);
  if (spw == 0)
  {
    return 0;
  }
  if (strncmp(spw->pw_passwd, "x", 3) == 0)
  {
    /* the system is using shadow */
    stp = getspnam(user);
    if (stp == 0)
    {
      return 0;
    }
    strncpy(hash, stp->sp_pwdp, 34);
  }
  else
  {
    /* old system with only passwd */
    strncpy(hash, spw->pw_passwd, 34);
  }
  hash[34] = '\0';
  if (strncmp(hash, "$1$", 3) == 0)
  {
    /* gnu style crypt(); */
    saltcnt = 3;
    while ((hash[saltcnt] != '$') && (saltcnt < 11))
    {
      salt[saltcnt] = hash[saltcnt];
      saltcnt++;
    }
    salt[saltcnt] = '$';
    salt[saltcnt + 1] = '\0';
  }
  else
  {
    /* classic two char salt */
    salt[0] = hash[0];
    salt[1] = hash[1];
    salt[2] = '\0';
  }
  encr = crypt(pass,salt);
  if (strncmp(encr, hash, 34) != 0)
  {
    return 0;
  }
  return 1;
}

#endif
