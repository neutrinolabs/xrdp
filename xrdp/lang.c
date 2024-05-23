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

#include <ctype.h>

#include "xrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"
#include "string_calls.h"
#include "toml.h"

// Macro to return 0..15 for a valid isxdigit() character
#define XDIGIT_TO_VAL(d) (\
                          isdigit(d) ? (d) - '0' : \
                          ((d) >= 'a' && (d) <= 'f') ? (d) - 'a' + 10 : \
                          (d) - 'A' + 10)

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
    index = INDEX_FROM_SCANCODE(scan_code, ext);

    if (num_lock &&
            index >= XR_RDP_SCAN_MIN_NUMLOCK && index <= XR_RDP_SCAN_MAX_NUMLOCK)
    {
        rv = &(keymap->keys_numlock[index - XR_RDP_SCAN_MIN_NUMLOCK]);
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
/**
 * Tests a table key to see if it's a valid scancode
 *
 * @param key Table key
 * @param[out] scancode scancode index value (0..255) if 1 is returned
 * @return Boolean != 0 if the key is valid
 */
static int
is_valid_scancode(const char *key, int *scancode)
{
    int rv = 0;
    int extended = 0;
    if ((key[0] == 'E' || key[0] == 'e') && key[1] == '0' && key[2] == '_')
    {
        extended = 1;
        key += 3;
    }

    if (isxdigit(key[0]) && isxdigit(key[1]) && key[2] == '\0')
    {
        rv = 1;
        *scancode = XDIGIT_TO_VAL(key[0]) * 16 + XDIGIT_TO_VAL(key[1]);
        *scancode = INDEX_FROM_SCANCODE(*scancode, extended);
    }
    return rv;
}

/*****************************************************************************/
/**
 * Tests a value to see if it's a valid KeySym (decimal number)
 *
 * @param val
 * @param[out] keysym. Keysym value if 1 is returned
 * @return Boolean != 0 if the string is valid
 */
static int
is_valid_keysym(const char *val, int *sym)
{
    int rv = 0;
    int s = 0;
    if (*val != '\0')
    {
        while (isdigit(*val))
        {
            s = s * 10 + (*val++ - '0');
        }
        if (*val == '\0')
        {
            *sym = s;
            rv = 1;
        }
    }

    return rv;
}

/*****************************************************************************/
/**
 * Tests a value to see if it's a valid unicode character (U+xxxx)
 *
 * @param val
 * @param[out] chr value if 1 is returned
 * @return Boolean != 0 if the string is valid
 */
static int
is_valid_unicode_char(const char *val, char32_t *chr)
{
    int rv = 0;

    if ((val[0] == 'U' || val[0] == 'u') &&
            val[1] == '+' && isxdigit(val[2]))
    {
        val += 2;  // Skip 'U+'
        const char *p = val;
        char32_t c = 0;

        while (isxdigit(*p))
        {
            c = c * 16 + XDIGIT_TO_VAL(*p);
            ++p;
        }

        if (*p == '\0' && (p - val) >= 4 && (p - val) <= 6)
        {
            rv = 1;
            *chr = c;
        }
    }

    return rv;
}

/*****************************************************************************/
/**
 * keymap must be cleared before calling this function
 */
static int
km_read_section(toml_table_t *tfile, const char *section_name,
                struct xrdp_key_info *keymap)
{
    toml_table_t *section = toml_table_in(tfile, section_name);

    if (section == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "Section [%s] not found in keymap file",
            section_name);
    }
    else
    {
        const char *key;
        toml_datum_t  val;
        int index;
        int scancode; // index value 0..255
        char *p;
        const char *unicode_str;
        for (index = 0 ; (key = toml_key_in(section, index)) != NULL; ++index)
        {
            if (!is_valid_scancode(key, &scancode))
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't parse value '%s' in [%s] in keymap file",
                    key, section_name);
                continue;
            }
            val = toml_string_in(section, key);
            if (!val.ok)
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't read value for [%s]:%s in keymap file",
                    section_name, key);
                continue;
            }

            // Does the value contain a unicode character?
            if ((p = strchr(val.u.s, ':')) != NULL)
            {
                unicode_str = (p + 1);
                *p = '\0'; // val is a copy, writeable by us
            }
            else
            {
                unicode_str = NULL;
            }

            /* Parse both values and add them to the keymap, logging any
             * errors */
            if (!is_valid_keysym(val.u.s, &keymap[scancode].sym))
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't read KeySym for [%s]:%s in keymap file",
                    section_name, key);
            }

            if (unicode_str != NULL &&
                    !is_valid_unicode_char(unicode_str, &keymap[scancode].chr))
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't read unicode character for [%s]:%s in keymap file",
                    section_name, key);
            }

            free(val.u.s);
        }
    }

    return 0;
}

