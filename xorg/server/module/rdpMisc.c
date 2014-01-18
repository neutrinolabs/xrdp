/*
Copyright 2005-2014 Jay Sorg

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

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

/* the g_ functions from os_calls.c */

/*****************************************************************************/
int
g_sck_recv(int sck, void *ptr, int len, int flags)
{
    return recv(sck, ptr, len, flags);
}

/*****************************************************************************/
void
g_sck_close(int sck)
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
g_sck_last_error_would_block(int sck)
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
g_sck_send(int sck, void *ptr, int len, int flags)
{
    return send(sck, ptr, len, flags);
}

/*****************************************************************************/
void *
g_malloc(int size, int zero)
{
    char *rv;

    rv = (char *)malloc(size);
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
        free(ptr);
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
g_sck_tcp_socket(void)
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
g_sck_local_socket_dgram(void)
{
    return socket(AF_UNIX, SOCK_DGRAM, 0);
}

/*****************************************************************************/
int
g_sck_local_socket_stream(void)
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
void
g_memset(void *d_ptr, const unsigned char chr, int size)
{
    memset(d_ptr, chr, size);
}

/*****************************************************************************/
int
g_sck_tcp_set_no_delay(int sck)
{
    int i;

    i = 1;
    setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
    return 0;
}

/*****************************************************************************/
int
g_sck_set_non_blocking(int sck)
{
    unsigned long i;

    i = fcntl(sck, F_GETFL);
    i = i | O_NONBLOCK;
    fcntl(sck, F_SETFL, i);
    return 0;
}

/*****************************************************************************/
int
g_sck_accept(int sck)
{
    struct sockaddr_in s;
    unsigned int i;

    i = sizeof(struct sockaddr_in);
    memset(&s, 0, i);
    return accept(sck, (struct sockaddr *)&s, &i);
}

/*****************************************************************************/
int
g_sck_select(int sck1, int sck2, int sck3)
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
g_sck_tcp_bind(int sck, char *port)
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
g_sck_local_bind(int sck, char *port)
{
    struct sockaddr_un s;

    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    strcpy(s.sun_path, port);
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_un));
}

/*****************************************************************************/
int
g_sck_listen(int sck)
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
/* produce a hex dump */
void
g_hexdump(void *p, long len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    offset = 0;
    line = (unsigned char *) p;

    while (offset < (int) len)
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
