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
 * jpeg compressor
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"

#if defined(XRDP_TJPEG)

/* turbo jpeg */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#include "log.h"

/*****************************************************************************/
int
xrdp_jpeg_compress(void *handle, char *in_data, int width, int height,
                   struct stream *s, int bpp, int byte_limit,
                   int start_line, struct stream *temp_s,
                   int e, int quality)
{
    int error;
    int i;
    int j;
    unsigned int pixel;
    unsigned int *src32;
    unsigned int *dst32;
    unsigned long cdata_bytes;
    unsigned char *src_buf;
    unsigned char *dst_buf;
    char *temp_buf;
    tjhandle tj_han;

    if (bpp != 24)
    {
        g_writeln("xrdp_jpeg_compress: bpp wrong %d", bpp);
        return height;
    }
    if (handle == 0)
    {
        g_writeln("xrdp_jpeg_compress: handle is nil");
        return height;
    }
    tj_han = (tjhandle) handle;
    cdata_bytes = byte_limit;
    src_buf = (unsigned char *) in_data;
    dst_buf = (unsigned char *) (s->p);
    temp_buf = 0;
    if (e == 0)
    {
        src_buf = (unsigned char*)in_data;
    }
    else
    {
        temp_buf = (char *) g_malloc((width + e) * height * 4, 0);
        dst32 = (unsigned int *) temp_buf;
        src32 = (unsigned int *) in_data;
        for (j = 0; j < height; j++)
        {
            for (i = 0; i < width; i++)
            {
                pixel = *src32;
                src32++;
                *dst32 = pixel;
                dst32++;
            }
            if (width > 0)
            {
                for (i = 0; i < e; i++)
                {
                    *dst32 = pixel;
                    dst32++;
                }
            }
        }
        src_buf = (unsigned char *) temp_buf;
    }
    dst_buf = (unsigned char*)(s->p);
    error = tjCompress(tj_han, src_buf, width + e, (width + e) * 4, height,
                       TJPF_XBGR, dst_buf, &cdata_bytes,
                       TJSAMP_420, quality, 0);
    if (error != 0)
    {
        log_message(LOG_LEVEL_ERROR,
                    "xrdp_jpeg_compress: tjCompress error: %s",
                    tjGetErrorStr());
    }

    s->p += cdata_bytes;
    g_free(temp_buf);
    return height;
}

/**
 * Compress a rectangular area (aka inner rectangle) inside our
 * frame buffer (inp_data)
 *****************************************************************************/

int
xrdp_codec_jpeg_compress(void *handle,
                         int   format,   /* input data format */
                         char *inp_data, /* input data */
                         int   width,    /* width of inp_data */
                         int   height,   /* height of inp_data */
                         int   stride,   /* inp_data stride, in bytes*/
                         int   x,        /* x loc in inp_data */
                         int   y,        /* y loc in inp_data */
                         int   cx,       /* width of area to compress */
                         int   cy,       /* height of area to compress */
                         int   quality,  /* higher numbers compress less */
                         char *out_data, /* dest for jpg image */
                         int  *io_len    /* length of out_data and on return */
                                         /* len of compressed data */
                         )
{
    tjhandle       tj_han;
    int            error;
    int            bpp;
    char          *src_ptr;
    unsigned long  lio_len;

    /*
     * note: for now we assume that format is always XBGR and ignore format
     */

    if (handle == 0)
    {
        g_writeln("xrdp_codec_jpeg_compress: handle is nil");
        return height;
    }

    tj_han = (tjhandle) handle;

    /* get bytes per pixel */
    bpp = stride / width;

    /* start of inner rect in inp_data */
    src_ptr = inp_data + (y * stride + x * bpp);

    lio_len = *io_len;
    /* compress inner rect */

    /* notes
     * TJPF_RGB no works, zero bytes
     * TJPF_BGR no works, not zero but no open
     * TJPF_RGBX no works, zero bytes
     * TJPF_BGRX no works, off scaled image
     * TJPF_XBGR works
     * TJPF_XRGB no works, zero bytes
     * TJPF_RGBA no works, zero bytes
     * TJPF_BGRA no works, zero bytes
     * TJPF_ABGR no works, zero bytes
     * TJPF_ARGB no works, zero bytes */

    error = tjCompress(tj_han,     /* opaque handle */
                       (unsigned char *) src_ptr,    /* source buf */
                       cx,         /* width of area to compress */
                       stride,     /* pitch */
                       cy,         /* height of area to compress */
                       TJPF_XBGR,  /* pixel size */
                       (unsigned char *) out_data,   /* dest buf */
                       &lio_len,   /* inner_buf length & compressed_size */
                       TJSAMP_420, /* jpeg sub sample */
                       quality,    /* jpeg quality */
                       0           /* flags */
                       );
    if (error != 0)
    {
        log_message(LOG_LEVEL_ERROR,
                    "xrdp_codec_jpeg_compress: tjCompress error: %s",
                    tjGetErrorStr());
    }

    *io_len = lio_len;
    return height;
}

