/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 *
 * generic operating system calls
 *
 * put all the os / arch define in here you want
 */

/* To test for Windows (64 bit or 32 bit) use _WIN32 and _WIN64 in addition
   for 64 bit windows.  _WIN32 is defined for both.
   To test for Linux use __linux__.
   To test for BSD use BSD */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif
#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#else
/* fix for solaris 10 with gcc 3.3.2 problem */
#if defined(sun) || defined(__sun)
#define ctid_t id_t
#endif
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#if defined(XRDP_ENABLE_VSOCK)
#include <linux/vm_sockets.h>
#endif
#include <sys/un.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <locale.h>

/* this is so we can use #ifdef BSD later */
/* This is the recommended way of detecting BSD in the
   FreeBSD Porter's Handbook. */
#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#include "os_calls.h"
#include "string_calls.h"
#include "log.h"

/* for clearenv() */
#if defined(_WIN32)
#else
extern char **environ;
#endif

#if defined(__linux__)
#include <linux/unistd.h>
#endif

/* sys/ucred.h needs to be included to use struct xucred
 * in FreeBSD and OS X. No need for other BSDs except GNU/kFreeBSD */
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__FreeBSD_kernel__)
#include <sys/ucred.h>
#endif

/* for solaris */
#if !defined(PF_LOCAL)
#define PF_LOCAL AF_UNIX
#endif
#if !defined(INADDR_NONE)
#define INADDR_NONE ((unsigned long)-1)
#endif

/*****************************************************************************/
int
g_rm_temp_dir(void)
{
    return 0;
}

/*****************************************************************************/
int
g_mk_socket_path(const char *app_name)
{
    if (!g_directory_exist(XRDP_SOCKET_PATH))
    {
        if (!g_create_path(XRDP_SOCKET_PATH"/"))
        {
            /* if failed, still check if it got created by someone else */
            if (!g_directory_exist(XRDP_SOCKET_PATH))
            {
                LOG(LOG_LEVEL_ERROR,
                    "g_mk_socket_path: g_create_path(%s) failed",
                    XRDP_SOCKET_PATH);
                return 1;
            }
        }
        g_chmod_hex(XRDP_SOCKET_PATH, 0x1777);
    }
    return 0;
}

/*****************************************************************************/
void
g_init(const char *app_name)
{
#if defined(_WIN32)
    WSADATA wsadata;

    WSAStartup(2, &wsadata);
#endif

    /* In order to get g_mbstowcs and g_wcstombs to work properly with
       UTF-8 non-ASCII characters, LC_CTYPE cannot be "C" or blank.
       To select UTF-8 encoding without specifying any countries/languages,
       "C.UTF-8" is used but provided in few systems.

       See also: https://sourceware.org/glibc/wiki/Proposals/C.UTF-8 */
    char *lc_ctype;
    lc_ctype = setlocale(LC_CTYPE, "C.UTF-8");
    if (lc_ctype == NULL)
    {
        /* use en_US.UTF-8 instead if not available */
        setlocale(LC_CTYPE, "en_US.UTF-8");
    }

    g_mk_socket_path(app_name);
}

/*****************************************************************************/
void
g_deinit(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
    fflush(stdout);
    fflush(stderr);
    g_rm_temp_dir();
}

/*****************************************************************************/
/* allocate memory, returns a pointer to it, size bytes are allocated,
   if zero is non zero, each byte will be set to zero */
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
/* free the memory pointed to by ptr, ptr can be zero */
void
g_free(void *ptr)
{
    if (ptr != 0)
    {
        free(ptr);
    }
}

/*****************************************************************************/
/* output text to stdout, try to use g_write / g_writeln instead to avoid
   linux / windows EOL problems */
void
g_printf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

/*****************************************************************************/
void
g_sprintf(char *dest, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsprintf(dest, format, ap);
    va_end(ap);
}

/*****************************************************************************/
int
g_snprintf(char *dest, int len, const char *format, ...)
{
    int err;
    va_list ap;

    va_start(ap, format);
    err = vsnprintf(dest, len, format, ap);
    va_end(ap);

    return err;
}

/*****************************************************************************/
void
g_writeln(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
#if defined(_WIN32)
    g_printf("\r\n");
#else
    g_printf("\n");
#endif
}

/*****************************************************************************/
void
g_write(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

/*****************************************************************************/
/* print a hex dump to stdout*/
void
g_hexdump(const char *p, int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    line = (unsigned char *)p;
    offset = 0;

    while (offset < len)
    {
        g_printf("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            g_printf("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            g_printf("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            g_printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        g_writeln("%s", "");
        offset += thisline;
        line += thisline;
    }
}

/*****************************************************************************/
void
g_memset(void *ptr, int val, int size)
{
    memset(ptr, val, size);
}

/*****************************************************************************/
void
g_memcpy(void *d_ptr, const void *s_ptr, int size)
{
    memcpy(d_ptr, s_ptr, size);
}

/*****************************************************************************/
int
g_getchar(void)
{
    return getchar();
}

/*****************************************************************************/
/*Returns 0 on success*/
int
g_tcp_set_no_delay(int sck)
{
    int ret = 1; /* error */
    int option_value;
    socklen_t option_len;

    option_len = sizeof(option_value);

    /* SOL_TCP IPPROTO_TCP */
    if (getsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);

            if (setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (char *)&option_value,
                           option_len) == 0)
            {
                ret = 0; /* success */
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "Error setting tcp_nodelay");
            }
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "Error getting tcp_nodelay");
    }

    return ret;
}

/*****************************************************************************/
/*Returns 0 on success*/
int
g_tcp_set_keepalive(int sck)
{
    int ret = 1; /* error */
    int option_value;
    socklen_t option_len;

    option_len = sizeof(option_value);

    /* SOL_TCP IPPROTO_TCP */
    if (getsockopt(sck, SOL_SOCKET, SO_KEEPALIVE, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);

            if (setsockopt(sck, SOL_SOCKET, SO_KEEPALIVE, (char *)&option_value,
                           option_len) == 0)
            {
                ret = 0; /* success */
            }
            else
            {
                LOG(LOG_LEVEL_ERROR, "Error setting tcp_keepalive");
            }
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "Error getting tcp_keepalive");
    }

    return ret;
}

