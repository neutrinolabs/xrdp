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
 * string which could be output for %'ch', where ch is a character
 */
struct info_string_tag
{
    char ch;
    const char *val;
};

#define INFO_STRING_END_OF_LIST { '\0', NULL }

/**
 * Map a bitmask to a string value
 *
 *
 * This structure is used by g_bitmask_to_str() to specify the
 * string for each bit in the bitmask
 */
struct bitmask_string
{
    int mask;
    const char *str;
};

#define BITMASK_STRING_END_OF_LIST { 0, NULL }

/**
 * Map a bitmask to a char value
 *
 *
 * This structure is used by g_bitmask_to_charstr() to specify the
 * char for each bit in the bitmask
 */
struct bitmask_char
{
    int mask;
    char c;
};

#define BITMASK_CHAR_END_OF_LIST { 0, '\0' }

enum
{
    // See g_sig2text()
    // Must be able to hold "SIG#%d" for INT_MIN
    //
    // ((sizeof(int) * 5 + 1) / 2) provides a very slight overestimate of
    // the bytes requires to store a decimal expansion of 'int':-
    // sizeof  INT_MAX     display bytes   ((sizeof(int) * 5 + 1)
    // (int)               needed           / 2)
    // ------  -------     -------------   ---------------------------
    // 1       127         3                3
    // 2       32767       5                5
    // 3       8388607     7                8
    // 4       2147483637  10              10
    // 8       9*(10**18)  19              20
    // 16      2*(10**38)  39              40
    // 32      6*(10**76)  77              80
    MAXSTRSIGLEN =  (3 + 1 + 1 + ((sizeof(int) * 5 + 1) / 2) + 1)
};

/*
 * Significant Universal Character Set (Unicode) characters
 */
enum
{
    UCS_WHITE_SQUARE  = 0x25a1,
    UCS_REPLACEMENT_CHARACTER  = 0xfffd
};

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
 * @return A pointer to the beginning of the joined string (ie. returns dest).
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

/**
 * Converts a bitmask into a string for output purposes
 *
 * Similar to g_bitmask_to_charstr(), but tokens are strings, separated
 * by delimiters.
 *
 * @param bitmask Bitmask to convert
 * @param bitdefs Definitions for strings for bits
 * @param delim Delimiter to use between strings
 * @param buff Output buff
 * @param bufflen Length of buff, including terminator '`\0'
 *
 * @return Total length excluding terminator which would be written, as
 *         in snprintf(). Can be used to check for overflow
 *
 * @note Any undefined bits in the bitmask are appended to the output as
 *       a hexadecimal constant.
 */
int
g_bitmask_to_str(int bitmask, const struct bitmask_string bitdefs[],
                 char delim, char *buff, int bufflen);

/***
 * Converts a string containing a series of tokens to a bitmask.
 *
 * Similar to g_charstr_to_bitmask(), but tokens are strings, separated
 * by delimiters.
 *
 * @param str Input string
 * @param bitdefs Array mapping tokens to bitmask values
 * @param delim Delimiter for tokens in str
 * @param[out] unrecognised Buffer for any unrecognised tokens
 * @param unrecognised_len Length of unrecognised including '\0';
 * @return bitmask value for recognised tokens
 */
int
g_str_to_bitmask(const char *str, const struct bitmask_string bitdefs[],
                 const char *delim, char *unrecognised,
                 int unrecognised_len);

/**
 * Converts a bitmask into a string for output purposes
 *
 * Similar to g_bitmask_to_str(), but tokens are individual characters, and
 * there are no delimiters.
 *
 * @param bitmask Bitmask to convert
 * @param bitdefs Definitions for strings for bits
 * @param buff Output buff
 * @param bufflen Length of buff, including terminator '`\0'
 * @param[out] rest Any unused bits which weren't covered by bitdefs.
 *                  May be NULL.
 *
 * @return Total length excluding terminator which would be written, as
 *         in snprintf(). Can be used to check for overflow
 *
 * @note Any undefined bits in the bitmask are appended to the output as
 *       a hexadecimal constant.
 */
