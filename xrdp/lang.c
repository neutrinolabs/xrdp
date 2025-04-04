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

/*
 * Struct representing the contents of a km file [General] section
 */
struct km_general
{
    unsigned int version;
    int caps_lock_supported;
};

const struct km_general km_general_default =
{
    .version = 0,
    .caps_lock_supported = 1
};

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
    // Don't take caps_lock into account if the keymap doesn't support it.
    caps_lock = caps_lock && keymap->caps_lock_supported;

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
/**
 * Parses the [General] section in a keymap file
 * @param tfile TOML file in memory
 * @param general result (initialised to defaults)
 */
static void
parse_km_general(toml_table_t *tfile, struct km_general *general)
{
    toml_table_t *section;

    if (tfile != NULL && (section = toml_table_in(tfile, "General")) != NULL)
    {
        toml_datum_t d;
        if ((d = toml_int_in(section, "version")).ok)
        {
            general->version = d.u.i;
        }
        if ((d = toml_bool_in(section, "caps_lock_supported")).ok)
        {
            general->caps_lock_supported = d.u.b;
        }
    }
}

/*****************************************************************************/
/**
 * Loads the [General] section only from a TOML file
 * @param filename Name of TOML file
 * @param quiet Set true to not log errors
 * @param[out] km_general Contents of [General] section. Defaults are provided.
 * @return 0 if the operation was successful
 */
static int
km_load_file_general(const char *filename, int quiet,
                     struct km_general *general)
{
    FILE *fp;
    toml_table_t *tfile;
    char errbuf[200];
    int rv = 1;

    *general = km_general_default;

    if ((fp = fopen(filename, "r")) == NULL)
    {
        if (!quiet)
        {
            LOG(LOG_LEVEL_ERROR, "Error loading keymap file %s (%s)",
                filename, g_get_strerror());
        }
    }
    else
    {
        tfile = toml_parse_file(fp, errbuf, sizeof(errbuf));
        fclose(fp);
        if (tfile == NULL)
        {
            if (!quiet)
            {
                LOG(LOG_LEVEL_ERROR, "Error in keymap file %s - %s",
                    filename, errbuf);
            }
        }
        else
        {
            parse_km_general(tfile, general);
            rv = 0;
            toml_free(tfile);
        }
    }

    return rv;
}

/*****************************************************************************/
/**
 * Boolean to test if a keylayout supports the caps_lock modifier key
 * @param keylayout keyboardLayout from TS_UD_CS_CORE (see [MS-RDPBCGR])
 * @return True if layout supports caps lock
 */
static int
keylayout_supports_caps_lock(int keylayout)
{
    char filename[256];
    struct km_general general;

    g_snprintf(filename, sizeof(filename),
               XRDP_CFG_PATH "/km-%08x.toml", keylayout);

    (void)km_load_file_general(filename, 1, &general);

    return general.caps_lock_supported;
}

