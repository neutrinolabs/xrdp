/**
 * Copyright (C) 2024 Matt Burt, all xrdp contributors
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
 * @file    common/scancode.h
 * @brief   Scancode handling
 *
 * This module provides functionality for the following:-
 * 1) Mapping from TS_KEYBOARD_EVENT PDU values to an 'RDP scancode'
 * 2) Handling RDP scancodes
 * 3) Convert between RDP scancodes and X11 keycodes.
 *
 * The values received in TS_KEYBOARD_EVENT PDUs are largely the same as
 * Windows scancodes. These are indirectly documented in the Microsoft
 * "Keyboard Scan Code Specification", Rev 1.3a (March 16th 2000) and
 * are otherwise known as "Scan code set 1" scancodes. This document no
 * longer appears to be available directly from the Microsoft website.
 *
 * A TS_KEYBOARD_EVENT_PDU contains two important values:-
 * 1) key_code       This is not unique. For example, left-shift and
 *                   right-shift share a key_code of 0x2a
 * 2) keyboard_flags Among other flags, contains KBDFLAGS_EXTENDED and
 *                   KBDFLAGS_EXTENDED1. These combine with the key_code
 *                   to allow a specific key to be determined.
 *
 * An 'RDP scancode' as defined by this module is a mapping of the
 * Windows key_code and keyboard_flags into a single value which
 * represents a unique key. For example:-
 * Left control  : key_code=0x1d, KBDFLAGS_EXTENDED=0 scancode = 0x1d
 * Right control : key_code=0x1d, KBDFLAGS_EXTENDED=1 scancode = 0x11d
 *
 * This model of unique keys more closely maps what X11 does with its
 * own keycodes.
 *
 * X11 keycodes are the X11 equivalent of RDP scancodes. In general, these
 * are specific to an X server. In practice however, these are nowadays
 * handled by the XKB extension and only two sets are in common use:-
 * - evdev : Linux, FreeBSD and possibly others
 * - base  : Everything else.
 *
 * This module presents a single source of truth for conversions between
 * RDP scancodes and X11 keycodes.
 */

#if !defined(SCANCODE_H)
#define SCANCODE_H

#include "xrdp_scancode_defs.h"

enum
{
    /**
     * Scancodes for keys used in the code are defined in the global
     * file xrdp_scancode_defs.h
     */

    /**
     * Scancode indexes for some of the keys in xrdp_scancode_defs.h
     */
    SCANCODE_INDEX_LSHIFT_KEY = SCANCODE_LSHIFT_KEY,
    SCANCODE_INDEX_RSHIFT_KEY = SCANCODE_RSHIFT_KEY,
    SCANCODE_INDEX_RALT_KEY = (SCANCODE_RALT_KEY & 0x7f) | 0x80,
    SCANCODE_INDEX_PAUSE_KEY = 0xf8,
    // 0xf9 - 0xff reserved for future extended1 mappings

    /**
     * Maximum value returned by scancode_to_index()
     */
    SCANCODE_MAX_INDEX = 255
};

/**
 * Convert a scancode to an index
 * @param scancode scancode in the range 0x00 - 0x2ff
 * @return index in the range 0..SCANCODE_MAX_INDEX (inclusive) or -1
 *
 * This function converts a 10-bit scancode into an 8-bit array index,
 * independent of the currently loaded keymap
 *
 * This is possible as the scancodes from 0x80 - 0x2ff are sparsely allocated.
 *
 * For scancodes in the range 0x00 - 0x7f, the index is identical to the
 * scancode. This includes scancodes for all the keys affected by
 * numlock.
 */
int
scancode_to_index(unsigned short scancode);

/**
 * Convert an index back to a scancode.
 * @param index in the range 0..SCANCODE_MAX_INDEX
 *
 * @result scancode which is mapped to the index value
 *
 * The result is always a valid scancode, even if the index is
 * not valid.
 */
unsigned short
scancode_from_index(int index);

/**
 * Looks up an RDP scancode and converts to an x11 keycode
 *
 * @param scancode Scancode. Extended scancodes have bit 9 set
 *                 (i.e. are in 0x100 - 0x1ff). Extended1 scancodes
 *                 (currently just the pause key) are in the range 0x200-0x2ff
 *  @return keycode, or 0 for no keycode
 */
unsigned short
scancode_to_x11_keycode(unsigned short scancode);

/**
 * Gets the next valid scancode from the list of valid scancodes
 * @param iter Value (initialised to zero), used to iterate
 *             over available scancodes.
 * @return Next valid scancode, or zero.
 *
 * The iterator is updated on a successful call. Use like this:-
 *
 * iter = 0;
 * while ((scancode = scancode_get_next(&iter)) != 0)
 * {
 *     . . .
 * }
 */
unsigned short
scancode_get_next(unsigned int *iter);

/**
 * Sets the keycode set used for the scancode translation
 *
 * @param kk_set "evdev", "base", or something more
 *        complex (e.g. "evdev+aliases(qwerty)")
 * @result 0 for success
 */
int
scancode_set_keycode_set(const char *kk_set);

/**
 * Gets the keycode set used for the scancode translation
 *
 * @result "evdev", or "base"
 */
const char *
scancode_get_keycode_set(void);

/**
 * Gets the XKB rules set which can be used to access the currently
 * loaded keycode set
 *
 * @result "evdev", or "base"
 */
const char *
scancode_get_xkb_rules(void);

#endif /* SCANCODE_H */
