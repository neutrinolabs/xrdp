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
 * @file auth.h
 * @brief User authentication definitions
 * @author Jay Sorg
 *
 */

#ifndef AUTH_H
#define AUTH_H

/**
 *
 * @brief Validates user's password
 * @param user user's login name
 * @param pass user's password
 * @return 0 on success, 1 on failure
 *
 */
long DEFAULT_CC
auth_userpass(char* user, char* pass);

/**
 *
 * @brief FIXME
 * @param in_val
 * @param in_display
 * @return 0 on success, 1 on failure
 *
 */
int DEFAULT_CC
auth_start_session(long in_val, int in_display);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int DEFAULT_CC
auth_end(long in_val);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int DEFAULT_CC
auth_set_env(long in_val);


#define AUTH_PWD_CHG_OK                0
#define AUTH_PWD_CHG_CHANGE            1
#define AUTH_PWD_CHG_CHANGE_MANDATORY  2
#define AUTH_PWD_CHG_NOT_NOW           3
#define AUTH_PWD_CHG_ERROR             4

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int DEFAULT_CC
auth_check_pwd_chg(char* user);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int DEFAULT_CC
auth_change_pwd(char* user, char* newpwd);

#endif