/*****************************************************************************/
int
get_keymaps(int keylayout, struct xrdp_keymap *keymap)
{
    int basic_key_layout = keylayout & 0x0000ffff;
    char filename[256];
    int layout_list[10];
    int layout_count = 0;
    int i;

    /* Work out a list of layouts to try to load */
    layout_list[layout_count++] = keylayout; // Requested layout
    if (basic_key_layout != keylayout)
    {
        layout_list[layout_count++] = basic_key_layout; // First fallback
    }
    layout_list[layout_count++] = 0x0409; // Last chance 'en-US'

    /* search for a loadable layout in the list */
    for (i = 0; i < layout_count; ++i)
    {
        // Convert key layout to a filename
        g_snprintf(filename, sizeof(filename),
                   XRDP_CFG_PATH "/km-%08x.toml", layout_list[i]);

        if (km_load_file(filename, keymap) == 0)
        {
            return 0;
        }
    }

    /* No luck finding anything */
    LOG(LOG_LEVEL_ERROR, "Cannot find a usable keymap file");

    return 0;
}

/*****************************************************************************/
int
km_load_file(const char *filename, struct xrdp_keymap *keymap)
{
    FILE *fp;
    toml_table_t *tfile;
    char errbuf[200];
    int rv = 0;

    if ((fp = fopen(filename, "r")) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Error loading keymap file %s (%s)",
            filename, g_get_strerror());
        rv = 1;
    }
    else if ((tfile = toml_parse_file(fp, errbuf, sizeof(errbuf))) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Error in keymap file %s - %s", filename, errbuf);
        fclose(fp);
        rv = 1;
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "Loading keymap file %s", filename);
        fclose(fp);

        /* Clear the whole keymap */
        memset(keymap, 0, sizeof(*keymap));

        /* read the keymap sections */
        km_read_section(tfile, "noshift", keymap->keys_noshift);
        km_read_section(tfile, "shift", keymap->keys_shift);
        km_read_section(tfile, "altgr", keymap->keys_altgr);
        km_read_section(tfile, "shiftaltgr", keymap->keys_shiftaltgr);
        km_read_section(tfile, "capslock", keymap->keys_capslock);
        km_read_section(tfile, "capslockaltgr", keymap->keys_capslockaltgr);
        km_read_section(tfile, "shiftcapslock", keymap->keys_shiftcapslock);
        km_read_section(tfile, "shiftcapslockaltgr",
                        keymap->keys_shiftcapslockaltgr);

        /* The numlock map is much smaller and offset by
         * XR_RDP_SCAN_MAX_NUMLOCK. Read the section into a temporary
         * area and copy it over */
        struct xrdp_key_info keys_numlock[256];
        int i;
        for (i = XR_RDP_SCAN_MIN_NUMLOCK; i <= XR_RDP_SCAN_MAX_NUMLOCK; ++i)
        {
            keys_numlock[i].sym = 0;
            keys_numlock[i].chr = 0;
        }
        km_read_section(tfile, "numlock", keys_numlock);
        for (i = XR_RDP_SCAN_MIN_NUMLOCK; i <= XR_RDP_SCAN_MAX_NUMLOCK; ++i)
        {
            keymap->keys_numlock[i - XR_RDP_SCAN_MIN_NUMLOCK] = keys_numlock[i];
        }

        toml_free(tfile);
    }

    return rv;
}
