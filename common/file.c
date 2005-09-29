/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2005

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
int APP_CC
file_read_line(struct stream* s, char* text)
{
  int i;
  char c;
  char* hold;

  if (!s_check(s))
  {
    return 1;
  }
  hold = s->p;
  i = 0;
  in_uint8(s, c);
  while (c != 10 && c != 13 && s_check(s))
  {
    text[i] = c;
    i++;
    in_uint8(s, c);
  }
  if (c == 10 || c == 13)
  {
    while ((c == 10 || c == 13) && s_check(s))
    {
      in_uint8(s, c);
    }
    s->p--;
  }
  text[i] = 0;
  if (text[0] == '[')
  {
    s->p = hold;
    return 1;
  }
  if (g_strlen(text) > 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/*****************************************************************************/
int APP_CC
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
file_read_section(int fd, char* section, struct list* names,
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
        if (g_strncasecmp(section, text, 255) == 0)
        {
          file_read_line(s, text);
          while (file_read_line(s, text) == 0)
          {
            file_split_name_value(text, name, value);
            list_add_item(names, (long)g_strdup(name));
            list_add_item(values, (long)g_strdup(value));
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