/*****************************************************************************/
void *
xrdp_jpeg_init(void)
{
    tjhandle tj_han;

    tj_han = tjInitCompress();
    return tj_han;
}

/*****************************************************************************/
int
xrdp_jpeg_deinit(void *handle)
{
    tjhandle tj_han;

    if (handle == 0)
    {
        return 0;
    }
    tj_han = (tjhandle) handle;
    tjDestroy(tj_han);
    return 0;
}

#elif defined(XRDP_JPEG)

/* libjpeg */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

#define JP_QUALITY 75

struct mydata_comp
{
    JOCTET *cb;
    int cb_bytes;
    int total_done;
    int overwrite;
};

/*****************************************************************************/
/* called at beginning */
static void
my_init_destination(j_compress_ptr cinfo)
{
    struct mydata_comp *md;

    md = (struct mydata_comp *)(cinfo->client_data);
    md->total_done = 0;
    md->overwrite = 0;
    cinfo->dest->next_output_byte = md->cb;
    cinfo->dest->free_in_buffer = md->cb_bytes;
}

/*****************************************************************************/
/* called when buffer is full and we need more space */
static int
my_empty_output_buffer(j_compress_ptr cinfo)
{
    struct mydata_comp *md;
    int chunk_bytes;

    md = (struct mydata_comp *)(cinfo->client_data);
    chunk_bytes = md->cb_bytes;
    md->total_done += chunk_bytes;
    cinfo->dest->next_output_byte = md->cb;
    cinfo->dest->free_in_buffer = md->cb_bytes;
    md->overwrite = 1;
    return 1;
}

/*****************************************************************************/
/* called at end */
static void
my_term_destination(j_compress_ptr cinfo)
{
    struct mydata_comp *md;
    int chunk_bytes;

    md = (struct mydata_comp *)(cinfo->client_data);
    chunk_bytes = md->cb_bytes - cinfo->dest->free_in_buffer;
    md->total_done += chunk_bytes;
}

/*****************************************************************************/
static int
jp_do_compress(JOCTET *data, int width, int height, int bpp, int quality,
               JOCTET *comp_data, int *comp_data_bytes)
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
static int
jpeg_compress(char *in_data, int width, int height,
              struct stream *s, struct stream *temp_s, int bpp,
              int byte_limit, int e, int quality)
{
    JOCTET *data;
    tui32 *src32;
    tui8 *dst8;
    tui32 pixel;
    int red;
    int blue;
    int green;
    int j;
    int i;
    int cdata_bytes;

    data = (JOCTET *) temp_s->data;
    dst8 = data;

    if (bpp == 24)
    {
        src32 = (tui32 *)in_data;

        for (j = 0; j < height; j++)
        {
            for (i = 0; i < width; i++)
            {
                pixel = src32[i + j * width];
                SPLITCOLOR32(red, green, blue, pixel);
                *(dst8++) = blue;
                *(dst8++) = green;
                *(dst8++) = red;
            }

            if (width > 0)
            {
                for (i = 0; i < e; i++)
                {
                    *(dst8++) = blue;
                    *(dst8++) = green;
                    *(dst8++) = red;
                }
            }
        }
    }
    else
    {
        g_writeln("bpp wrong %d", bpp);
    }

    cdata_bytes = byte_limit;
    jp_do_compress(data, width + e, height, 24, quality, (JOCTET *) s->p,
                   &cdata_bytes);
    s->p += cdata_bytes;
    return cdata_bytes;
}

/*****************************************************************************/
int
xrdp_jpeg_compress(void *handle, char *in_data, int width, int height,
                   struct stream *s, int bpp, int byte_limit,
                   int start_line, struct stream *temp_s,
                   int e, int quality)
{
    jpeg_compress(in_data, width, height, s, temp_s, bpp, byte_limit,
                  e, quality);
    return height;
}

/*****************************************************************************/
int
xrdp_codec_jpeg_compress(void *handle, int format, char *inp_data, int width,
                         int height, int stride, int x, int y, int cx, int cy,
                         int quality, char *out_data, int *io_len)
{
    return 0;
}

/*****************************************************************************/
void *
xrdp_jpeg_init(void)
{
    return 0;
}

/*****************************************************************************/
int
xrdp_jpeg_deinit(void *handle)
{
    return 0;
}

#else

/*****************************************************************************/
int
xrdp_jpeg_compress(void *handle, char *in_data, int width, int height,
                   struct stream *s, int bpp, int byte_limit,
                   int start_line, struct stream *temp_s,
                   int e, int quality)
{
    return height;
}

/*****************************************************************************/
int
xrdp_codec_jpeg_compress(void *handle, int format, char *inp_data, int width,
                         int height, int stride, int x, int y, int cx, int cy,
                         int quality, char *out_data, int *io_len)
{
    return 0;
}

/*****************************************************************************/
void *
xrdp_jpeg_init(void)
{
    return 0;
}

/*****************************************************************************/
int
xrdp_jpeg_deinit(void *handle)
{
    return 0;
}

#endif
