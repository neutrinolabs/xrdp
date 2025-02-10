/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <unistd.h>
#include <ctype.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* See the FT2 documentation - this builds an error table */
#undef FTERRORS_H_
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, NULL } };

static const struct
{
    int          err_code;
    const char  *err_msg;
} ft_errors[] =
#include <freetype/fterrors.h>
#ifdef __cppcheck__
    // avoid syntaxError by providing the array contents
    {};
#endif


#include "arch.h"
#include "defines.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"

#include "fv1.h"

#define DEFAULT_POINT_SIZE 10
#define DEFAULT_MAX_CHAR 0x4dff

/**
 * sans10 compatibility choices
 */
enum sans10_compat
{
    S10_OFF = 0,
    S10_ON,
    S10_AUTO
};

/**
 * Parsed program arguments
 */
struct program_args
{
    const char *input_file;
    const char *output_file;
    char font_name[FV1_MAX_FONT_NAME_SIZE + 1];
    unsigned short point_size;
    /** Last character value in file */
    unsigned int max_ucode;
    /** Are we generating san10 in compatibility mode? */
    enum sans10_compat sans10_compatibility;
};

struct x_dimensions
{
    unsigned short width;
    short offset;
    unsigned short inc_by;
};

/**
 * Table of some character settings in the original sans-10.fv1 font
 */
static const struct x_dimensions
    original_sans10_data[] =
{
    /* 0x20 - 0x3f */
    {1, 0, 4}, {1, 2, 5}, {3, 1, 5}, {9, 1, 11},
    {7, 1, 8}, {11, 0, 12}, {9, 1, 11}, {1, 1, 3},
    {3, 1, 5}, {3, 1, 5}, {7, 0, 7}, {9, 1, 11},
    {2, 1, 4}, {3, 1, 5}, {1, 2, 4}, {4, 0, 4},
    {6, 1, 8}, {5, 2, 8}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {6, 1, 8}, {1, 2, 4}, {2, 1, 4},
    {8, 1, 11}, {8, 1, 11}, {8, 1, 11}, {5, 1, 7},
    /* 0x40 - 0x5f */
    {11, 1, 13}, {9, 0, 9}, {7, 1, 9}, {7, 1, 9},
    {8, 1, 10}, {6, 1, 8}, {5, 1, 7}, {8, 1, 10},
    {8, 1, 10}, {1, 1, 3}, {3, -1, 3}, {7, 1, 8},
    {6, 1, 7}, {9, 1, 11}, {8, 1, 10}, {8, 1, 10},
    {6, 1, 8}, {8, 1, 10}, {7, 1, 8}, {7, 1, 9},
    {7, 0, 7}, {8, 1, 10}, {9, 0, 9}, {11, 0, 11},
    {8, 0, 8}, {7, 0, 7}, {8, 1, 10}, {3, 1, 5},
    {4, 0, 4}, {3, 1, 5}, {8, 1, 11}, {7, 0, 7},
    /* 0x60 - 0x7f */
    {3, 1, 7}, {6, 1, 8}, {6, 1, 8}, {5, 1, 7},
    {6, 1, 8}, {6, 1, 8}, {4, 0, 4}, {6, 1, 8},
    {6, 1, 8}, {1, 1, 3}, {2, 0, 3}, {6, 1, 7},
    {1, 1, 3}, {11, 1, 13}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {6, 1, 8}, {4, 1, 5}, {5, 1, 7},
    {4, 0, 5}, {6, 1, 8}, {7, 0, 7}, {9, 0, 9},
    {7, 0, 7}, {7, 0, 7}, {5, 1, 7}, {5, 2, 8},
    {1, 2, 4}, {5, 2, 8}, {8, 1, 11}, {7, 1, 8},
    /* 0x80 - 0x9f */
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    {7, 1, 8}, {7, 1, 8}, {7, 1, 8}, {7, 1, 8},
    /* 0xa0 - 0xbf */
    {1, 0, 4}, {1, 2, 5}, {6, 1, 8}, {7, 0, 8},
    {7, 0, 8}, {7, 1, 8}, {1, 2, 4}, {5, 1, 7},
    {3, 2, 7}, {9, 2, 13}, {5, 1, 6}, {6, 1, 8},
    {8, 1, 11}, {3, 1, 5}, {9, 2, 13}, {4, 1, 7},
    {4, 1, 7}, {9, 1, 11}, {4, 1, 5}, {4, 1, 5},
    {3, 2, 7}, {7, 1, 8}, {6, 1, 8}, {1, 1, 4},
    {3, 2, 7}, {3, 1, 5}, {5, 1, 6}, {6, 1, 8},
    {12, 1, 13}, {11, 1, 13}, {12, 1, 13}, {5, 1, 7},
    /* 0xc0 - 0xdf */
    {9, 0, 9}, {9, 0, 9}, {9, 0, 9}, {9, 0, 9},
    {9, 0, 9}, {9, 0, 9}, {12, 0, 13}, {7, 1, 9},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {2, 0, 3}, {2, 1, 3}, {5, -1, 3}, {3, 0, 3},
    {9, 0, 10}, {8, 1, 10}, {8, 1, 10}, {8, 1, 10},
    {8, 1, 10}, {8, 1, 10}, {8, 1, 10}, {7, 2, 11},
    {8, 1, 10}, {8, 1, 10}, {8, 1, 10}, {8, 1, 10},
    {8, 1, 10}, {7, 0, 7}, {6, 1, 8}, {6, 1, 8},
    /* 0xe0 - 0xff */
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {6, 1, 8}, {11, 1, 13}, {5, 1, 7},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {3, 0, 3}, {3, 1, 3}, {5, -1, 3}, {3, 0, 3},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {8, 1, 11},
    {6, 1, 8}, {6, 1, 8}, {6, 1, 8}, {6, 1, 8},
    {6, 1, 8}, {7, 0, 7}, {6, 1, 8}, {7, 0, 7}
};


