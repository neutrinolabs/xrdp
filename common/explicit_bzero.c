/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg and all contributors, 2004-2026
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
 * Definition of non-standard explicit_bzero() function
 *
 * This BSD function is the current de facto standard for clearing
 * memory containing sensitive material.
 *
 * Copied from the public domain implementation by Ted Unangst. The
 * indirect call to bzero() is replaced with one to memset(), and
 * the possibility of calling the Annex K memset_s() function is eliminated
 * for simplicity.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#ifndef HAVE_EXPLICIT_BZERO
#include <string.h>

#include "explicit_bzero.h"

/*
 * Indirect memset through a volatile pointer to hopefully avoid
 * dead-store optimisation eliminating the call.
 */
static void *(* volatile indirect_memset)(void *, int, size_t) = memset;

void
explicit_bzero(void *p, size_t n)
{
    /*
     * clang -fsanitize=memory needs to intercept memset-like functions
     * to correctly detect memory initialisation. Make sure one is called
     * directly since our indirection trick above sucessfully confuses it.
     */
#if defined(__has_feature)
# if __has_feature(memory_sanitizer)
    memset(p, 0, n);
# endif
#endif

    indirect_memset(p, 0, n);
}

#endif // EXPLICIT_BZERO_H
