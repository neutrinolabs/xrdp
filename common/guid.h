/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2021
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
 *
 */

/**
 * @file common/guid.h
 * @brief GUID manipulation declarations
  */

#ifndef GUID_H
#define GUID_H

#include "arch.h"

#define GUID_SIZE 16  /* bytes */
#define GUID_STR_SIZE (GUID_SIZE * 2 + 1)   /* Size for string representation */

/**
 * Use a struct for the guid so we can easily copy by assignment
 */
struct guid
{
    char g[GUID_SIZE];
};

/**
 * Get an initialised GUID
 *
 * @return new GUID
 */
struct guid guid_new(void);

/**
 * Clears an initialised GUID, so guid_is_set() returns true
 *
 * @param guid GUID to clear
 */
void
guid_clear(struct guid *guid);

/**
 * Checks if a GUID is initialised
 *
 * @param guid GUID to check (can be NULL)
 * @return non-zero if GUID is set
 */
int
guid_is_set(const struct guid *guid);

/**
 * Converts a GUID to a string representation
 *
 * @param guid GUID to represent
 * @param str pointer to at least GUID_STR_SIZE bytes to store the
 *            representation
 * @return str is returned for convenience
 */
const char *guid_to_str(const struct guid *guid, char *str);

#endif
