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

#include "arch.h"
#include "os_calls.h"

#include <security/pam_appl.h>

struct t_user_pass
{
  char user[256];
  char pass[256];
};

struct t_auth_info
{
  struct t_user_pass user_pass;
  int session_opened;
  int did_setcred;
  struct pam_conv pamc;
  pam_handle_t* ph;
};

/******************************************************************************/
static int DEFAULT_CC
verify_pam_conv(int num_msg, const struct pam_message** msg,
                struct pam_response** resp, void* appdata_ptr)
{
  int i;
  struct pam_response* reply;
  struct t_user_pass* user_pass;

  reply = g_malloc(sizeof(struct pam_response) * num_msg, 1);
  for (i = 0; i < num_msg; i++)
  {
    switch (msg[i]->msg_style)
    {
      case PAM_PROMPT_ECHO_ON: /* username */
        user_pass = appdata_ptr;
        reply[i].resp = g_strdup(user_pass->user);
        reply[i].resp_retcode = PAM_SUCCESS;
        break;
      case PAM_PROMPT_ECHO_OFF: /* password */
        user_pass = appdata_ptr;
        reply[i].resp = g_strdup(user_pass->pass);
        reply[i].resp_retcode = PAM_SUCCESS;
        break;
      default:
        g_printf("unknown in verify_pam_conv\n\r");
        g_free(reply);
        return PAM_CONV_ERR;
    }
  }
  *resp = reply;
  return PAM_SUCCESS;
}

/******************************************************************************/
static void DEFAULT_CC
get_service_name(char* service_name)
{
  service_name[0] = 0;
  if (g_file_exist("/etc/pam.d/sesman"))
  {
    g_strncpy(service_name, "sesman", 255);
  }
  else
  {
    g_strncpy(service_name, "gdm", 255);
  }
}

/******************************************************************************/
/* returns long, zero is no go */
long DEFAULT_CC
auth_userpass(char* user, char* pass)
{
  int error;
  struct t_auth_info* auth_info;
  char service_name[256];

  get_service_name(service_name);
  auth_info = g_malloc(sizeof(struct t_auth_info), 1);
  g_strncpy(auth_info->user_pass.user, user, 255);
  g_strncpy(auth_info->user_pass.pass, pass, 255);
  auth_info->pamc.conv = &verify_pam_conv;
  auth_info->pamc.appdata_ptr = &(auth_info->user_pass);
  error = pam_start(service_name, 0, &(auth_info->pamc), &(auth_info->ph));
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_start failed: %s\n\r", pam_strerror(auth_info->ph, error));
    g_free(auth_info);
    return 0;
  }
  error = pam_authenticate(auth_info->ph, 0);
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_authenticate failed: %s\n\r",
                         pam_strerror(auth_info->ph, error));
    g_free(auth_info);
    return 0;
  }
  error = pam_acct_mgmt(auth_info->ph, 0);
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_acct_mgmt failed: %s\n\r",
                         pam_strerror(auth_info->ph, error));
    g_free(auth_info);
    return 0;
  }
  return (long)auth_info;
}

/******************************************************************************/
/* returns error */
int DEFAULT_CC
auth_start_session(long in_val)
{
  struct t_auth_info* auth_info;
  int error;

  auth_info = (struct t_auth_info*)in_val;
  error = pam_setcred(auth_info->ph, PAM_ESTABLISH_CRED);
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_setcred failed: %s\n\r", pam_strerror(auth_info->ph, error));
    return 1;
  }
  auth_info->did_setcred = 1;
  error = pam_open_session(auth_info->ph, 0);
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_open_session failed: %s\n\r",
                       pam_strerror(auth_info->ph, error));
    return 1;
  }
  auth_info->session_opened = 1;
  return 0;
}

/******************************************************************************/
/* returns error */
/* cleanup */
int DEFAULT_CC
auth_end(long in_val)
{
  struct t_auth_info* auth_info;

  auth_info = (struct t_auth_info*)in_val;
  if (auth_info != 0)
  {
    if (auth_info->ph != 0)
    {
      if (auth_info->session_opened)
      {
        pam_close_session(auth_info->ph, 0);
      }
      if (auth_info->did_setcred)
      {
        pam_setcred(auth_info->ph, PAM_DELETE_CRED);
      }
      pam_end(auth_info->ph, PAM_SUCCESS);
      auth_info->ph = 0;
    }
  }
  g_free(auth_info);
  return 0;
}
