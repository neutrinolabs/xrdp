/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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

 /* module based logging */

#ifndef _MLOG_H
#define _MLOG_H

/*
 * Note1: to enable debug messages, in your .c file, #define LOCAL_DEBUG
 *        BEFORE including this file
 *
 * Note2: in your .c file, #define MODULE_NAME, 8 chars long, to print each
 *       log entry with your module name
 */

#define LOG_ERROR       0
#define LOG_INFO        1
#define LOG_DEBUG_LOW   2
#define LOG_DEBUG_HIGH  3
#define LOG_LEVEL       LOG_ERROR

/*
 * print always
 */

#define log_error(_params...)                                         \
do                                                                    \
{                                                                     \
        g_write("[%"PRIu64"]: %s %s: %d: ERROR: ", g_time3(),         \
                MODULE_NAME, __func__, __LINE__);                     \
        g_writeln (_params);                                          \
}                                                                     \
while(0)

#define log_always(_params...)                                        \
do                                                                    \
{                                                                     \
        g_write("[%"PRIu64"]: %s %s: %d: ALWAYS: ", g_time3(),        \
                MODULE_NAME, __func__, __LINE__);                     \
        g_writeln (_params);                                          \
}                                                                     \
while(0)

/*
 * print conditionally
 */

#ifdef LOCAL_DEBUG
#define log_info(_params...)                                          \
do                                                                    \
{                                                                     \
    if (LOG_INFO <= LOG_LEVEL)                                        \
    {                                                                 \
        g_write("[%"PRIu64"]: %s %s: %d: INFO:  ", g_time3(),         \
                MODULE_NAME, __func__, __LINE__);                     \
        g_writeln (_params);                                          \
    }                                                                 \
}                                                                     \
while(0)
#else
#define log_info(_params...)
#endif

#ifdef LOCAL_DEBUG
#define log_debug_low(_params...)                                     \
do                                                                    \
{                                                                     \
    if (LOG_DEBUG_LOW <= LOG_LEVEL)                                   \
    {                                                                 \
        g_write("[%"PRIu64"]: %s %s: %d: DEBUG: ", g_time3(),         \
                MODULE_NAME, __func__, __LINE__);                     \
        g_writeln (_params);                                          \
    }                                                                 \
}                                                                     \
while(0)
#else
#define log_debug_low(_params...)
#endif

#ifdef LOCAL_DEBUG
#define log_debug_high(_params...)                                    \
do                                                                    \
{                                                                     \
    if (LOG_DEBUG_HIGH <= LOG_LEVEL)                                  \
    {                                                                 \
        g_write("[%"PRIu64"]: %s %s: %d: DEBUG: ", g_time3(),         \
                MODULE_NAME, __func__, __LINE__);                     \
        g_writeln (_params);                                          \
    }                                                                 \
}                                                                     \
while(0)
#else
#define log_debug_high(_params...)
#endif

#endif /* #ifndef _MLOG_H */
