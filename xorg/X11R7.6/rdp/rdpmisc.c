/*
Copyright 2005-2013 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

the rest

*/

#include "rdp.h"

#include <sys/un.h>

Bool noFontCacheExtension = 1;

static int g_crc_seed = 0xffffffff;
static int g_crc_table[256] =
{
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
  0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
  0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
  0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
  0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
  0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
  0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
  0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
  0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
  0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
  0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
  0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
  0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
  0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
  0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
  0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
  0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
  0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
  0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
  0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
  0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
  0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

#define CRC_START(in_crc) (in_crc) = g_crc_seed
#define CRC_PASS(in_pixel, in_crc) \
  (in_crc) = g_crc_table[((in_crc) ^ (in_pixel)) & 0xff] ^ ((in_crc) >> 8)
#define CRC_END(in_crc) (in_crc) = ((in_crc) ^ g_crc_seed)

/******************************************************************************/
/* print a time-stamped message to the log file (stderr). */
void
rdpLog(char *format, ...)
{
    va_list args;
    char buf[256];
    time_t clock;

    va_start(args, format);
    time(&clock);
    strftime(buf, 255, "%d/%m/%y %T ", localtime(&clock));
    fprintf(stderr, "%s", buf);
    vfprintf(stderr, format, args);
    fflush(stderr);
    va_end(args);
}

/******************************************************************************/
int
rdpBitsPerPixel(int depth)
{
    if (depth == 1)
    {
        return 1;
    }
    else if (depth <= 8)
    {
        return 8;
    }
    else if (depth <= 16)
    {
        return 16;
    }
    else
    {
        return 32;
    }
}

/******************************************************************************/
void
rdpClientStateChange(CallbackListPtr *cbl, pointer myData, pointer clt)
{
    dispatchException &= ~DE_RESET; /* hack - force server not to reset */
}

/******************************************************************************/
int
DPMSSupported(void)
{
    return 0;
}

/******************************************************************************/
int
DPSMGet(int *level)
{
    return -1;
}

/******************************************************************************/
void
DPMSSet(int level)
{
}

/******************************************************************************/
void
AddOtherInputDevices(void)
{
}

/******************************************************************************/
void
OpenInputDevice(DeviceIntPtr dev, ClientPtr client, int *status)
{
}

/******************************************************************************/
int
SetDeviceValuators(register ClientPtr client, DeviceIntPtr dev,
                   int *valuators, int first_valuator, int num_valuators)
{
    return BadMatch;
}

/******************************************************************************/
int
SetDeviceMode(register ClientPtr client, DeviceIntPtr dev, int mode)
{
    return BadMatch;
}

/******************************************************************************/
int
ChangeKeyboardDevice(DeviceIntPtr old_dev, DeviceIntPtr new_dev)
{
    return BadMatch;
}

/******************************************************************************/
int
ChangeDeviceControl(register ClientPtr client, DeviceIntPtr dev,
                    void *control)
{
    return BadMatch;
}

/******************************************************************************/
int
ChangePointerDevice(DeviceIntPtr old_dev, DeviceIntPtr new_dev,
                    unsigned char x, unsigned char y)
{
    return BadMatch;
}

/******************************************************************************/
void
CloseInputDevice(DeviceIntPtr d, ClientPtr client)
{
}

/* the g_ functions from os_calls.c */

/*****************************************************************************/
int
g_tcp_recv(int sck, void *ptr, int len, int flags)
{
    return recv(sck, ptr, len, flags);
}

/*****************************************************************************/
void
g_tcp_close(int sck)
{
    if (sck == 0)
    {
        return;
    }

    shutdown(sck, 2);
    close(sck);
}

/*****************************************************************************/
int
g_tcp_last_error_would_block(int sck)
{
    return (errno == EWOULDBLOCK) || (errno == EINPROGRESS);
}

/*****************************************************************************/
void
g_sleep(int msecs)
{
    usleep(msecs * 1000);
}

/*****************************************************************************/
int
g_tcp_send(int sck, void *ptr, int len, int flags)
{
    return send(sck, ptr, len, flags);
}

/*****************************************************************************/
void *
g_malloc(int size, int zero)
{
    char *rv;

    //#ifdef _XSERVER64
#if 1
    /* I thought xalloc would work here but I guess not, why, todo */
    rv = (char *)malloc(size);
#else
    rv = (char *)Xalloc(size);
#endif

    if (zero)
    {
        if (rv != 0)
        {
            memset(rv, 0, size);
        }
    }

    return rv;
}

/*****************************************************************************/
void
g_free(void *ptr)
{
    if (ptr != 0)
    {
        //#ifdef _XSERVER64
#if 1
        /* I thought xfree would work here but I guess not, why, todo */
        free(ptr);
#else
        Xfree(ptr);
#endif
    }
}

/*****************************************************************************/
void
g_sprintf(char *dest, char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsprintf(dest, format, ap);
    va_end(ap);
}

/*****************************************************************************/
int
g_tcp_socket(void)
{
    int rv;
    int i;

    i = 1;
    rv = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(rv, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
    setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(i));
    return rv;
}

/*****************************************************************************/
int
g_tcp_local_socket_dgram(void)
{
    return socket(AF_UNIX, SOCK_DGRAM, 0);
}

/*****************************************************************************/
int
g_tcp_local_socket_stream(void)
{
    return socket(AF_UNIX, SOCK_STREAM, 0);
}

/*****************************************************************************/
void
g_memcpy(void *d_ptr, const void *s_ptr, int size)
{
    memcpy(d_ptr, s_ptr, size);
}

/*****************************************************************************/
int
g_tcp_set_no_delay(int sck)
{
    int i;

    i = 1;
    setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
    return 0;
}

/*****************************************************************************/
int
g_tcp_set_non_blocking(int sck)
{
    unsigned long i;

    i = fcntl(sck, F_GETFL);
    i = i | O_NONBLOCK;
    fcntl(sck, F_SETFL, i);
    return 0;
}

/*****************************************************************************/
int
g_tcp_accept(int sck)
{
    struct sockaddr_in s;
    unsigned int i;

    i = sizeof(struct sockaddr_in);
    memset(&s, 0, i);
    return accept(sck, (struct sockaddr *)&s, &i);
}

/*****************************************************************************/
int
g_tcp_select(int sck1, int sck2, int sck3)
{
    fd_set rfds;
    struct timeval time;
    int max;
    int rv;

    time.tv_sec = 0;
    time.tv_usec = 0;
    FD_ZERO(&rfds);

    if (sck1 > 0)
    {
        FD_SET(((unsigned int)sck1), &rfds);
    }

    if (sck2 > 0)
    {
        FD_SET(((unsigned int)sck2), &rfds);
    }

    if (sck3 > 0)
    {
        FD_SET(((unsigned int)sck3), &rfds);
    }

    max = sck1;

    if (sck2 > max)
    {
        max = sck2;
    }

    if (sck3 > max)
    {
        max = sck3;
    }

    rv = select(max + 1, &rfds, 0, 0, &time);

    if (rv > 0)
    {
        rv = 0;

        if (FD_ISSET(((unsigned int)sck1), &rfds))
        {
            rv = rv | 1;
        }

        if (FD_ISSET(((unsigned int)sck2), &rfds))
        {
            rv = rv | 2;
        }

        if (FD_ISSET(((unsigned int)sck3), &rfds))
        {
            rv = rv | 4;
        }
    }
    else
    {
        rv = 0;
    }

    return rv;
}

/*****************************************************************************/
int
g_tcp_bind(int sck, char *port)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons(atoi(port));
    s.sin_addr.s_addr = INADDR_ANY;
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
}

