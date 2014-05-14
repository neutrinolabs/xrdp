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

#include <rfxcodec_encode.h>

/******************************************************************************/
int
main(int argc, char **argv)
{
    void *han;
    int error;
    int cdata_bytes;
    int fd;
    char *cdata;
    char *buf;
    struct rfx_rect regions[1];
    struct rfx_tile tiles[1];
    int quants[10] = { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 };
    //int quants[10] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };
    //int quants[10] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };

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
    //memset(buf, 1, 64 * 64 * 4);
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
    error = rfxcodec_encode(han, cdata, &cdata_bytes, buf, 64, 64, 64 * 4,
                            regions, 1, tiles, 1, quants, 1);
    printf("main: rfxcodec_encode error %d cdata_bytes %d\n", error, cdata_bytes);
    rfxcodec_encode_destroy(han);
    free(buf);
    free(cdata);
    return 0;
}
