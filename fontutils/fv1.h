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
 * @file    fontutils/fv1.h
 * @brief   Definitions relating to fv1 font files
 */
#if !defined(FV1_H)
#define FV1_H

struct list;

#define FV1_DEVICE_DPI 96

#define FV1_MIN_CHAR 0x20  /* First character value in file */

#define FV1_MIN_FILE_SIZE 48
#define FV1_MAX_FILE_SIZE (10 * 1024 * 1024)
#define FV1_MAX_FONT_NAME_SIZE 32

#define FV1_MAX_GLYPH_DATASIZE 512

struct fv1_glyph
{
    unsigned short width;   /* Width of glyph */
    unsigned short height;  /* Height of glyph */
    short baseline; /* Offset from cursor pos to 1st row of glyph */
    short offset; /* Space before glyph (can be -ve) */
    unsigned short inc_by; /* Total width of glyph + spacing */
    /* Standard C++ does not yet support C99 flexible array members */
#ifdef __cplusplus
    unsigned char data[1];
#else
    unsigned char data[];
#endif
};

struct fv1_file
{
    char font_name[FV1_MAX_FONT_NAME_SIZE + 1];
    short point_size; /* Input point size (for reference) */
    short style;
    short body_height; /* Body height (pixels) */
    short min_descender; /* Min descender of the glyphs in the font */
    struct list *glyphs; /* Glyphs are struct fv1_glyph * */

};

/**
 * Get a glyph pointer for a unicode character
 */
#define FV1_GET_GLYPH(fv1,ucode) \
    (((ucode) < FV1_MIN_CHAR) \
     ? NULL \
     : (struct fv1_glyph *)list_get_item(fv1->glyphs, (ucode) - FV1_MIN_CHAR))

/**
 * First glyph not in file
 */
#define FV1_GLYPH_END(fv1) (fv1->glyphs->count + FV1_MIN_CHAR)

struct fv1_file *
fv1_file_load(const char *filename);

void
fv1_file_free(struct fv1_file *);

struct fv1_file *
fv1_file_create(void);

struct fv1_glyph *
fv1_alloc_glyph(int ucode, /* Unicode character for error reporting if known */
                unsigned short width,
                unsigned short height);

enum fv1_realloc_mode
{
    FV1_AT_TOP,
    FV1_AT_BOTTOM
};

int
fv1_file_save(const struct fv1_file *fv1, const char *filename);

#endif
