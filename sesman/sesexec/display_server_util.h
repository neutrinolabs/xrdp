/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg and contributors 2004-2025
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
 * @file display_server_util.h
 * @brief Display server utilities
 * @author Matt Burt
 *
 */

#ifndef XWAIT_H
#define XWAIT_H

#include <sys/types.h>

struct login_info;

enum display_server_status
{
    DS_STATUS_OK = 0,
    DS_STATUS_MISC_ERROR,
    DS_STATUS_TIMED_OUT,
    DS_STATUS_FAILED_TO_START
};

/**
 *
 * @brief waits for the display server to start
 *
 * A standalone program is used for this, so the status of the display
 * server can be determined.
 *
 * @param login_info Login info for user
 * @param env_names Environment to set for user (names)
 * @param env_values Environment to set for user (values)
 * @param command (and parameters) to run to wait for the display server
 * @param setenvvar Callback for handling any environment
 *                  variables sent by the wait command
 * @param closure Extra parameter for setenvvar
 * @return status
 *
 */
enum display_server_status
wait_for_display_server(struct login_info *login_info,
                        const struct list *env_names,
                        const struct list *env_values,
                        struct list *cmd,
                        void (*setenvvar)(const char *name,
                                const char *value,
                                void *closure),
                        void *closure);

#endif
