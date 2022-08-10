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
 *
 * fonts
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <limits.h>
#include <unistd.h>
#include <ctype.h>

#include "list.h"
#include "os_calls.h"
#include "parse.h"
#include "string_calls.h"
#include "fv1.h"

/**
 * What the program is doing
 */
enum program_mode
{
    PM_UNSPECIFIED = 0,
    PM_INFO,
    PM_GLYPH_INFO_TABLE,
    PM_SHOW_CHAR
};

/**
 * Parsed program arguments
 */
struct program_args
{
    const char *font_file;
    enum program_mode mode;
    int ucode; /* Unicode character to display in 'c' mode */
};

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

    pa->font_file = NULL;
    pa->mode = PM_UNSPECIFIED;
    pa->ucode = 0;

    while ((opt = getopt(argc, argv, "c:it")) != -1)
    {
        switch (opt)
        {
            case 'i':
                if (pa->mode == PM_UNSPECIFIED)
                {
                    pa->mode = PM_INFO;
                }
                else
                {
                    LOG(LOG_LEVEL_ERROR, "Can't have two modes set");
                    params_ok = 0;
                }
                break;

            case 't':
                if (pa->mode == PM_UNSPECIFIED)
                {
                    pa->mode = PM_GLYPH_INFO_TABLE;
                }
                else
                {
                    LOG(LOG_LEVEL_ERROR, "Can't have two modes set");
                    params_ok = 0;
                }
                break;

            case 'c':
                if (pa->mode == PM_UNSPECIFIED)
                {
                    pa->mode = PM_SHOW_CHAR;
                    if (toupper(optarg[0]) == 'U' && optarg[1] == '+')
                    {
                        char *hex = g_strdup(optarg);
                        hex[0] = '0';
                        hex[1] = 'x';
                        pa->ucode = g_atoix(hex);
                        g_free(hex);
                    }
                    else if (optarg[0] == '@')
                    {
                        pa->ucode = optarg[1];
                    }
                    else
                    {
                        pa->ucode = g_atoix(optarg);
                    }
                }
                else
                {
                    LOG(LOG_LEVEL_ERROR, "Can't have two modes set");
                    params_ok = 0;
                }
                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Unrecognised switch '%c'", (char)opt);
                params_ok = 0;
        }
    }

    if (argc <= optind)
    {
        LOG(LOG_LEVEL_ERROR, "No font file specified");
        params_ok = 0;
    }
    else if ((argc - optind) > 1)
    {
        LOG(LOG_LEVEL_ERROR, "Unexpected arguments after font file");
        params_ok = 0;
    }
    else
    {
        pa->font_file = argv[optind];
    }

    return params_ok;
}

/**************************************************************************//**
 * Displays information about a font file
 *
 * @param fv1 loaded font file
 */
