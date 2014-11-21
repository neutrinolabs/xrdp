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
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file verify_user_user.c
 * @brief Authenticate user using BSD password system
 * @author Renaud Allard
 *
 */

#include "sesman.h"

#define _XOPEN_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <login_cap.h>
#include <bsd_auth.h>

#ifndef SECS_PER_DAY
#define SECS_PER_DAY (24L*3600L)
#endif

extern struct config_sesman* g_cfg; /* in sesman.c */

/******************************************************************************/
/* returns boolean */
long DEFAULT_CC
auth_userpass(char* user, char* pass)
{
	int ret = auth_userokay(user, NULL, "auth-xrdp", pass);
	return ret;
}

/******************************************************************************/
/* returns error */
int DEFAULT_CC
auth_start_session(long in_val, int in_display)
{
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
auth_end(long in_val)
{
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
auth_set_env(long in_val)
{
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
auth_check_pwd_chg(char* user)
{
  return 0;
}

int DEFAULT_CC
auth_change_pwd(char* user, char* newpwd)
{
  return 0;
}

/**
 *
 * @brief Password encryption
 * @param pwd Old password
 * @param pln Plaintext new password
 * @param crp Crypted new password
 *
 */

static int DEFAULT_CC
auth_crypt_pwd(char* pwd, char* pln, char* crp)
{
  return 0;
}

/**
 *
 * @return 1 if the account is disabled, 0 otherwise
 *
 */
static int DEFAULT_CC
auth_account_disabled(struct spwd* stp)
{
  return 0;
}
