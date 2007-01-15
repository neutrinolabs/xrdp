/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   read a config file
*/

#include "arch.h"
#include "os_calls.h"
#include "list.h"
#include "file.h"
#include "parse.h"

/*****************************************************************************/
/* returns error
   returns 0 if everything is ok
   returns 1 if problem reading file */
int APP_CC
file_read_sections(int fd, struct list* names)
{
  struct stream* s;
  char text[256];
  char c;
  int in_it;
  int in_it_index;
  int len;
  int index;
  int rv;

  rv = 0;
  g_file_seek(fd, 0);
  in_it_index = 0;
  in_it = 0;
  g_memset(text, 0, 256);
  list_clear(names);
  make_stream(s);
  init_stream(s, 8192);
  len = g_file_read(fd, s->data, 8192);
  if (len > 0)
  {
    s->end = s->p + len;
    for (index = 0; index < len; index++)
    {
      in_uint8(s, c);
      if (c == '[')
      {
        in_it = 1;
      }
      else if (c == ']')
      {
        list_add_item(names, (long)g_strdup(text));
        in_it = 0;
        in_it_index = 0;
        g_memset(text, 0, 256);
      }
      else if (in_it)
      {
        text[in_it_index] = c;
        in_it_index++;
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
static int APP_CC
file_read_line(struct stream* s, char* text)
{
  int i;
  int skip_to_end;
  int at_end;
  char c;
  char* hold;

  skip_to_end = 0;
  if (!s_check_rem(s, 1))
  {
    return 1;
  }
  hold = s->p;
  i = 0;
  in_uint8(s, c);
  while (c != 10 && c != 13)
  {
    if (c == '#' || c == '!')
    {
      skip_to_end = 1;
    }
    if (!skip_to_end)
    {
      text[i] = c;
      i++;
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
  if (text[0] == '[')
  {
    s->p = hold;
    return 1;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
file_split_name_value(char* text, char* name, char* value)
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
    if (text[i] == '=')
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
  return 0;
}

/*****************************************************************************/
/* return error */
int APP_CC
file_read_section(int fd, const char* section, struct list* names,
                  struct list* values)
{
  struct stream* s;
  char text[512];
  char name[512];
  char value[512];
  char c;
  int in_it;
  int in_it_index;
  int len;
  int index;

  g_file_seek(fd, 0);
  in_it_index = 0;
  in_it = 0;
  g_memset(text, 0, 512);
  list_clear(names);
  list_clear(values);
  make_stream(s);
  init_stream(s, 8192);
  len = g_file_read(fd, s->data, 8192);
  if (len > 0)
  {
    s->end = s->p + len;
    for (index = 0; index < len; index++)
    {
      in_uint8(s, c);
      if (c == '[')
      {
        in_it = 1;
      }
      else if (c == ']')
      {
        if (g_strcasecmp(section, text) == 0)
        {
          file_read_line(s, text);
          while (file_read_line(s, text) == 0)
          {
            if (g_strlen(text) > 0)
            {
              file_split_name_value(text, name, value);
              list_add_item(names, (long)g_strdup(name));
              list_add_item(values, (long)g_strdup(value));
            }
          }
          free_stream(s);
          return 0;
        }
        in_it = 0;
        in_it_index = 0;
        g_memset(text, 0, 512);
      }
      else if (in_it)
      {
        text[in_it_index] = c;
        in_it_index++;
      }
    }
  }
  free_stream(s);
  return 1;
}
