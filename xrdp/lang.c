/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * keylayout
 * maximum unicode 19996(0x4e00)
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"
#include "string_calls.h"

/* map for rdp to x11 scancodes
   code1 is regular scancode, code2 is extended scancode */
struct codepair
{
    tui8 code1;
    tui8 code2;
};
static struct codepair g_map[] =
{
    { 0, 0 }, { 9, 0 }, { 10, 0 }, { 11, 0 }, { 12, 0 }, /* 0 - 4 */
    { 13, 0 }, { 14, 0 }, { 15, 0 }, { 16, 0 }, { 17, 0 }, /* 5 - 9 */
    { 18, 0 }, { 19, 0 }, { 20, 0 }, { 21, 0 }, { 22, 0 }, /* 10 - 14 */
    { 23, 0 }, { 24, 0 }, { 25, 0 }, { 26, 0 }, { 27, 0 }, /* 15 - 19 */
    { 28, 0 }, { 29, 0 }, { 30, 0 }, { 31, 0 }, { 32, 0 }, /* 20 - 24 */
    { 33, 0 }, { 34, 0 }, { 35, 0 }, { 36, 108 }, { 37, 109 }, /* 25 - 29 */
    { 38, 0 }, { 39, 0 }, { 40, 0 }, { 41, 0 }, { 42, 0 }, /* 30 - 34 */
    { 43, 0 }, { 44, 0 }, { 45, 0 }, { 46, 0 }, { 47, 0 }, /* 35 - 39 */
    { 48, 0 }, { 49, 0 }, { 50, 0 }, { 51, 0 }, { 52, 0 }, /* 40 - 44 */
    { 53, 0 }, { 54, 0 }, { 55, 0 }, { 56, 0 }, { 57, 0 }, /* 45 - 49 */
    { 58, 0 }, { 59, 0 }, { 60, 0 }, { 61, 112 }, { 62, 0 }, /* 50 - 54 */
    { 63, 111 }, { 64, 113 }, { 65, 0 }, { 66, 0 }, { 67, 0 }, /* 55 - 59 */
    { 68, 0 }, { 69, 0 }, { 70, 0 }, { 71, 0 }, { 72, 0 }, /* 60 - 64 */
    { 73, 0 }, { 74, 0 }, { 75, 0 }, { 76, 0 }, { 77, 0 }, /* 65 - 69 */
    { 78, 0 }, { 79, 97 }, { 80, 98 }, { 81, 99 }, { 82, 0 }, /* 70 - 74 */
    { 83, 100 }, { 84, 0 }, { 85, 102 }, { 86, 0 }, { 87, 103 }, /* 75 - 79 */
    { 88, 104 }, { 89, 105 }, { 90, 106 }, { 91, 107 }, { 92, 0 }, /* 80 - 84 */
    { 93, 0 }, { 94, 0 }, { 95, 0 }, { 96, 0 }, { 97, 0 }, /* 85 - 89 */
    { 98, 0 }, { 0, 115 }, { 0, 116 }, { 0, 117 }, { 102, 0 }, /* 90 - 94 */
    { 103, 0 }, { 104, 0 }, { 105, 0 }, { 106, 0 }, { 107, 0 }, /* 95 - 99 */
    { 108, 0 }, { 109, 0 }, { 110, 0 }, { 111, 0 }, { 112, 0 }, /* 100 - 104 */
    { 113, 0 }, { 114, 0 }, { 115, 0 }, { 116, 0 }, { 117, 0 }, /* 105 - 109 */
    { 118, 0 }, { 119, 0 }, { 120, 0 }, { 121, 0 }, { 122, 0 }, /* 110 - 114 */
    { 123, 0 }, { 124, 0 }, { 125, 0 }, { 126, 0 }, { 127, 0 }, /* 115 - 119 */
    { 128, 0 }, { 129, 0 }, { 130, 0 }, { 131, 0 }, { 132, 0 }, /* 120 - 124 */
    { 133, 0 }, { 134, 0 }, { 135, 0 } /* 125 - 127 */
};

/*****************************************************************************/
struct xrdp_key_info *
get_key_info_from_scan_code(int device_flags, int scan_code, int *keys,
                            int caps_lock, int num_lock, int scroll_lock,
                            struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *rv;
    int shift;
    int altgr;
    int ext;
    int index;

    ext = device_flags & KBD_FLAG_EXT;  /* 0x0100 */
    shift = keys[42] || keys[54];
    altgr = keys[56] & KBD_FLAG_EXT;  /* right alt */
    rv = 0;
    scan_code = scan_code & 0x7f;
    index = ext ? g_map[scan_code].code2 : g_map[scan_code].code1;

    /* keymap file is created with numlock off so we have to do this */
    if ((index >= 79) && (index <= 91))
    {
        if (num_lock)
        {
            rv = &(keymap->keys_shift[index]);
        }
        else
        {
            rv = &(keymap->keys_noshift[index]);
        }
    }
    else if (shift && caps_lock && altgr)
    {
        rv = &(keymap->keys_shiftcapslockaltgr[index]);
    }
    else if (shift && caps_lock)
    {
        rv = &(keymap->keys_shiftcapslock[index]);
    }
    else if (shift && altgr)
    {
        rv = &(keymap->keys_shiftaltgr[index]);
    }
    else if (shift)
    {
        rv = &(keymap->keys_shift[index]);
    }
    else if (caps_lock && altgr)
    {
        rv = &(keymap->keys_capslockaltgr[index]);
    }
    else if (caps_lock)
    {
        rv = &(keymap->keys_capslock[index]);
    }
    else if (altgr)
    {
        rv = &(keymap->keys_altgr[index]);
    }
    else
    {
        rv = &(keymap->keys_noshift[index]);
    }

    return rv;
}