/*****************************************************************************/
/* returns a newly created socket or -1 on error */
/* in win32 a socket is an unsigned int, in linux, it's an int */
int
g_tcp_socket(void)
{
    int rv;
    int option_value;
    socklen_t option_len;

#if defined(XRDP_ENABLE_IPV6)
    rv = (int)socket(AF_INET6, SOCK_STREAM, 0);
    if (rv < 0)
    {
        switch (errno)
        {
            case EAFNOSUPPORT: /* if IPv6 not supported, retry IPv4 */
                LOG(LOG_LEVEL_INFO, "IPv6 not supported, falling back to IPv4");
                rv = (int)socket(AF_INET, SOCK_STREAM, 0);
                break;

            default:
                LOG(LOG_LEVEL_ERROR, "g_tcp_socket: %s", g_get_strerror());
                return -1;
        }
    }
#else
    rv = (int)socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (rv < 0)
    {
        LOG(LOG_LEVEL_ERROR, "g_tcp_socket: %s", g_get_strerror());
        return -1;
    }
#if defined(XRDP_ENABLE_IPV6)
    option_len = sizeof(option_value);
    if (getsockopt(rv, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value != 0)
        {
#if defined(XRDP_ENABLE_IPV6ONLY)
            option_value = 1;
#else
            option_value = 0;
#endif
            option_len = sizeof(option_value);
            if (setsockopt(rv, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&option_value,
                           option_len) < 0)
            {
                LOG(LOG_LEVEL_ERROR, "g_tcp_socket: setsockopt() failed");
            }
        }
    }
#endif
    option_len = sizeof(option_value);
    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            if (setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (char *)&option_value,
                           option_len) < 0)
            {
                LOG(LOG_LEVEL_ERROR, "g_tcp_socket: setsockopt() failed");
            }
        }
    }

    option_len = sizeof(option_value);

    if (getsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                   &option_len) == 0)
    {
        if (option_value < (1024 * 32))
        {
            option_value = 1024 * 32;
            option_len = sizeof(option_value);
            if (setsockopt(rv, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                           option_len) < 0)
            {
                LOG(LOG_LEVEL_ERROR, "g_tcp_socket: setsockopt() failed");
            }
        }
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
int
g_sck_set_send_buffer_bytes(int sck, int bytes)
{
    int option_value;
    socklen_t option_len;

    option_value = bytes;
    option_len = sizeof(option_value);
    if (setsockopt(sck, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                   option_len) != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int
g_sck_get_send_buffer_bytes(int sck, int *bytes)
{
    int option_value;
    socklen_t option_len;

    option_value = 0;
    option_len = sizeof(option_value);
    if (getsockopt(sck, SOL_SOCKET, SO_SNDBUF, (char *)&option_value,
                   &option_len) != 0)
    {
        return 1;
    }
    *bytes = option_value;
    return 0;
}

/*****************************************************************************/
/* returns error */
int
g_sck_set_recv_buffer_bytes(int sck, int bytes)
{
    int option_value;
    socklen_t option_len;

    option_value = bytes;
    option_len = sizeof(option_value);
    if (setsockopt(sck, SOL_SOCKET, SO_RCVBUF, (char *)&option_value,
                   option_len) != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int
g_sck_get_recv_buffer_bytes(int sck, int *bytes)
{
    int option_value;
    socklen_t option_len;

    option_value = 0;
    option_len = sizeof(option_value);
    if (getsockopt(sck, SOL_SOCKET, SO_RCVBUF, (char *)&option_value,
                   &option_len) != 0)
    {
        return 1;
    }
    *bytes = option_value;
    return 0;
}

/*****************************************************************************/
int
g_sck_local_socket(void)
{
#if defined(_WIN32)
    return -1;
#else
    return socket(PF_LOCAL, SOCK_STREAM, 0);
#endif
}

/*****************************************************************************/
int
g_sck_vsock_socket(void)
{
#if defined(XRDP_ENABLE_VSOCK)
    return socket(PF_VSOCK, SOCK_STREAM, 0);
#else
    return -1;
#endif
}

/*****************************************************************************/
/* returns error */
int
g_sck_get_peer_cred(int sck, int *pid, int *uid, int *gid)
{
#if defined(SO_PEERCRED)
    socklen_t ucred_length;
    struct myucred
    {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } credentials;

    ucred_length = sizeof(credentials);
    if (getsockopt(sck, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length))
    {
        return 1;
    }
    if (pid != 0)
    {
        *pid = credentials.pid;
    }
    if (uid != 0)
    {
        *uid = credentials.uid;
    }
    if (gid != 0)
    {
        *gid = credentials.gid;
    }
    return 0;
#elif defined(LOCAL_PEERCRED)
    /* FreeBSD, OS X reach here*/
    struct xucred xucred;
    unsigned int xucred_length;
    xucred_length = sizeof(xucred);

    if (getsockopt(sck, SOL_SOCKET, LOCAL_PEERCRED, &xucred, &xucred_length))
    {
        return 1;
    }
    if (pid != 0)
    {
        *pid = 0; /* can't get pid in FreeBSD, OS X */
    }
    if (uid != 0)
    {
        *uid = xucred.cr_uid;
    }
    if (gid != 0)
    {
        *gid = xucred.cr_gid;
    }
    return 0;
#else
    return 1;
#endif
}

/*****************************************************************************/
void
g_sck_close(int sck)
{
#if defined(_WIN32)
    closesocket(sck);
#else
    char sockname[128];
    union
    {
        struct sockaddr sock_addr;
        struct sockaddr_in sock_addr_in;
#if defined(XRDP_ENABLE_IPV6)
        struct sockaddr_in6 sock_addr_in6;
#endif
#if defined(XRDP_ENABLE_VSOCK)
        struct sockaddr_vm sock_addr_vm;
#endif
    } sock_info;
    socklen_t sock_len = sizeof(sock_info);

    memset(&sock_info, 0, sizeof(sock_info));

    if (getsockname(sck, &sock_info.sock_addr, &sock_len) == 0)
    {
        switch (sock_info.sock_addr.sa_family)
        {
            case AF_INET:
            {
                struct sockaddr_in *sock_addr_in = &sock_info.sock_addr_in;

                g_snprintf(sockname, sizeof(sockname), "AF_INET %s:%d",
                           inet_ntoa(sock_addr_in->sin_addr),
                           ntohs(sock_addr_in->sin_port));
                break;
            }

#if defined(XRDP_ENABLE_IPV6)

            case AF_INET6:
            {
                char addr[48];
                struct sockaddr_in6 *sock_addr_in6 = &sock_info.sock_addr_in6;

                g_snprintf(sockname, sizeof(sockname), "AF_INET6 %s port %d",
                           inet_ntop(sock_addr_in6->sin6_family,
                                     &sock_addr_in6->sin6_addr, addr, sizeof(addr)),
                           ntohs(sock_addr_in6->sin6_port));
                break;
            }

#endif

            case AF_UNIX:
                g_snprintf(sockname, sizeof(sockname), "AF_UNIX");
                break;

#if defined(XRDP_ENABLE_VSOCK)

            case AF_VSOCK:
            {
                struct sockaddr_vm *sock_addr_vm = &sock_info.sock_addr_vm;

                g_snprintf(sockname,
                           sizeof(sockname),
                           "AF_VSOCK cid %d port %d",
                           sock_addr_vm->svm_cid,
                           sock_addr_vm->svm_port);
                break;
            }

#endif

            default:
                g_snprintf(sockname, sizeof(sockname), "unknown family %d",
                           sock_info.sock_addr.sa_family);
                break;
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "getsockname() failed on socket %d: %s",
            sck, g_get_strerror());

        if (errno == EBADF || errno == ENOTSOCK)
        {
            return;
        }

        g_snprintf(sockname, sizeof(sockname), "unknown");
    }

    if (close(sck) == 0)
    {
        LOG(LOG_LEVEL_DEBUG, "Closed socket %d (%s)", sck, sockname);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "Cannot close socket %d (%s): %s", sck,
            sockname, g_get_strerror());
    }

#endif
}

#if defined(XRDP_ENABLE_IPV6)
/*****************************************************************************/
/* Helper function for g_tcp_connect.                                        */
static int
connect_loopback(int sck, const char *port)
{
    struct sockaddr_in6 sa;
    struct sockaddr_in s;
    int res;

    // First IPv6
    g_memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_loopback;             // IPv6 ::1
    sa.sin6_port = htons((tui16)atoi(port));
    res = connect(sck, (struct sockaddr *)&sa, sizeof(sa));
    if (res == -1 && errno == EINPROGRESS)
    {
        return -1;
    }
    if (res == 0 || (res == -1 && errno == EISCONN))
    {
        return 0;
    }

    // else IPv4
    g_memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // IPv4 127.0.0.1
    s.sin_port = htons((tui16)atoi(port));
    res = connect(sck, (struct sockaddr *)&s, sizeof(s));
    if (res == -1 && errno == EINPROGRESS)
    {
        return -1;
    }
    if (res == 0 || (res == -1 && errno == EISCONN))
    {
        return 0;
    }

    // else IPv6 with IPv4 address
    g_memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::FFFF:127.0.0.1", &sa.sin6_addr);
    sa.sin6_port = htons((tui16)atoi(port));
    res = connect(sck, (struct sockaddr *)&sa, sizeof(sa));
    if (res == -1 && errno == EINPROGRESS)
    {
        return -1;
    }
    if (res == 0 || (res == -1 && errno == EISCONN))
    {
        return 0;
    }

    return -1;
}
#endif

/*****************************************************************************/
/* returns error, zero is good                                               */
/* The connection might get to be in progress, if so -1 is returned.         */
/* The caller needs to call again to check if succeed.                       */
#if defined(XRDP_ENABLE_IPV6)
int
g_tcp_connect(int sck, const char *address, const char *port)
{
    int res = 0;
    struct addrinfo p;
    struct addrinfo *h = (struct addrinfo *)NULL;
    struct addrinfo *rp = (struct addrinfo *)NULL;

    g_memset(&p, 0, sizeof(struct addrinfo));

    p.ai_socktype = SOCK_STREAM;
    p.ai_protocol = IPPROTO_TCP;
    p.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
    p.ai_family = AF_INET6;
    if (g_strcmp(address, "127.0.0.1") == 0)
    {
        return connect_loopback(sck, port);
    }
    else
    {
        res = getaddrinfo(address, port, &p, &h);
    }
    if (res != 0)
    {
        LOG(LOG_LEVEL_ERROR, "g_tcp_connect(%d, %s, %s): getaddrinfo() failed: %s",
            sck, address, port, gai_strerror(res));
    }
    if (res > -1)
    {
        if (h != NULL)
        {
            for (rp = h; rp != NULL; rp = rp->ai_next)
            {
                res = connect(sck, (struct sockaddr *)(rp->ai_addr),
                              rp->ai_addrlen);
                if (res == -1 && errno == EINPROGRESS)
                {
                    break; /* Return -1 */
                }
                if (res == 0 || (res == -1 && errno == EISCONN))
                {
                    res = 0;
                    break; /* Success */
                }
            }
            freeaddrinfo(h);
        }
    }

    return res;
}
#else
int
g_tcp_connect(int sck, const char *address, const char *port)
{
    struct sockaddr_in s;
    struct hostent *h;
    int res;

    g_memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((tui16)atoi(port));
    s.sin_addr.s_addr = inet_addr(address);
    if (s.sin_addr.s_addr == INADDR_NONE)
    {
        h = gethostbyname(address);
        if (h != 0)
        {
            if (h->h_name != 0)
            {
                if (h->h_addr_list != 0)
                {
                    if ((*(h->h_addr_list)) != 0)
                    {
                        s.sin_addr.s_addr = *((int *)(*(h->h_addr_list)));
                    }
                }
            }
        }
    }
    res = connect(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_in));

    /* Mac OSX connect() returns -1 for already established connections */
    if (res == -1 && errno == EISCONN)
    {
        res = 0;
    }

    return res;
}
#endif

/*****************************************************************************/
/* returns error, zero is good */
int
g_sck_local_connect(int sck, const char *port)
{
#if defined(_WIN32)
    return -1;
#else
    struct sockaddr_un s;

    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    strncpy(s.sun_path, port, sizeof(s.sun_path));
    s.sun_path[sizeof(s.sun_path) - 1] = 0;

    return connect(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_un));
#endif
}

/*****************************************************************************/
int
g_sck_set_non_blocking(int sck)
{
    unsigned long i;

#if defined(_WIN32)
    i = 1;
    ioctlsocket(sck, FIONBIO, &i);
#else
    i = fcntl(sck, F_GETFL);
    i = i | O_NONBLOCK;
    if (fcntl(sck, F_SETFL, i) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "g_sck_set_non_blocking: fcntl() failed");
    }
#endif
    return 0;
}

#if defined(XRDP_ENABLE_IPV6)
/*****************************************************************************/
/* returns error, zero is good */
int
g_tcp_bind(int sck, const char *port)
{
    struct sockaddr_in6 sa;
    struct sockaddr_in s;
    int errno6;

    // First IPv6
    g_memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_any;                 // IPv6 ::
    sa.sin6_port = htons((tui16)atoi(port));
    if (bind(sck, (struct sockaddr *)&sa, sizeof(sa)) == 0)
    {
        return 0;
    }
    errno6 = errno;

    // else IPv4
    g_memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(INADDR_ANY);     // IPv4 0.0.0.0
    s.sin_port = htons((tui16)atoi(port));
    if (bind(sck, (struct sockaddr *)&s, sizeof(s)) == 0)
    {
        return 0;
    }

    LOG(LOG_LEVEL_ERROR, "g_tcp_bind(%d, %s) failed "
        "bind IPv6 (errno=%d) and IPv4 (errno=%d).",
        sck, port, errno6, errno);
    return -1;
}
#else
int
g_tcp_bind(int sck, const char *port)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((tui16)atoi(port));
    s.sin_addr.s_addr = INADDR_ANY;
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
}
#endif

