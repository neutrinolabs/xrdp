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
 * read a config file
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "list.h"
#include "file.h"
#include "parse.h"

#define FILE_MAX_LINE_BYTES 2048

static int
file_read_ini_line(struct stream *s, char *text, int text_bytes);

/*****************************************************************************/
/* look up for a section name within str (i.e. pattern [section_name])
 * if a section name is found, this function return 1 and copy the section
 * inplace of str. */
static int
line_lookup_for_section_name(char *str, int str_bytes)
{
    int name_index_start;
    int index;
    char c;

    name_index_start = -1;
    index = 0;
    while ((c = str[index]) != 0)
    {
        if (c == '[')
        {
            name_index_start = index + 1;
        }
        else if (c == ']' && name_index_start > 0)
        {
            if (name_index_start + index >= str_bytes)
            {
                return 0;
            }
            for (index = index - name_index_start; index > 0; index--)
            {
                str[0] = str[name_index_start];
                str++;
            }
            str[0] = 0;
            return 1;
        }
        ++index;
    }
    return 0;
}


/*****************************************************************************/
/* returns error
   returns 0 if everything is ok
   returns 1 if problem reading file */
static int
l_file_read_sections(int fd, int max_file_size, struct list *names)
{
    struct stream *s;
    char text[FILE_MAX_LINE_BYTES];
    int len;
    int rv;

    rv = 0;
    g_file_seek(fd, 0);
    g_memset(text, 0, FILE_MAX_LINE_BYTES);
    list_clear(names);
    make_stream(s);
    init_stream(s, max_file_size);
    len = g_file_read(fd, s->data, max_file_size);

    if (len > 0)
    {
        s->end = s->p + len;
        while (file_read_ini_line(s, text, FILE_MAX_LINE_BYTES) == 0)
        {
            if (line_lookup_for_section_name(text, FILE_MAX_LINE_BYTES) != 0)
            {
                list_add_strdup(names, text);
            }
        }
    }
    else if (len < 0)
    {
        rv = 1;
    }

    free_stream(s);
    return rv;
}

/*****************************************************************************/
/* Read a line in the stream 's', removing comments.
 * returns error
 * returns 0 if everything is ok
 * returns 1 if problem reading file */
static int
file_read_ini_line(struct stream *s, char *text, int text_bytes)
{
    int i;
    int skip_to_end;
    int at_end;
    char c;

    skip_to_end = 0;

    if (!s_check_rem(s, 1))
    {
        return 1;
    }

    i = 0;
    in_uint8(s, c);

    while (c != 10 && c != 13)
    {
        /* these mean skip the rest of the line */
        if (c == '#' || c == ';')
        {
            skip_to_end = 1;
        }

        if (!skip_to_end)
        {
            text[i] = c;
            i++;
            if (i >= text_bytes)
            {
                return 1;
            }
        }

        if (s_check_rem(s, 1))
        {
            in_uint8(s, c);
        }
        else
        {
            c = 0;
            break;
        }
    }

    if (c == 10 || c == 13)
    {
        at_end = 0;

        while (c == 10 || c == 13)
        {
            if (s_check_rem(s, 1))
            {
                in_uint8(s, c);
            }
            else
            {
                at_end = 1;
                break;
            }
        }

        if (!at_end)
        {
            s->p--;
        }
    }

    text[i] = 0;

    return 0;
}


/*****************************************************************************/
/* returns error */
static int
file_split_name_value(char *text, char *name, char *value)
{
    int len;
    int i;
    int value_index;
    int name_index;
    int on_to;

    value_index = 0;
    name_index = 0;
    on_to = 0;
    name[0] = 0;
    value[0] = 0;
    len = g_strlen(text);

    for (i = 0; i < len; i++)
    {
        if (text[i] == '=' && !on_to)
        {
            on_to = 1;
        }
        else if (on_to)
        {
            value[value_index] = text[i];
            value_index++;
            value[value_index] = 0;
        }
        else
        {
            name[name_index] = text[i];
            name_index++;
            name[name_index] = 0;
        }
    }

    g_strtrim(name, 3); /* trim both right and left */
    g_strtrim(value, 3); /* trim both right and left */
    return 0;
}

