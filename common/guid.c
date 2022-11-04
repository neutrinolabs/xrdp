/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2021 Matt Burt, all xrdp contributors
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
 * @file common/guid.c
 * @brief GUID manipulation definitions
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "guid.h"
#include "os_calls.h"
#include "string_calls.h"

struct guid
guid_new(void)
{
    struct guid guid = {0};
    g_random(guid.g, sizeof(guid.g));
    return guid;
}

void
guid_clear(struct guid *guid)
{
    g_memset(&guid->g, '\x00', GUID_SIZE);
}

int
guid_is_set(const struct guid *guid)
{
    unsigned int i;
    int rv = 0;
    if (guid != NULL)
    {
        for (i = 0 ; i < GUID_SIZE; ++i)
        {
            if (guid->g[i] != '\x00')
            {
                rv = 1;
                break;
            }
        }
    }

    return rv;

}

const char *guid_to_str(const struct guid *src, char *dest)
{
    const unsigned char *guid = (const unsigned char *)src->g;

    /*
     * Flipping integers into little-endian
     * See also: https://devblogs.microsoft.com/oldnewthing/20220928-00/?p=107221
     */
    g_sprintf(dest, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
              guid[3], guid[2], guid[1], guid[0],
              guid[5], guid[4],
              guid[7], guid[6],
              guid[8], guid[9],
              guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);

    return dest;
}
