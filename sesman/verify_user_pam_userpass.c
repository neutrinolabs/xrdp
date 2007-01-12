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
 * @file verify_user_pam_userpass.c
 * @brief Authenticate user using pam_userpass module
 * @author Jay Sorg
 * 
 */

#include "arch.h"
#include "os_calls.h"

#include <security/pam_userpass.h>

#define SERVICE "xrdp"

/******************************************************************************/
/* returns boolean */
int DEFAULT_CC
auth_userpass(char* user, char* pass)
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

/******************************************************************************/
/* returns error */
int DEFAULT_CC
auth_start_session(void)
{
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
auth_end(void)
{
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
auth_set_env(void)
{
  return 0;
}