/**************************************************************************//**
 * Parses a unicode character value
 *
 * @param str String containing value
 * @return Resulting character value
 */
static unsigned int
parse_ucode_name(const char *str)
{
    unsigned int rv;
    if (toupper(str[0]) == 'U' && str[1] == '+')
    {
        char *hex = g_strdup(str);
        hex[0] = '0';
        hex[1] = 'x';
        rv = g_atoix(hex);
        g_free(hex);
    }
    else if (str[0] == '@')
    {
        rv = str[1];
    }
    else
    {
        rv = g_atoix(str);
    }
    return rv;
}

/**************************************************************************//**
 * Parses the program args
 *
 * @param argc Passed to main
 * @param @argv Passed to main
 * @param pa program_pargs structure for resulting values
 * @return !=0 for success
 */
static int
parse_program_args(int argc, char *argv[], struct program_args *pa)
{
    int params_ok = 1;
    int opt;

    pa->input_file = NULL;
    pa->output_file = NULL;
    pa->font_name[0] = '\0';
    pa->point_size = DEFAULT_POINT_SIZE;
    pa->max_ucode = DEFAULT_MAX_CHAR;
    pa->sans10_compatibility = S10_AUTO;

    while ((opt = getopt(argc, argv, "n:p:m:C:")) != -1)
    {
        switch (opt)
        {
            case 'n':
                g_snprintf(pa->font_name, FV1_MAX_FONT_NAME_SIZE + 1, "%s",
                           optarg);
                break;

            case 'p':
                pa->point_size = g_atoi(optarg);
                break;

            case 'm':
                pa->max_ucode = parse_ucode_name(optarg);
                break;

            case 'C':
                if (toupper(optarg[0]) == 'A')
                {
                    pa->sans10_compatibility = S10_AUTO;
                }
                else if (g_text2bool(optarg))
                {
                    pa->sans10_compatibility = S10_ON;
                }
                else
                {
                    pa->sans10_compatibility = S10_OFF;
                }
                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Unrecognised switch '%c'", (char)opt);
                params_ok = 0;
        }
    }

    if (pa->max_ucode < FV1_MIN_CHAR)
    {
        LOG(LOG_LEVEL_ERROR, "-m argument must be at least %d",
            FV1_MIN_CHAR);
        params_ok = 0;
    }

    switch (argc - optind)
    {
        case 0:
            LOG(LOG_LEVEL_ERROR, "No input file specified");
            params_ok = 0;
            break;

        case 1:
            LOG(LOG_LEVEL_ERROR, "No output file specified");
            params_ok = 0;
            break;
        case 2:
            pa->input_file = argv[optind];
            pa->output_file = argv[optind + 1];
            break;

        default:
            LOG(LOG_LEVEL_ERROR, "Unexpected arguments after output file");
            params_ok = 0;
    }

    return params_ok;
}

/**************************************************************************//**
 * Checks whether the specified glyph row is blank
 * @param g Glyph
 * @param row Row number between 0 and g->height - 1
 * @result Boolean
 */