/*****************************************************************************/
int
g_tcp_local_bind(int sck, char *port)
{
    struct sockaddr_un s;

    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    strcpy(s.sun_path, port);
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_un));
}

/*****************************************************************************/
int
g_tcp_listen(int sck)
{
    return listen(sck, 2);
}

/*****************************************************************************/
/* returns boolean */
int
g_create_dir(const char *dirname)
{
    return mkdir(dirname, (mode_t) - 1) == 0;
}

/*****************************************************************************/
/* returns boolean, non zero if the directory exists */
int
g_directory_exist(const char *dirname)
{
    struct stat st;

    if (stat(dirname, &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }
    else
    {
        return 0;
    }
}

/*****************************************************************************/
/* returns error */
int
g_chmod_hex(const char *filename, int flags)
{
    int fl;

    fl = 0;
    fl |= (flags & 0x4000) ? S_ISUID : 0;
    fl |= (flags & 0x2000) ? S_ISGID : 0;
    fl |= (flags & 0x1000) ? S_ISVTX : 0;
    fl |= (flags & 0x0400) ? S_IRUSR : 0;
    fl |= (flags & 0x0200) ? S_IWUSR : 0;
    fl |= (flags & 0x0100) ? S_IXUSR : 0;
    fl |= (flags & 0x0040) ? S_IRGRP : 0;
    fl |= (flags & 0x0020) ? S_IWGRP : 0;
    fl |= (flags & 0x0010) ? S_IXGRP : 0;
    fl |= (flags & 0x0004) ? S_IROTH : 0;
    fl |= (flags & 0x0002) ? S_IWOTH : 0;
    fl |= (flags & 0x0001) ? S_IXOTH : 0;
    return chmod(filename, fl);
}

/*****************************************************************************/
/* returns directory where UNIX sockets are located */
const char *
g_socket_dir(void)
{
    const char *socket_dir;

    socket_dir = getenv("XRDP_SOCKET_PATH");
    if (socket_dir == NULL || socket_dir[0] == '\0')
    {
        socket_dir = "/tmp/.xrdp";
    }

    return socket_dir;
}

/* produce a hex dump */
void
hexdump(unsigned char *p, unsigned int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    offset = 0;
    line = p;

    while (offset < len)
    {
        ErrorF("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            ErrorF("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            ErrorF("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            ErrorF("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        ErrorF("\n");
        offset += thisline;
        line += thisline;
    }
}

/*
  stub for XpClient* functions.
*/

/*****************************************************************************/
Bool
XpClientIsBitmapClient(ClientPtr client)
{
    return 1;
}

/*****************************************************************************/
Bool
XpClientIsPrintClient(ClientPtr client, FontPathElementPtr fpe)
{
    return 0;
}

/*****************************************************************************/
int
PrinterOptions(int argc, char **argv, int i)
{
    return i;
}

/*****************************************************************************/
void
PrinterInitOutput(ScreenInfo *pScreenInfo, int argc, char **argv)
{
}

/*****************************************************************************/
void
PrinterUseMsg(void)
{
}

/*****************************************************************************/
void
PrinterInitGlobals(void)
{
}

/*****************************************************************************/
void
FontCacheExtensionInit(INITARGS)
{
}

/******************************************************************************/
void
RegionAroundSegs(RegionPtr reg, xSegment *segs, int nseg)
{
    int index;
    BoxRec box;
    RegionRec treg;

    index = 0;

    while (index < nseg)
    {
        if (segs[index].x1 < segs[index].x2)
        {
            box.x1 = segs[index].x1;
            box.x2 = segs[index].x2;
        }
        else
        {
            box.x1 = segs[index].x2;
            box.x2 = segs[index].x1;
        }

        box.x2++;

        if (segs[index].y1 < segs[index].y2)
        {
            box.y1 = segs[index].y1;
            box.y2 = segs[index].y2;
        }
        else
        {
            box.y1 = segs[index].y2;
            box.y2 = segs[index].y1;
        }

        box.y2++;
        RegionInit(&treg, &box, 0);
        RegionUnion(reg, reg, &treg);
        RegionUninit(&treg);
        index++;
    }
}

/******************************************************************************/
int
get_crc(char* data, int data_bytes)
{
    int crc;
    int index;

    CRC_START(crc);
    for (index = 0; index < data_bytes; index++)
    {
        CRC_PASS(data[index], crc);
    }
    CRC_END(crc);
    return crc;
}

/*****************************************************************************/
int
get_mstime(void)
{
    struct timeval tp;

    gettimeofday(&tp, 0);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}
