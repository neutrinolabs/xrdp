/**
 * xrdp: A Remote Desktop Protocol server.
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
 * @file wlwait.h
 * @brief Wait for a Wayland compositor to start listening
 */

#ifndef WLWAIT_H
#define WLWAIT_H

#include <sys/types.h>

/**
 * Waits for a Wayland compositor process to listen on its socket
 *
 * Compositors choose their own socket name (e.g. "wayland-1") in
 * $XDG_RUNTIME_DIR, and not all of them can be told which one to use.
 * libwayland holds a lock on "<socket>.lock" while the socket is in use,
 * so the socket is found from the lock held by the compositor process.
 *
 * @param pid PID of the compositor
 * @param runtime_dir XDG_RUNTIME_DIR of the compositor
 * @param timeout_ms How long to wait
 * @param[out] path Socket path
 * @param path_size Size of path buffer
 * @return 0 for success
 */
int
wait_for_wayland_socket(pid_t pid, const char *runtime_dir,
                        unsigned int timeout_ms,
                        char *path, unsigned int path_size);

#endif
