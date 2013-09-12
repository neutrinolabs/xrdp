/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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

#if !defined(ARCH_H)
#define ARCH_H

#if !(defined(L_ENDIAN) || defined(B_ENDIAN))
/* check endianess */
#if defined(__sparc__) || defined(__PPC__) || defined(__ppc__) || \
    defined(__hppa__)
#define B_ENDIAN
#else
#define L_ENDIAN
#endif
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__) || defined(__ppc__) || defined(__arm__)
#define NEED_ALIGN
#endif
#endif

/* defines for thread creation factory functions */
#if defined(_WIN32)
#define THREAD_RV unsigned long
#define THREAD_CC __stdcall
#else
#define THREAD_RV void*
#define THREAD_CC
#endif

#if defined(__BORLANDC__) || defined(_WIN32)
#define APP_CC __fastcall
#define DEFAULT_CC __cdecl
#else
#define APP_CC
#define DEFAULT_CC
#endif

#if defined(_WIN32)
#if defined(__BORLANDC__)
#define EXPORT_CC _export __cdecl
#else
#define EXPORT_CC
#endif
#else
#define EXPORT_CC
#endif

typedef char ti8;
typedef unsigned char tui8;
typedef signed char tsi8;
typedef short ti16;
typedef unsigned short tui16;
typedef signed short tsi16;
typedef int ti32;
typedef unsigned int tui32;
typedef signed int tsi32;
#if defined(_WIN64)
/* Microsoft's VC++ compiler uses the more backwards-compatible LLP64 model.
   Most other 64 bit compilers(Solaris, AIX, HP, Linux, Mac OS X) use
   the LP64 model.
   long is 32 bits in LLP64 model, 64 bits in LP64 model. */
typedef __int64 tbus;
#else
typedef long tbus;
#endif
typedef tbus thandle;
typedef tbus tintptr;
/* wide char, socket */
#if defined(_WIN32)
typedef unsigned short twchar;
typedef unsigned int tsock;
typedef unsigned __int64 tui64;
typedef signed __int64 tsi64;
#else
typedef int twchar;
typedef int tsock;
typedef unsigned long long tui64;
typedef signed long long tsi64;
#endif

#endif
