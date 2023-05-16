/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2022
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
 * @file    fontutils/fv1.c
 * @brief   Definitions relating to fv1 font files
 */
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stddef.h>

#include "list.h"
#include "os_calls.h"
#include "parse.h"
#include "string_calls.h"
#include "fv1.h"

const static char FV1_SIGNATURE[] = {'F', 'N', 'T', '1'};

/*****************************************************************************/
struct fv1_glyph *
fv1_alloc_glyph(int ucode,
                unsigned short width,
                unsigned short height)
{
    int datasize = FONT_DATASIZE_FROM_GEOMETRY(width, height);
    struct fv1_glyph *glyph = NULL;
    char ucode_str[16] = {'\0'};
    if (ucode < 0)
    {
        g_snprintf(ucode_str, sizeof(ucode_str), "Glyph");
    }
    else
    {
        g_snprintf(ucode_str, sizeof(ucode_str), "Glyph:U+%04X", ucode);
    }

    if (datasize < 0 || datasize > FV1_MAX_GLYPH_DATASIZE)
    {

        /* shouldn't happen */
        LOG(LOG_LEVEL_ERROR, "%s - datasize %d out of bounds",
            ucode_str, datasize);
    }
    else
    {
        glyph = (struct fv1_glyph *)
                g_malloc( offsetof(struct fv1_glyph, data) + datasize, 1);

        if (glyph == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "%s - out of memory", ucode_str);
        }
        else
        {
            glyph->width = width;
            glyph->height = height;
        }
    }

    return glyph;
}

/*****************************************************************************/
struct fv1_file *
fv1_file_create(void)
{
    struct fv1_file *fv1 = (struct fv1_file *)g_new0(struct fv1_file, 1);
    if (fv1 == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "fv1_file_create - out of memory");
    }
    else
    {
        fv1->style = 1; /* Unused at present - compatibility value */
        fv1->glyphs = list_create();
        fv1->glyphs->auto_free = 1;
    }
    return fv1;
}

/*****************************************************************************/
static void
add_glyphs_from_stream(struct fv1_file *fv1, struct stream *s)
{
    unsigned short width;
    unsigned short height;
    int datasize;

    struct fv1_glyph *glyph;

    while (s_check_rem(s, 4))
    {
        in_sint16_le(s, width);
        in_sint16_le(s, height);

        datasize = FONT_DATASIZE_FROM_GEOMETRY(width, height);

        if (datasize < 0 || datasize > FV1_MAX_GLYPH_DATASIZE)
        {
            LOG(LOG_LEVEL_ERROR,
                "Font:%s Glyph:%d - datasize %d out of bounds",
                fv1->font_name, FV1_GLYPH_END(fv1), datasize);
            break;
        }

        if (!s_check_rem(s, 6 + 6 + datasize))
        {
            LOG(LOG_LEVEL_ERROR,
                "Font:%s Glyph:%d - not enough data for glyph",
                fv1->font_name, FV1_GLYPH_END(fv1));
            break;
        }

        if ((glyph = fv1_alloc_glyph(FV1_GLYPH_END(fv1), width, height)) == NULL)
        {
            break;
        }

        in_sint16_le(s, glyph->baseline);
        in_sint16_le(s, glyph->offset);
        in_sint16_le(s, glyph->inc_by);
        in_uint8s(s, 6);

        in_uint8a(s, glyph->data, datasize);

        list_add_item(fv1->glyphs, (tintptr)glyph);
    }
}

/*****************************************************************************/
struct fv1_file *
fv1_file_load(const char *filename)
{
    struct fv1_file *fv1 = NULL;

    if (!g_file_exist(filename))
    {
        LOG(LOG_LEVEL_ERROR, "Can't find font file %s", filename);
    }
    else
    {
        int file_size = g_file_get_size(filename);
        if (file_size < FV1_MIN_FILE_SIZE || file_size > FV1_MAX_FILE_SIZE)
        {
            LOG(LOG_LEVEL_ERROR, "Font file %s has bad size %d",
                filename, file_size);
        }
        else
        {
            struct stream *s;
            int fd;
            make_stream(s);
            init_stream(s, file_size + 1024);
            fd = g_file_open_ro(filename);

            if (fd < 0)
            {
                LOG(LOG_LEVEL_ERROR, "Can't open font file %s", filename);
            }
            else
            {
                int b = g_file_read(fd, s->data, file_size + 1024);
                g_file_close(fd);

                if (b < FV1_MIN_FILE_SIZE)
                {
                    LOG(LOG_LEVEL_ERROR, "Font file %s is too small",
                        filename);
                }
                else
                {
                    char sig[sizeof(FV1_SIGNATURE)];
                    s->end = s->data + b;
                    in_uint8a(s, sig, sizeof(FV1_SIGNATURE));
                    if (g_memcmp(sig, FV1_SIGNATURE, sizeof(sig)) != 0)
                    {
                        LOG(LOG_LEVEL_ERROR,
                            "Font file %s has wrong signature",
                            filename);
                    }
                    else if ((fv1 = fv1_file_create()) != NULL)
                    {
                        in_uint8a(s, fv1->font_name, FV1_MAX_FONT_NAME_SIZE);
                        fv1->font_name[FV1_MAX_FONT_NAME_SIZE] = '\0';
                        in_uint16_le(s, fv1->point_size);
                        in_uint16_le(s, fv1->style);
                        in_uint16_le(s, fv1->body_height);
                        in_uint16_le(s, fv1->min_descender);
                        in_uint8s(s, 4);

                        add_glyphs_from_stream(fv1, s);
                    }
                }
            }

            free_stream(s);
        }
    }

    return fv1;
}

