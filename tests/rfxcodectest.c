/**
 * RFX codec encoder test
 *
 * Copyright 2014-2015 Jay Sorg <jay.sorg@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rfxcodec_encode.h>

static const unsigned char g_rfx_default_quantization_values[] =
{
    /* LL3 LH3 HL3 HH3 LH2 HL2 HH2 LH1 HL1 HH1 */
    0x66,  0x66,  0x77,  0x88,  0x98,
    0x99,  0x99,  0xaa,  0xcc,  0xdc
};

/*****************************************************************************/
static int
get_mstime(void)
{
    struct timeval tp;

    gettimeofday(&tp, 0);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

/******************************************************************************/
static int
speed_random(int count, const char *quants)
{
    void *han;
    int error;
    int index;
    int cdata_bytes;
    int fd;
    char *cdata;
    char *buf;
    struct rfx_rect regions[1];
    struct rfx_tile tiles[2];
    int stime;
    int etime;
    int tiles_per_second;
    int num_regions;
    int num_tiles;
    int num_quants;
    int flags;

    printf("speed_random:\n");
    //flags = RFX_FLAGS_RLGR1 | RFX_FLAGS_NOACCEL;
    flags = RFX_FLAGS_RLGR1;
    //flags = RFX_FLAGS_RLGR3;
    //flags = RFX_FLAGS_RLGR1 | RFX_FLAGS_ALPHAV1;
    error = rfxcodec_encode_create_ex(1920, 1024, RFX_FORMAT_BGRA, flags, &han);
    if (error != 0)
    {
        printf("speed_random: rfxcodec_encode_create_ex failed\n");
        return 1;
    }
    printf("speed_random: rfxcodec_encode_create_ex ok\n");
    cdata = (char *) malloc(128 * 64 * 4);
    cdata_bytes = 128 * 64 * 4;
    buf = (char *) malloc(128 * 64 * 4);
#if 1
    fd = open("/dev/urandom", O_RDONLY);
    //fd = open("/dev/zero", O_RDONLY);
    if (read(fd, buf, 128 * 64 * 4) != 128 * 64 * 4)
    {
        printf("speed_random: read error\n");
    }
    close(fd);
#else
    memset(buf, 0x7f, 128 * 64 * 4);
#endif
    regions[0].x = 0;
    regions[0].y = 0;
    regions[0].cx = 128;
    regions[0].cy = 64;
    num_regions = 1;
    tiles[0].x = 0;
    tiles[0].y = 0;
    tiles[0].cx = 64;
    tiles[0].cy = 64;
    tiles[0].quant_y = 0;
    tiles[0].quant_cb = 0;
    tiles[0].quant_cr = 0;
    tiles[1].x = 64;
    tiles[1].y = 0;
    tiles[1].cx = 64;
    tiles[1].cy = 64;
    tiles[1].quant_y = 0;
    tiles[1].quant_cb = 0;
    tiles[1].quant_cr = 0;
    num_tiles = 1;
    num_quants = 1;
    error = 0;
    stime = get_mstime();
    flags = 0;
    //flags = RFX_FLAGS_ALPHAV1;
    for (index = 0; index < count; index++)
    {
        error = rfxcodec_encode(han, cdata, &cdata_bytes, buf, 64, 64, 64 * 4,
                                regions, num_regions, tiles, num_tiles,
                                quants, num_quants, flags);
        if (error != 0)
        {
            break;
        }
    }
    etime = get_mstime();
    tiles_per_second = count * num_tiles * 1000 / (etime - stime + 1);
    printf("speed_random: cdata_bytes %d count %d ms time %d "
           "tiles_per_second %d\n",
           cdata_bytes, count, etime - stime, tiles_per_second);
    rfxcodec_encode_destroy(han);
    free(buf);
    free(cdata);
    return 0;
}

struct bmp_magic
{
    char magic[2];
};

struct bmp_hdr
{
    unsigned int   size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offset;
};

struct dib_hdr
{
    unsigned int   hdr_size;
    int            width;
    int            height;
    unsigned short nplanes;
    unsigned short bpp;
    unsigned int   compress_type;
    unsigned int   image_size;
    int            hres;
    int            vres;
    unsigned int   ncolors;
    unsigned int   nimpcolors;
};

/******************************************************************************/
static int
load_bmp_file(int in_fd, char **data, int *width, int *height)
{
    struct bmp_magic bmpm;
    struct bmp_hdr   bmph;
    struct dib_hdr   dibh;
    int awidth;
    int aheight;
    int line_bytes;
    int index;
    int jndex;
    int red;
    int gre;
    int blu;
    int *dst32;
    char *line;
    char *line_ptr;

    if (read(in_fd, &bmpm, sizeof(struct bmp_magic)) != sizeof(struct bmp_magic))
    {
        return 1;
    }
    if (read(in_fd, &bmph, sizeof(struct bmp_hdr)) != sizeof(struct bmp_hdr))
    {
        return 1;
    }
    if (read(in_fd, &dibh, sizeof(struct dib_hdr)) != sizeof(struct dib_hdr))
    {
        return 1;
    }
    if (dibh.bpp != 24)
    {
        printf("only support 24 bpp bmp file now\n");
        return 1;
    }
    printf("bpp %d\n", dibh.bpp);
    *width = dibh.width;
    *height = dibh.height;
    awidth = (dibh.width + 63) & ~63;
    aheight = (dibh.height + 63) & ~63;
    *data = (char *) malloc(awidth * aheight * 4);

    line_bytes = dibh.width * 3;
    line_bytes = (line_bytes + 3) & ~3;
    line = (char *) malloc(line_bytes);

    memset(*data, 0, awidth * aheight);
    for (index = 0; index < dibh.height; index++)
    {
        dst32 = (int *) (*data);
        dst32 += index * awidth;

        line_ptr = line;
        if (read(in_fd, line, line_bytes) != line_bytes)
        {
            return 1;
        }
        for (jndex = 0; jndex < dibh.width; jndex++)
        {
            red = *(line_ptr++);
            gre = *(line_ptr++);
            blu = *(line_ptr++);
            *(dst32++) = (red << 16) | (gre << 8) | blu;
        }
    }

    free(line);

    return 0; 
}

/******************************************************************************/
static int
encode_file(char *data, int width, int height, char *cdata, int *cdata_bytes,
            const char *quants, int num_quants)
{
    int awidth;
    int aheight;
    int num_tiles;
    int index;
    int jndex;
    int error;
    int num_regions;
    struct rfx_tile *tiles;
    struct rfx_tile *tiles_ptr;
    void *han;
    struct rfx_rect regions[1];

    error = rfxcodec_encode_create_ex(1920, 1024, RFX_FORMAT_BGRA, RFX_FLAGS_RLGR1, &han);
    if (error != 0)
    {
        printf("encode_file: rfxcodec_encode_create_ex failed\n");
        return 1;
    }

    awidth = (width + 63) & ~63;
    aheight = (height + 63) & ~63;

    num_tiles = (awidth / 64) * (aheight / 64);
    tiles = (struct rfx_tile *) malloc(num_tiles * sizeof(struct rfx_tile));
    tiles_ptr = tiles;
    for (index = 0; index < aheight; index += 64)
    {
        for (jndex = 0; jndex < awidth; jndex += 64)
        {
            tiles_ptr[0].x = jndex;
            tiles_ptr[0].y = index;
            tiles_ptr[0].cx = 64;
            tiles_ptr[0].cy = 64;
            tiles_ptr[0].quant_y = 0;
            tiles_ptr[0].quant_cb = num_quants - 1;
            tiles_ptr[0].quant_cr = num_quants - 1;
            tiles_ptr++;
        }
    }

    regions[0].x = 0;
    regions[0].y = 0;
    regions[0].cx = width;
    regions[0].cy = height;
    num_regions = 1;

    error = rfxcodec_encode(han, cdata, cdata_bytes, data, width, height, width * 4,
                            regions, num_regions, tiles, num_tiles,
                            quants, num_quants, 0);
    if (error != 0)
    {
        printf("encode_file: rfxcodec_encode failed error %d\n", error);
        return 1;
    }

    rfxcodec_encode_destroy(han);


    free(tiles);
    return 0; 
}

/******************************************************************************/
static int
read_file(int count, const char *quants, int num_quants,
          const char *in_file, const char *out_file)
{
    int in_fd;
    int out_fd;
    int width;
    int height;
    int cdata_bytes;
    char *data;
    char *cdata;

    in_fd = open(in_file, O_RDONLY);
    if (in_fd == -1)
    {
        printf("error opening %s\n", in_file);
        return 1;
    }
    out_fd = -1;
    if (out_file[0] != 0)
    {
        if (access(out_file, F_OK) == 0) 
        {
            printf("out files exists\n");
            return 1;
        }
        out_fd = open(out_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (out_fd == -1)
        {
            printf("error opening %s\n", out_file);
            return 1;
        }
    }

    data = 0;
    width = 0;
    height = 0;
    if (load_bmp_file(in_fd, &data, &width, &height) != 0)
    {
        printf("load_bmp_file failed\n");
        return 1;
    }
    printf("loaded file ok width %d height %d\n", width, height);
    cdata_bytes = (width + 64) * (height + 64);
    cdata = (char *) malloc(cdata_bytes);
    if (encode_file(data, width, height, cdata, &cdata_bytes, quants, num_quants) != 0)
    {
        printf("encode_file failed\n");
        return 1;
    }
    printf("encode data ok bytes %d\n", cdata_bytes);
    
    if (out_fd != -1)
    {
        if (write(out_fd, cdata, cdata_bytes) != cdata_bytes) 
        {
            printf("write failed\n");
        }
    }

    free(data);
    free(cdata);
    close(in_fd);
    if (out_fd != -1)
    {
        close(out_fd);
    }
    return 0; 
}

/******************************************************************************/
static int
out_usage(void)
{
    printf("rfxdectest usage\n");
    printf("this program is used for testing the rfx encoder for both speed "
           "and integrity\n");
    printf("examples\n");
    printf("  ./rfxcodectest --speed --count 1000\n");
    printf("  ./rfxcodectest -i infile.bmp -o outfile.rfx\n");
    printf("\n");
    return 0;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int index;
    int do_speed;
    int do_read;
    int count;
    char in_file[256];
    char out_file[256];
    const char *quants = (const char *) g_rfx_default_quantization_values;

    do_speed = 0;
    do_read = 0;
    in_file[0] = 0;
    out_file[0] = 0;
    count = 1;
    if (argc < 2)
    {
        return out_usage();
    }
    for (index = 1; index < argc; index++)
    {
        if (strcmp("--speed", argv[index]) == 0)
        {
            do_speed = 1;
        }
        else if (strcmp("--count", argv[index]) == 0)
        {
            index++;
            count = atoi(argv[index]);
        }
        else if (strcmp("-i", argv[index]) == 0)
        {
            index++;
            snprintf(in_file, 255, "%s", argv[index]);
            do_read = 1;
        }
        else if (strcmp("-o", argv[index]) == 0)
        {
            index++;
            snprintf(out_file, 255, "%s", argv[index]);
        }
        else
        {
            return out_usage();
        }
    }
    if (do_speed)
    {
        speed_random(count, quants);
    }
    if (do_read)
    {
        read_file(count, quants, 2, in_file, out_file);
    }
    return 0; 
}
