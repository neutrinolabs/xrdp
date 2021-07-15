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

#include "arch.h"

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
g_format_info_string(char *dest, unsigned int len,
                     const char *format,
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

/**
 * Joins an array of strings into a single string.
 *
 * Note: The joiner is placed between each source string. The joiner is not
 *      placed after the last source string. If there is only one source string,
 *      then the result string will be equal to the source string.
 *
 * Note: any content that is present in dest will be overwritten with the new
 *      joined string.
 *
 * Note: If the destination array is not large enough to hold the entire
 *      contents of the joined string, then the joined string will be truncated
 *      to fit in the destination array.
 *
 * @param[out] dest The destination array to write the joined string into.
 * @param[in] dest_len The max number of characters to write to the destination
 *      array including the terminating null. Must be > 0
 * @param[in] joiner The string to concatenate between each source string.
 *      The joiner string may be NULL which is processed as a zero length string.
 * @param[in] src An array of strings to join. The array must be non-null.
 *      Array items may be NULL and are processed as zero length strings.
 * @param[in] src_len The number of strings to join in the src array. Must be > 0
 * @return A pointer to the begining of the joined string (ie. returns dest).
 */
char *
g_strnjoin(char *dest, int dest_len, const char *joiner, const char *src[], int src_len);

/**
 * Converts a binary array into a hux dump suitable for displaying to a user.
 *
 * The format of the hex dump is:
 * 0000   01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16   ................
 * /\     /\                                                /\
 * |      |                                                 |
 * |      |                                                 ascii representation of bytes
 * |      hex representation of bytes
 * offset from beginning of the byte array in hex
 *
 * Note: the ascii representation uses '.' for all non-printable
 * characters (eg. below 32 or above 127).
 *
 * Note: the string contains embedded new lines, but is not new line terminated.
 *
 * @param[in] src Value to convert
 * @param[in] len The number of bytes in src to convert
 * @return string containing the hex dump that must be free'd by the caller
 */
char *
g_bytes_to_hexdump(const char *src, int len);

/**
 * Extracts the display number from an X11 display string
 *
 * @param Display string (i.e. g_getenv("DISPLAY"))
 *
 * @result <0 if the string could not be parsed, or >=0 for a display number
 */
int
g_get_display_num_from_display(const char *display_text);

int      g_strlen(const char *text);
const char *g_strchr(const char *text, int c);
const char *g_strnchr(const char *text, int c, int len);
char    *g_strcpy(char *dest, const char *src);
char    *g_strncpy(char *dest, const char *src, int len);
char    *g_strcat(char *dest, const char *src);
char    *g_strncat(char *dest, const char *src, int len);
char    *g_strdup(const char *in);
char    *g_strndup(const char *in, const unsigned int maxlen);
int      g_strcmp(const char *c1, const char *c2);
int      g_strncmp(const char *c1, const char *c2, int len);
int      g_strncmp_d(const char *c1, const char *c2, const char delim, int len);
int      g_strcasecmp(const char *c1, const char *c2);
int      g_strncasecmp(const char *c1, const char *c2, int len);
int      g_atoi(const char *str);
/**
 * Extends g_atoi(), Converts decimal and hexadecimal number String to integer
 *
 * Prefix hexadecimal numbers with '0x'
 *
 * @param str String to convert to an integer
 * @return int Integer expression of a string
 */
int      g_atoix(const char *str);
int      g_htoi(char *str);
int      g_bytes_to_hexstr(const void *bytes, int num_bytes, char *out_str,
                           int bytes_out_str);
int      g_pos(const char *str, const char *to_find);
int      g_mbstowcs(twchar *dest, const char *src, int n);
int      g_wcstombs(char *dest, const twchar *src, int n);
int      g_strtrim(char *str, int trim_flags);
#endif