static int
is_blank_glyph_row(const struct fv1_glyph *g, unsigned int row)
{
    if (g->width == 0 || row >= g->height)
    {
        return 1;
    }

    const unsigned int glyph_row_size = ((g->width + 7) / 8);
    const unsigned char *dataptr = g->data + (row * glyph_row_size);
    const unsigned int masks[] =
    { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

    /* Check for set pixels in all the leading bytes */
    unsigned int x;
    for (x = g->width ; x > 8 ; x -= 8)
    {
        if (*dataptr++ != 0)
        {
            return 0;
        }
    }

    /* Use a single test to check the pixels in the last byte */
    return ((*dataptr & masks[x - 1]) == 0);
}

/**************************************************************************//**
 * Returns a string for a freetype2 error
 * @param error Freetype2 error code
 * @param buff Pointer to buffer for error string
 * @param bufflen Length of above
 */
static void
get_ft_error(FT_Error error, char *buff, unsigned int bufflen)
{
    const char *errstr = NULL;
    unsigned int i;
    for (i = 0 ; i < (sizeof(ft_errors) / sizeof(ft_errors[0])); ++i)
    {
        if (ft_errors[i].err_code == error)
        {
            errstr = ft_errors[i].err_msg;
            break;
        }
    }

    if (errstr != NULL)
    {
        g_snprintf(buff, bufflen, "[%s]", errstr);
    }
    else
    {
        g_snprintf(buff, bufflen,
                   "[errorcode %d (no description)]", (int)error);
    }
}

/**************************************************************************//**
 * Implement sans10 compatibility for a glyph
 *
 * The original Windows font generator made a few different choices for the
 * character x offset than freetype2 does. These are particularly noticeable
 * with a small font.
 *
 * This routine checks the glyph, and implements the original offset size
 * for popular English characters, which are all that the user will probably
 * be displaying with xrdp v0.9.x
 *
 * @param g Glyph to check
 */
static void
implement_sans10_compatibility(struct fv1_glyph *g, unsigned int ucode)
{
    const unsigned int max_index =
        sizeof(original_sans10_data) / sizeof(original_sans10_data[0]);

    if (ucode < FV1_MIN_CHAR || ucode >= max_index + FV1_MIN_CHAR)
    {
        return;
    }

    const struct x_dimensions *d =
            &original_sans10_data[ucode - FV1_MIN_CHAR];

    if (g->offset != d->offset)
    {
        if (g->width != d->width)
        {
            LOG(LOG_LEVEL_WARNING,
                "Can't set compatibility offset for U+%04X: width %d != %d",
                ucode, g->width, d->width);
        }
        else if (g->inc_by != d->inc_by)
        {
            LOG(LOG_LEVEL_WARNING,
                "Can't set compatibility offset for U+%04X: inc_by %d != %d",
                ucode, g->inc_by, d->inc_by);
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "Changing compatibility offset for U+%04X: from %d to %d",
                ucode, g->offset, d->offset);
        }

        g->offset = d->offset;
    }
}

/**************************************************************************//**
 * Converts a freetype 2 bitmap to a fv1 glyph
 * @param ft_glyph Freetype2 glyph. Must be a monochrome bitmap
 * @param ucode Unicode character for error reporting
 * @param pa Program args
 * @return fv1 glyph, or NULL for error
 */
static struct fv1_glyph *
convert_mono_glyph(FT_GlyphSlot ft_glyph, unsigned int ucode,
                   const struct program_args *pa)
{
    short width = ft_glyph->bitmap.width;
    short height =  ft_glyph->bitmap.rows;
    struct fv1_glyph *g;

    /* Number of bytes in a glyph row */
    const unsigned int glyph_row_size = ((width + 7) / 8);

    if ((g = fv1_alloc_glyph(ucode, width, height)) != NULL)
    {
        g->baseline = -(ft_glyph->metrics.horiBearingY / 64);
        g->offset = ft_glyph->metrics.horiBearingX / 64;
        g->inc_by = ft_glyph->metrics.horiAdvance / 64;

        if (FONT_DATASIZE(g) > 0)
        {
            const unsigned char *srcptr = ft_glyph->bitmap.buffer;
            unsigned char *dstptr = g->data;
            unsigned int y;

            for (y = 0; y < g->height; ++y)
            {
                g_memcpy(dstptr, srcptr, glyph_row_size);
                dstptr += glyph_row_size;
                srcptr += ft_glyph->bitmap.pitch;
            }

            /* Optimise the glyph by removing any blank lines at the bottom
             * and top */
            if (g->width > 0)
            {
                while (g->height > 0 &&
                        is_blank_glyph_row(g, g->height - 1))
                {
                    --g->height;
                }

                y = 0;
                while (y < g->height && is_blank_glyph_row(g, y))
                {
                    ++y;
                }

                if (y > 0)
                {
                    g->baseline += y;
                    g->height -= y;
                    g_memmove(g->data, g->data + (y * glyph_row_size),
                              g->height * glyph_row_size);
                }
            }
        }

        if (pa->sans10_compatibility != S10_OFF)
        {
            implement_sans10_compatibility(g, ucode);
        }
    }

    return g;
}

