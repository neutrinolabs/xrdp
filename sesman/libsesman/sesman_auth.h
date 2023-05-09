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
 * @file sesman_auth.h
 * @brief User authentication definitions
 * @author Jay Sorg
 *
 */

#ifndef SESMAN_AUTH_H
#define SESMAN_AUTH_H

/**
 * Opaque type used to represent an authentication handle
 */
struct auth_info;
#include "scp_application_types.h"

/**
 *
 * @brief Validates user's password
 * @param user user's login name
 * @param pass user's password
 * @param client_ip IP address of connecting client (or ""/NULL if not known)
 * @param[out] Error code for the operation. E_SCP_LOGIN_OK on success.
 * @return auth handle on success, NULL on failure
 *
 */
struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, enum scp_login_status *errorcode);

/**
 *
 * @brief Gets an auth handle for a UDS login
 *
 * @param uid User ID
 * @param[out] Error code for the operation. E_SCP_LOGIN_OK on success.
 *             Can be NULL if this information isn't required.
 * @return auth handle on success, NULL on failure
 *
 */
struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode);

/**
 *
 * @brief Starts a session
 * @param auth_info. Auth handle created by auth_userpass
 * @param display_num Display number
 * @return 0 on success, 1 on failure
 *
 * The resources allocated when the session is started are de-allocated
 * by auth_end() - there is no separate way to do this.
 */
int
auth_start_session(struct auth_info *auth_info, int display_num);

/**
 *
 * @brief Deallocates an auth handle and releases all resources
 * @param auth_info. Auth handle created by auth_userpass
 * @return 0 on success, 1 on failure
 *
 */
int
auth_end(struct auth_info *auth_info);

/**
 *
 * @brief Sets up the environment for a session started
 *        with auth_start_session()
 *
 * This call is only effective for PAM-based environments. It must be made
 * after the context has been switched to the logged-in user.
 *
 * @param auth_info. Auth handle created by auth_userpass
 * @return 0 on success, 1 on failure
 *
 */
int
auth_set_env(struct auth_info *auth_info);


#define AUTH_PWD_CHG_OK                0
#define AUTH_PWD_CHG_CHANGE            1
#define AUTH_PWD_CHG_CHANGE_MANDATORY  2
#define AUTH_PWD_CHG_NOT_NOW           3
#define AUTH_PWD_CHG_ERROR             4

/**
 *
 * @brief WIP - Checks to see if the password for a user needs changing
 *
 * @param user - Username to check
 * @return 0 on success, 1 on failure
 *
 */
int
auth_check_pwd_chg(const char *user);

/**
 *
 * @brief WIP - Changes the password for a user
 *
 * @param user Username to check
 * @param newpwd New password for user
 * @return 0 on success, 1 on failure
 *
 */
int
auth_change_pwd(const char *user, const char *newpwd);

#endif