/*****************************************************************************/
void
fv1_file_free(struct fv1_file *fv1)
{
    if (fv1 != NULL)
    {
        list_delete(fv1->glyphs);
        g_free(fv1);
    }
}

/*****************************************************************************/
int
write_stream(int fd, struct stream *s)
{
    const char *p = s->data;
    int rv = 1;

    while (p < s->end)
    {
        int len = g_file_write(fd, p, s->end - p);
        if (len <= 0)
        {
            rv = 0;
            break;
        }
        p += len;
    }

    return rv;
}

/*****************************************************************************/
int
fv1_file_save(const struct fv1_file *fv1, const char *filename)
{
    int fd;
    struct fv1_glyph *blank_glyph; /* Needed for bad characters */

    fd = g_file_open_ex(filename, 0, 1, 1, 1);
    int rv = 1;
    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Unable to open %s for writing [%s]", filename,
            g_get_strerror());
    }
    else
    {
        struct stream *s;
        make_stream(s);
        init_stream(s, 1024);

        /* Write the header */
        out_uint8a(s, FV1_SIGNATURE, sizeof(FV1_SIGNATURE));
        int len = g_strlen(fv1->font_name);
        if (len > FV1_MAX_FONT_NAME_SIZE)
        {
            len = FV1_MAX_FONT_NAME_SIZE;
        }
        out_uint8a(s, fv1->font_name,  len);
        while (len++ < FV1_MAX_FONT_NAME_SIZE)
        {
            out_uint8(s, '\0');
        }
        out_uint16_le(s, fv1->point_size);
        out_uint16_le(s, fv1->style);
        out_uint16_le(s, fv1->body_height);
        out_uint16_le(s, fv1->min_descender);
        out_uint8a(s, "\0\0\0\0", 4);
        s_mark_end(s);
        if (!write_stream(fd, s))
        {
            LOG(LOG_LEVEL_ERROR, "Unable to write file header [%s]",
                g_get_strerror());
        }
        else if ((blank_glyph = fv1_alloc_glyph(-1, 0, 0)) == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "Unable to allocate blank glyph");
        }
        else
        {
            int u;

            for (u = FV1_MIN_CHAR; u < FV1_GLYPH_END(fv1); ++u)
            {
                const struct fv1_glyph *g = FV1_GET_GLYPH(fv1, u);
                int datasize;
                if (g == NULL)
                {
                    LOG(LOG_LEVEL_WARNING, "Glyph %d is not set", u);
                    g = blank_glyph;
                }
                else
                {
                    datasize = FONT_DATASIZE(g);
                    if (datasize > FV1_MAX_GLYPH_DATASIZE)
                    {
                        LOG(LOG_LEVEL_WARNING,
                            "glyph %d datasize %d exceeds max of %d"
                            " - glyph will be blank",
                            u, datasize, FV1_MAX_GLYPH_DATASIZE);
                        g = blank_glyph;
                    }
                }
                init_stream(s, 16 + FONT_DATASIZE(g));
                out_uint16_le(s, g->width);
                out_uint16_le(s, g->height);
                out_uint16_le(s, g->baseline);
                out_uint16_le(s, g->offset);
                out_uint16_le(s, g->inc_by);
                out_uint8a(s, "\0\0\0\0\0\0", 6);
                out_uint8a(s, g->data, FONT_DATASIZE(g));
                s_mark_end(s);

                if (!write_stream(fd, s))
                {
                    LOG(LOG_LEVEL_ERROR, "Unable to write glyph %d [%s]",
                        u, g_get_strerror());
                    break;
                }
            }
            g_free(blank_glyph);

            rv = (u == FV1_GLYPH_END(fv1)) ? 0 : 1;
        }
        free_stream(s);
        g_file_close(fd);
    }

    return rv;
}
