#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "common.h"

// multi byte values are stored in little endian

struct bmp_magic
{
    char magic[2];
};

struct bmp_hdr
{
    uint32_t size;        // file size in bytes
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;      // offset to image data, in bytes
};

struct dib_hdr
{
    uint32_t  hdr_size;
    int32_t   width;
    int32_t   height;
    uint16_t  nplanes;
    uint16_t  bpp;
    uint32_t  compress_type;
    uint32_t  image_size;
    int32_t   hres;
    int32_t   vres;
    uint32_t  ncolors;
    uint32_t  nimpcolors;
};

// forward declarations
int parse_bmp(char *filename, struct pic_info *pic_info);
int parse_bmp_24(struct bmp_hdr *bmp_hdr, struct dib_hdr *dib_hdr, int fd, struct pic_info *pic_info);

int parse_bmp(char *filename, struct pic_info *pic_info)
{
    int  got_magic;
    int  fd;
    int  rval;

    struct bmp_magic   magic;
    struct bmp_hdr     bmp_hdr;
    struct dib_hdr     dib_hdr;

    if ((fd = open(filename, O_RDONLY)) < 0) {
        printf("error opeing %s\n", filename);
        return -1;
    }

    // read BMP magic...
    if ((rval = read(fd, magic.magic, 2)) != 2) {
        fprintf(stderr, "error reading BMP signature from file %s\n", filename);
        return -1;
    }

    got_magic = 0;

    // ...and confirm that this is indeed a BMP file
    if ((magic.magic[0] == 'B') && (magic.magic[1] == 'M')) {
        // BM – Windows 3.1x, 95, NT, ... etc
        got_magic = 1;
    }
    else if ((magic.magic[0] == 'B') && (magic.magic[1] == 'A')) {
        // BA – OS/2 struct Bitmap Array
        got_magic = 1;
    }
    else if ((magic.magic[0] == 'C') && (magic.magic[1] == 'I')) {
        // CI – OS/2 struct Color Icon
        got_magic = 1;
    }
    else if ((magic.magic[0] == 'C') && (magic.magic[1] == 'P')) {
        // CP – OS/2 const Color Pointer
        got_magic = 1;
    }
    else if ((magic.magic[0] == 'I') && (magic.magic[1] == 'C')) {
        // IC – OS/2 struct Icon
        got_magic = 1;
    }
    else if ((magic.magic[0] == 'P') && (magic.magic[1] == 'T')) {
        // PT – OS/2 Pointer
        got_magic = 1;
    }

    if (!got_magic) {
        fprintf(stderr, "%s is not a valid BMP file\n", filename);
        return -1;
    }

    // read BMP header
    if ((rval = read(fd, &bmp_hdr, sizeof(bmp_hdr))) < sizeof(bmp_hdr)) {
        fprintf(stderr, "error BMP header from file %s\n", filename);
        return -1;
    }

    // read DIB header
    if ((rval = read(fd, &dib_hdr, sizeof(dib_hdr))) < sizeof(dib_hdr)) {
        fprintf(stderr, "error reading DIB header from file %s\n", filename);
        return -1;
    }

#if 0
    printf("header size:   %d\n", dib_hdr.hdr_size);
    printf("width:         %d\n", dib_hdr.width);
    printf("height:        %d\n", dib_hdr.height);
    printf("num planes:    %d\n", dib_hdr.nplanes);
    printf("bpp:           %d\n", dib_hdr.bpp);
    printf("comp type:     %d\n", dib_hdr.compress_type);
    printf("image size:    %d\n", dib_hdr.image_size);
    printf("hres:          %d\n", dib_hdr.hres);
    printf("vres:          %d\n", dib_hdr.vres);
    printf("ncolors:       %d\n", dib_hdr.ncolors);
    printf("nimpcolors:    %d\n", dib_hdr.nimpcolors);
#endif

    if (dib_hdr.compress_type) {
        printf("TODO: compressed images not yet supported\n");
        return -1;
    }

    pic_info->width = dib_hdr.width;
    pic_info->height = dib_hdr.height;

    if (dib_hdr.bpp == 24) {
        rval = parse_bmp_24(&bmp_hdr, &dib_hdr, fd, pic_info);
    }
    close(fd);
    return rval;
}

/**
 * extract 24bit BMP data from image file
 *
 * @return 0 on success
 * @return -1 on failure
 */

int parse_bmp_24(
    struct bmp_hdr  *bmp_hdr,
    struct dib_hdr  *dib_hdr,
    int             fd,
    struct pic_info *pic_info
)
{
    char *file_data;
    char *ptr_file_data;
    char *mem_data;
    char *ptr_mem_data;
    char *cptr;

    int  w = dib_hdr->width;    // picture width
    int  h = dib_hdr->height;   // picture height
    int  bpl;                   // bytes per line
    int  bytes;
    int  i;
    int  j;

    // bytes per image line = width x bytes_per_pixel + padding
    i = (w * 3) % 4;
    j = (i == 0) ? 0 : 4 - i;
    bpl = w * 3 + j;

    // 24 bit depth, no alpha channel
    file_data = (char *) malloc(h * bpl);

    // point to first line in image data, which is stored in reverse order
    ptr_file_data = (file_data + dib_hdr->image_size) - bpl;

    // 24 bit depth, with alpha channel
    mem_data  = (char *) malloc(w * h * 4);
    ptr_mem_data = mem_data;

    pic_info->pixel_data = ptr_mem_data;

    // seek to beginning of pixel data
    lseek(fd, bmp_hdr->offset, SEEK_SET);

    // read all pixel data
    bytes = read(fd, file_data, dib_hdr->image_size);

    // convert 24bit to 24 bit with alpha and store in reverse
    for (i = 0; i < h; i ++)
    {
        cptr = ptr_file_data;
        for (j = 0; j < w; j++)
        {
            *ptr_mem_data++ = *cptr++; // blue value
            *ptr_mem_data++ = *cptr++; // green value
            *ptr_mem_data++ = *cptr++; // red value
            *ptr_mem_data++ = 0;       // alpha channel
        }
        ptr_file_data -= bpl;
    }

    free(file_data);
    return 0;
}
