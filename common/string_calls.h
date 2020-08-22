/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2020
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
 * generic string handling calls
 */

#if !defined(STRING_CALLS_H)
#define STRING_CALLS_H

/**
 * Map a character to a string value
 *
 * This structure is used by g_format_info_string() to specify the
 * string which chould be output for %'ch', where ch is a character
 */
struct info_string_tag
{
    char ch;
    const char *val;
};

#define INFO_STRING_END_OF_LIST { '\0', NULL }

/**
 * Processes a format string for general info
 *
 * @param[out] dest Destination buffer
 * @param[in] len Length of buffer, including space for a terminator
 * @param[in] format Format string to process
 * @param[in] map Array of struct info_string_tag.
 *
 * Where a '%<ch>' is encountered in the format string, the map is scanned
 * and the corresponding string is copied instead of '%<ch>'.
 *
 * '%%' is always replaced with a single '%' in the output. %<ch> strings
 * not present in the map are ignored.
 *
 * The map is terminated with INFO_STRING_END_OF_LIST
 *
 * Caller can check for buffer truncation by comparing the result with
 * the buffer length (as in snprintf())
 */
unsigned int
g_format_info_string(char* dest, unsigned int len,
                      const char* format,
                      const struct info_string_tag map[]);


/**
 * Converts a boolean to a string for output
 *
 * @param[in] value Value to convert
 * @return String representation
 */
const char *
g_bool2text(int value);

/**
 * Converts a string to a boolean value
 *
 * @param[in] s String to convert
 * @return machine representation
 */
int
g_text2bool(const char *s);

#endif
