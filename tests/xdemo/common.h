#ifndef __XDEMO_H
#define __XDEMO_H

#define DEBUG

#ifdef DEBUG
#define dprint(x...) printf(x)
#else
#define dprint(x...)
#endif

struct pic_info
{
    int  width;
    int  height;
    char *pixel_data;
};

#endif