static void
display_font_file_info(const struct fv1_file *fv1)
{
    g_printf("Font name : %s\n", fv1->font_name);
    g_printf("Point size (%d DPI) : %d\n", FV1_DEVICE_DPI, fv1->point_size);
    g_printf("Style : %d\n", fv1->style);
    if (fv1->body_height == 0 && fv1->glyphs->count > 0)
    {
        const struct fv1_glyph *g =
            (const struct fv1_glyph *)fv1->glyphs->items[0];
        g_printf("Body height : %d (from 1st glyph)\n", -g->baseline + 1);
    }
    else
    {
        g_printf("Body height : %d\n", fv1->body_height);
    }
    g_printf("Descender : %d\n", fv1->min_descender);

    if (fv1->glyphs->count == 0)
    {
        g_printf("\nFile contains no glyphs\n");
    }
    else
    {
        g_printf("Min glyph index : U+%04X\n", FV1_MIN_CHAR);
        g_printf("Max glyph index : U+%04X\n", FV1_GLYPH_END(fv1) - 1);

        /* Work out the statistics */
        unsigned short max_width = 0;
        int max_width_ucode = 0;
        unsigned short max_height = 0;
        int max_height_ucode = 0;
        short min_baseline = 0;
        int min_baseline_ucode = 0;
        short min_offset = 0;
        int min_offset_ucode = 0;
        short max_offset = 0;
        int max_offset_ucode = 0;
        unsigned short max_inc_by = 0;
        int max_inc_by_ucode = 0;

        /* Derived quantities */
        short min_y_descender = SHRT_MAX;
        int min_y_descender_ucode = 0;
        int max_datasize = -1;
        int max_datasize_ucode = 0;

        /* Loop and output macros */
#define SET_MIN(ucode,field,val) \
    if ((field) < (val)) \
    { \
        val = (field); \
        val##_ucode = (ucode); \
    }

#define SET_MAX(ucode,field,val) \
    if ((field) > (val)) \
    { \
        val = (field); \
        val##_ucode = (ucode); \
    }

#define OUTPUT_INFO(string, val) \
    if (val##_ucode > 0) \
    { \
        g_printf(string, val, val##_ucode); \
    }
        int u;
        for (u = FV1_MIN_CHAR ; u < FV1_GLYPH_END(fv1); ++u)
        {
            const struct fv1_glyph *g = FV1_GET_GLYPH(fv1, u);
            if (g != NULL)
            {
                short y_descender = - (g->baseline + g->height);
                int datasize = FONT_DATASIZE(g);

                SET_MAX(u, g->width, max_width);
                SET_MAX(u, g->height, max_height);
                SET_MIN(u, g->baseline, min_baseline);
                SET_MIN(u, g->offset, min_offset);
                SET_MAX(u, g->offset, max_offset);
                SET_MAX(u, g->inc_by, max_inc_by);
                SET_MIN(u, y_descender, min_y_descender);
                SET_MAX(u, datasize, max_datasize);
            }
        }

        OUTPUT_INFO("Max glyph width : %d (U+%04X)\n", max_width);
        OUTPUT_INFO("Max glyph height : %d (U+%04X)\n", max_height);
        OUTPUT_INFO("Min glyph y-baseline : %d (U+%04X)\n", min_baseline);
        OUTPUT_INFO("Min glyph y-descender : %d (U+%04X)\n", min_y_descender);
        OUTPUT_INFO("Min glyph x-offset : %d (U+%04X)\n", min_offset);
        OUTPUT_INFO("Max glyph x-offset : %d (U+%04X)\n", max_offset);
        OUTPUT_INFO("Max glyph x-inc_by : %d (U+%04X)\n", max_inc_by);
        OUTPUT_INFO("Max glyph datasize : %d (U+%04X)\n", max_datasize);

#undef SET_MIN
#undef SET_MAX
#undef OUTPUT_INFO
    }
}

/**************************************************************************//**
 * Display info in a table about all the glyphs
 * @param fv1 font file
 */
static void
display_glyph_info_table(const struct fv1_file *fv1)
{
    int u;
    g_printf("character,width,height,baseline,offset,inc_by,datasize\n");

    for (u = FV1_MIN_CHAR; u < FV1_GLYPH_END(fv1); ++u)
    {
        const struct fv1_glyph *g = FV1_GET_GLYPH(fv1, u);
        if (g != NULL)
        {
            int datasize = FONT_DATASIZE(g);
            g_printf("%d,%hu,%hu,%hd,%hd,%hu,%d\n",
                     u, g->width, g->height, g->baseline,
                     g->offset, g->inc_by, datasize);
        }
    }
}

/**************************************************************************//**
 * Converts a font data row to a printable string
 *
 * @param rowdata Pointer to the first byte of the row data
 * @param width Number of pixels in the row
 * @param out Output buffer. Must be sized by the caller to be at
 *            least width+1 bytes
 */
static void
row_to_str(const unsigned char *rowdata, int width, char *out)
{
    int x;
    int mask = 1 << 7;
    for (x = 0 ; x < width ; ++x)
    {
        out[x] = ((*rowdata & mask) != 0) ? 'X' : '.';
        mask >>= 1;
        if (mask == 0)
        {
            mask = 1 << 7;
            ++rowdata;
        }
    }
    out[width] = '\0';
}

/**************************************************************************//**
 * Display info about a specific glyph
 * @param ucode Unicode character value
 * @param g Glyph
 */
static void
display_glyph_info(int ucode, const struct fv1_glyph *g)
{

    char *row_buffer = (char *)g_malloc(g->width + 1, 0);
    if (row_buffer == NULL)
    {
        g_printf("<No memory>\n");
    }
    else
    {
        g_printf("Glyph : U+%04X\n", ucode);
        g_printf("  Width : %d\n", g->width);
        g_printf("  Height : %d\n", g->height);
        g_printf("  Baseline : %d\n", g->baseline);
        g_printf("  Offset : %d\n", g->offset);
        g_printf("  Inc By : %d\n", g->inc_by);

        g_printf("  Data :\n");
        int y;
        const unsigned char *dataptr = g->data;

        for (y = 0 ; y < g->height; ++y)
        {
            row_to_str(dataptr, g->width, row_buffer);
            g_printf("    %+3d: %s\n", y + g->baseline, row_buffer);
            dataptr += (g->width + 7) / 8;
        }
        g_free(row_buffer);
    }
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
    struct fv1_file *fv1 = NULL;
    struct log_config *logging;
    struct program_args pa;
    int rv = 1;

    logging = log_config_init_for_console(LOG_LEVEL_WARNING,
                                          g_getenv("DUMPFV1_LOG_LEVEL"));
    log_start_from_param(logging);
    log_config_free(logging);

    if (parse_program_args(argc, argv, &pa) &&
            (fv1 = fv1_file_load(pa.font_file)) != NULL)
    {
        switch (pa.mode)
        {
            case PM_INFO:
                display_font_file_info(fv1);
                rv = 0;
                break;

            case PM_GLYPH_INFO_TABLE:
                display_glyph_info_table(fv1);
                rv = 0;
                break;

            case PM_SHOW_CHAR:
                if (pa.ucode < FV1_MIN_CHAR)
                {
                    LOG(LOG_LEVEL_ERROR, "Value for -c must be at least U+%04X",
                        FV1_MIN_CHAR);
                }
                else if (pa.ucode >= FV1_GLYPH_END(fv1))
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Value for -c must be less than U+%04X",
                        FV1_GLYPH_END(fv1));
                }
                else
                {
                    const struct fv1_glyph *g =
                        (const struct fv1_glyph *)
                        list_get_item(fv1->glyphs, pa.ucode - FV1_MIN_CHAR);
                    display_glyph_info(pa.ucode, g);
                    rv = 0;
                }
                break;

            default:
                LOG(LOG_LEVEL_ERROR, "Specify one of '-i' or '-c'");
                break;
        }
    }

    fv1_file_free(fv1);
    log_end();

    return rv;
}
