/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*/

#if !defined(ARCH_H)
#define ARCH_H

/* check endianess */
#if defined(__sparc__) || defined(__PPC__) || defined(__ppc__)
#define B_ENDIAN
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define L_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
#define B_ENDIAN
#endif
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__) || defined(__ppc__)
#define NEED_ALIGN
#endif

/* defines for thread creation factory functions */
#if defined(_WIN32)
#define THREAD_RV unsigned long
#define THREAD_CC __stdcall
#else
#define THREAD_RV void*
#define THREAD_CC
#endif

#if defined(__BORLANDC__)
#define APP_CC __fastcall
#define DEFAULT_CC __cdecl
#else
#define APP_CC
#define DEFAULT_CC
#endif

#if defined(_WIN32)
#define EXPORT_CC _export __cdecl
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
typedef long tbus;

#endif
