/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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
 */

/**
 * @file    common/scancode.c
 * @brief   Scancode handling
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif
#include <stddef.h>
#include <string.h>

#include "scancode.h"

struct scancode_to_keycode
{
    unsigned short scancode; // 0x1xx implies an extended key
    unsigned char keycode;
};

#define ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

// Sources:-
// file:/usr/share/X11/xkb/keycodes/evdev
// https://wiki.osdev.org/PS/2_Keyboard
// https://www.kbdlayout.info/
static const struct scancode_to_keycode
    evdev_scancode_to_keycode_map[] =
{
    //                   Virtual              XKB
    //                   Key (US)             Symbol
    //                   --------             ------
    { 0x001, 9 },   //   VK_ESCAPE            ESC
    { 0x002, 10 },  //   VK_1                 AE01
    { 0x003, 11 },  //   VK_2                 AE02
    { 0x004, 12 },  //   VK_3                 AE03
    { 0x005, 13 },  //   VK_4                 AE04
    { 0x006, 14 },  //   VK_5                 AE05
    { 0x007, 15 },  //   VK_6                 AE06
    { 0x008, 16 },  //   VK_7                 AE07
    { 0x009, 17 },  //   VK_8                 AE08
    { 0x00a, 18 },  //   VK_9                 AE09
    { 0x00b, 19 },  //   VK_0                 AE10
    { 0x00c, 20 },  //   VK_OEM_MINUS         AE11
    { 0x00d, 21 },  //   VK_OEM_PLUS          AE12
    { 0x00e, 22 },  //   VK_BACK              BKSP
    { 0x00f, 23 },  //   VK_TAB               TAB
    { 0x010, 24 },  //   VK_Q                 AD01
    { 0x011, 25 },  //   VK_W                 AD02
    { 0x012, 26 },  //   VK_E                 AD03
    { 0x013, 27 },  //   VK_R                 AD04
    { 0x014, 28 },  //   VK_T                 AD05
    { 0x015, 29 },  //   VK_Y                 AD06
    { 0x016, 30 },  //   VK_U                 AD07
    { 0x017, 31 },  //   VK_I                 AD08
    { 0x018, 32 },  //   VK_O                 AD09
    { 0x019, 33 },  //   VK_P                 AD10
    { 0x01a, 34 },  //   VK_OEM_4             AD11
    { 0x01b, 35 },  //   VK_OEM_6             AD12
    { 0x01c, 36 },  //   VK_RETURN            RTRN
    { 0x01d, 37 },  //   VK_LCONTROL          LCTL
    { 0x01e, 38 },  //   VK_A                 AC01
    { 0x01f, 39 },  //   VK_S                 AC02
    { 0x020, 40 },  //   VK_D                 AC03
    { 0x021, 41 },  //   VK_F                 AC04
    { 0x022, 42 },  //   VK_G                 AC05
    { 0x023, 43 },  //   VK_H                 AC06
    { 0x024, 44 },  //   VK_J                 AC07
    { 0x025, 45 },  //   VK_K                 AC08
    { 0x026, 46 },  //   VK_L                 AC09
    { 0x027, 47 },  //   VK_OEM_1             AC10
    { 0x028, 48 },  //   VK_OEM_7             AC11
    { 0x029, 49 },  //   VK_OEM_3             TLDE
    { 0x02a, 50 },  //   VK_LSHIFT            LFSH
    { 0x02b, 51 },  //   VK_OEM_5             BKSL
    { 0x02c, 52 },  //   VK_Z                 AB01
    { 0x02d, 53 },  //   VK_X                 AB02
    { 0x02e, 54 },  //   VK_C                 AB03
    { 0x02f, 55 },  //   VK_V                 AB04
    { 0x030, 56 },  //   VK_B                 AB05
    { 0x031, 57 },  //   VK_N                 AB06
    { 0x032, 58 },  //   VK_M                 AB07
    { 0x033, 59 },  //   VK_OEM_COMMA         AB08
    { 0x034, 60 },  //   VK_OEM_PERIOD        AB09
    { 0x035, 61 },  //   VK_OEM_2             AB10
    { 0x036, 62 },  //   VK_RSHIFT            RTSH
    { 0x037, 63 },  //   VK_MULTIPLY          KPMU
    { 0x038, 64 },  //   VK_LMENU             LALT
    { 0x039, 65 },  //   VK_SPACE             SPCE
    { 0x03a, 66 },  //   VK_CAPITAL           CAPS
    { 0x03b, 67 },  //   VK_F1                FK01
    { 0x03c, 68 },  //   VK_F2                FK02
    { 0x03d, 69 },  //   VK_F3                FK03
    { 0x03e, 70 },  //   VK_F4                FK04
    { 0x03f, 71 },  //   VK_F5                FK05
    { 0x040, 72 },  //   VK_F6                FK06
    { 0x041, 73 },  //   VK_F7                FK07
    { 0x042, 74 },  //   VK_F8                FK08
    { 0x043, 75 },  //   VK_F9                FK09
    { 0x044, 76 },  //   VK_F10               FK10
    { 0x045, 77 },  //   VK_NUMLOCK           NMLK
    { 0x046, 78 },  //   VK_SCROLL            SCLK
    { 0x047, 79 },  //   VK_HOME              KP7
    { 0x048, 80 },  //   VK_UP                KP8
    { 0x049, 81 },  //   VK_PRIOR             KP9
    { 0x04a, 82 },  //   VK_SUBTRACT          KPSU
    { 0x04b, 83 },  //   VK_LEFT              KP4
    { 0x04c, 84 },  //   VK_CLEAR             KP5
    { 0x04d, 85 },  //   VK_RIGHT             KP6
    { 0x04e, 86 },  //   VK_ADD               KPAD
    { 0x04f, 87 },  //   VK_END               KP1
    { 0x050, 88 },  //   VK_DOWN              KP2
    { 0x051, 89 },  //   VK_NEXT              KP3
    { 0x052, 90 },  //   VK_INSERT            KP0
    { 0x053, 91 },  //   VK_DELETE            KPDL
    { 0x056, 94 },  //   VK_OEM_102           LSGT
    { 0x057, 95 },  //   VK_F11               FK11
    { 0x058, 96 },  //   VK_F12               FK12
    { 0x070, 101 }, //   -                    HKTG
    { 0x073, 97 },  //   VK_ABNT_C1           AB11
    { 0x079, 100 }, //   -                    HENK
    { 0x07b, 102 }, //   VK_OEM_PA1           MUHE
    { 0x07d, 132 }, //   -                    AE13
    { 0x07e, 129 }, //   VK_ABNT_C2           KPPT (Brazil ABNT2)
    { 0x110, 173 }, //   VK_MEDIA_PREV_TRACK  I173 (KEY_PREVIOUSSONG)
    { 0x119, 171 }, //   VK_MEDIA_NEXT_TRACK  I171 (KEY_NEXTSONG)
    { 0x11c, 104 }, //   VK_RETURN            KPEN
    { 0x11d, 105 }, //   VK_RCONTROL          RCTL
    { 0x120, 121 }, //   VK_VOLUME_MUTE       MUTE
    { 0x121, 148 }, //   VK_LAUNCH_APP2       I148 (KEY_CALC)
    { 0x122, 172 }, //   VK_PLAY_PAUSE        I172 (KEY_PLAYPAUSE)
    { 0x124, 174 }, //   VK_MEDIA_STOP        I174 (KEY_STOPCD)
    { 0x12e, 122 }, //   VK_VOLUME_DOWN       VOL-
    { 0x130, 123 }, //   VK_VOLUME_UP         VOL+
    { 0x132, 180 }, //   VK_BROWSER_HOME      I180 (KEY_HOMEPAGE)
    { 0x135, 106 }, //   VK_DIVIDE            KPDV
    { 0x137, 107 }, //   VK_SNAPSHOT          PRSC
    { 0x138, 108 }, //   VK_RMENU             RALT
    { 0x147, 110 }, //   VK_HOME              HOME
    { 0x148, 111 }, //   VK_UP                UP
    { 0x149, 112 }, //   VK_PRIOR             PGUP (KEY_COMPUTER)
    { 0x14b, 113 }, //   VK_LEFT              LEFT
    { 0x14d, 114 }, //   VK_RIGHT             RGHT
    { 0x14f, 115 }, //   VK_END               END
    { 0x150, 116 }, //   VK_DOWN              DOWN
    { 0x151, 117 }, //   VK_NEXT              PGDN
    { 0x152, 118 }, //   VK_INSERT            INS
    { 0x153, 119 }, //   VK_DELETE            DELE
    { 0x15b, 133 }, //   VK_LWIN              LWIN
    { 0x15c, 134 }, //   VK_RWIN              RWIN
    { 0x15d, 135 }, //   VK_APPS              COMP
    { 0x165, 225 }, //   VK_BROWSER_SEARCH    I225 (KEY_SEARCH)
    { 0x166, 164 }, //   VK_BROWSER_FAVORITES I164 (KEY_BOOKMARKS)
    { 0x16b, 165 }, //   VK_LAUNCH_APP1       I165 (KEY_COMPUTER)
    { 0x16c, 163 }, //   VK_LAUNCH_MAIL       I163 (KEY_MAIL)
    { 0x21d, 127 }  //   VK_PAUSE             PAUS (KEY_PAUSE)
};

// Sources:-
// file:/usr/share/X11/xkb/keycodes/xfree86
// https://wiki.osdev.org/PS/2_Keyboard
// https://www.kbdlayout.info/
static const struct scancode_to_keycode
    base_scancode_to_keycode_map[] =
{
    //                   Virtual              XKB
    //                   Key (US)             Symbol
    //                   --------             ------
    { 0x001, 9 },   //   VK_ESCAPE            ESC
    { 0x002, 10 },  //   VK_1                 AE01
    { 0x003, 11 },  //   VK_2                 AE02
    { 0x004, 12 },  //   VK_3                 AE03
    { 0x005, 13 },  //   VK_4                 AE04
    { 0x006, 14 },  //   VK_5                 AE05
    { 0x007, 15 },  //   VK_6                 AE06
    { 0x008, 16 },  //   VK_7                 AE07
    { 0x009, 17 },  //   VK_8                 AE08
    { 0x00a, 18 },  //   VK_9                 AE09
    { 0x00b, 19 },  //   VK_0                 AE10
    { 0x00c, 20 },  //   VK_OEM_MINUS         AE11
    { 0x00d, 21 },  //   VK_OEM_PLUS          AE12
    { 0x00e, 22 },  //   VK_BACK              BKSP
    { 0x00f, 23 },  //   VK_TAB               TAB
    { 0x010, 24 },  //   VK_Q                 AD01
    { 0x011, 25 },  //   VK_W                 AD02
    { 0x012, 26 },  //   VK_E                 AD03
    { 0x013, 27 },  //   VK_R                 AD04
    { 0x014, 28 },  //   VK_T                 AD05
    { 0x015, 29 },  //   VK_Y                 AD06
    { 0x016, 30 },  //   VK_U                 AD07
    { 0x017, 31 },  //   VK_I                 AD08
    { 0x018, 32 },  //   VK_O                 AD09
    { 0x019, 33 },  //   VK_P                 AD10
    { 0x01a, 34 },  //   VK_OEM_4             AD11
    { 0x01b, 35 },  //   VK_OEM_6             AD12
    { 0x01c, 36 },  //   VK_RETURN            RTRN
    { 0x01d, 37 },  //   VK_LCONTROL          LCTL
    { 0x01e, 38 },  //   VK_A                 AC01
    { 0x01f, 39 },  //   VK_S                 AC02
    { 0x020, 40 },  //   VK_D                 AC03
    { 0x021, 41 },  //   VK_F                 AC04
    { 0x022, 42 },  //   VK_G                 AC05
    { 0x023, 43 },  //   VK_H                 AC06
    { 0x024, 44 },  //   VK_J                 AC07
    { 0x025, 45 },  //   VK_K                 AC08
    { 0x026, 46 },  //   VK_L                 AC09
    { 0x027, 47 },  //   VK_OEM_1             AC10
    { 0x028, 48 },  //   VK_OEM_7             AC11
    { 0x029, 49 },  //   VK_OEM_3             TLDE
    { 0x02a, 50 },  //   VK_LSHIFT            LFSH
    { 0x02b, 51 },  //   VK_OEM_5             BKSL
    { 0x02c, 52 },  //   VK_Z                 AB01
    { 0x02d, 53 },  //   VK_X                 AB02
    { 0x02e, 54 },  //   VK_C                 AB03
    { 0x02f, 55 },  //   VK_V                 AB04
    { 0x030, 56 },  //   VK_B                 AB05
    { 0x031, 57 },  //   VK_N                 AB06
    { 0x032, 58 },  //   VK_M                 AB07
    { 0x033, 59 },  //   VK_OEM_COMMA         AB08
    { 0x034, 60 },  //   VK_OEM_PERIOD        AB09
    { 0x035, 61 },  //   VK_OEM_2             AB10
    { 0x036, 62 },  //   VK_RSHIFT            RTSH
    { 0x037, 63 },  //   VK_MULTIPLY          KPMU
    { 0x038, 64 },  //   VK_LMENU             LALT
    { 0x039, 65 },  //   VK_SPACE             SPCE
    { 0x03a, 66 },  //   VK_CAPITAL           CAPS
    { 0x03b, 67 },  //   VK_F1                FK01
    { 0x03c, 68 },  //   VK_F2                FK02
    { 0x03d, 69 },  //   VK_F3                FK03
    { 0x03e, 70 },  //   VK_F4                FK04
    { 0x03f, 71 },  //   VK_F5                FK05
    { 0x040, 72 },  //   VK_F6                FK06
    { 0x041, 73 },  //   VK_F7                FK07
    { 0x042, 74 },  //   VK_F8                FK08
    { 0x043, 75 },  //   VK_F9                FK09
    { 0x044, 76 },  //   VK_F10               FK10
    { 0x045, 77 },  //   VK_NUMLOCK           NMLK
    { 0x046, 78 },  //   VK_SCROLL            SCLK
    { 0x047, 79 },  //   VK_HOME              KP7
    { 0x048, 80 },  //   VK_UP                KP8
    { 0x049, 81 },  //   VK_PRIOR             KP9
    { 0x04a, 82 },  //   VK_SUBTRACT          KPSU
    { 0x04b, 83 },  //   VK_LEFT              KP4
    { 0x04c, 84 },  //   VK_CLEAR             KP5
    { 0x04d, 85 },  //   VK_RIGHT             KP6
    { 0x04e, 86 },  //   VK_ADD               KPAD
    { 0x04f, 87 },  //   VK_END               KP1
    { 0x050, 88 },  //   VK_DOWN              KP2
    { 0x051, 89 },  //   VK_NEXT              KP3
    { 0x052, 90 },  //   VK_INSERT            KP0
    { 0x053, 91 },  //   VK_DELETE            KPDL
    { 0x056, 94 },  //   VK_OEM_102           LSGT
    { 0x057, 95 },  //   VK_F11               FK11
    { 0x058, 96 },  //   VK_F12               FK12
    { 0x070, 208 }, //   -                    HKTG
    { 0x073, 211 }, //   VK_ABNT_C1           AB11
    { 0x079, 129 }, //   -                    XFER
    { 0x07b, 131 }, //   VK_OEM_PA1           NFER
    { 0x07d, 133 }, //   -                    AE13
    { 0x07e, 134 }, //   VK_ABNT_C2           KPPT (Brazil ABNT2)
    { 0x11c, 108 }, //   VK_RETURN            KPEN
    { 0x11d, 109 }, //   VK_RCONTROL          RCTL
    { 0x120, 141 }, //   VK_VOLUME_MUTE       MUTE
    { 0x12e, 142 }, //   VK_VOLUME_DOWN       VOL-
    { 0x130, 143 }, //   VK_VOLUME_UP         VOL+
    { 0x135, 112 }, //   VK_DIVIDE            KPDV
    { 0x137, 121 }, //   VK_SNAPSHOT          PRSC
    { 0x138, 113 }, //   VK_RMENU             RALT
    { 0x147, 97 },  //   VK_HOME              HOME
    { 0x148, 98 },  //   VK_UP                UP
    { 0x149, 99 },  //   VK_PRIOR             PGUP (KEY_COMPUTER)
    { 0x14b, 100 }, //   VK_LEFT              LEFT
    { 0x14d, 102 }, //   VK_RIGHT             RGHT
    { 0x14f, 103 }, //   VK_END               END
    { 0x150, 104 }, //   VK_DOWN              DOWN
    { 0x151, 105 }, //   VK_NEXT              PGDN
    { 0x152, 106 }, //   VK_INSERT            INS
    { 0x153, 107 }, //   VK_DELETE            DELE
    { 0x15b, 115 }, //   VK_LWIN              LWIN
    { 0x15c, 116 }, //   VK_RWIN              RWIN
    { 0x15d, 117 }, //   VK_APPS              COMP
    { 0x21d, 110 }  //   VK_PAUSE             PAUS (KEY_PAUSE)
};

struct map_settings
{
    const struct scancode_to_keycode *map;
    const char *name;
    unsigned int size;
};

enum settings_index
{
    SI_EVDEV = 0,
    SI_BASE
};

const struct map_settings global_settings[] =
{
    {
        // SI_EVDEV
        .map = evdev_scancode_to_keycode_map,
        .name = "evdev",
        .size = ELEMENTS(evdev_scancode_to_keycode_map),
    },
    {
        // SI_BASE
        .map = base_scancode_to_keycode_map,
        .name = "base",
        .size = ELEMENTS(base_scancode_to_keycode_map)
    }
};

// Default mapping set is "evdev"
const struct map_settings *settings = &global_settings[SI_EVDEV];

/*****************************************************************************/
int
scancode_to_index(unsigned short scancode)
{
    if (scancode <= 0x7f)
    {
        return scancode;
    }
    if (scancode <= 0xff)
    {
        // 0x80 - 0xff : Invalid code
        return -1;
    }
    if (scancode <= 0x177)
    {
        // 01x100 - 0x177 : Move bit 9 to bit 8
        return (scancode & 0x7f) | 0x80;
    }

    if (scancode == SCANCODE_PAUSE_KEY)
    {
        return SCANCODE_INDEX_PAUSE_KEY;
    }

    // This leaves the following which are all rejected
    // 0x178 - 0x17f (currently unused). These would map to indexes 0xf8
    //               to 0xff which we are reserving for extended1 keys.
    // 0x180 - 0x1ff Illegal format
    // >0x200        Anything not mentioned explicitly above (e.g.
    //               SCANCODE_PAUSE_KEY)
    return -1;
}