/*****************************************************************************/
int
get_keysym_from_scan_code(int device_flags, int scan_code, int *keys,
                          int caps_lock, int num_lock, int scroll_lock,
                          struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *ki;

    ki = get_key_info_from_scan_code(device_flags, scan_code, keys,
                                     caps_lock, num_lock, scroll_lock,
                                     keymap);

    if (ki == 0)
    {
        return 0;
    }

    return ki->sym;
}

/*****************************************************************************/
char32_t
get_char_from_scan_code(int device_flags, int scan_code, int *keys,
                        int caps_lock, int num_lock, int scroll_lock,
                        struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *ki;

    ki = get_key_info_from_scan_code(device_flags, scan_code, keys,
                                     caps_lock, num_lock, scroll_lock,
                                     keymap);

    if (ki == 0)
    {
        return 0;
    }

    return ki->chr;
}

/*****************************************************************************/
static int
km_read_section(int fd, const char *section_name, struct xrdp_key_info *keymap)
{
    struct list *names;
    struct list *values;
    int index;
    int code;
    int pos1;
    char *name;
    char *value;

    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;

    if (file_read_section(fd, section_name, names, values) == 0)
    {
        for (index = names->count - 1; index >= 0; index--)
        {
            name = (char *)list_get_item(names, index);
            value = (char *)list_get_item(values, index);

            if ((name != 0) && (value != 0))
            {
                if (g_strncasecmp(name, "key", 3) == 0)
                {
                    code = g_atoi(name + 3);
                }
                else
                {
                    code = g_atoi(name);
                }

                if ((code >= 0) && (code < 256))
                {
                    pos1 = g_pos(value, ":");

                    if (pos1 >= 0)
                    {
                        keymap[code].chr = g_atoi(value + pos1 + 1);
                    }

                    keymap[code].sym = g_atoi(value);
                }
            }
        }
    }

    list_delete(names);
    list_delete(values);
    return 0;
}

/*****************************************************************************/
int
get_keymaps(int keylayout, struct xrdp_keymap *keymap)
{
    int basic_key_layout = keylayout & 0x0000ffff;
    char *filename;
    struct xrdp_keymap *lkeymap;

    filename = (char *)g_malloc(256, 0);

    /* check if there is a keymap file e.g. km-e00100411.ini */
    g_snprintf(filename, 255, "%s/km-%08x.ini", XRDP_CFG_PATH, keylayout);

    /* if the file does not exist, use only lower 16 bits instead */
    if (!g_file_exist(filename))
    {
        LOG(LOG_LEVEL_WARNING, "Cannot find keymap file %s", filename);
        /* e.g. km-00000411.ini */
        g_snprintf(filename, 255, "%s/km-%08x.ini", XRDP_CFG_PATH, basic_key_layout);
    }

    /* finally, use 'en-us' */
    if (!g_file_exist(filename))
    {
        LOG(LOG_LEVEL_WARNING, "Cannot find keymap file %s", filename);
        g_snprintf(filename, 255, "%s/km-00000409.ini", XRDP_CFG_PATH);
    }

    if (g_file_exist(filename))
    {

        lkeymap = (struct xrdp_keymap *)g_malloc(sizeof(struct xrdp_keymap), 0);
        /* make a copy of the built-in keymap */
        g_memcpy(lkeymap, keymap, sizeof(struct xrdp_keymap));

        km_load_file(filename, keymap);

        if (g_memcmp(lkeymap, keymap, sizeof(struct xrdp_keymap)) != 0)
        {
            LOG(LOG_LEVEL_WARNING,
                "local keymap file for 0x%08x found and doesn't match "
                "built in keymap, using local keymap file", keylayout);
        }

        g_free(lkeymap);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "File does not exist: %s", filename);
    }

    g_free(filename);
    return 0;
}

/*****************************************************************************/
int km_load_file(const char *filename, struct xrdp_keymap *keymap)
{
    int fd;

    LOG(LOG_LEVEL_INFO, "Loading keymap file %s", filename);
    fd = g_file_open_ro(filename);

    if (fd != -1)
    {
        /* read the keymaps */
        km_read_section(fd, "noshift", keymap->keys_noshift);
        km_read_section(fd, "shift", keymap->keys_shift);
        km_read_section(fd, "altgr", keymap->keys_altgr);
        km_read_section(fd, "shiftaltgr", keymap->keys_shiftaltgr);
        km_read_section(fd, "capslock", keymap->keys_capslock);
        km_read_section(fd, "capslockaltgr", keymap->keys_capslockaltgr);
        km_read_section(fd, "shiftcapslock", keymap->keys_shiftcapslock);
        km_read_section(fd, "shiftcapslockaltgr", keymap->keys_shiftcapslockaltgr);

        g_file_close(fd);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "Error loading keymap file %s (%s)", filename, g_get_strerror());
        return 1;
    }

    return 0;
}