/*****************************************************************************/
int
g_sck_local_bind(int sck, const char *port)
{
#if defined(_WIN32)
    return -1;
#else
    struct sockaddr_un s;

    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    strncpy(s.sun_path, port, sizeof(s.sun_path));
    s.sun_path[sizeof(s.sun_path) - 1] = 0;

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_un));
#endif
}

/*****************************************************************************/
int
g_sck_vsock_bind(int sck, const char *port)
{
#if defined(XRDP_ENABLE_VSOCK)
    struct sockaddr_vm s;

    g_memset(&s, 0, sizeof(struct sockaddr_vm));
    s.svm_family = AF_VSOCK;
    s.svm_port = atoi(port);
    s.svm_cid = VMADDR_CID_ANY;

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_vm));
#else
    return -1;
#endif
}

/*****************************************************************************/
int
g_sck_vsock_bind_address(int sck, const char *port, const char *address)
{
#if defined(XRDP_ENABLE_VSOCK)
    struct sockaddr_vm s;

    g_memset(&s, 0, sizeof(struct sockaddr_vm));
    s.svm_family = AF_VSOCK;
    s.svm_port = atoi(port);
    s.svm_cid = atoi(address);

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_vm));
#else
    return -1;
#endif
}


#if defined(XRDP_ENABLE_IPV6)
/*****************************************************************************/
/* Helper function for g_tcp_bind_address.                                   */
static int
bind_loopback(int sck, const char *port)
{
    struct sockaddr_in6 sa;
    struct sockaddr_in s;
    int errno6;
    int errno4;

    // First IPv6
    g_memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_loopback;             // IPv6 ::1
    sa.sin6_port = htons((tui16)atoi(port));
    if (bind(sck, (struct sockaddr *)&sa, sizeof(sa)) == 0)
    {
        return 0;
    }
    errno6 = errno;

    // else IPv4
    g_memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // IPv4 127.0.0.1
    s.sin_port = htons((tui16)atoi(port));
    if (bind(sck, (struct sockaddr *)&s, sizeof(s)) == 0)
    {
        return 0;
    }
    errno4 = errno;

    // else IPv6 with IPv4 address
    g_memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::FFFF:127.0.0.1", &sa.sin6_addr);
    sa.sin6_port = htons((tui16)atoi(port));
    if (bind(sck, (struct sockaddr *)&sa, sizeof(sa)) == 0)
    {
        return 0;
    }

    LOG(LOG_LEVEL_ERROR, "bind_loopback(%d, %s) failed; "
        "IPv6 ::1 (errno=%d), IPv4 127.0.0.1 (errno=%d) and IPv6 ::FFFF:127.0.0.1 (errno=%d).",
        sck, port, errno6, errno4, errno);
    return -1;
}

/*****************************************************************************/
/* Helper function for g_tcp_bind_address.                                   */
/* Returns error, zero is good.                                              */
static int
getaddrinfo_bind(int sck, const char *port, const char *address)
{
    int res;
    int error;
    struct addrinfo hints;
    struct addrinfo *list;
    struct addrinfo *i;

    res = -1;
    g_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = 0;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    error = getaddrinfo(address, port, &hints, &list);
    if (error == 0)
    {
        i = list;
        while ((i != 0) && (res < 0))
        {
            res = bind(sck, i->ai_addr, i->ai_addrlen);
            i = i->ai_next;
        }
        freeaddrinfo(list);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "getaddrinfo error: %s", gai_strerror(error));
        return -1;
    }
    return res;
}

/*****************************************************************************/
/* Binds a socket to a port. If no specified address the port will be bind   */
/* to 'any', i.e. available on all network.                                  */
/* For bind to local host, see valid address strings below.                  */
/* Returns error, zero is good.                                              */
int
g_tcp_bind_address(int sck, const char *port, const char *address)
{
    int res;

    if ((address == 0) ||
            (address[0] == 0) ||
            (g_strcmp(address, "0.0.0.0") == 0) ||
            (g_strcmp(address, "::") == 0))
    {
        return g_tcp_bind(sck, port);
    }

    if ((g_strcmp(address, "127.0.0.1") == 0) ||
            (g_strcmp(address, "::1") == 0) ||
            (g_strcmp(address, "localhost") == 0))
    {
        return bind_loopback(sck, port);
    }

    // Let getaddrinfo translate the address string...
    // IPv4: ddd.ddd.ddd.ddd
    // IPv6: x:x:x:x:x:x:x:x%<interface>, or x::x:x:x:x%<interface>
    res = getaddrinfo_bind(sck, port, address);
    if (res != 0)
    {
        // If fail and it is an IPv4 address, try with the mapped address
        struct in_addr a;
        if ((inet_aton(address, &a) == 1) && (strlen(address) <= 15))
        {
            char sz[7 + 15 + 1];
            sprintf(sz, "::FFFF:%s", address);
            res = getaddrinfo_bind(sck, port, sz);
            if (res == 0)
            {
                return 0;
            }
        }

        LOG(LOG_LEVEL_ERROR, "g_tcp_bind_address(%d, %s, %s) Failed!",
            sck, port, address);
        return -1;
    }
    return 0;
}
#else
int
g_tcp_bind_address(int sck, const char *port, const char *address)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = htons((tui16)atoi(port));
    s.sin_addr.s_addr = INADDR_ANY;
    if (inet_aton(address, &s.sin_addr) < 0)
    {
        return -1; /* bad address */
    }
    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
}
#endif

/*****************************************************************************/
/* returns error, zero is good */
int
g_sck_listen(int sck)
{
    return listen(sck, 2);
}

