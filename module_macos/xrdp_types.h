/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026 Neutrinos Software Corporation
 * Some portions Classify(r)
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
 * @file xrdp_types.h
 * @brief Minimal type definitions for standalone xrdp module
 */

#ifndef XRDP_TYPES_H
#define XRDP_TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Platform-specific types from arch.h */
#if defined(__APPLE__)
    typedef uint64_t tui64;
    typedef int64_t  tsi64;
    typedef uint32_t tui32;
    typedef int32_t  tsi32;
    typedef uint16_t tui16;
    typedef int16_t  tsi16;
    typedef uint8_t  tui8;
    typedef int8_t   tsi8;
    typedef intptr_t tintptr;
    typedef uintptr_t tbus;
#else
    #error "Unsupported platform"
#endif

/* Export macro */
#define EXPORT_CC

/* Memory allocation (from os_calls.h) */
static inline void* g_malloc(int size, int zero)
{
    void* ptr = malloc(size);
    if (ptr && zero)
    {
        memset(ptr, 0, size);
    }
    return ptr;
}

static inline void g_free(void* ptr)
{
    free(ptr);
}

/* Logging */
#define LOG_LEVEL_ERROR  0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_INFO   2
#define LOG_LEVEL_DEBUG  3

#define LOG(level, fmt, ...) do { \
    printf("[%s] " fmt "\n", \
           level == LOG_LEVEL_ERROR ? "ERROR" : \
           level == LOG_LEVEL_WARNING ? "WARN" : \
           level == LOG_LEVEL_INFO ? "INFO" : "DEBUG", \
           ##__VA_ARGS__); \
} while(0)

#endif /* XRDP_TYPES_H */