int
g_bitmask_to_charstr(int bitmask, const struct bitmask_char bitdefs[],
                     char *buff, int bufflen, int *rest);

/***
 * Converts a string containing a series of characters to a bitmask.
 *
 * Similar to g_str_to_bitmask(), but tokens are individual characters, and
 * there are no delimiters.
 *
 * @param str Input string
 * @param bitdefs Array mapping tokens to bitmask values
 * @param delim Delimiter for tokens in str
 * @param[out] unrecognised Buffer for any unrecognised tokens
 * @param unrecognised_len Length of unrecognised including '\0';
 * @return bitmask value for recognised tokens
 */
int
g_charstr_to_bitmask(const char *str, const struct bitmask_char bitdefs[],
                     char *unrecognised, int unrecognised_len);

int      g_strlen(const char *text);
char    *g_strchr(const char *text, int c);
char    *g_strrchr(const char *text, int c);
char    *g_strnchr(const char *text, int c, int len);
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
char    *g_strstr(const char *haystack, const char *needle);

/** trim spaces and tabs, anything <= space
 *
 * @param str (assumed to be UTF-8)
 * @param trim_flags 1 trim left, 2 trim right, 3 trim both, 4 trim through
 * @return != 0 - trim_flags not recognised
 * this will always shorten the string or not change it */
int      g_strtrim(char *str, int trim_flags);

/**
 * Maps a signal number to a string, i.e. SIGHUP -> "SIGHUP"
 *
 * @param signum Signal number
 * @param sigstr buffer for result
 * @return sigstr, for convenience
 *
 * Buffer is assumed to be at least MAXSTRSIGLEN
 *
 * The string "SIG#<num>" is returned for unrecognised signums
 */
char    *g_sig2text(int signum, char sigstr[]);

/**
 * Get the next Unicode character from a UTF-8 string
 *
 * @param utf8str_ref UTF 8 string [by reference]
 * @param len_ref Length of string [by reference] or NULL
 * @return Unicode character
 *
 * On return, utf8str and len are updated to point past the decoded character.
 * Unrecognised characters are mapped to UCS_REPLACEMENT_CHARACTER
 *
 * len is not needed if your utf8str has a terminator, or is known to
 * be well-formed.
 */
char32_t
utf8_get_next_char(const char **utf8str_ref, unsigned int *len_ref);

/**
 * Convert a Unicode character to UTF-8
 * @param c32 Unicode character
 * @param u8str buffer containing at least MAXLEN_UTF8_CHAR  bytes for result
 * @return Number of bytes written to u8str. Can be NULL if only the
 *         length is needed.
 *
 * The bytes written to u8str are unterminated
 */
#define MAXLEN_UTF8_CHAR 4
unsigned int
utf_char32_to_utf8(char32_t c32, char *u8str);

/**
 * Returns the number of Unicode characters in a UTF-8 string
 * @param utf8str UTF-8 string
 * @result Number of Unicode characters in the string (terminator not included)
 */
unsigned int
utf8_char_count(const char *utf8str);

/**
 * Returns the number of UTF-16 words required to store a UTF-8 string
 * @param utf8str UTF-8 string
 * @param len Length of UTF-8 string
 * @result number of words to store UTF-8 string as UTF-16.
 */
unsigned int
utf8_as_utf16_word_count(const char *utf8str, unsigned int len);

/**
 * Add a Unicode character into a UTF-8 string
 * @param utf8str Pointer to UTF-8 string
 * @param len Length of buffer for UTF-8 string (includes NULL)
 * @param c32 character to add
 * @param index Where to add the codepoint
 * @return 1 for success, 0 if no character was inserted
 *
 * This routine has to parse the string as it goes, so can be slow.
 */
int
utf8_add_char_at(char *utf8str, unsigned int len, char32_t c32,
                 unsigned int index);

/**
 * Remove a Unicode character from a UTF-8 string
 * @param utf8str Pointer to UTF-8 string
 * @param index Where to remove the codepoint from (0-based)
 * @return Character removed, or 0 if no character was removed
 *
 * This routine has to parse the string as it goes, so can be slow.
 */
char32_t
utf8_remove_char_at(char *utf8str, unsigned int index);

#endif
