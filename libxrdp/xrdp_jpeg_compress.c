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
   Copyright (C) Jay Sorg 2012

   jpeg compressor

*/

#include "libxrdp.h"

#if defined(XRDP_JPEG)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

#define JP_QUALITY 75

struct mydata_comp
{
  char* cb;
  int cb_bytes;
  int total_done;
  int overwrite;
};

/*****************************************************************************/
/* called at begining */
static void DEFAULT_CC
my_init_destination(j_compress_ptr cinfo)
{
  struct mydata_comp* md;

  md = (struct mydata_comp*)(cinfo->client_data);
  md->total_done = 0;
  md->overwrite = 0;
  cinfo->dest->next_output_byte = md->cb;
  cinfo->dest->free_in_buffer = md->cb_bytes;
}

/*****************************************************************************/
/* called when buffer is full and we need more space */
static int DEFAULT_CC
my_empty_output_buffer(j_compress_ptr cinfo)
{
  struct mydata_comp* md;
  int chunk_bytes;

  md = (struct mydata_comp*)(cinfo->client_data);
  chunk_bytes = md->cb_bytes;
  md->total_done += chunk_bytes;
  cinfo->dest->next_output_byte = md->cb;
  cinfo->dest->free_in_buffer = md->cb_bytes;
  md->overwrite = 1;
  return 1;
}

/*****************************************************************************/
/* called at end */
static void DEFAULT_CC
my_term_destination(j_compress_ptr cinfo)
{
  struct mydata_comp* md;
  int chunk_bytes;

  md = (struct mydata_comp*)(cinfo->client_data);
  chunk_bytes = md->cb_bytes - cinfo->dest->free_in_buffer;
  md->total_done += chunk_bytes;
}

/*****************************************************************************/
static int APP_CC
jp_do_compress(char* data, int width, int height, int bpp, int quality,
               char* comp_data, int* comp_data_bytes)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  struct jpeg_destination_mgr dst_mgr;
  struct mydata_comp md;
  JSAMPROW row_pointer[4];
  int Bpp;

  Bpp = (bpp + 7) / 8;
  memset(&cinfo, 0, sizeof(cinfo));
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  memset(&md, 0, sizeof(md));
  md.cb = comp_data,
  md.cb_bytes = *comp_data_bytes;
  cinfo.client_data = &md;
  memset(&dst_mgr, 0, sizeof(dst_mgr));
  dst_mgr.init_destination = my_init_destination;
  dst_mgr.empty_output_buffer = my_empty_output_buffer;
  dst_mgr.term_destination = my_term_destination;
  cinfo.dest = &dst_mgr;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = Bpp;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  cinfo.num_components = 3;
  cinfo.dct_method = JDCT_FLOAT;
  jpeg_set_quality(&cinfo, quality, 1);
  jpeg_start_compress(&cinfo, 1);
  while (cinfo.next_scanline + 3 < cinfo.image_height)
  {
    row_pointer[0] = data;
    data += width * Bpp;
    row_pointer[1] = data;
    data += width * Bpp;
    row_pointer[2] = data;
    data += width * Bpp;
    row_pointer[3] = data;
    data += width * Bpp;
    jpeg_write_scanlines(&cinfo, row_pointer, 4);
  }
  while (cinfo.next_scanline < cinfo.image_height)
  {
    row_pointer[0] = data;
    data += width * Bpp;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo);
  *comp_data_bytes = md.total_done;
  jpeg_destroy_compress(&cinfo);
  if (md.overwrite)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
jpeg_compress(char* in_data, int width, int height,
              struct stream* s, struct stream* temp_s, int bpp,
              int byte_limit, int e, int quality)
{
  char* data;
  tui32* src32;
  tui16* src16;
  tui8* dst8;
  tui32 pixel;
  int red;
  int blue;
  int green;
  int j;
  int i;
  int cdata_bytes;

  data = temp_s->data;
  dst8 = data;
  if (bpp == 24)
  {
    src32 = (tui32*)in_data;
    for (j = 0; j < height; j++)
    {
      for (i = 0; i < width; i++)
      {
        pixel = src32[i + j * width];
        SPLITCOLOR32(red, green, blue, pixel);
        *(dst8++)= blue;
        *(dst8++)= green;
        *(dst8++)= red;
      }
      for (i = 0; i < e; i++)
      {
        *(dst8++) = blue;
        *(dst8++) = green;
        *(dst8++) = red;
      }
    }
  }
  else
  {
    g_writeln("bpp wrong %d", bpp);
  }
  cdata_bytes = byte_limit;
  jp_do_compress(data, width + e, height, 24, quality, s->p, &cdata_bytes);
  s->p += cdata_bytes;
  return cdata_bytes;
}

/*****************************************************************************/
int APP_CC
xrdp_jpeg_compress(char* in_data, int width, int height,
                   struct stream* s, int bpp, int byte_limit,
                   int start_line, struct stream* temp_s,
                   int e, int quality)
{
  jpeg_compress(in_data, width, height, s, temp_s, bpp, byte_limit,
                e, quality);
  return height;
}

#else

/*****************************************************************************/
int APP_CC
xrdp_jpeg_compress(char* in_data, int width, int height,
                   struct stream* s, int bpp, int byte_limit,
                   int start_line, struct stream* temp_s,
                   int e, int quality)
{
  return height;
}

#endif
