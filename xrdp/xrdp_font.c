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
 * fonts
 */

/* fv1 files are described in fontutils/README_fv1.txt */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <ctype.h>

#include "xrdp.h"
#include "log.h"
#include "string_calls.h"

#if 0 /* not used */
static char w_char[] =
{
    0x00, 0x00, 0x00, // ........................
    0x00, 0x00, 0x00, // ........................
    0x00, 0x00, 0x00, // ........................
    0x08, 0x20, 0x80, // ....X.....X.....X.......
    0x08, 0x50, 0x80, // ....X....X.X....X.......
    0x04, 0x51, 0x00, // .....X...X.X...X........
    0x04, 0x51, 0x00, // .....X...X.X...X........
    0x04, 0x51, 0x00, // .....X...X.X...X........
    0x02, 0x8a, 0x00, // ......X.X...X.X.........
    0x02, 0x8a, 0x00, // ......X.X...X.X.........
    0x02, 0x8a, 0x00, // ......X.X...X.X.........
    0x01, 0x04, 0x00, // .......X.....X..........
    0x01, 0x04, 0x00, // .......X.....X..........
    0x00, 0x00, 0x00, // ........................
    0x00, 0x00, 0x00, // ........................
    0x00, 0x00, 0x00, // ........................
};
#endif

// First character allocated in the 'struct xrdp_font.chars' array
#define FIRST_CHAR ' '

/*****************************************************************************/
/**
 * Parses the fv1_select configuration value to get the font to use,
 * based on the DPI of the primary monitor
 *
 * @param globals Configuration globals
 * @param dpi DPI of primary monitor. If not known, a suitable
 *            default should be passed in here.
 * @param[out] font_name Name of font to use
 * @param[in] font_name_len Length of font name buffer
 */
static void
get_font_name_from_dpi(const struct xrdp_cfg_globals *globals,
                       unsigned int dpi,
                       char *font_name, int font_name_len)
{
    int bad_selector = 0;

    font_name[0] = '\0';

    const char *fv1_select = globals->fv1_select;
    if (fv1_select == NULL || fv1_select[0] == '\0')
    {
        fv1_select = DEFAULT_FV1_SELECT;
    }

    const char *p = fv1_select;

    while (p != NULL)
    {
        /* DPI value must be next in string */
        if (!isdigit(*p))
        {
            bad_selector = 1;
            break;
        }
        unsigned int field_dpi = g_atoi(p);
        if (field_dpi <= dpi)
        {
            /* Use this font */
            p = g_strchr(p, ':');
            if (p == NULL)
            {
                bad_selector = 1;
            }
            else
            {
                ++p;
                const char *q = g_strchr(p, ',');
                if (q == NULL)
                {
                    q = p + g_strlen(p);
                }
                if (q - p > (font_name_len - 1))
                {
                    q = p + font_name_len - 1;
                }
                g_memcpy(font_name, p, q - p);
                font_name[q - p] = '\0';
            }
            break;
        }
        else
        {
            p = g_strchr(p, ',');
            if (p != NULL)
            {
                ++p;
            }
        }
    }

    if (bad_selector)
    {
        LOG(LOG_LEVEL_WARNING, "Unable to parse fv1_select configuration");
    }

    if (font_name[0] == '\0')
    {
        LOG(LOG_LEVEL_WARNING, "Loading default font " DEFAULT_FONT_NAME);
        g_snprintf(font_name, font_name_len, DEFAULT_FONT_NAME);
    }
}