/*****************************************************************************/
int
km_load_file(const char *filename, struct xrdp_keymap *keymap)
{
    FILE *fp;
    toml_table_t *tfile;
    char errbuf[200];
    int rv = 0;
    struct km_general general = km_general_default;

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

        /* Check to see if we should expect caps lock entries */
        parse_km_general(tfile, &general);
        keymap->caps_lock_supported = general.caps_lock_supported;

        /* read the keymap sections */
        km_read_section(tfile, "noshift", keymap->keys_noshift);
        km_read_section(tfile, "shift", keymap->keys_shift);
        km_read_section(tfile, "altgr", keymap->keys_altgr);
        km_read_section(tfile, "shiftaltgr", keymap->keys_shiftaltgr);
        if (keymap->caps_lock_supported)
        {
            km_read_section(tfile, "capslock", keymap->keys_capslock);
            km_read_section(tfile, "capslockaltgr", keymap->keys_capslockaltgr);
            km_read_section(tfile, "shiftcapslock", keymap->keys_shiftcapslock);
            km_read_section(tfile, "shiftcapslockaltgr",
                            keymap->keys_shiftcapslockaltgr);
        }

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
/***
 * reverse-looks up a keylayout in a TOML table
 *
 * @param keylayout (e.g. 0x000000409)
 * @param table TOML table
 * @param rdp_layout Buffer for result
 * @param rdp_layout_len Size of rdp_layout
 * @return != 0 for a successful lookup
 */
static int
lookup_keylayout(int keylayout,
                 toml_table_t *table,
                 char rdp_layout[], unsigned int rdp_layout_len)
{
    int i;
    const char *key;
    toml_datum_t datum;
    toml_array_t *arr;

    for (i = 0 ; (key = toml_key_in(table, i)) != NULL ; ++i)
    {
        if ((datum = toml_int_in(table, key)).ok)
        {
            /* It's an integer */
            if ((int)datum.u.i == keylayout)
            {
                strlcpy(rdp_layout, key, rdp_layout_len);
                return 1;
            }
        }
        else if ((arr = toml_array_in(table, key)) != NULL)
        {
            /* It's an array */
            int nelem = toml_array_nelem(arr);
            int j;
            for (j = 0 ; j < nelem; ++j)
            {
                if ((datum = toml_int_at(arr, j)).ok)
                {
                    /* It's an integer */
                    if ((int)datum.u.i == keylayout)
                    {
                        strlcpy(rdp_layout, key, rdp_layout_len);
                        return 1;
                    }
                }
                else
                {
                    LOG(LOG_LEVEL_WARNING,
                        "%s:%d in table [%s] is not a valid value",
                        key, j + 1, toml_table_key(table));
                }
            }
        }
        else
        {
            LOG(LOG_LEVEL_WARNING,
                "Key '%s' in table [%s] is neither an integer or an array",
                key, toml_table_key(table));
        }
    }
    return 0;
}

/*****************************************************************************/
/**
 * Looks up a string in a TOML table based on a key
 * @param arr TOML table
 * @param key Key to look up
 * @param out Buffer for result
 * @param out_size Length of buffer for result
 * @return != 0 if a string was read
 */
static int
get_toml_string(const toml_table_t *arr, const char *key,
                char out[], unsigned int out_size)
{
    toml_datum_t datum = toml_string_in(arr, key);
    if (datum.ok)
    {
        strlcpy(out, datum.u.s, out_size);
        free(datum.u.s);
    }
    return datum.ok;
}

/*****************************************************************************/
/**
 * Looks up an integer in a TOML table based on a key
 * @param arr TOML table
 * @param key Key to look up
 * @param default_val Default to return if no match
 *
 * @return value for key, or default_val
 */
static int
get_toml_int(const toml_table_t *arr, const char *key, int default_val)
{
    toml_datum_t datum = toml_int_in(arr, key);
    return (datum.ok) ? (int)datum.u.i : default_val;
}

/*****************************************************************************/
/**
 * Reads settable parameters from a TOML table
 *
 * @param table Table
 * @param layouts      Buffer to get 'layouts' parameter
 * @param layouts_size Size of the above
 * @param map          Buffer to get 'map' parameter
 * @param map_size     Size of the above
 * @param model        Buffer to get 'model' parameter
 * @param model_size   Size of the above
 * @param variant      Buffer to get 'variant' parameter
 * @param variant_size Size of the above
 * @param options      Buffer to get 'options' parameter
 * @param options_size Size of the above
 */
static void
read_param_set(toml_table_t *table,
               char layouts[], unsigned int layouts_size,
               char map[], unsigned int map_size,
               char model[], unsigned int model_size,
               char variant[], unsigned int variant_size,
               char options[], unsigned int options_size)
{
    (void)get_toml_string(table, "layouts", layouts, layouts_size);
    (void)get_toml_string(table, "map", map, map_size);
    (void)get_toml_string(table, "model", model, model_size);
    (void)get_toml_string(table, "variant", variant, variant_size);
    (void)get_toml_string(table, "options", options, options_size);
}

/*****************************************************************************/
/**
 * Scans the [overrides.kb_type] sub-tables
 *
 * Looks for matches on keyboard type/subtype, and sets the keyboard
 * parameters appropriately.
 *
 * @param kb_type                TOML source table
 * @param client_info            Client info pointer
 * @param layouts_table_str      Buffer to receive name of layouts table
 * @param layouts_table_str_len  Length of above buffer
 * @param map_table_str          Buffer to receive name of map table
 * @param map_table_str_len      Length of above buffer
 */
static void
scan_overrides_kbtype_tables(toml_table_t *kb_type,
                             struct xrdp_client_info *client_info,
                             char layouts_table_str[],
                             unsigned int layouts_table_str_len,
                             char map_table_str[],
                             unsigned int map_table_str_len)
{
    int i;
    const char *key;

    for (i = 0 ; (key = toml_key_in(kb_type, i)) != NULL ; ++i)
    {
        toml_table_t *subtable = toml_table_in(kb_type, key);
        if (subtable == NULL)
        {
            continue; // Guard - shouldn't happen
        }
        int kb_type = get_toml_int(subtable, "type", 0);
        int kb_subtype = get_toml_int(subtable, "subtype", 0);
        if (kb_type == 0 && kb_subtype == 0)
        {
            LOG(LOG_LEVEL_WARNING, "Keyboard config - table "
                "[overrides.kbtype.%s] is missing both type and subtype",
                key);
            continue;
        }
        if (kb_type != 0 && kb_type != client_info->keyboard_type)
        {
            continue; // Table specified a type which doesn't match
        }
        if (kb_subtype != 0 && kb_subtype != client_info->keyboard_subtype)
        {
            continue; // Table specified a subtype which doesn't match
        }

        LOG(LOG_LEVEL_DEBUG, "Keyboard config - "
            "Matched overrides table [overrides.kb_type.%s]", key);

        // Override any values we can find in the subtable
        read_param_set(subtable,
                       layouts_table_str, layouts_table_str_len,
                       map_table_str, map_table_str_len,
                       client_info->xup.model, sizeof(client_info->xup.model),
                       client_info->xup.variant, sizeof(client_info->xup.variant),
                       client_info->xup.options, sizeof(client_info->xup.options));
    }
}

/*****************************************************************************/
static void
read_toml_config(struct xrdp_client_info *client_info,
                 const char filename[],
                 toml_table_t *tfile)
{
    char layouts_table_str[64];
    char map_table_str[64];
    char layout_name[64];

    toml_table_t *defaults;
    toml_table_t *overrides;
    toml_table_t *layouts_table;
    toml_table_t *map_table;


    /* Step 1 - Parse the [defaults] table looking for the
     * layouts table name, the map table name, and (optionally) the
     * XKB model, variant and options
     */
    if ((defaults = toml_table_in(tfile, "defaults")) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Keyboard config - can't find [defaults] table");
        return;
    }

    layouts_table_str[0] = '\0';
    map_table_str[0] = '\0';
    read_param_set(defaults,
                   layouts_table_str, sizeof(layouts_table_str),
                   map_table_str, sizeof(map_table_str),
                   client_info->xup.model, sizeof(client_info->xup.model),
                   client_info->xup.variant, sizeof(client_info->xup.variant),
                   client_info->xup.options, sizeof(client_info->xup.options));

    if (layouts_table_str[0] == '\0' || map_table_str[0] == '\0')
    {
        LOG(LOG_LEVEL_ERROR, "Keyboard config - "
            "[defaults] table is missing layouts and/or map keys");
        return;
    }

    // Step 2 - Now scan the [overrides] table, looking for
    // subtables
    if ((overrides = toml_table_in(tfile, "overrides")) != NULL)
    {
        toml_table_t *kb_type = toml_table_in(overrides, "kb_type");
        if (kb_type != NULL)
        {
            // Look for overrides on type and/or subtype
            scan_overrides_kbtype_tables(kb_type,
                                         client_info,
                                         layouts_table_str,
                                         sizeof(layouts_table_str),
                                         map_table_str, sizeof(map_table_str));
        }
    }

    /* Check we have valid layouts and map tables */
    if ((layouts_table = toml_table_in(tfile, layouts_table_str)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't find layouts table ['%s'] in %s",
            layouts_table_str, filename);
        return;
    }

    if ((map_table = toml_table_in(tfile, map_table_str)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Can't find map table ['%s'] in %s",
            map_table_str, filename);
        return;
    }

    /* Step 2 - Look up the keylayout passed from the client to get a
     * layout_name */
    layout_name[0] = '\0';
    if (!lookup_keylayout(client_info->keylayout, layouts_table,
                          layout_name, sizeof(layout_name)))
    {
        if ((client_info->keylayout & ~0xffff) != 0)
        {
            // We failed to match the layout, but we may be able
            // to match on the lower 16-bits
            int alt_layout = client_info->keylayout & 0xffff;
            if (lookup_keylayout(alt_layout, layouts_table,
                                 layout_name, sizeof(layout_name)))
            {
                LOG(LOG_LEVEL_INFO,
                    "Failed to match layout 0x%08X, but matched 0x%04X to %s",
                    client_info->keylayout, alt_layout, layout_name);
            }
        }
    }
    if (layout_name[0] == '\0')
    {
        LOG(LOG_LEVEL_ERROR, "Can't find layout %08x in layout table %s",
            client_info->keylayout, layouts_table_str);
        return;
    }

    // Step 3 - use the layout name we matched from the layouts table to
    // lookup the corresponding X11 layout in the map table, e.g. if we
    // matched 0xe0200411 to 'rdp_layout_jp, copy 'jp' for the client.
    if (!get_toml_string(map_table, layout_name,
                         client_info->xup.layout, sizeof(client_info->xup.layout)))
    {
        LOG(LOG_LEVEL_ERROR, "Can't find layout name %s in map table %s",
            layout_name, map_table_str);
    }
}

/*****************************************************************************/
void
xrdp_init_xkb_layout(struct xrdp_client_info *client_info)
{
    FILE *fp;
    const char *keyboard_cfg_file = XRDP_CFG_PATH "/xrdp_keyboard.toml";

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
    client_info->xup.model[0] = '\0';
    client_info->xup.variant[0] = '\0';
    strlcpy(client_info->xup.layout, "us", sizeof(client_info->xup.layout));
    if (client_info->keyboard_subtype == 0)
    {
        /* default - standard subtype */
        client_info->keyboard_subtype = 1;
    }

    LOG(LOG_LEVEL_DEBUG, "keyboard_cfg_file %s", keyboard_cfg_file);

    if ((fp = fopen(keyboard_cfg_file, "r")) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Error loading keyboard config file %s (%s)",
            keyboard_cfg_file, g_get_strerror());
    }
    else
    {
        char errbuf[200];
        toml_table_t *tfile = toml_parse_file(fp, errbuf, sizeof(errbuf));
        fclose(fp); // No longer need this open
        fp = NULL;

        if (tfile == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Error in keyboard config file %s - %s",
                keyboard_cfg_file, errbuf);
        }
        else
        {
            read_toml_config(client_info, keyboard_cfg_file, tfile);
            LOG(LOG_LEVEL_INFO, "xrdp_init_xkb_layout: model [%s] variant [%s] "
                "layout [%s] options [%s]",
                client_info->xup.model, client_info->xup.variant,
                client_info->xup.layout, client_info->xup.options);
            toml_free(tfile);
        }
    }

    // Initialise the rules and a few keycodes for xorgxrdp
    strlcpy(client_info->xup.xkb_rules, scancode_get_xkb_rules(),
            sizeof(client_info->xup.xkb_rules));
    if (keylayout_supports_caps_lock(client_info->keylayout))
    {
        client_info->xup.x11_keycode_caps_lock =
            scancode_to_x11_keycode(SCANCODE_CAPS_KEY);
    }
    else
    {
        LOG(LOG_LEVEL_INFO, "xrdp_init_xkb_layout: caps lock is not supported");
        client_info->xup.x11_keycode_caps_lock = 0;
    }
    client_info->xup.x11_keycode_num_lock =
        scancode_to_x11_keycode(SCANCODE_NUMLOCK_KEY);
    client_info->xup.x11_keycode_scroll_lock =
        scancode_to_x11_keycode(SCANCODE_SCROLL_KEY);
}