/*****************************************************************************/
unsigned short
scancode_from_index(int index)
{
    index &= 0xff;
    unsigned short result;
    if (index == SCANCODE_INDEX_PAUSE_KEY)
    {
        result = SCANCODE_PAUSE_KEY;
    }
    else if (index < 0x80)
    {
        result = index;
    }
    else
    {
        result = (index & 0x7f) | 0x100;
    }
    return result;
}

/*****************************************************************************/
unsigned short
scancode_to_x11_keycode(unsigned short scancode)
{
    unsigned int min = 0;
    unsigned int max = settings->size;
    unsigned short rv = 0;

    // Check scancode is in range of map
    if (scancode >= settings->map[min].scancode &&
            scancode <= settings->map[max - 1].scancode)
    {
        // Use a binary chop to locate the correct index
        // in logarithmic time.
        while (1)
        {
            unsigned int index = (min + max) / 2;
            if (scancode == settings->map[index].scancode)
            {
                rv = settings->map[index].keycode;
                break;
            }
            // Adjust min or max, checking for end
            // of iteration
            if (scancode < settings->map[index].scancode)
            {
                max = index;
            }
            else if (min == index)
            {
                // We've already checked this value
                break;
            }
            else
            {
                min = index;
            }
        }
    }

    return rv;
}

/*****************************************************************************/
unsigned short
scancode_get_next(unsigned int *iter)
{
    unsigned short rv = 0;
    if (*iter < settings->size)
    {
        rv = settings->map[*iter].scancode;
        ++(*iter);
    }

    return rv;
}

/*****************************************************************************/
int
scancode_set_keycode_set(const char *kk_set)
{
    int rv = 0;

    if (kk_set == NULL)
    {
        rv = 1;
    }
    else if (strncmp(kk_set, "evdev", 5) == 0)
    {
        settings = &global_settings[SI_EVDEV];
    }
    else if (strncmp(kk_set, "base", 4) == 0)
    {
        settings = &global_settings[SI_BASE];
    }
    else if (strncmp(kk_set, "xfree86", 7) == 0)
    {
        settings = &global_settings[SI_BASE];
    }
    else
    {
        rv = 1;
    }

    return rv;
}

/*****************************************************************************/
const char *
scancode_get_keycode_set(void)
{
    return settings->name;
}

/*****************************************************************************/
const char *
scancode_get_xkb_rules(void)
{
    // Currently supported keycods map directly to the same name for
    // the rules which use them.
    return settings->name;
}
