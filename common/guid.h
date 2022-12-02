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
#define GUID_STR_SIZE (GUID_SIZE * 2 + 4 + 1)   /* w/ 4 hyphens + null terminator */


/**
 * Use a struct for the guid so we can easily copy by assignment.
 * We use an array of char so that
 * we can compare GUIDs with a straight memcmp()
 *
 * Some fields of the GUID are in little-endian-order as specified by
 * [MS-DTYP]. This is at odds with RFC4122 which specifies big-endian
 * order for all fields.
 *
 * Octets RFC4122 field
 * ------ -------------
 * 0-3    time_low (little-endian)
 * 4-5    time_mid (little-endian)
 * 6-7    time_hi_and_version (little-endian)
 * 8      clock_seq_hi_and_reserved
 * 9      clock_seq_low (in order)
 * 10-15  node
  */
struct guid
{
    char g[GUID_SIZE];
};

/**
 * Get an initialised GUID
 *
 * The GUID is compatible with RFC4122 section 4.4.
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
 * @param dest destionation pointer to at least GUID_STR_SIZE
 *             bytes to store the representation
 * @return dest is returned for convenience
 */
const char *guid_to_str(const struct guid *guid, char *dest);

#endif