/*****************************************************************************/
int
g_tcp_accept(int sck)
{
    int ret;
    char msg[256];
    union
    {
        struct sockaddr sock_addr;
        struct sockaddr_in sock_addr_in;
#if defined(XRDP_ENABLE_IPV6)
        struct sockaddr_in6 sock_addr_in6;
#endif
    } sock_info;

    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sock_len);

    ret = accept(sck, (struct sockaddr *)&sock_info, &sock_len);

    if (ret > 0)
    {
        switch (sock_info.sock_addr.sa_family)
        {
            case AF_INET:
            {
                struct sockaddr_in *sock_addr_in = &sock_info.sock_addr_in;

                g_snprintf(msg, sizeof(msg), "A connection received from %s port %d",
                           inet_ntoa(sock_addr_in->sin_addr),
                           ntohs(sock_addr_in->sin_port));
                LOG(LOG_LEVEL_INFO, "%s", msg);

                break;
            }

#if defined(XRDP_ENABLE_IPV6)

            case AF_INET6:
            {
                struct sockaddr_in6 *sock_addr_in6 = &sock_info.sock_addr_in6;
                char addr[256];

                inet_ntop(sock_addr_in6->sin6_family,
                          &sock_addr_in6->sin6_addr, addr, sizeof(addr));
                g_snprintf(msg, sizeof(msg), "A connection received from %s port %d",
                           addr, ntohs(sock_addr_in6->sin6_port));
                LOG(LOG_LEVEL_INFO, "%s", msg);

                break;

            }

#endif
        }
    }

    return ret;
}

/*****************************************************************************/
int
g_sck_accept(int sck, char *addr, int addr_bytes, char *port, int port_bytes)
{
    int ret;
    char msg[256];
    union
    {
        struct sockaddr sock_addr;
        struct sockaddr_in sock_addr_in;
#if defined(XRDP_ENABLE_IPV6)
        struct sockaddr_in6 sock_addr_in6;
#endif
        struct sockaddr_un sock_addr_un;
#if defined(XRDP_ENABLE_VSOCK)
        struct sockaddr_vm sock_addr_vm;
#endif
    } sock_info;

    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sock_len);

    ret = accept(sck, (struct sockaddr *)&sock_info, &sock_len);

    if (ret > 0)
    {
        switch (sock_info.sock_addr.sa_family)
        {
            case AF_INET:
            {
                struct sockaddr_in *sock_addr_in = &sock_info.sock_addr_in;

                g_snprintf(addr, addr_bytes, "%s", inet_ntoa(sock_addr_in->sin_addr));
                g_snprintf(port, port_bytes, "%d", ntohs(sock_addr_in->sin_port));
                g_snprintf(msg, sizeof(msg),
                           "AF_INET connection received from %s port %s",
                           addr, port);
                break;
            }

#if defined(XRDP_ENABLE_IPV6)

            case AF_INET6:
            {
                struct sockaddr_in6 *sock_addr_in6 = &sock_info.sock_addr_in6;

                inet_ntop(sock_addr_in6->sin6_family,
                          &sock_addr_in6->sin6_addr, addr, addr_bytes);
                g_snprintf(port, port_bytes, "%d", ntohs(sock_addr_in6->sin6_port));
                g_snprintf(msg, sizeof(msg),
                           "AF_INET6 connection received from %s port %s",
                           addr, port);
                break;
            }

#endif

            case AF_UNIX:
            {
                g_strncpy(addr, "", addr_bytes - 1);
                g_strncpy(port, "", port_bytes - 1);
                g_snprintf(msg, sizeof(msg), "AF_UNIX connection received");
                break;
            }

#if defined(XRDP_ENABLE_VSOCK)

            case AF_VSOCK:
            {
                struct sockaddr_vm *sock_addr_vm = &sock_info.sock_addr_vm;

                g_snprintf(addr, addr_bytes - 1, "%d", sock_addr_vm->svm_cid);
                g_snprintf(port, addr_bytes - 1, "%d", sock_addr_vm->svm_port);

                g_snprintf(msg,
                           sizeof(msg),
                           "AF_VSOCK connection received from cid: %s port: %s",
                           addr,
                           port);

                break;
            }

#endif
            default:
            {
                g_strncpy(addr, "", addr_bytes - 1);
                g_strncpy(port, "", port_bytes - 1);
                g_snprintf(msg, sizeof(msg),
                           "connection received, unknown socket family %d",
                           sock_info.sock_addr.sa_family);
                break;
            }
        }


        LOG(LOG_LEVEL_INFO, "Socket %d: %s", ret, msg);

    }

    return ret;
}

/*****************************************************************************/
/*
 * TODO: this function writes not only IP address, name is confusing
 */
void
g_write_ip_address(int rcv_sck, char *ip_address, int bytes)
{
    char *addr;
    int port;
    int ok;

    union
    {
        struct sockaddr sock_addr;
        struct sockaddr_in sock_addr_in;
#if defined(XRDP_ENABLE_IPV6)
        struct sockaddr_in6 sock_addr_in6;
#endif
        struct sockaddr_un sock_addr_un;
    } sock_info;

    ok = 0;
    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sock_len);
#if defined(XRDP_ENABLE_IPV6)
    addr = (char *)g_malloc(INET6_ADDRSTRLEN, 1);
#else
    addr = (char *)g_malloc(INET_ADDRSTRLEN, 1);
#endif

    if (getpeername(rcv_sck, (struct sockaddr *)&sock_info, &sock_len) == 0)
    {
        switch (sock_info.sock_addr.sa_family)
        {
            case AF_INET:
            {
                struct sockaddr_in *sock_addr_in = &sock_info.sock_addr_in;
                g_snprintf(addr, INET_ADDRSTRLEN, "%s", inet_ntoa(sock_addr_in->sin_addr));
                port = ntohs(sock_addr_in->sin_port);
                ok = 1;
                break;
            }

#if defined(XRDP_ENABLE_IPV6)

            case AF_INET6:
            {
                struct sockaddr_in6 *sock_addr_in6 = &sock_info.sock_addr_in6;
                inet_ntop(sock_addr_in6->sin6_family,
                          &sock_addr_in6->sin6_addr, addr, INET6_ADDRSTRLEN);
                port = ntohs(sock_addr_in6->sin6_port);
                ok = 1;
                break;
            }

#endif

            default:
            {
                break;
            }

        }

        if (ok)
        {
            g_snprintf(ip_address, bytes, "%s:%d - socket: %d", addr, port, rcv_sck);
        }
    }

    if (!ok)
    {
        g_snprintf(ip_address, bytes, "NULL:NULL - socket: %d", rcv_sck);
    }

    g_free(addr);
}

/*****************************************************************************/
void
g_sleep(int msecs)
{
#if defined(_WIN32)
    Sleep(msecs);
#else
    usleep(msecs * 1000);
#endif
}

/*****************************************************************************/
int
g_sck_last_error_would_block(int sck)
{
#if defined(_WIN32)
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINPROGRESS);
#endif
}

/*****************************************************************************/
int
g_sck_recv(int sck, void *ptr, int len, int flags)
{
#if defined(_WIN32)
    return recv(sck, (char *)ptr, len, flags);
#else
    return recv(sck, ptr, len, flags);
#endif
}

/*****************************************************************************/
int
g_sck_send(int sck, const void *ptr, int len, int flags)
{
#if defined(_WIN32)
    return send(sck, (const char *)ptr, len, flags);
#else
    return send(sck, ptr, len, flags);
#endif
}