/**************************************************************************//**
 * Main
 *
 * @param argc Argument count
 * @param argv Arguments
 */
int
main(int argc, char *argv[])
{
    FT_Library library = NULL;   /* handle to library     */
    FT_Face face = NULL;      /* handle to face object */
    FT_Error error;
    struct fv1_glyph *g;
    struct program_args pa;
    struct log_config *logging;
    int rv = 1;

    logging = log_config_init_for_console(LOG_LEVEL_WARNING,
                                          g_getenv("MKFV1_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    struct fv1_file *fv1 = fv1_file_create();

    if (fv1 == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Memory allocation failure");
    }
    else if (parse_program_args(argc, argv, &pa))
    {
        char errstr[128];

        if ((error = FT_Init_FreeType(&library)) != 0)
        {
            get_ft_error(error, errstr, sizeof(errstr));
            LOG(LOG_LEVEL_ERROR, "Error initializing freetype library %s",
                errstr);
        }
        else if ((error = FT_New_Face(library, pa.input_file, 0, &face)) != 0)
        {
            get_ft_error(error, errstr, sizeof(errstr));
            LOG(LOG_LEVEL_ERROR, "Error loading font file %s %s",
                pa.input_file, errstr);
        }
        else if ((error = FT_Set_Char_Size(face, 0,
                                           pa.point_size * 64,
                                           FV1_DEVICE_DPI, 0)) != 0)
        {
            get_ft_error(error, errstr, sizeof(errstr));
            LOG(LOG_LEVEL_ERROR, "Error setting point size to %u %s",
                pa.point_size, errstr);
        }
        else
        {
            const char *fname =
                (pa.font_name[0] != '\0') ?  pa.font_name :
                (face->family_name != NULL) ? face->family_name :
                /* Default */ "";

            g_snprintf(fv1->font_name, FV1_MAX_FONT_NAME_SIZE + 1, "%s", fname);
            fv1->point_size = pa.point_size;
            fv1->body_height = face->size->metrics.height / 64;
            fv1->min_descender = face->size->metrics.descender / 64;

            if (pa.sans10_compatibility == S10_AUTO)
            {
                if (g_strcmp(fv1->font_name, "DejaVu Sans") == 0 &&
                        fv1->point_size == 10)
                {
                    pa.sans10_compatibility = S10_ON;
                }
                else
                {
                    pa.sans10_compatibility = S10_OFF;
                }
            }

            unsigned int u;
            for (u = FV1_MIN_CHAR; u <= pa.max_ucode; ++u)
            {
                /* retrieve glyph index from character code */
                FT_UInt glyph_index = FT_Get_Char_Index(face, u);

                /* load glyph image into the slot (erase previous one) */
                error = FT_Load_Glyph(face, glyph_index,
                                      FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
                if (error)
                {
                    get_ft_error(error, errstr, sizeof(errstr));
                    LOG(LOG_LEVEL_WARNING,
                        "Unable to get bitmap for U+%04X %s", u, errstr);
                    g = NULL;
                }
                else if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP ||
                         face->glyph->bitmap.pixel_mode != FT_PIXEL_MODE_MONO)
                {
                    LOG(LOG_LEVEL_WARNING,
                        "Internal error; U+%04X was not loaded as a bitmap",
                        u);
                    g = NULL;
                }
                else
                {
                    g = convert_mono_glyph(face->glyph, u, &pa);
                }
                list_add_item(fv1->glyphs, (tintptr)g);
            }

            rv = fv1_file_save(fv1,  pa.output_file);
        }
    }

    FT_Done_FreeType(library);
    fv1_file_free(fv1);
    log_end();

    return rv;
}
