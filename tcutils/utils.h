#ifndef UTILS_H
#define UTILS_H

#include <QDebug>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QStatusBar>
#include <xrdpapi.h>

/*
 * stream definition and macros to access them;
 * all definitions default to little endian
 */

typedef struct stream
{
    char* data;    /* holds stream data           */
    char* pos;     /* current read/write position */
    int   size;    /* number of bytes in data     */
} STREAM;

#define qstream_new(_s, _size)                 \
do                                             \
{                                              \
    (_s).data = (char *) malloc(_size);        \
    (_s).pos = (_s).data;                      \
    (_s).size = (_size);                       \
}                                              \
while (0)

#define qstream_new_zero(_s, _size)            \
do                                             \
{                                              \
    (_s).data = (char *) calloc((_size), 1);   \
    (_s).pos = (_s).data;                      \
    (_s).size = (_size);                       \
}                                              \
while (0)

#define qstream_free(_s)              \
do                                    \
{                                     \
    if ((_s)->data)                   \
        free((_s)->data);             \
}                                     \
while (0)

#define qstream_set_pos(_s, _p)       \
do                                    \
{                                     \
    (_s)->pos = (_s)->data + (_p);    \
}                                     \
while (0)

#define qstream_inc_pos(_s, _v)       \
do                                    \
{                                     \
    (_s)->pos += (_v);                \
}                                     \
while (0)

#define qstream_rd_u8(_s, _v)                \
do                                           \
{                                            \
    (_v) = *((unsigned char *) ((_s)->pos)); \
    (_s)->pos++;                             \
}                                            \
while (0)

#define qstream_rd_u16(_s, _v)                        \
do                                                    \
{                                                     \
    (_v) = (unsigned short)                           \
    (                                                 \
        (*((unsigned char *) ((_s)->pos + 0)) << 0) | \
        (*((unsigned char *) ((_s)->pos + 1)) << 8)   \
    );                                                \
    (_s)->pos += 2;                                   \
} while (0)

#define qstream_rd_u32(_s, _v)                          \
do                                                      \
{                                                       \
    (_v) = (unsigned int)                               \
    (                                                   \
        (*((unsigned char *) ((_s)->pos + 0)) << 0)  |  \
        (*((unsigned char *) ((_s)->pos + 1)) << 8)  |  \
        (*((unsigned char *) ((_s)->pos + 2)) << 16) |  \
        (*((unsigned char *) ((_s)->pos + 3)) << 24)    \
    );                                                  \
    (_s)->pos += 4;                                     \
} while (0)

#define qstream_wr_u8(_s, _v)                           \
do                                                      \
{                                                       \
    *((_s)->pos) = (unsigned char) (_v);                \
    (_s)->pos++;                                        \
} while (0)

#define qstream_wr_u16(_s, _v)                          \
do                                                      \
{                                                       \
    *((_s)->pos) = (unsigned char) ((_v) >> 0);         \
    (_s)->pos++;                                        \
    *((_s)->pos) = (unsigned char) ((_v) >> 8);         \
    (_s)->pos++;                                        \
} while (0)

#define qstream_wr_u32(_s, _v)                          \
do                                                      \
{                                                       \
  *((_s)->pos) = (unsigned char) ((_v) >> 0);           \
  (_s)->pos++;                                          \
  *((_s)->pos) = (unsigned char) ((_v) >> 8);           \
  (_s)->pos++;                                          \
  *((_s)->pos) = (unsigned char) ((_v) >> 16);          \
  (_s)->pos++;                                          \
  *((_s)->pos) = (unsigned char) ((_v) >> 24);          \
  (_s)->pos++;                                          \
} while (0)

/* list of commands we support; this list should match the one in */
/* NeutrinoRDP channels/tcutils/tcutils_main.h                    */
enum TCU_COMMANDS
{
    TCU_CMD_GET_MOUNT_LIST = 1,
    TCU_CMD_UNMOUNT_DEVICE
};

/* umount error codes */
enum TCU_UMOUNT_ERROR
{
    UMOUNT_SUCCESS = 0,
    UMOUNT_BUSY,
    UMOUNT_NOT_MOUNTED,
    UMOUNT_OP_NOT_PERMITTED,
    UMOUNT_PERMISSION_DENIED,
    UMOUNT_UNKNOWN
};

class Utils
{
public:
    Utils();
    static int getMountList(void *wtsChannel, QList<QListWidgetItem *> *itemList);
    static int unmountDevice(void *wtsChannel, QString device, QStatusBar *statusBar);
};

#endif // UTILS_H
