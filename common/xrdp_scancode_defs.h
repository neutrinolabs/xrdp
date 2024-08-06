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
 * @file    common/xrdp_scancode_defs.h
 * @brief   Scancode global definitions, shared with xorgxrdp
 */

#if !defined(XRDP_SCANCODE_DEFS_H)
#define XRDP_SCANCODE_DEFS_H

enum
{
    /**
     * Scancodes for keys received from the RDP client
     */
    SCANCODE_LSHIFT_KEY = 0x2a,
    SCANCODE_RSHIFT_KEY = 0x36,
    SCANCODE_LCTRL_KEY = 0x1d,
    SCANCODE_RCTRL_KEY = 0x11d,
    SCANCODE_CAPS_KEY = 0x3a,
    SCANCODE_NUMLOCK_KEY = 0x45,
    SCANCODE_SCROLL_KEY = 0x46, // Scroll lock
    SCANCODE_LALT_KEY = 0x38,
    SCANCODE_RALT_KEY = 0x138,
    SCANCODE_LWIN_KEY = 0x15b,
    SCANCODE_RWIN_KEY = 0x15c,
    SCANCODE_MENU_KEY = 0x15d,

    SCANCODE_ESC_KEY = 0x01,
    SCANCODE_BACKSPACE_KEY = 0x0e,
    SCANCODE_ENTER_KEY = 0x1c,
    SCANCODE_TAB_KEY = 0x0f,
    SCANCODE_PAUSE_KEY = 0x21d,

    SCANCODE_KP_ENTER_KEY = 0x11c,
    SCANCODE_KP_DEL_KEY = 0x53,
    SCANCODE_KP_1_KEY = 0x4f,
    SCANCODE_KP_2_KEY = 0x50,
    SCANCODE_KP_4_KEY = 0x4b,
    SCANCODE_KP_6_KEY = 0x4d,
    SCANCODE_KP_7_KEY = 0x47,
    SCANCODE_KP_8_KEY = 0x48,

    SCANCODE_LEFT_ARROW_KEY = 0x14b,
    SCANCODE_RIGHT_ARROW_KEY = 0x14d,
    SCANCODE_UP_ARROW_KEY = 0x148,
    SCANCODE_DOWN_ARROW_KEY = 0x150,

    SCANCODE_HOME_KEY = 0x147,
    SCANCODE_DEL_KEY = 0x153,
    SCANCODE_END_KEY = 0x14f,

    /**
     * Keys affected by numlock
     * (this is not the whole keypad)
     */
    SCANCODE_MIN_NUMLOCK = SCANCODE_KP_7_KEY,
    SCANCODE_MAX_NUMLOCK = SCANCODE_KP_DEL_KEY
};

// Convert key_code and flags values received from a TS_KEYBOARD_EVENT
// into a value suitable for use by this module
#define SCANCODE_FROM_KBD_EVENT(key_code,keyboard_flags) \
    (((key_code) & 0x7f) | ((keyboard_flags) & 0x300))

// Convert a scancode used by this module back into a
// TS_KEYBOARD_EVENT keyCode value
#define SCANCODE_TO_KBD_EVENT_KEY_CODE(scancode) ((scancode) & 0x7f)

// Convert a scancode used by this module back into a
// TS_KEYBOARD_EVENT keyboardFlags value
#define SCANCODE_TO_KBD_EVENT_KBD_FLAGS(scancode) ((scancode) & 0x300)

#endif /* XRDP_SCANCODE_DEFS_H */