/*****************************************************************************/
struct xrdp_font *
xrdp_font_create(struct xrdp_wm *wm, unsigned int dpi)
{
    struct xrdp_font *self;
    struct stream *s;
    int fd;
    int b;
    int i;
    unsigned int char_count;
    unsigned int datasize; // Size of glyph data on disk
    int file_size;
    struct xrdp_font_char *f;
    const char *file_path;
    char file_path_buff[256];
    int min_descender;
    char font_name[256];
    const struct xrdp_cfg_globals *globals = &wm->xrdp_config->cfg_globals;
    LOG_DEVEL(LOG_LEVEL_TRACE, "in xrdp_font_create");

    if (dpi == 0)
    {
        LOG(LOG_LEVEL_WARNING, "No DPI value is available to find login font");
        dpi = globals->default_dpi;
        LOG(LOG_LEVEL_WARNING, "Using the default_dpi of %u", dpi);
    }
    get_font_name_from_dpi(globals, dpi, font_name, sizeof(font_name));

    if (font_name[0] == '/')
    {
        /* User specified absolute path */
        file_path = font_name;
    }
    else
    {
        g_snprintf(file_path_buff, sizeof(file_path_buff),
                   XRDP_SHARE_PATH "/%s",
                   font_name);
        file_path = file_path_buff;
    }

    if (!g_file_exist(file_path))
    {
        /* Try to fall back to the default */
        const char *default_file_path = XRDP_SHARE_PATH "/" DEFAULT_FONT_NAME;
        if (g_file_exist(default_file_path))
        {
            LOG(LOG_LEVEL_WARNING,
                "xrdp_font_create: font file [%s] does not exist - using [%s]",
                file_path, default_file_path);
            file_path = default_file_path;
        }
        else
        {
            LOG(LOG_LEVEL_ERROR,
                "xrdp_font_create: Can't load either [%s] or [%s]",
                file_path, default_file_path);
            return 0;
        }
    }

    file_size = g_file_get_size(file_path);

    if (file_size < 1)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_font_create: error reading font from file [%s]",
            file_path);
        return 0;
    }

    self = (struct xrdp_font *)g_malloc(sizeof(struct xrdp_font), 1);
    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_font_create: "
            "Can't allocate memory for font");
        return self;
    }
    self->wm = wm;
    make_stream(s);
    init_stream(s, file_size + 1024);
    fd = g_file_open_ro(file_path);

    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_font_create: Can't open %s - %s", file_path,
            g_get_strerror());
        g_free(self);
        self = NULL;
    }
    else
    {
        b = g_file_read(fd, s->data, file_size + 1024);
        g_file_close(fd);

        // Got at least a header?
        if (b < (4 + 32 + 2 + 2 + 2 + 2 + 4))
        {
            LOG(LOG_LEVEL_ERROR,
                "xrdp_font_create: Font %s is truncated", file_path);
            g_free(self);
            self = NULL;
        }
        else
        {
            s->end = s->data + b;
            in_uint8s(s, 4);
            in_uint8a(s, self->name, 32);
            in_uint16_le(s, self->size);
            in_uint16_le(s, self->style);
            in_uint16_le(s, self->body_height);
            in_sint16_le(s, min_descender);
            in_uint8s(s, 4);
            char_count = FIRST_CHAR;

            while (!s_check_end(s))
            {
                if (!s_check_rem(s, 16))
                {
                    LOG(LOG_LEVEL_WARNING,
                        "xrdp_font_create: "
                        "Can't parse header for character U+%X", char_count);
                    break;
                }

                if (char_count >= MAX_FONT_CHARS)
                {
                    LOG(LOG_LEVEL_WARNING,
                        "xrdp_font_create: "
                        "Ignoring characters >= U+%x", MAX_FONT_CHARS);
                    break;
                }

                f = self->chars + char_count;
                in_sint16_le(s, i);
                f->width = i;
                in_sint16_le(s, i);
                f->height = i;
                in_sint16_le(s, i);
                /* Move the glyph up so there are no descenders */
                f->baseline = i + min_descender;
                in_sint16_le(s, i);
                f->offset = i;
                in_sint16_le(s, i);
                f->incby = i;
                in_uint8s(s, 6);
                datasize = FONT_DATASIZE(f);

                if (datasize < 0 || datasize > 512)
                {
                    /* shouldn't happen */
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "datasize for U+%x wrong "
                        "width %d, height %d, datasize %d",
                        char_count, f->width, f->height, datasize);
                    break;
                }

                if (!s_check_rem(s, datasize))
                {
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "Not enough data for character U+%X", char_count);
                    break;
                }

                if (datasize == 0)
                {
                    /* Allocate a single blank pixel for the glyph, so
                     * that it can be added to the glyph cache if required */
                    f->width = 1;
                    f->height = 1;

                    /* GOTCHA - we need to allocate more than one byte in
                     * memory for this glyph */
                    f->data = (char *)g_malloc(FONT_DATASIZE(f), 1);
                }
                else
                {
                    f->data = (char *)g_malloc(datasize, 0);
                }

                if (f->data == NULL)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "Allocation error for character U+%X", char_count);
                    break;
                }
                in_uint8a(s, f->data, datasize);

                ++char_count;
            }

            self->char_count = char_count;
            if (char_count <= FIRST_CHAR)
            {
                /* We read no characters from the font */
                xrdp_font_delete(self);
                self = NULL;
            }
            else
            {
                if (self->body_height == 0)
                {
                    /* Older font made for xrdp v0.9.x. Synthesise this
                     * value from the first glyph */
                    self->body_height = -self->chars[FIRST_CHAR].baseline + 1;
                }

                // Find a default glyph
                if (char_count > UCS_WHITE_SQUARE)
                {
                    self->default_char = &self->chars[UCS_WHITE_SQUARE];
                }
                else if (char_count > '?')
                {
                    self->default_char = &self->chars['?'];
                }
                else
                {
                    self->default_char = &self->chars[FIRST_CHAR];
                }
            }
        }
    }

    free_stream(s);
    /*
      self->font_items[0].offset = -4;
      self->font_items[0].baseline = -16;
      self->font_items[0].width = 24;
      self->font_items[0].height = 16;
      self->font_items[0].data = g_malloc(3 * 16, 0);
      g_memcpy(self->font_items[0].data, w_char, 3 * 16);
    */
    LOG_DEVEL(LOG_LEVEL_TRACE, "out xrdp_font_create");
    return self;
}

/*****************************************************************************/
/* free the font and all the items */
void
xrdp_font_delete(struct xrdp_font *self)
{
    unsigned int i;

    if (self == 0)
    {
        return;
    }

    for (i = FIRST_CHAR; i < self->char_count; i++)
    {
        g_free(self->chars[i].data);
    }

    g_free(self);
}

/*****************************************************************************/
/* compare the two font items returns 1 if they match */
int
xrdp_font_item_compare(struct xrdp_font_char *font1,
                       struct xrdp_font_char *font2)
{
    int datasize;

    if (font1 == 0)
    {
        return 0;
    }

    if (font2 == 0)
    {
        return 0;
    }

    if (font1->offset != font2->offset)
    {
        return 0;
    }

    if (font1->baseline != font2->baseline)
    {
        return 0;
    }

    if (font1->width != font2->width)
    {
        return 0;
    }

    if (font1->height != font2->height)
    {
        return 0;
    }

    datasize = FONT_DATASIZE(font1);

    if (g_memcmp(font1->data, font2->data, datasize) == 0)
    {
        return 1;
    }

    return 0;
}
