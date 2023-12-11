/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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

#include <stdlib.h>
#include <string.h>

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
#if defined(_WIN64)
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef signed __int64 intptr_t;
typedef unsigned __int64 uintptr_t;
#else
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
typedef signed long intptr_t;
typedef unsigned long uintptr_t;
#endif
#endif

typedef int bool_t;

// Define Unicode character types
#if defined(HAVE_UCHAR_H)
#include <uchar.h>
#elif defined(HAVE_STDINT_H)
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#else
typedef uint16_t char16_t;
typedef uint32_t char32_t;
#endif

/* you can define L_ENDIAN or B_ENDIAN and NEED_ALIGN or NO_NEED_ALIGN
   in the makefile to override */

/* check endianness */
#if !(defined(L_ENDIAN) || defined(B_ENDIAN))
#if !defined(__BYTE_ORDER) && defined(__linux__)
#include <endian.h>
#endif

#if defined(BYTE_ORDER)
#if BYTE_ORDER == BIG_ENDIAN
#define B_ENDIAN
#else
#define L_ENDIAN
#endif
#endif

#if !(defined(L_ENDIAN) || defined(B_ENDIAN))
#if defined(__sparc__) || \
    defined(__s390__) || defined (__s390x__) || \
    defined(__hppa__) || defined (__m68k__) || \
    (defined(__PPC__) && defined(__BIG_ENDIAN__)) || \
    (defined(__ppc__) && defined(__BIG_ENDIAN__))
#define B_ENDIAN
#else
#define L_ENDIAN
#endif
#endif
#endif

/* check if we need to align data */
#if !(defined(NEED_ALIGN) || defined(NO_NEED_ALIGN))
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__m68k__) || defined(__mips__) || \
    defined(__ia64__) || defined(__arm__) || defined(__sh__) || \
    (defined(__PPC__) && defined(__BIG_ENDIAN__)) || \
    (defined(__ppc__) && defined(__BIG_ENDIAN__)) || \
    defined(__loongarch__) || defined(__e2k__)
#define NEED_ALIGN
#elif defined(__x86__) || defined(__x86_64__) || \
      defined(__AMD64__) || defined(_M_IX86) || defined (_M_AMD64) || \
      defined(__i386__) || defined(__aarch64__) || \
      defined(__PPC__) || defined(__LITTLE_ENDIAN__) || \
      defined(__s390__) || defined (__s390x__) || \
      defined(__riscv)
#define NO_NEED_ALIGN
#else
#warning unknown arch
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

#if defined(_WIN32)
#if defined(__BORLANDC__)
#define EXPORT_CC _export __cdecl
#else
#define EXPORT_CC
#endif
#else
#define EXPORT_CC
#endif

#ifndef DEFINED_Ts
#define DEFINED_Ts
typedef int8_t ti8;
typedef uint8_t tui8;
typedef int8_t tsi8;
typedef int16_t ti16;
typedef uint16_t tui16;
typedef int16_t tsi16;
typedef int32_t ti32;
typedef uint32_t tui32;
typedef int32_t tsi32;
typedef int64_t ti64;
typedef uint64_t tui64;
typedef int64_t tsi64;
typedef bool_t tbool;
typedef intptr_t tbus;
typedef intptr_t tintptr;

/* socket */
#if defined(_WIN32)
typedef unsigned int tsock;
#else
typedef int tsock;
#endif
#endif /* DEFINED_Ts */

/* format string verification */
#if defined(HAVE_FUNC_ATTRIBUTE_FORMAT)
#define printflike(arg_format, arg_first_check) \
    __attribute__((__format__(__printf__, arg_format, arg_first_check)))
#else
#define printflike(arg_format, arg_first_check)
#endif

/* module interface */
#ifdef __cplusplus
extern "C" {
#endif
tintptr mod_init(void);
int mod_exit(tintptr);
#ifdef __cplusplus
}
#endif

#endif
