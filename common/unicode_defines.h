/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2023
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
 * @file common/unicode_defines.h
 *
 * Defines used internally by the implementations of the Unicode routines
 */

#if !defined(UNICODE_DEFINES_H)
#define UNICODE_DEFINES_H

/**
 * Is this byte a valid UTF-8 continuation character?
 */
#define IS_VALID_CONTINUATION_CHAR(c) ((c) >= 0x80 && (c) < 0xc0)

/**
 * Is this character one of the end-of-plane non-characters?
 *
 * These are U+xFFFE and U+xFFFF for x in (0..10}
 */
#define IS_PLANE_END_NON_CHARACTER(c32) (((c32) & 0xfffe) == 0xfffe)

/**
 * Is this character one of the additional non-characters?
 *
 * 32 additional non-charactersare defined in the
 * "Arabic Presentation Forms-A" Unicode block */
#define IS_ARABIC_NON_CHARACTER(c32) ((c32) >= 0xfdd0 && (c32) <= 0xfdef)

// Invalid characters, based on UTF-8 decoding range
//
// By 'invalid' we mean characters that should not be encoded or
// decoded when switching between UTF-8 and UTF-32
//
// See "UTF-8 decoder capability and stress test" Markus Kuhn 2015-08-28
#define INVALID_UNICODE_0_TO_7F(c) (0)   // No invalid characters
#define INVALID_UNICODE_80_TO_7FF(c) (0) // No invalid characters
#define INVALID_UNICODE_800_TO_FFFF(c) \
    (((c) >= 0xd800 && (c) <= 0xdfff) || /* Surrogate pairs */ \
     IS_ARABIC_NON_CHARACTER(c) || \
     IS_PLANE_END_NON_CHARACTER(c))

#define INVALID_UNICODE_10000_TO_1FFFFF(c) \
    (IS_PLANE_END_NON_CHARACTER(c) || (c) > 0x10ffff)

// Returns true for all 'invalid' Unicode chars
#define INVALID_UNICODE(c) \
    ( \
      INVALID_UNICODE_0_TO_7F(c) || \
      INVALID_UNICODE_80_TO_7FF(c) || \
      INVALID_UNICODE_800_TO_FFFF(c) || \
      INVALID_UNICODE_10000_TO_1FFFFF(c) \
    )

#endif // UNICODE_DEFINES_H