/*****************************************************************************/
/* returns boolean */
int
g_sck_socket_ok(int sck)
{
    int opt;
    socklen_t opt_len;

    opt_len = sizeof(opt);

    if (getsockopt(sck, SOL_SOCKET, SO_ERROR, (char *)(&opt), &opt_len) == 0)
    {
        if (opt == 0)
        {
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************/
/* wait 'millis' milliseconds for the socket to be able to write */
/* returns boolean */
int
g_sck_can_send(int sck, int millis)
{
    fd_set wfds;
    struct timeval time;
    int rv;

    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;
    FD_ZERO(&wfds);

    if (sck > 0)
    {
        FD_SET(((unsigned int)sck), &wfds);
        rv = select(sck + 1, 0, &wfds, 0, &time);

        if (rv > 0)
        {
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************/
/* wait 'millis' milliseconds for the socket to be able to receive */
/* returns boolean */
int
g_sck_can_recv(int sck, int millis)
{
    fd_set rfds;
    struct timeval time;
    int rv;

    g_memset(&time, 0, sizeof(time));
    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;
    FD_ZERO(&rfds);

    if (sck > 0)
    {
        FD_SET(((unsigned int)sck), &rfds);
        rv = select(sck + 1, &rfds, 0, 0, &time);

        if (rv > 0)
        {
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************/
int
g_sck_select(int sck1, int sck2)
{
    fd_set rfds;
    struct timeval time;
    int max;
    int rv;

    g_memset(&time, 0, sizeof(struct timeval));
    FD_ZERO(&rfds);

    if (sck1 > 0)
    {
        FD_SET(((unsigned int)sck1), &rfds);
    }

    if (sck2 > 0)
    {
        FD_SET(((unsigned int)sck2), &rfds);
    }

    max = sck1;

    if (sck2 > max)
    {
        max = sck2;
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
    }
    else
    {
        rv = 0;
    }

    return rv;
}

/*****************************************************************************/
/* returns boolean */
static int
g_fd_can_read(int fd)
{
    fd_set rfds;
    struct timeval time;
    int rv;

    g_memset(&time, 0, sizeof(time));
    FD_ZERO(&rfds);
    FD_SET(((unsigned int)fd), &rfds);
    rv = select(fd + 1, &rfds, 0, 0, &time);
    if (rv == 1)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* O_NONBLOCK = 0x00000800 */
static int
g_set_nonblock(int fd)
{
    int error;
    int flags;

    error = fcntl(fd, F_GETFL);
    if (error < 0)
    {
        return 1;
    }
    flags = error;
    if ((flags & O_NONBLOCK) != O_NONBLOCK)
    {
        flags |= O_NONBLOCK;
        error = fcntl(fd, F_SETFL, flags);
        if (error < 0)
        {
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
/* returns 0 on error */
tintptr
g_create_wait_obj(const char *name)
{
#ifdef _WIN32
    tintptr obj;

    obj = (tintptr)CreateEvent(0, 1, 0, name);
    return obj;
#else
    int fds[2];
    int error;

    error = pipe(fds);
    if (error != 0)
    {
        return 0;
    }
    if (g_set_nonblock(fds[0]) != 0)
    {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }
    if (g_set_nonblock(fds[1]) != 0)
    {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }
    return (fds[1] << 16) | fds[0];
#endif
}

/*****************************************************************************/
/* returns 0 on error */
tintptr
g_create_wait_obj_from_socket(tintptr socket, int write)
{
#ifdef _WIN32
    /* Create and return corresponding event handle for WaitForMultipleObjects */
    WSAEVENT event;
    long lnetevent = 0;

    g_memset(&event, 0, sizeof(WSAEVENT));

    event = WSACreateEvent();
    lnetevent = (write ? FD_WRITE : FD_READ) | FD_CLOSE;

    if (WSAEventSelect(socket, event, lnetevent) == 0)
    {
        return (tbus)event;
    }
    else
    {
        return 0;
    }

#else
    return socket;
#endif
}

/*****************************************************************************/
void
g_delete_wait_obj_from_socket(tintptr wait_obj)
{
#ifdef _WIN32

    if (wait_obj == 0)
    {
        return;
    }

    WSACloseEvent((HANDLE)wait_obj);
#else
#endif
}

/*****************************************************************************/
/* returns error */
int
g_set_wait_obj(tintptr obj)
{
#ifdef _WIN32
    if (obj == 0)
    {
        return 0;
    }
    SetEvent((HANDLE)obj);
    return 0;
#else
    int error;
    int fd;
    int written;
    int to_write;
    char buf[4] = "sig";

    if (obj == 0)
    {
        return 0;
    }
    fd = obj & 0xffff;
    if (g_fd_can_read(fd))
    {
        /* already signalled */
        return 0;
    }
    fd = obj >> 16;
    to_write = 4;
    written = 0;
    while (written < to_write)
    {
        error = write(fd, buf + written, to_write - written);
        if (error == -1)
        {
            error = errno;
            if ((error == EAGAIN) || (error == EWOULDBLOCK) ||
                    (error == EINPROGRESS) || (error == EINTR))
            {
                /* ok */
            }
            else
            {
                return 1;
            }
        }
        else if (error > 0)
        {
            written += error;
        }
        else
        {
            return 1;
        }
    }
    return 0;
#endif
}

/*****************************************************************************/
/* returns error */
int
g_reset_wait_obj(tintptr obj)
{
#ifdef _WIN32
    if (obj == 0)
    {
        return 0;
    }
    ResetEvent((HANDLE)obj);
    return 0;
#else
    char buf[4];
    int error;
    int fd;

    if (obj == 0)
    {
        return 0;
    }
    fd = obj & 0xffff;
    while (g_fd_can_read(fd))
    {
        error = read(fd, buf, 4);
        if (error == -1)
        {
            error = errno;
            if ((error == EAGAIN) || (error == EWOULDBLOCK) ||
                    (error == EINPROGRESS) || (error == EINTR))
            {
                /* ok */
            }
            else
            {
                return 1;
            }
        }
        else if (error == 0)
        {
            return 1;
        }
    }
    return 0;
#endif
}

/*****************************************************************************/
/* returns boolean */
int
g_is_wait_obj_set(tintptr obj)
{
#ifdef _WIN32
    if (obj == 0)
    {
        return 0;
    }
    if (WaitForSingleObject((HANDLE)obj, 0) == WAIT_OBJECT_0)
    {
        return 1;
    }
    return 0;
#else
    if (obj == 0)
    {
        return 0;
    }
    return g_fd_can_read(obj & 0xffff);
#endif
}

/*****************************************************************************/
/* returns error */
int
g_delete_wait_obj(tintptr obj)
{
#ifdef _WIN32
    if (obj == 0)
    {
        return 0;
    }
    /* Close event handle */
    CloseHandle((HANDLE)obj);
    return 0;
#else
    if (obj == 0)
    {
        return 0;
    }
    close(obj & 0xffff);
    close(obj >> 16);
    return 0;
#endif
}

/*****************************************************************************/
/* returns error */
int
g_obj_wait(tintptr *read_objs, int rcount, tintptr *write_objs, int wcount,
           int mstimeout)
{
#ifdef _WIN32
    HANDLE handles[256];
    DWORD count;
    DWORD error;
    int j;
    int i;

    j = 0;
    count = rcount + wcount;

    for (i = 0; i < rcount; i++)
    {
        handles[j++] = (HANDLE)(read_objs[i]);
    }

    for (i = 0; i < wcount; i++)
    {
        handles[j++] = (HANDLE)(write_objs[i]);
    }

    if (mstimeout < 1)
    {
        mstimeout = INFINITE;
    }

    error = WaitForMultipleObjects(count, handles, FALSE, mstimeout);

    if (error == WAIT_FAILED)
    {
        return 1;
    }

    return 0;
#else
    fd_set rfds;
    fd_set wfds;
    struct timeval time;
    struct timeval *ptime;
    int i = 0;
    int res = 0;
    int max = 0;
    int sck = 0;

    max = 0;
    if (mstimeout < 1)
    {
        ptime = 0;
    }
    else
    {
        g_memset(&time, 0, sizeof(struct timeval));
        time.tv_sec = mstimeout / 1000;
        time.tv_usec = (mstimeout % 1000) * 1000;
        ptime = &time;
    }

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    /* Find the highest descriptor number in read_obj */
    if (read_objs != NULL)
    {
        for (i = 0; i < rcount; i++)
        {
            sck = read_objs[i] & 0xffff;

            if (sck > 0)
            {
                FD_SET(sck, &rfds);

                if (sck > max)
                {
                    max = sck; /* max holds the highest socket/descriptor number */
                }
            }
        }
    }
    else if (rcount > 0)
    {
        LOG(LOG_LEVEL_ERROR, "Programming error read_objs is null");
        return 1; /* error */
    }

    if (write_objs != NULL)
    {
        for (i = 0; i < wcount; i++)
        {
            sck = (int)(write_objs[i]);

            if (sck > 0)
            {
                FD_SET(sck, &wfds);

                if (sck > max)
                {
                    max = sck; /* max holds the highest socket/descriptor number */
                }
            }
        }
    }
    else if (wcount > 0)
    {
        LOG(LOG_LEVEL_ERROR, "Programming error write_objs is null");
        return 1; /* error */
    }

    res = select(max + 1, &rfds, &wfds, 0, ptime);

    if (res < 0)
    {
        /* these are not really errors */
        if ((errno == EAGAIN) ||
                (errno == EWOULDBLOCK) ||
                (errno == EINPROGRESS) ||
                (errno == EINTR)) /* signal occurred */
        {
            return 0;
        }

        return 1; /* error */
    }

    return 0;
#endif
}

/*****************************************************************************/
void
g_random(char *data, int len)
{
#if defined(_WIN32)
    int index;

    srand(g_time1());

    for (index = 0; index < len; index++)
    {
        data[index] = (char)rand(); /* rand returns a number between 0 and
                                   RAND_MAX */
    }

#else
    int fd;

    memset(data, 0x44, len);
    fd = open("/dev/urandom", O_RDONLY);

    if (fd == -1)
    {
        fd = open("/dev/random", O_RDONLY);
    }

    if (fd != -1)
    {
        if (read(fd, data, len) != len)
        {
        }

        close(fd);
    }

#endif
}

/*****************************************************************************/
int
g_abs(int i)
{
    return abs(i);
}

/*****************************************************************************/
int
g_memcmp(const void *s1, const void *s2, int len)
{
    return memcmp(s1, s2, len);
}

/*****************************************************************************/
/* returns -1 on error, else return handle or file descriptor */
int
g_file_open(const char *file_name)
{
#if defined(_WIN32)
    return (int)CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
#else
    int rv;

    rv =  open(file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (rv == -1)
    {
        /* can't open read / write, try to open read only */
        rv =  open(file_name, O_RDONLY);
    }

    return rv;
#endif
}

/*****************************************************************************/
/* returns -1 on error, else return handle or file descriptor */
int
g_file_open_ex(const char *file_name, int aread, int awrite,
               int acreate, int atrunc)
{
#if defined(_WIN32)
    return -1;
#else
    int rv;
    int flags;

    flags = 0;
    if (aread && awrite)
    {
        flags |= O_RDWR;
    }
    else if (aread)
    {
        flags |= O_RDONLY;
    }
    else if (awrite)
    {
        flags |= O_WRONLY;
    }
    if (acreate)
    {
        flags |= O_CREAT;
    }
    if (atrunc)
    {
        flags |= O_TRUNC;
    }
    rv =  open(file_name, flags, S_IRUSR | S_IWUSR);
    return rv;
#endif
}

/*****************************************************************************/
/* returns error, always 0 */
int
g_file_close(int fd)
{
#if defined(_WIN32)
    CloseHandle((HANDLE)fd);
#else
    close(fd);
#endif
    return 0;
}

/*****************************************************************************/
/* read from file, returns the number of bytes read or -1 on error */
int
g_file_read(int fd, char *ptr, int len)
{
#if defined(_WIN32)

    if (ReadFile((HANDLE)fd, (LPVOID)ptr, (DWORD)len, (LPDWORD)&len, 0))
    {
        return len;
    }
    else
    {
        return -1;
    }

#else
    return read(fd, ptr, len);
#endif
}

/*****************************************************************************/
/* write to file, returns the number of bytes written or -1 on error */
int
g_file_write(int fd, const char *ptr, int len)
{
#if defined(_WIN32)

    if (WriteFile((HANDLE)fd, (LPVOID)ptr, (DWORD)len, (LPDWORD)&len, 0))
    {
        return len;
    }
    else
    {
        return -1;
    }

#else
    return write(fd, ptr, len);
#endif
}

/*****************************************************************************/
/* move file pointer, returns offset on success, -1 on failure */
int
g_file_seek(int fd, int offset)
{
#if defined(_WIN32)
    int rv;

    rv = (int)SetFilePointer((HANDLE)fd, offset, 0, FILE_BEGIN);

    if (rv == (int)INVALID_SET_FILE_POINTER)
    {
        return -1;
    }
    else
    {
        return rv;
    }

#else
    return (int)lseek(fd, offset, SEEK_SET);
#endif
}

/*****************************************************************************/
/* do a write lock on a file */
/* return boolean */
int
g_file_lock(int fd, int start, int len)
{
#if defined(_WIN32)
    return LockFile((HANDLE)fd, start, 0, len, 0);
#else
    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = len;

    if (fcntl(fd, F_SETLK, &lock) == -1)
    {
        return 0;
    }

    return 1;
#endif
}

/*****************************************************************************/
/* returns error */
int
g_chmod_hex(const char *filename, int flags)
{
#if defined(_WIN32)
    return 0;
#else
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
#endif
}

/*****************************************************************************/
/* returns error, zero is ok */
int
g_chown(const char *name, int uid, int gid)
{
    return chown(name, uid, gid);
}

/*****************************************************************************/
/* returns error, always zero */
int
g_mkdir(const char *dirname)
{
#if defined(_WIN32)
    return 0;
#else
    return mkdir(dirname, S_IRWXU);
#endif
}

/*****************************************************************************/
/* gets the current working directory and puts up to maxlen chars in
   dirname
   always returns 0 */
char *
g_get_current_dir(char *dirname, int maxlen)
{
#if defined(_WIN32)
    GetCurrentDirectoryA(maxlen, dirname);
    return 0;
#else

    if (getcwd(dirname, maxlen) == 0)
    {
    }

    return 0;
#endif
}

/*****************************************************************************/
/* returns error, zero on success and -1 on failure */
int
g_set_current_dir(const char *dirname)
{
#if defined(_WIN32)

    if (SetCurrentDirectoryA(dirname))
    {
        return 0;
    }
    else
    {
        return -1;
    }

#else
    return chdir(dirname);
#endif
}

/*****************************************************************************/
/* returns boolean, non zero if the file exists */
int
g_file_exist(const char *filename)
{
#if defined(_WIN32)
    return 0; // use FileAge(filename) <> -1
#else
    return access(filename, F_OK) == 0;
#endif
}

/*****************************************************************************/
/* returns boolean, non zero if the file is readable */
int
g_file_readable(const char *filename)
{
#if defined(_WIN32)
    return _waccess(filename, 04) == 0;
#else
    return access(filename, R_OK) == 0;
#endif
}

/*****************************************************************************/
/* returns boolean, non zero if the directory exists */
int
g_directory_exist(const char *dirname)
{
#if defined(_WIN32)
    return 0; // use GetFileAttributes and check return value
    // is not -1 and FILE_ATTRIBUTE_DIRECTORY bit is set
#else
    struct stat st;

    if (stat(dirname, &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }
    else
    {
        return 0;
    }

#endif
}

/*****************************************************************************/
/* returns boolean */
int
g_create_dir(const char *dirname)
{
#if defined(_WIN32)
    return CreateDirectoryA(dirname, 0); // test this
#else
    return mkdir(dirname, (mode_t) - 1) == 0;
#endif
}

/*****************************************************************************/
/* will try to create directories up to last / in name
   example /tmp/a/b/c/readme.txt will try to create /tmp/a/b/c
   returns boolean */
int
g_create_path(const char *path)
{
    char *pp;
    char *sp;
    char *copypath;
    int status;

    status = 1;
    copypath = g_strdup(path);
    pp = copypath;
    sp = strchr(pp, '/');

    while (sp != 0)
    {
        if (sp != pp)
        {
            *sp = 0;

            if (!g_directory_exist(copypath))
            {
                if (!g_create_dir(copypath))
                {
                    status = 0;
                    break;
                }
            }

            *sp = '/';
        }

        pp = sp + 1;
        sp = strchr(pp, '/');
    }

    g_free(copypath);
    return status;
}

/*****************************************************************************/
/* returns boolean */
int
g_remove_dir(const char *dirname)
{
#if defined(_WIN32)
    return RemoveDirectoryA(dirname); // test this
#else
    return rmdir(dirname) == 0;
#endif
}

/*****************************************************************************/
/* returns non zero if the file was deleted */
int
g_file_delete(const char *filename)
{
#if defined(_WIN32)
    return DeleteFileA(filename);
#else
    return unlink(filename) != -1;
#endif
}

/*****************************************************************************/
/* returns file size, -1 on error */
int
g_file_get_size(const char *filename)
{
#if defined(_WIN32)
    return -1;
#else
    struct stat st;

    if (stat(filename, &st) == 0)
    {
        return (int)(st.st_size);
    }
    else
    {
        return -1;
    }

#endif
}

/*****************************************************************************/
long
g_load_library(char *in)
{
#if defined(_WIN32)
    return (long)LoadLibraryA(in);
#else
    return (long)dlopen(in, RTLD_LOCAL | RTLD_LAZY);
#endif
}

/*****************************************************************************/
int
g_free_library(long lib)
{
    if (lib == 0)
    {
        return 0;
    }

#if defined(_WIN32)
    return FreeLibrary((HMODULE)lib);
#else
    return dlclose((void *)lib);
#endif
}

/*****************************************************************************/
/* returns NULL if not found */
void *
g_get_proc_address(long lib, const char *name)
{
    if (lib == 0)
    {
        return 0;
    }

#if defined(_WIN32)
    return GetProcAddress((HMODULE)lib, name);
#else
    return dlsym((void *)lib, name);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_system(char *aexec)
{
#if defined(_WIN32)
    return 0;
#else
    return system(aexec);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
char *
g_get_strerror(void)
{
#if defined(_WIN32)
    return 0;
#else
    return strerror(errno);
#endif
}

/*****************************************************************************/
int
g_get_errno(void)
{
#if defined(_WIN32)
    return GetLastError();
#else
    return errno;
#endif
}

/*****************************************************************************/
/* does not work in win32 */

#define ARGS_STR_LEN 1024

int
g_execvp(const char *p1, char *args[])
{
#if defined(_WIN32)
    return 0;
#else

    int rv;
    char args_str[ARGS_STR_LEN];
    int args_len;

    args_len = 0;
    while (args[args_len] != NULL)
    {
        args_len++;
    }

    g_strnjoin(args_str, ARGS_STR_LEN, " ", (const char **) args, args_len);

    LOG(LOG_LEVEL_DEBUG,
        "Calling exec (excutable: %s, arguments: %s)",
        p1, args_str);

    g_rm_temp_dir();
    rv = execvp(p1, args);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR,
        "Error calling exec (excutable: %s, arguments: %s) "
        "returned errno: %d, description: %s",
        p1, args_str, g_get_errno(), g_get_strerror());

    g_mk_socket_path(0);
    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_execlp3(const char *a1, const char *a2, const char *a3)
{
#if defined(_WIN32)
    return 0;
#else
    int rv;
    const char *args[] = {a2, a3, NULL};
    char args_str[ARGS_STR_LEN];

    g_strnjoin(args_str, ARGS_STR_LEN, " ", args, 2);

    LOG(LOG_LEVEL_DEBUG,
        "Calling exec (excutable: %s, arguments: %s)",
        a1, args_str);

    g_rm_temp_dir();
    rv = execlp(a1, a2, a3, (void *)0);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR,
        "Error calling exec (excutable: %s, arguments: %s) "
        "returned errno: %d, description: %s",
        a1, args_str, g_get_errno(), g_get_strerror());

    g_mk_socket_path(0);
    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_child_stop(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGCHLD, func);
#endif
}

/*****************************************************************************/

void
g_signal_segfault(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGSEGV, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_hang_up(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGHUP, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_user_interrupt(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGINT, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_terminate(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGTERM, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_pipe(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGPIPE, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_usr1(void (*func)(int))
{
#if defined(_WIN32)
#else
    signal(SIGUSR1, func);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_fork(void)
{
#if defined(_WIN32)
    return 0;
#else
    int rv;

    rv = fork();

    if (rv == 0) /* child */
    {
        g_mk_socket_path(0);
    }
    else if (rv == -1) /* error */
    {
        LOG(LOG_LEVEL_ERROR,
            "Process fork failed with errno: %d, description: %s",
            g_get_errno(), g_get_strerror());
    }

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_setgid(int pid)
{
#if defined(_WIN32)
    return 0;
#else
    return setgid(pid);
#endif
}

/*****************************************************************************/
/* returns error, zero is success, non zero is error */
/* does not work in win32 */
int
g_initgroups(const char *user, int gid)
{
#if defined(_WIN32)
    return 0;
#else
    return initgroups(user, gid);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
/* returns user id */
int
g_getuid(void)
{
#if defined(_WIN32)
    return 0;
#else
    return getuid();
#endif
}

/*****************************************************************************/
/* does not work in win32 */
/* returns user id */
int
g_getgid(void)
{
#if defined(_WIN32)
    return 0;
#else
    return getgid();
#endif
}

/*****************************************************************************/
/* does not work in win32 */
/* On success, zero is returned. On error, -1 is returned */
int
g_setuid(int pid)
{
#if defined(_WIN32)
    return 0;
#else
    return setuid(pid);
#endif
}

/*****************************************************************************/
int
g_setsid(void)
{
#if defined(_WIN32)
    return -1;
#else
    return setsid();
#endif
}

/*****************************************************************************/
int
g_getlogin(char *name, unsigned int len)
{
#if defined(_WIN32)
    return -1;
#else
    return getlogin_r(name, len);
#endif
}

/*****************************************************************************/
int
g_setlogin(const char *name)
{
#ifdef BSD
    return setlogin(name);
#else
    return -1;
#endif
}

/*****************************************************************************/
/* does not work in win32
   returns pid of process that exits or zero if signal occurred */
int
g_waitchild(void)
{
#if defined(_WIN32)
    return 0;
#else
    int wstat;
    int rv;

    rv = waitpid(0, &wstat, WNOHANG);

    if (rv == -1)
    {
        if (errno == EINTR) /* signal occurred */
        {
            rv = 0;
        }
    }

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32
   returns pid of process that exits or zero if signal occurred */
int
g_waitpid(int pid)
{
#if defined(_WIN32)
    return 0;
#else
    int rv = 0;

    if (pid < 0)
    {
        rv = -1;
    }
    else
    {
        rv = waitpid(pid, 0, 0);

        if (rv == -1)
        {
            if (errno == EINTR) /* signal occurred */
            {
                rv = 0;
            }
        }
    }

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32
   returns exit status code of child process with pid */
struct exit_status
g_waitpid_status(int pid)
{
    struct exit_status exit_status;

#if defined(_WIN32)
    exit_status.exit_code = -1;
    exit_status.signal_no = 0;
    return exit_status;
#else
    int rv;
    int status;

    exit_status.exit_code = -1;
    exit_status.signal_no = 0;

    if (pid > 0)
    {
        LOG(LOG_LEVEL_DEBUG, "waiting for pid %d to exit", pid);
        rv = waitpid(pid, &status, 0);

        if (rv != -1)
        {
            if (WIFEXITED(status))
            {
                exit_status.exit_code = WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status))
            {
                exit_status.signal_no = WTERMSIG(status);
            }
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "wait for pid %d returned unknown result", pid);
        }
    }

    return exit_status;
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_clearenv(void)
{
#if defined(_WIN32)
#else
#if defined(BSD)
    environ[0] = 0;
#else
    environ = 0;
#endif
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_setenv(const char *name, const char *value, int rewrite)
{
#if defined(_WIN32)
    return 0;
#else
    return setenv(name, value, rewrite);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
char *
g_getenv(const char *name)
{
#if defined(_WIN32)
    return 0;
#else
    return getenv(name);
#endif
}

/*****************************************************************************/
int
g_exit(int exit_code)
{
    exit(exit_code);
    return 0;
}

/*****************************************************************************/
int
g_getpid(void)
{
#if defined(_WIN32)
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

/*****************************************************************************/
/* does not work in win32 */
int
g_sigterm(int pid)
{
#if defined(_WIN32)
    return 0;
#else
    return kill(pid, SIGTERM);
#endif
}

/*****************************************************************************/
/* returns 0 if ok */
/* the caller is responsible to free the buffs */
/* does not work in win32 */
int
g_getuser_info(const char *username, int *gid, int *uid, char **shell,
               char **dir, char **gecos)
{
#if defined(_WIN32)
    return 1;
#else
    struct passwd *pwd_1;

    pwd_1 = getpwnam(username);

    if (pwd_1 != 0)
    {
        if (gid != 0)
        {
            *gid = pwd_1->pw_gid;
        }

        if (uid != 0)
        {
            *uid = pwd_1->pw_uid;
        }

        if (dir != 0)
        {
            *dir = g_strdup(pwd_1->pw_dir);
        }

        if (shell != 0)
        {
            *shell = g_strdup(pwd_1->pw_shell);
        }

        if (gecos != 0)
        {
            *gecos = g_strdup(pwd_1->pw_gecos);
        }

        return 0;
    }

    return 1;
#endif
}

/*****************************************************************************/
/* returns 0 if ok */
/* does not work in win32 */
int
g_getgroup_info(const char *groupname, int *gid)
{
#if defined(_WIN32)
    return 1;
#else
    struct group *g;

    g = getgrnam(groupname);

    if (g != 0)
    {
        if (gid != 0)
        {
            *gid = g->gr_gid;
        }

        return 0;
    }

    return 1;
#endif
}

/*****************************************************************************/
/* returns error */
/* if zero is returned, then ok is set */
/* does not work in win32 */
int
g_check_user_in_group(const char *username, int gid, int *ok)
{
#if defined(_WIN32)
    return 1;
#else
    struct group *groups;
    int i;

    groups = getgrgid(gid);

    if (groups == 0)
    {
        return 1;
    }

    *ok = 0;
    i = 0;

    while (0 != groups->gr_mem[i])
    {
        if (0 == g_strcmp(groups->gr_mem[i], username))
        {
            *ok = 1;
            break;
        }

        i++;
    }

    return 0;
#endif
}

/*****************************************************************************/
/* returns the time since the Epoch (00:00:00 UTC, January 1, 1970),
   measured in seconds.
   for windows, returns the number of seconds since the machine was
   started. */
int
g_time1(void)
{
#if defined(_WIN32)
    return GetTickCount() / 1000;
#else
    return time(0);
#endif
}

/*****************************************************************************/
/* returns the number of milliseconds since the machine was
   started. */
int
g_time2(void)
{
#if defined(_WIN32)
    return (int)GetTickCount();
#else
    struct tms tm;
    clock_t num_ticks = 0;
    g_memset(&tm, 0, sizeof(struct tms));
    num_ticks = times(&tm);
    return (int)(num_ticks * 10);
#endif
}

/*****************************************************************************/
/* returns time in milliseconds, uses gettimeofday
   does not work in win32 */
int
g_time3(void)
{
#if defined(_WIN32)
    return 0;
#else
    struct timeval tp;

    gettimeofday(&tp, 0);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
#endif
}

/******************************************************************************/
/******************************************************************************/
struct bmp_magic
{
    char magic[2];
};

struct bmp_hdr
{
    unsigned int   size;        /* file size in bytes */
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int   offset;      /* offset to image data, in bytes */
};

struct dib_hdr
{
    unsigned int   hdr_size;
    int            width;
    int            height;
    unsigned short nplanes;
    unsigned short bpp;
    unsigned int   compress_type;
    unsigned int   image_size;
    int            hres;
    int            vres;
    unsigned int   ncolors;
    unsigned int   nimpcolors;
};

/******************************************************************************/
int
g_save_to_bmp(const char *filename, char *data, int stride_bytes,
              int width, int height, int depth, int bits_per_pixel)
{
    struct bmp_magic bm;
    struct bmp_hdr bh;
    struct dib_hdr dh;
    int bytes;
    int fd;
    int index;
    int i1;
    int pixel;
    int extra;
    int file_stride_bytes;
    char *line;
    char *line_ptr;

    if ((depth == 24) && (bits_per_pixel == 32))
    {
    }
    else if ((depth == 32) && (bits_per_pixel == 32))
    {
    }
    else
    {
        LOG(LOG_LEVEL_ERROR,
            "g_save_to_bpp: unimplemented for: depth %d, bits_per_pixel %d",
            depth, bits_per_pixel);
        return 1;
    }
    bm.magic[0] = 'B';
    bm.magic[1] = 'M';

    /* scan lines are 32 bit aligned, bottom 2 bits must be zero */
    file_stride_bytes = width * ((depth + 7) / 8);
    extra = file_stride_bytes;
    extra = extra & 3;
    extra = (4 - extra) & 3;
    file_stride_bytes += extra;

    bh.size = sizeof(bm) + sizeof(bh) + sizeof(dh) + height * file_stride_bytes;
    bh.reserved1 = 0;
    bh.reserved2 = 0;
    bh.offset = sizeof(bm) + sizeof(bh) + sizeof(dh);

    dh.hdr_size = sizeof(dh);
    dh.width = width;
    dh.height = height;
    dh.nplanes = 1;
    dh.bpp = depth;
    dh.compress_type = 0;
    dh.image_size = height * file_stride_bytes;
    dh.hres = 0xb13;
    dh.vres = 0xb13;
    dh.ncolors = 0;
    dh.nimpcolors = 0;

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: open error");
        return 1;
    }
    bytes = write(fd, &bm, sizeof(bm));
    if (bytes != sizeof(bm))
    {
        LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: write error");
    }
    bytes = write(fd, &bh, sizeof(bh));
    if (bytes != sizeof(bh))
    {
        LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: write error");
    }
    bytes = write(fd, &dh, sizeof(dh));
    if (bytes != sizeof(dh))
    {
        LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: write error");
    }
    data += stride_bytes * height;
    data -= stride_bytes;
    if ((depth == 24) && (bits_per_pixel == 32))
    {
        line = (char *) malloc(file_stride_bytes);
        memset(line, 0, file_stride_bytes);
        for (index = 0; index < height; index++)
        {
            line_ptr = line;
            for (i1 = 0; i1 < width; i1++)
            {
                pixel = ((int *)data)[i1];
                *(line_ptr++) = (pixel >>  0) & 0xff;
                *(line_ptr++) = (pixel >>  8) & 0xff;
                *(line_ptr++) = (pixel >> 16) & 0xff;
            }
            bytes = write(fd, line, file_stride_bytes);
            if (bytes != file_stride_bytes)
            {
                LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: write error");
            }
            data -= stride_bytes;
        }
        free(line);
    }
    else if (depth == bits_per_pixel)
    {
        for (index = 0; index < height; index++)
        {
            bytes = write(fd, data, width * (bits_per_pixel / 8));
            if (bytes != width * (bits_per_pixel / 8))
            {
                LOG(LOG_LEVEL_ERROR, "g_save_to_bpp: write error");
            }
            data -= stride_bytes;
        }
    }
    else
    {
        LOG(LOG_LEVEL_ERROR,
            "g_save_to_bpp: unimplemented for: depth %d, bits_per_pixel %d",
            depth, bits_per_pixel);
    }
    close(fd);
    return 0;
}

/*****************************************************************************/
/* returns pointer or nil on error */
void *
g_shmat(int shmid)
{
#if defined(_WIN32)
    return 0;
#else
    return shmat(shmid, 0, 0);
#endif
}

/*****************************************************************************/
/* returns -1 on error 0 on success */
int
g_shmdt(const void *shmaddr)
{
#if defined(_WIN32)
    return -1;
#else
    return shmdt(shmaddr);
#endif
}

/*****************************************************************************/
/* returns -1 on error 0 on success */
int
g_gethostname(char *name, int len)
{
    return gethostname(name, len);
}

static unsigned char g_reverse_byte[0x100] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*****************************************************************************/
/* mirror each byte while copying */
int
g_mirror_memcpy(void *dst, const void *src, int len)
{
    tui8 *dst8;
    const tui8 *src8;

    dst8 = (tui8 *) dst;
    src8 = (const tui8 *) src;
    while (len > 0)
    {
        *dst8 = g_reverse_byte[*src8];
        dst8++;
        src8++;
        len--;
    }
    return 0;
}

/*****************************************************************************/
int
g_tcp4_socket(void)
{
#if defined(XRDP_ENABLE_IPV6ONLY)
    return -1;
#else
    int rv;
    int option_value;
    socklen_t option_len;

    rv = socket(AF_INET, SOCK_STREAM, 0);
    if (rv < 0)
    {
        return -1;
    }
    option_len = sizeof(option_value);
    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR,
                   (char *) &option_value, &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            if (setsockopt(rv, SOL_SOCKET, SO_REUSEADDR,
                           (char *) &option_value, option_len) < 0)
            {
            }
        }
    }
    return rv;
#endif
}

/*****************************************************************************/
int
g_tcp4_bind_address(int sck, const char *port, const char *address)
{
#if defined(XRDP_ENABLE_IPV6ONLY)
    return -1;
#else
    struct sockaddr_in s;

    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(INADDR_ANY);
    s.sin_port = htons((uint16_t) atoi(port));
    if (inet_aton(address, &s.sin_addr) < 0)
    {
        return -1; /* bad address */
    }
    if (bind(sck, (struct sockaddr *) &s, sizeof(s)) < 0)
    {
        return -1;
    }
    return 0;
#endif
}

/*****************************************************************************/
int
g_tcp6_socket(void)
{
#if defined(XRDP_ENABLE_IPV6)
    int rv;
    int option_value;
    socklen_t option_len;

    rv = socket(AF_INET6, SOCK_STREAM, 0);
    if (rv < 0)
    {
        return -1;
    }
    option_len = sizeof(option_value);
    if (getsockopt(rv, IPPROTO_IPV6, IPV6_V6ONLY,
                   (char *) &option_value, &option_len) == 0)
    {
#if defined(XRDP_ENABLE_IPV6ONLY)
        if (option_value == 0)
        {
            option_value = 1;
#else
        if (option_value != 0)
        {
            option_value = 0;
#endif
            option_len = sizeof(option_value);
            if (setsockopt(rv, IPPROTO_IPV6, IPV6_V6ONLY,
                           (char *) &option_value, option_len) < 0)
            {
            }
        }
    }
    option_len = sizeof(option_value);
    if (getsockopt(rv, SOL_SOCKET, SO_REUSEADDR,
                   (char *) &option_value, &option_len) == 0)
    {
        if (option_value == 0)
        {
            option_value = 1;
            option_len = sizeof(option_value);
            if (setsockopt(rv, SOL_SOCKET, SO_REUSEADDR,
                           (char *) &option_value, option_len) < 0)
            {
            }
        }
    }
    return rv;
#else
    return -1;
#endif
}

/*****************************************************************************/
int
g_tcp6_bind_address(int sck, const char *port, const char *address)
{
#if defined(XRDP_ENABLE_IPV6)
    int rv;
    int error;
    struct addrinfo hints;
    struct addrinfo *list;
    struct addrinfo *i;

    rv = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = 0;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    error = getaddrinfo(address, port, &hints, &list);
    if (error == 0)
    {
        i = list;
        while ((i != NULL) && (rv < 0))
        {
            rv = bind(sck, i->ai_addr, i->ai_addrlen);
            i = i->ai_next;
        }
        freeaddrinfo(list);
    }
    else
    {
        return -1;
    }
    return rv;
#else
    return -1;
#endif
}