/*****************************************************************************/
/* return error */
static int
l_file_read_section(int fd, int max_file_size, const char *section,
                    struct list *names, struct list *values)
{
    struct stream *s;
    char *data;
    char *text;
    char *name;
    char *value;
    char *lvalue;
    int len;
    int file_size;

    data = (char *) g_malloc(FILE_MAX_LINE_BYTES * 3, 0);
    text = data;
    name = text + FILE_MAX_LINE_BYTES;
    value = name + FILE_MAX_LINE_BYTES;

    file_size = 32 * 1024; /* 32 K file size limit */
    g_file_seek(fd, 0);
    g_memset(text, 0, FILE_MAX_LINE_BYTES);
    list_clear(names);
    list_clear(values);
    make_stream(s);
    init_stream(s, file_size);
    len = g_file_read(fd, s->data, file_size);

    if (len > 0)
    {
        s->end = s->p + len;
        while (file_read_ini_line(s, text, FILE_MAX_LINE_BYTES) == 0)
        {
            if (line_lookup_for_section_name(text, FILE_MAX_LINE_BYTES) != 0)
            {
                if (g_strcasecmp(section, text) == 0)
                {
                    while (file_read_ini_line(s, text,
                                              FILE_MAX_LINE_BYTES) == 0)
                    {
                        if (line_lookup_for_section_name(text, FILE_MAX_LINE_BYTES) != 0)
                        {
                            break;
                        }

                        if (g_strlen(text) > 0)
                        {
                            file_split_name_value(text, name, value);
                            list_add_strdup(names, name);

                            if (value[0] == '$')
                            {
                                lvalue = g_getenv(value + 1);

                                if (lvalue != 0)
                                {
                                    list_add_strdup(values, lvalue);
                                }
                                else
                                {
                                    list_add_strdup(values, "");
                                }
                            }
                            else
                            {
                                list_add_strdup(values, value);
                            }
                        }
                    }
                    free_stream(s);
                    g_free(data);
                    return 0;
                }
            }
        }
    }

    free_stream(s);
    g_free(data);
    return 1;
}

/*****************************************************************************/
/* returns error
   returns 0 if everything is ok
   returns 1 if problem reading file */
/* 32 K file size limit */
int
file_read_sections(int fd, struct list *names)
{
    return l_file_read_sections(fd, 32 * 1024, names);
}

/*****************************************************************************/
/* return error */
/* this function should be preferred over file_read_sections because it can
   read any file size */
int
file_by_name_read_sections(const char *file_name, struct list *names)
{
    int fd;
    int file_size;
    int rv;

    file_size = g_file_get_size(file_name);

    if (file_size < 1)
    {
        return 1;
    }

    fd = g_file_open_ro(file_name);

    if (fd < 0)
    {
        return 1;
    }

    rv = l_file_read_sections(fd, file_size, names);
    g_file_close(fd);
    return rv;
}

/*****************************************************************************/
/* return error */
/* 32 K file size limit */
int
file_read_section(int fd, const char *section,
                  struct list *names, struct list *values)
{
    return l_file_read_section(fd, 32 * 1024, section, names, values);
}

/*****************************************************************************/
/* return error */
/* this function should be preferred over file_read_section because it can
   read any file size */
int
file_by_name_read_section(const char *file_name, const char *section,
                          struct list *names, struct list *values)
{
    int fd;
    int file_size;
    int rv;

    file_size = g_file_get_size(file_name);

    if (file_size < 1)
    {
        return 1;
    }

    fd = g_file_open_ro(file_name);

    if (fd < 0)
    {
        return 1;
    }

    rv = l_file_read_section(fd, file_size, section, names, values);
    g_file_close(fd);
    return rv;
}
