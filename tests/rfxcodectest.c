
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

/******************************************************************************/
int
main(int argc, char **argv)
{
    void *han;

    printf("main:\n");
    han = rfxcodec_encode_create(1920, 1024, RFX_FORMAT_BGRA);
    if (han == 0)
    {
        printf("main: rfxcodec_encode_create failed\n");
        return 1;
    }
    printf("main: rfxcodec_encode_create ok\n");
    rfxcodec_encode_destroy(han);
    return 0;
}
