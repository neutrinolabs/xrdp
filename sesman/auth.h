/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
 * @return non-zero handle on success, 0 on failure
 *
 */
long
auth_userpass(const char *user, const char *pass, int *errorcode);

/**
 *
 * @brief FIXME
 * @param in_val
 * @param in_display
 * @return 0 on success, 1 on failure
 *
 */
int
auth_start_session(long in_val, int in_display);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int
auth_stop_session(long in_val);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int
auth_end(long in_val);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int
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
int
auth_check_pwd_chg(const char *user);

/**
 *
 * @brief FIXME
 * @param in_val
 * @return 0 on success, 1 on failure
 *
 */
int
auth_change_pwd(const char *user, const char *newpwd);

#endif
