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
   Copyright (C) Jay Sorg 2010

   freerdp wrapper

*/

#include "xrdp-freerdp.h"

char* APP_CC
convert_bitmap(int in_bpp, int out_bpp, char* bmpdata,
               int width, int height, int* palette)
{
  char* out;
  char* src;
  char* dst;
  int i;
  int j;
  int red;
  int green;
  int blue;
  int pixel;

  if ((in_bpp == 8) && (out_bpp == 8))
  {
    out = (char*)g_malloc(width * height, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui8*)src);
        pixel = palette[pixel];
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR8(red, green, blue);
        *dst = pixel;
        src++;
        dst++;
      }
    }
    return out;
  }
  if ((in_bpp == 8) && (out_bpp == 16))
  {
    out = (char*)g_malloc(width * height * 2, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui8*)src);
        pixel = palette[pixel];
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR16(red, green, blue);
        *((tui16*)dst) = pixel;
        src++;
        dst += 2;
      }
    }
    return out;
  }
  if ((in_bpp == 8) && (out_bpp == 24))
  {
    out = (char*)g_malloc(width * height * 4, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui8*)src);
        pixel = palette[pixel];
        SPLITCOLOR32(red, green, blue, pixel);
        pixel = COLOR24RGB(red, green, blue);
        *((tui32*)dst) = pixel;
        src++;
        dst += 4;
      }
    }
    return out;
  }
  if ((in_bpp == 15) && (out_bpp == 16))
  {
    out = (char*)g_malloc(width * height * 2, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui16*)src);
        SPLITCOLOR15(red, green, blue, pixel);
        pixel = COLOR16(red, green, blue);
        *((tui16*)dst) = pixel;
        src += 2;
        dst += 2;
      }
    }
    return out;
  }
  if ((in_bpp == 15) && (out_bpp == 24))
  {
    out = (char*)g_malloc(width * height * 4, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui16*)src);
        SPLITCOLOR15(red, green, blue, pixel);
        pixel = COLOR24RGB(red, green, blue);
        *((tui32*)dst) = pixel;
        src += 2;
        dst += 4;
      }
    }
    return out;
  }
  if ((in_bpp == 16) && (out_bpp == 16))
  {
    return bmpdata;
  }
  if ((in_bpp == 16) && (out_bpp == 24))
  {
    out = (char*)g_malloc(width * height * 4, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        pixel = *((tui16*)src);
        SPLITCOLOR16(red, green, blue, pixel);
        pixel = COLOR24RGB(red, green, blue);
        *((tui32*)dst) = pixel;
        src += 2;
        dst += 4;
      }
    }
    return out;
  }
  if ((in_bpp == 24) && (out_bpp == 24))
  {
    out = (char*)g_malloc(width * height * 4, 0);
    src = bmpdata;
    dst = out;
    for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
      {
        blue = *((tui8*)src);
        src++;
        green = *((tui8*)src);
        src++;
        red = *((tui8*)src);
        src++;
        pixel = COLOR24RGB(red, green, blue);
        *((tui32*)dst) = pixel;
        dst += 4;
      }
    }
    return out;
  }
  if ((in_bpp == 32) && (out_bpp == 24))
  {
    return bmpdata;
  }
  if ((in_bpp == 32) && (out_bpp == 32))
  {
    return bmpdata;
  }    
  if ((in_bpp == 15) && (out_bpp == 15))
  {
    return bmpdata;
  }
  g_writeln("convert_bitmap: error unknown conversion from %d to %d",
            in_bpp, out_bpp);
  return 0;
}

/*****************************************************************************/
/* returns color or 0 */
int APP_CC
convert_color(int in_bpp, int out_bpp, int in_color, int* palette)
{
  int pixel;
  int red;
  int green;
  int blue;

  if ((in_bpp == 1) && (out_bpp == 24))
  {
    pixel = in_color == 0 ? 0 : 0xffffff;
    return pixel;
  }
  if ((in_bpp == 8) && (out_bpp == 8))
  {
    pixel = palette[in_color];
    SPLITCOLOR32(red, green, blue, pixel);
    pixel = COLOR8(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 8) && (out_bpp == 16))
  {
    pixel = palette[in_color];
    SPLITCOLOR32(red, green, blue, pixel);
    pixel = COLOR16(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 8) && (out_bpp == 24))
  {
    pixel = palette[in_color];
    SPLITCOLOR32(red, green, blue, pixel);
    pixel = COLOR24BGR(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 15) && (out_bpp == 16))
  {
    pixel = in_color;
    SPLITCOLOR15(red, green, blue, pixel);
    pixel = COLOR16(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 15) && (out_bpp == 24))
  {
    pixel = in_color;
    SPLITCOLOR15(red, green, blue, pixel);
    pixel = COLOR24BGR(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 16) && (out_bpp == 16))
  {
    return in_color;
  }
  if ((in_bpp == 16) && (out_bpp == 24))
  {
    pixel = in_color;
    SPLITCOLOR16(red, green, blue, pixel);
    pixel = COLOR24BGR(red, green, blue);
    return pixel;
  }
  if ((in_bpp == 24) && (out_bpp == 24))
  {
    return in_color;
  }
  if ((in_bpp == 32) && (out_bpp == 24))
  {
    return in_color;
  }
  if ((in_bpp == 32) && (out_bpp == 32))
  {
    return in_color;
  }  
  if ((in_bpp == 15) && (out_bpp == 15))
  {
    return in_color;
  }
  g_writeln("convert_color: error unknown conversion from %d to %d",
            in_bpp, out_bpp);
  return 0;
}
