/**
 * RFX codec encoder test
 *
 * Copyright 2014 Jay Sorg <jay.sorg@gmail.com>
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

#include <rfxcodec_encode.h>

static const int g_rfx_default_quantization_values[] =
{
    /* LL3 LH3 HL3 HH3 LH2 HL2 HH2 LH1 HL1 HH1 */
       6,  6,  6,  6,  7,  7,  8,  8,  8,  9
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
speed_random(int count, const int *quants)
{
    void *han;
    int error;
    int index;
    int cdata_bytes;
    int fd;
    char *cdata;
    char *buf;
    struct rfx_rect regions[1];
    struct rfx_tile tiles[1];
    int stime;
    int etime;
    int tiles_per_second;

    printf("main:\n");
    han = rfxcodec_encode_create(1920, 1024, RFX_FORMAT_BGRA, RFX_FLAGS_NONE);
    if (han == 0)
    {
        printf("main: rfxcodec_encode_create failed\n");
        return 1;
    }
    printf("main: rfxcodec_encode_create ok\n");
    cdata = (char *) malloc(64 * 64 * 4);
    cdata_bytes = 64 * 64 * 4;
    buf = (char *) malloc(64 * 64 * 4);
    fd = open("/dev/urandom", O_RDONLY);
    read(fd, buf, 64 * 64 * 4);
    close(fd);
    regions[0].x = 0;
    regions[0].y = 0;
    regions[0].cx = 64;
    regions[0].cy = 64;
    tiles[0].x = 0;
    tiles[0].y = 0;
    tiles[0].cx = 64;
    tiles[0].cy = 64;
    tiles[0].quant_y = 0;
    tiles[0].quant_cb = 0;
    tiles[0].quant_cr = 0;
    error = 0;
    stime = get_mstime();
    for (index = 0; index < count; index++)
    {
        error = rfxcodec_encode(han, cdata, &cdata_bytes, buf, 64, 64, 64 * 4,
                                regions, 1, tiles, 1, quants, 1);
        if (error != 0)
        {
            break;
        }
    }
    etime = get_mstime();
    tiles_per_second = count * 1000 / (etime - stime);
    printf("speed_random: cdata_bytes %d count %d ms time %d "
           "tiles_per_second %d\n",
           cdata_bytes, count, etime - stime, tiles_per_second);
    rfxcodec_encode_destroy(han);
    free(buf);
    free(cdata);
    return 0;
}

/******************************************************************************/
static int
out_usage(void)
{
    printf("rfxdectest usage\n");
    printf("this program is used for testing the rfx encoder for both speed "
           "and integrity\n");
    return 0;
}

/******************************************************************************/
int
main(int argc, char **argv)
{
    int index;
    int do_speed;
    int count;
    const int *quants = g_rfx_default_quantization_values;

    do_speed = 0;
    count = 0;
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
        else
        {
            return out_usage();
        }
    }
    if (do_speed)
    {
        speed_random(count, quants);
    }
    return 0;
}
