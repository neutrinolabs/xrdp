/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2025
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
 * @file session_parameters.c
 * @brief Parameters used to start a session
 * @author Jay Sorg, Simone Fedele, Matt Burt
 *
 */

#include <string.h>
#include <stdlib.h>

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "session_parameters.h"

/******************************************************************************/
struct session_parameters *
copy_session_parameters(const struct session_parameters *sp)
{
    struct session_parameters *cp;
    // What string length do we need?
    unsigned int string_length = 0;
    string_length += strlen(sp->shell) + 1;
    string_length += strlen(sp->directory) + 1;

    cp = (struct session_parameters *)malloc(sizeof(*cp) + string_length);

    if (cp != NULL)
    {
        /* Copy all the scalar parameters... */
        *cp = *sp;

        /* ...and then the strings */
        char *memptr = (char *)(cp + 1);

#define COPY_STRING(dest,src) \
    (dest) = memptr; \
    strcpy(memptr, src); \
    memptr += strlen(memptr) + 1

        COPY_STRING(cp->shell, sp->shell);
        COPY_STRING(cp->directory, sp->directory);

#undef COPY_STRING
    }

    return cp;
}
