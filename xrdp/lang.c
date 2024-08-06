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
get_key_info_from_kbd_event(int keyboard_flags, int key_code, int *keys,
                            int caps_lock, int num_lock, int scroll_lock,
                            struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *rv;
    int shift;
    int altgr;
    int index;

    shift = keys[SCANCODE_INDEX_LSHIFT_KEY] || keys[SCANCODE_INDEX_RSHIFT_KEY];
    altgr = keys[SCANCODE_INDEX_RALT_KEY];  /* right alt */
    rv = 0;

    index = scancode_to_index(SCANCODE_FROM_KBD_EVENT(key_code, keyboard_flags));
    if (index >= 0)
    {
        // scancode_to_index() guarantees to map numlock scancodes
        // to the same index values.
        if (num_lock &&
                index >= SCANCODE_MIN_NUMLOCK &&
                index <= SCANCODE_MAX_NUMLOCK)
        {
            rv = &(keymap->keys_numlock[index - SCANCODE_MIN_NUMLOCK]);
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
    }

    return rv;
}

/*****************************************************************************/
int
get_keysym_from_kbd_event(int keyboard_flags, int key_code, int *keys,
                          int caps_lock, int num_lock, int scroll_lock,
                          struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *ki;

    ki = get_key_info_from_kbd_event(keyboard_flags, key_code, keys,
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
get_char_from_kbd_event(int keyboard_flags, int key_code, int *keys,
                        int caps_lock, int num_lock, int scroll_lock,
                        struct xrdp_keymap *keymap)
{
    struct xrdp_key_info *ki;

    ki = get_key_info_from_kbd_event(keyboard_flags, key_code, keys,
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
 * Converts a table key to a scancode index value
 *
 * @param key Table key
 * @return index >= 0, or < 0 for an invalid key
 */
static int
key_to_scancode_index(const char *key)
{
    int rv = -1;
    int keyboard_flags = 0;
    if ((key[0] == 'E' || key[0] == 'e') && key[2] == '_')
    {
        if (key[1] == '0')
        {
            keyboard_flags |= KBDFLAGS_EXTENDED;
            key += 3;
        }
        else if (key[1] == '1')
        {
            keyboard_flags |= KBDFLAGS_EXTENDED1;
            key += 3;
        }
    }

    if (isxdigit(key[0]) && isxdigit(key[1]) && key[2] == '\0')
    {
        int code = XDIGIT_TO_VAL(key[0]) * 16 + XDIGIT_TO_VAL(key[1]);
        rv = scancode_to_index(SCANCODE_FROM_KBD_EVENT(code, keyboard_flags));
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
        int i;
        char *p;
        const char *unicode_str;
        for (i = 0 ; (key = toml_key_in(section, i)) != NULL; ++i)
        {
            // Get a scancode index from the key if possible
            int sindex = key_to_scancode_index(key);
            if (sindex < 0)
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
            if (!is_valid_keysym(val.u.s, &keymap[sindex].sym))
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't read KeySym for [%s]:%s in keymap file",
                    section_name, key);
            }

            if (unicode_str != NULL &&
                    !is_valid_unicode_char(unicode_str, &keymap[sindex].chr))
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
         * SCANCODE_MIX_NUMLOCK. Read the section into a temporary
         * area and copy it over */
        struct xrdp_key_info keys_numlock[SCANCODE_MAX_INDEX + 1];
        int i;
        for (i = SCANCODE_MIN_NUMLOCK; i <= SCANCODE_MAX_NUMLOCK; ++i)
        {
            keys_numlock[i].sym = 0;
            keys_numlock[i].chr = 0;
        }
        km_read_section(tfile, "numlock", keys_numlock);
        for (i = SCANCODE_MIN_NUMLOCK; i <= SCANCODE_MAX_NUMLOCK; ++i)
        {
            keymap->keys_numlock[i - SCANCODE_MIN_NUMLOCK] = keys_numlock[i];
        }

        toml_free(tfile);
    }

    return rv;
}

/*****************************************************************************/
void
xrdp_init_xkb_layout(struct xrdp_client_info *client_info)
{
    int fd;
    int index = 0;
    int bytes;
    struct list *names = (struct list *)NULL;
    struct list *items = (struct list *)NULL;
    struct list *values = (struct list *)NULL;
    char *item = (char *)NULL;
    char *value = (char *)NULL;
    char *q = (char *)NULL;
    char keyboard_cfg_file[256] = { 0 };
    char rdp_layout[256] = { 0 };

    const struct xrdp_keyboard_overrides *ko =
            &client_info->xrdp_keyboard_overrides;

    LOG(LOG_LEVEL_INFO, "xrdp_init_xkb_layout: Keyboard information sent"
        " by the RDP client, keyboard_type:[0x%02X], keyboard_subtype:[0x%02X],"
        " keylayout:[0x%08X]",
        client_info->keyboard_type, client_info->keyboard_subtype,
        client_info->keylayout);

    if (ko->type != -1)
    {
        LOG(LOG_LEVEL_INFO, "overrode keyboard_type 0x%02X"
            " with 0x%02X", client_info->keyboard_type, ko->type);
        client_info->keyboard_type = ko->type;
    }
    if (ko->subtype != -1)
    {
        LOG(LOG_LEVEL_INFO, "overrode keyboard_subtype 0x%02X"
            " with 0x%02X", client_info->keyboard_subtype,
            ko->subtype);
        client_info->keyboard_subtype = ko->subtype;
    }
    if (ko->layout != -1)
    {
        LOG(LOG_LEVEL_INFO, "overrode keylayout 0x%08X"
            " with 0x%08X", client_info->keylayout, ko->layout);
        client_info->keylayout = ko->layout;
    }
    /* infer model/variant */
    /* TODO specify different X11 keyboard models/variants */
    g_memset(client_info->model, 0, sizeof(client_info->model));
    g_memset(client_info->variant, 0, sizeof(client_info->variant));
    g_strncpy(client_info->layout, "us", sizeof(client_info->layout) - 1);
    if (client_info->keyboard_subtype == 3)
    {
        /* macintosh keyboard */
        bytes = sizeof(client_info->variant);
        g_strncpy(client_info->variant, "mac", bytes - 1);
    }
    else if (client_info->keyboard_subtype == 0)
    {
        /* default - standard subtype */
        client_info->keyboard_subtype = 1;
    }

    g_snprintf(keyboard_cfg_file, 255, "%s/xrdp_keyboard.ini", XRDP_CFG_PATH);
    LOG(LOG_LEVEL_DEBUG, "keyboard_cfg_file %s", keyboard_cfg_file);

    fd = g_file_open_ro(keyboard_cfg_file);

    if (fd >= 0)
    {
        int section_found = -1;
        char section_rdp_layouts[256] = { 0 };
        char section_layouts_map[256] = { 0 };

        names = list_create();
        names->auto_free = 1;
        items = list_create();
        items->auto_free = 1;
        values = list_create();
        values->auto_free = 1;

        file_read_sections(fd, names);
        for (index = 0; index < names->count; index++)
        {
            q = (char *)list_get_item(names, index);
            if (g_strncasecmp("default", q, 8) != 0)
            {
                int i;

                file_read_section(fd, q, items, values);

                for (i = 0; i < items->count; i++)
                {
                    item = (char *)list_get_item(items, i);
                    value = (char *)list_get_item(values, i);
                    LOG(LOG_LEVEL_DEBUG, "xrdp_init_xkb_layout: item %s value %s",
                        item, value);
                    if (g_strcasecmp(item, "keyboard_type") == 0)
                    {
                        int v = g_atoi(value);
                        if (v == client_info->keyboard_type)
                        {
                            section_found = index;
                        }
                    }
                    else if (g_strcasecmp(item, "keyboard_subtype") == 0)
                    {
                        int v = g_atoi(value);
                        if (v != client_info->keyboard_subtype &&
                                section_found == index)
                        {
                            section_found = -1;
                            break;
                        }
                    }
                    else if (g_strcasecmp(item, "rdp_layouts") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            g_strncpy(section_rdp_layouts, value, 255);
                        }
                    }
                    else if (g_strcasecmp(item, "layouts_map") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            g_strncpy(section_layouts_map, value, 255);
                        }
                    }
                    else if (g_strcasecmp(item, "model") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->model);
                            g_memset(client_info->model, 0, bytes);
                            g_strncpy(client_info->model, value, bytes - 1);
                        }
                    }
                    else if (g_strcasecmp(item, "variant") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->variant);
                            g_memset(client_info->variant, 0, bytes);
                            g_strncpy(client_info->variant, value, bytes - 1);
                        }
                    }
                    else if (g_strcasecmp(item, "options") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->options);
                            g_memset(client_info->options, 0, bytes);
                            g_strncpy(client_info->options, value, bytes - 1);
                        }
                    }
                    else
                    {
                        /*
                         * mixing items from different sections will result in
                         * skipping over current section.
                         */
                        LOG(LOG_LEVEL_DEBUG, "xrdp_init_xkb_layout: skipping "
                            "configuration item - %s, continuing to next "
                            "section", item);
                        break;
                    }
                }

                list_clear(items);
                list_clear(values);
            }
        }

        if (section_found == -1)
        {
            g_memset(section_rdp_layouts, 0, sizeof(char) * 256);
            g_memset(section_layouts_map, 0, sizeof(char) * 256);
            // read default section
            file_read_section(fd, "default", items, values);
            for (index = 0; index < items->count; index++)
            {
                item = (char *)list_get_item(items, index);
                value = (char *)list_get_item(values, index);
                if (g_strcasecmp(item, "rdp_layouts") == 0)
                {
                    g_strncpy(section_rdp_layouts, value, 255);
                }
                else if (g_strcasecmp(item, "layouts_map") == 0)
                {
                    g_strncpy(section_layouts_map, value, 255);
                }
            }
            list_clear(items);
            list_clear(values);
        }

        /* load the map */
        file_read_section(fd, section_rdp_layouts, items, values);
        for (index = 0; index < items->count; index++)
        {
            int rdp_layout_id;
            item = (char *)list_get_item(items, index);
            value = (char *)list_get_item(values, index);
            rdp_layout_id = g_htoi(value);
            if (rdp_layout_id == client_info->keylayout)
            {
                g_strncpy(rdp_layout, item, 255);
                break;
            }
        }
        list_clear(items);
        list_clear(values);
        file_read_section(fd, section_layouts_map, items, values);
        for (index = 0; index < items->count; index++)
        {
            item = (char *)list_get_item(items, index);
            value = (char *)list_get_item(values, index);
            if (g_strcasecmp(item, rdp_layout) == 0)
            {
                bytes = sizeof(client_info->layout);
                g_strncpy(client_info->layout, value, bytes - 1);
                break;
            }
        }

        list_delete(names);
        list_delete(items);
        list_delete(values);

        LOG(LOG_LEVEL_INFO, "xrdp_init_xkb_layout: model [%s] variant [%s] "
            "layout [%s] options [%s]", client_info->model,
            client_info->variant, client_info->layout, client_info->options);
        g_file_close(fd);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_init_xkb_layout: error opening %s",
            keyboard_cfg_file);
    }

    // Initialise the rules and a few keycodes for xorgxrdp
    snprintf(client_info->xkb_rules, sizeof(client_info->xkb_rules),
             "%s", scancode_get_xkb_rules());
    client_info->x11_keycode_caps_lock =
        scancode_to_x11_keycode(SCANCODE_CAPS_KEY);
    client_info->x11_keycode_num_lock =
        scancode_to_x11_keycode(SCANCODE_NUMLOCK_KEY);
    client_info->x11_keycode_scroll_lock =
        scancode_to_x11_keycode(SCANCODE_SCROLL_KEY);
}
