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
 * Convert between RDP scancodes and X11 keycodes.
 *
 * RDP scancodes are largely the same as Windows scancodes. These are
 * indirectly documented in the Microsoft "Keyboard Scan Code Specification",
 * Rev 1.3a (March 16th 2000) and are otherwise known as "Scan code set
 * 1" scancodes. This document no longer appears to be available directly from
 * the Microsoft website.
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

/**
 * Looks up an RDP scancode
 *
 * @param scancode Scancode. Extended scancodes have bit 9 set
 *                 (i.e. are in 0x100 - 0x1ff)
 *  @return keycode, or 0 for no keycode
 */
unsigned short
scancode_to_keycode(unsigned short scancode);

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

#endif /* SCANCODE_H */
