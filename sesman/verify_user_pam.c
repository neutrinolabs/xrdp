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
  char* user;
  char* pass;
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
/* returns boolean */
int DEFAULT_CC
auth_userpass(char* user, char* pass)
{
  int error;
  int null_tok;
  struct t_user_pass user_pass;
  struct pam_conv pamc;
  pam_handle_t* ph;

  user_pass.user = user;
  user_pass.pass = pass;
  pamc.conv = &verify_pam_conv;
  pamc.appdata_ptr = &user_pass;
  error = pam_start("gdm", 0, &pamc, &ph);
  if (error != PAM_SUCCESS)
  {
    g_printf("pam_start failed\n\r");
    return 0;
  }
  null_tok = 0;
  error = pam_authenticate(ph, null_tok);
  if (error != PAM_SUCCESS)
  {
    pam_end(ph, PAM_SUCCESS);
    return 0;
  }
  pam_end(ph, PAM_SUCCESS);
  return 1;
}
