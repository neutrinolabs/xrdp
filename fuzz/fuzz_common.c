#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

//#define NO_FUZZ

#include <stddef.h>
#include <stdint.h>

#ifdef NO_FUZZ
#include <stdio.h>
#include <stdlib.h>
#endif

extern int do_fuzz(const uint8_t *data, size_t data_size);

#ifndef NO_FUZZ
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t data_size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t data_size)
{
    return do_fuzz(data, data_size);
}
#else
int main(int argc, char * argv[])
{
    if (argc < 2 || argc > 3)
        return EXIT_FAILURE;

    FILE* f = fopen(argv[1], "rb");
    if (!f)
        return EXIT_FAILURE;

    long size = ftell(f);
    if (size == 0) {
        fclose(f);
        return EXIT_FAILURE;
    }

    uint8_t* buf = malloc(size);
    if (!buf) {
        fclose(f);
        return EXIT_FAILURE;
    }

    size_t bytes_read = fread(buf, sizeof(uint8_t), size, f);
    if (bytes_read != size) {
        free(buf);
        fclose(f);
        return EXIT_FAILURE;
    }

    fclose(f);
    f = NULL;

    const int cnt = (argc == 3) ? atoi(argv[2]) : 1;

    for (int i = 0; i < cnt; ++i)
        do_fuzz(buf, size);

    free(buf);

    return EXIT_SUCCESS;
}
#endif
