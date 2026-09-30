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
 * Declaration of non-standard explicit_bzero() function
 *
 * This BSD function is the current de-facto standard for clearing
 * memory containing sensitive material.
 */

#if !defined(EXPLICIT_BZERO_H)
#define EXPLICIT_BZERO_H

#ifndef CONFIG_AC_H
#   error config_ac.h not visible in explicit_bzero.h
#endif

#ifdef HAVE_EXPLICIT_BZERO

// The header containing the function declaration is platform-dependent
#if defined(__FreeBSD__)
#include <strings.h>
#else
#include <string.h>
#endif

#else // HAVE_EXPLICIT_BZERO

#include <stddef.h>

void explicit_bzero(void *s, size_t n);

#endif // HAVE_EXPLICIT_BZERO
#endif // EXPLICIT_BZERO_H
