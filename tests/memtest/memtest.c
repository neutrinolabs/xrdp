
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmem.h"
#include "log.h"

int main(int argc, char **argv)
{
    void *obj;
    unsigned int addr1;
    unsigned int addr2;
    unsigned int addr3;
    unsigned int addr4;
    unsigned int addr5;
    int index;
    int rd;
    struct log_config *config;

    config = log_config_init_for_console(LOG_LEVEL_DEBUG, NULL);
    log_start_from_param(config);
    log_config_free(config);
    /* Disable stdout buffering, as this can confuse the error
     * reporting when running in libcheck fork mode */
    setvbuf(stdout, NULL, _IONBF, 0);

    srand(time(0));
    obj = libmem_init(0x80000000, 64 * 1024 * 1024);
    for (index = 0; index < 256; index++)
    {
        rd = rand() & 0xffff;
        printf("1 rd %d\n", rd);
        addr1 = libmem_alloc(obj, rd);
        rd = rand() & 0xffff;
        printf("2 rd %d\n", rd);
        addr2 = libmem_alloc(obj, rd);
        rd = rand() & 0xffff;
        printf("3 rd %d\n", rd);
        addr3 = libmem_alloc(obj, rd);
        rd = rand() & 0xffff;
        printf("4 rd %d\n", rd);
        addr4 = libmem_alloc(obj, rd);
        addr5 = libmem_alloc(obj, rd);
        libmem_free(obj, addr1);
        printf("5\n");
        addr1 = libmem_alloc(obj, 64);
        printf("6\n");
        libmem_free(obj, addr3);
        printf("7\n");
        addr3 = libmem_alloc(obj, 64 * 1024);
        libmem_free(obj, addr5);
        addr5 = libmem_alloc(obj, 64 * 1024);
        printf("8\n");
        libmem_free(obj, addr1);
        printf("9\n");
        libmem_free(obj, addr2);
        printf("10\n");
        libmem_free(obj, addr3);
        libmem_free(obj, addr4);
        if (index == 255)
        {
            libmem_set_flags(obj, 1);
        }
        libmem_free(obj, addr5);
    }
    libmem_deinit(obj);
    log_end();
    return 0;
}
