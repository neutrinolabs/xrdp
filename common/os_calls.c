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
#if defined(__linux__)
#include <linux/vm_sockets.h>
#elif defined(__FreeBSD__)
// sockaddr_hvs is not available outside the kernel for whatever reason
struct sockaddr_hvs
{
    unsigned char sa_len;
    sa_family_t   sa_family;
    unsigned int  hvs_port;
    unsigned char hvs_zero[sizeof(struct sockaddr) -  sizeof(sa_family_t) - sizeof(unsigned char) - sizeof(unsigned int)];
};
#endif
#endif
#include <poll.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif
#include <sys/mman.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>
#endif
#ifdef HAVE_SETUSERCONTEXT
#include <login_cap.h>
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
#include "limits.h"
#include "string_calls.h"
#include "log.h"
#include "xrdp_constants.h"

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

/**
 * Type big enough to hold socket address information for any connecting type
 */
union sock_info
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
#if defined(XRDP_ENABLE_IPV6)
    struct sockaddr_in6 sa_in6;
#endif
    struct sockaddr_un sa_un;
#if defined(XRDP_ENABLE_VSOCK)
#if defined(__linux__)
    struct sockaddr_vm sa_vm;
#elif defined(__FreeBSD__)
    struct sockaddr_hvs sa_hvs;
#endif
#endif
};

/*****************************************************************************/
int
g_rm_temp_dir(void)
{
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
            case EPROTONOSUPPORT: /* if IPv6 is supported, but don't have an IPv6 address */
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
g_sck_local_socketpair(int sck[2])
{
#if defined(_WIN32)
    return -1;
#else
    return socketpair(PF_LOCAL, SOCK_STREAM, 0, sck);
#endif
}

/*****************************************************************************/
int
g_sck_vsock_socket(void)
{
#if defined(XRDP_ENABLE_VSOCK)
#if defined(__linux__)
    LOG(LOG_LEVEL_DEBUG, "g_sck_vsock_socket: returning Linux vsock socket");
    return socket(PF_VSOCK, SOCK_STREAM, 0);
#elif defined(__FreeBSD__)
    LOG(LOG_LEVEL_DEBUG, "g_sck_vsock_socket: returning FreeBSD Hyper-V socket");
    return socket(AF_HYPERV, SOCK_STREAM, 0); // docs say to use AF_HYPERV here - PF_HYPERV does not exist
#else
    LOG(LOG_LEVEL_DEBUG, "g_sck_vsock_socket: vsock enabled at compile time, but platform is unsupported");
    return -1;
#endif
#else
    LOG(LOG_LEVEL_DEBUG, "g_sck_vsock_socket: vsock disabled at compile time");
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

    if (getsockopt(sck, SOL_LOCAL, LOCAL_PEERCRED, &xucred, &xucred_length))
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

static const char *
get_peer_description(const union sock_info *sock_info,
                     char *desc, unsigned int bytes)
{
    if (bytes > 0)
    {
        int family = sock_info->sa.sa_family;
        switch (family)
        {
            case AF_INET:
            {
                char ip[INET_ADDRSTRLEN];
                const struct sockaddr_in *sa_in = &sock_info->sa_in;
                if (inet_ntop(family, &sa_in->sin_addr,
                              ip, sizeof(ip)) != NULL)
                {
                    g_snprintf(desc, bytes, "%s:%d", ip,
                               ntohs(sa_in->sin_port));
                }
                else
                {
                    g_snprintf(desc, bytes, "<unknown AF_INET>:%d",
                               ntohs(sa_in->sin_port));
                }
                break;
            }

#if defined(XRDP_ENABLE_IPV6)
            case AF_INET6:
            {
                char ip[INET6_ADDRSTRLEN];
                const struct sockaddr_in6 *sa_in6 = &sock_info->sa_in6;
                if (inet_ntop(family, &sa_in6->sin6_addr,
                              ip, sizeof(ip)) != NULL)
                {
                    g_snprintf(desc, bytes, "[%s]:%d", ip,
                               ntohs(sa_in6->sin6_port));
                }
                else
                {
                    g_snprintf(desc, bytes, "[<unknown AF_INET6>]:%d",
                               ntohs(sa_in6->sin6_port));
                }
                break;
            }
#endif

            case AF_UNIX:
            {
                g_snprintf(desc, bytes, "AF_UNIX");
                break;
            }

#if defined(XRDP_ENABLE_VSOCK)
#if defined(__linux__)

            case AF_VSOCK:
            {
                const struct sockaddr_vm *sa_vm = &sock_info->sa_vm;

                g_snprintf(desc, bytes, "AF_VSOCK:cid=%u/port=%u",
                           sa_vm->svm_cid, sa_vm->svm_port);

                break;
            }

#elif defined(__FreeBSD__)

            case AF_HYPERV:
            {
                const struct sockaddr_hvs *sa_hvs = &sock_info->sa_hvs;

                g_snprintf(desc, bytes, "AF_HYPERV:port=%u", sa_hvs->hvs_port);

                break;
            }

#endif
#endif
            default:
                g_snprintf(desc, bytes, "Unknown address family %d", family);
                break;
        }
    }

    return desc;
}

/*****************************************************************************/
void
g_sck_close(int sck)
{
#if defined(_WIN32)
    closesocket(sck);
#else
    char sockname[MAX_PEER_DESCSTRLEN];

    union sock_info sock_info;
    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sizeof(sock_info));

    if (getsockname(sck, &sock_info.sa, &sock_len) == 0)
    {
        get_peer_description(&sock_info, sockname, sizeof(sockname));
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
#if defined(__linux__)
    struct sockaddr_vm s;

    g_memset(&s, 0, sizeof(struct sockaddr_vm));
    s.svm_family = AF_VSOCK;
    s.svm_port = atoi(port);
    s.svm_cid = VMADDR_CID_ANY;

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_vm));
#elif defined(__FreeBSD__)
    struct sockaddr_hvs s;

    g_memset(&s, 0, sizeof(struct sockaddr_hvs));
    s.sa_family = AF_HYPERV;
    s.hvs_port = atoi(port);

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_hvs));
#else
    return -1;
#endif
#else
    return -1;
#endif
}

/*****************************************************************************/
int
g_sck_vsock_bind_address(int sck, const char *port, const char *address)
{
#if defined(XRDP_ENABLE_VSOCK)
#if defined(__linux__)
    struct sockaddr_vm s;

    g_memset(&s, 0, sizeof(struct sockaddr_vm));
    s.svm_family = AF_VSOCK;
    s.svm_port = atoi(port);
    s.svm_cid = atoi(address);

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_vm));
#elif defined(__FreeBSD__)
    struct sockaddr_hvs s;

    g_memset(&s, 0, sizeof(struct sockaddr_hvs));
    s.sa_family = AF_HYPERV;
    s.hvs_port = atoi(port);
    // channel/address currently unsupported in FreeBSD 13.

    return bind(sck, (struct sockaddr *)&s, sizeof(struct sockaddr_hvs));
#else
    return -1;
#endif
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
g_sck_accept(int sck)
{
    int ret;
    union sock_info sock_info;
    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sock_len);

    ret = accept(sck, (struct sockaddr *)&sock_info, &sock_len);

    if (ret > 0)
    {
        char description[MAX_PEER_DESCSTRLEN];
        get_peer_description(&sock_info, description, sizeof(description));
        LOG(LOG_LEVEL_INFO, "Socket %d: connection accepted from %s",
            ret, description);

    }

    return ret;
}

/*****************************************************************************/

const char *
g_sck_get_peer_ip_address(int sck,
                          char *ip, unsigned int bytes,
                          unsigned short *port)
{
    if (bytes > 0)
    {
        int ok = 0;
        union sock_info sock_info;

        socklen_t sock_len = sizeof(sock_info);
        memset(&sock_info, 0, sock_len);

        if (getpeername(sck, (struct sockaddr *)&sock_info, &sock_len) == 0)
        {
            int family = sock_info.sa.sa_family;
            switch (family)
            {
                case AF_INET:
                {
                    struct sockaddr_in *sa_in = &sock_info.sa_in;
                    if (inet_ntop(family, &sa_in->sin_addr, ip, bytes) != NULL)
                    {
                        ok = 1;
                        if (port != NULL)
                        {
                            *port = ntohs(sa_in->sin_port);
                        }
                    }
                    break;
                }

#if defined(XRDP_ENABLE_IPV6)

                case AF_INET6:
                {
                    struct sockaddr_in6 *sa_in6 = &sock_info.sa_in6;
                    if (inet_ntop(family, &sa_in6->sin6_addr, ip, bytes) != NULL)
                    {
                        ok = 1;
                        if (port != NULL)
                        {
                            *port = ntohs(sa_in6->sin6_port);
                        }
                    }
                    break;
                }

#endif
                default:
                    break;
            }
        }

        if (!ok)
        {
            ip[0] = '\0';
        }
    }

    return ip;
}

/*****************************************************************************/

const char *
g_sck_get_peer_description(int sck,
                           char *desc, unsigned int bytes)
{
    union sock_info sock_info;
    socklen_t sock_len = sizeof(sock_info);
    memset(&sock_info, 0, sock_len);

    if (getpeername(sck, (struct sockaddr *)&sock_info, &sock_len) == 0)
    {
        get_peer_description(&sock_info, desc, bytes);
    }

    return desc;
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
g_pipe(int fd[2])
{
    return pipe(fd);
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
g_sck_recv(int sck, void *ptr, unsigned int len, int flags)
{
#if defined(_WIN32)
    return recv(sck, (char *)ptr, len, flags);
#else
    return recv(sck, ptr, len, flags);
#endif
}

/*****************************************************************************/
int
g_sck_send(int sck, const void *ptr, unsigned int len, int flags)
{
#if defined(_WIN32)
    return send(sck, (const char *)ptr, len, flags);
#else
    return send(sck, ptr, len, flags);
#endif
}

/*****************************************************************************/
int
g_sck_recv_fd_set(int sck, void *ptr, unsigned int len,
                  int fds[], unsigned int maxfd,
                  unsigned int *fdcount)
{
    int rv = -1;
#if !defined(_WIN32)
    // The POSIX API gives us no way to see how much ancillary data is
    // present for recvmsg() - just use a big buffer.
    //
    // Use a union, so control_un.control is properly aligned.
    union
    {
        struct cmsghdr cm;
        unsigned char control[8192];
    } control_un;
    struct msghdr msg = {0};

    *fdcount = 0;

    /* Set up descriptor for vanilla data */
    struct iovec iov[1] = { {ptr, len} };
    msg.msg_iov = &iov[0];
    msg.msg_iovlen = 1;

    /* Add in the ancillary data buffer */
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    if ((rv = recvmsg(sck, &msg, 0)) > 0)
    {
        struct cmsghdr *cmsg;
        if ((msg.msg_flags & MSG_CTRUNC) != 0)
        {
            LOG(LOG_LEVEL_WARNING, "Ancillary data on recvmsg() was truncated");
        }

        // Iterate over the cmsghdr structures in the ancillary data
        for (cmsg = CMSG_FIRSTHDR(&msg);
                cmsg != NULL;
                cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            if (cmsg->cmsg_level == SOL_SOCKET &&
                    cmsg->cmsg_type == SCM_RIGHTS)
            {
                const unsigned char *data = CMSG_DATA(cmsg);
                unsigned int data_len = cmsg->cmsg_len - CMSG_LEN(0);

                // Check the data length doesn't point past the end of
                // control_un.control (see below). This shouldn't happen,
                // but is conceivable if the ancillary data is truncated
                // and the OS doesn't handle that properly.
                //
                // <--  (sizeof(control_un.control)   -->
                // +------------------------------------+
                // |                                    |
                // +------------------------------------+
                // ^                       ^
                // |                       | <- data_len ->
                // |                       |
                // control_un.control      data
                unsigned int max_data_len =
                    sizeof(control_un.control) - (data - control_un.control);
                if (len > max_data_len)
                {
                    len = max_data_len;
                }

                // Process all the file descriptors in the structure
                while (data_len >= sizeof(int))
                {
                    int fd;
                    memcpy(&fd, data, sizeof(int));
                    data += sizeof(int);
                    data_len -= sizeof(int);

                    if (*fdcount < maxfd)
                    {
                        fds[(*fdcount)++] = fd;
                    }
                    else
                    {
                        // No room in the user's buffer for this fd
                        close(fd);
                    }
                }
            }
        }
    }
#endif /* !WIN32 */

    return rv;
}

/*****************************************************************************/
int
g_sck_send_fd_set(int sck, const void *ptr, unsigned int len,
                  int fds[], unsigned int fdcount)
{
    int rv = -1;
#if !defined(_WIN32)
    struct msghdr msg = {0};

    /* Set up descriptor for vanilla data */
    struct iovec iov[1] = { {(void *)ptr, len} };
    msg.msg_iov = &iov[0];
    msg.msg_iovlen = 1;

    if (fdcount > 0)
    {
        unsigned int fdsize = sizeof(fds[0]) * fdcount; /* Payload size */
        /* Allocate ancillary data structure */
        msg.msg_controllen  = CMSG_SPACE(fdsize);
        msg.msg_control = (struct cmsghdr *)g_malloc(msg.msg_controllen, 1);
        if (msg.msg_control == NULL)
        {
            /* Memory allocation failure */
            LOG(LOG_LEVEL_ERROR, "Error allocating buffer for %u fds",
                fdcount);
            return -1;
        }

        /* Fill in the ancillary data structure */
        struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
        cmptr->cmsg_len = CMSG_LEN(fdsize);
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmptr), fds, fdsize);
    }

    rv = sendmsg(sck, &msg, 0);
    g_free(msg.msg_control);

#endif /* !WIN32 */

    return rv;
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
    int rv = 0;
    if (sck > 0)
    {
        struct pollfd pollfd;

        pollfd.fd = sck;
        pollfd.events = POLLOUT;
        pollfd.revents = 0;
        if (poll(&pollfd, 1, millis) > 0)
        {
            if ((pollfd.revents & POLLOUT) != 0)
            {
                rv = 1;
            }
        }
    }

    return rv;
}

/*****************************************************************************/
/* wait 'millis' milliseconds for the socket to be able to receive */
/* returns boolean */
int
g_sck_can_recv(int sck, int millis)
{
    int rv = 0;

    if (sck > 0)
    {
        struct pollfd pollfd;

        pollfd.fd = sck;
        pollfd.events = POLLIN;
        pollfd.revents = 0;
        if (poll(&pollfd, 1, millis) > 0)
        {
            if ((pollfd.revents & (POLLIN | POLLHUP)) != 0)
            {
                rv = 1;
            }
        }
    }

    return rv;
}

/*****************************************************************************/
int
g_sck_select(int sck1, int sck2)
{
    struct pollfd pollfd[2] = {0};
    int rvmask[2] = {0}; /* Output masks corresponding to fds in pollfd */

    unsigned int i = 0;
    int rv = 0;

    if (sck1 > 0)
    {
        pollfd[i].fd = sck1;
        pollfd[i].events = POLLIN;
        rvmask[i] = 1;
        ++i;
    }

    if (sck2 > 0)
    {
        pollfd[i].fd = sck2;
        pollfd[i].events = POLLIN;
        rvmask[i] = 2;
        ++i;
    }

    if (poll(pollfd, i, 0) > 0)
    {
        if ((pollfd[0].revents & (POLLIN | POLLHUP)) != 0)
        {
            rv |= rvmask[0];
        }

        if ((pollfd[1].revents & (POLLIN | POLLHUP)) != 0)
        {
            rv |= rvmask[1];
        }
    }

    return rv;
}

/*****************************************************************************/
/* returns boolean */
static int
g_fd_can_read(int fd)
{
    int rv = 0;
    if (fd > 0)
    {
        struct pollfd pollfd;

        pollfd.fd = fd;
        pollfd.events = POLLIN;
        pollfd.revents = 0;
        if (poll(&pollfd, 1, 0) > 0)
        {
            if ((pollfd.revents & (POLLIN | POLLHUP)) != 0)
            {
                rv = 1;
            }
        }
    }

    return rv;
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
    g_file_set_cloexec(fds[0], 1);
    g_file_set_cloexec(fds[1], 1);
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
#error "Win32 is no longer supported."
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
    fd = obj & USHRT_MAX;
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
#define MAX_HANDLES 256
#ifdef _WIN32
    HANDLE handles[MAX_HANDLES];
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

    if (mstimeout < 0)
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
    struct pollfd pollfd[MAX_HANDLES];
    int sck;
    int i = 0;
    unsigned int j = 0;
    int rv = 1;

    if (read_objs == NULL && rcount != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Programming error read_objs is null");
    }
    else if (write_objs == NULL && wcount != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Programming error write_objs is null");
    }
    /* Check carefully for int overflow in passed-in counts */
    else if ((unsigned int)rcount > MAX_HANDLES ||
             (unsigned int)wcount > MAX_HANDLES ||
             ((unsigned int)rcount + (unsigned int)wcount) > MAX_HANDLES)
    {
        LOG(LOG_LEVEL_ERROR, "Programming error too many handles");
    }
    else
    {
        if (mstimeout < 0)
        {
            mstimeout = -1;
        }

        for (i = 0; i < rcount ; ++i)
        {
            sck = read_objs[i] & 0xffff;
            if (sck > 0)
            {
                pollfd[j].fd = sck;
                pollfd[j].events = POLLIN;
                ++j;
            }
        }

        for (i = 0; i < wcount; ++i)
        {
            sck = write_objs[i];
            if (sck > 0)
            {
                pollfd[j].fd = sck;
                pollfd[j].events = POLLOUT;
                ++j;
            }
        }

        rv = (poll(pollfd, j, mstimeout) < 0);
        if (rv != 0)
        {
            /* these are not really errors */
            if ((errno == EAGAIN) ||
                    (errno == EWOULDBLOCK) ||
                    (errno == EINPROGRESS) ||
                    (errno == EINTR)) /* signal occurred */
            {
                rv = 0;
            }
        }
    }

    return rv;
#endif
#undef MAX_HANDLES
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
g_file_open_rw(const char *file_name)
{
#if defined(_WIN32)
    return (int)CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
#else
    return open(file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
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
/* returns -1 on error, else return handle or file descriptor */
int
g_file_open_ro(const char *file_name)
{
    return g_file_open_ex(file_name, 1, 0, 0, 0);
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
int
g_file_is_open(int fd)
{
    return (fcntl(fd, F_GETFD) >= 0);
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
/* Gets the close-on-exec flag for a file descriptor */
int
g_file_get_cloexec(int fd)
{
    int rv = 0;
    int flags = fcntl(fd, F_GETFD);
    if (flags >= 0 && (flags & FD_CLOEXEC) != 0)
    {
        rv = 1;
    }

    return rv;
}

/*****************************************************************************/
/* Sets/clears the close-on-exec flag for a file descriptor */
/* return boolean */
int
g_file_set_cloexec(int fd, int status)
{
    int rv = 0;
    int current_flags = fcntl(fd, F_GETFD);
    if (current_flags >= 0)
    {
        int new_flags;
        if (status)
        {
            new_flags = current_flags | FD_CLOEXEC;
        }
        else
        {
            new_flags = current_flags & ~FD_CLOEXEC;
        }
        if (new_flags != current_flags)
        {
            rv = (fcntl(fd, F_SETFD, new_flags) >= 0);
        }
    }

    return rv;
}

/*****************************************************************************/
struct list *
g_get_open_fds(int min, int max)
{
    struct list *result = list_create();

    if (result != NULL)
    {
        if (max < 0)
        {
            max = sysconf(_SC_OPEN_MAX);
        }

        if (max > min)
        {
            struct pollfd *fds = g_new0(struct pollfd, max - min);
            int i;

            if (fds == NULL)
            {
                goto nomem;
            }

            for (i = min ; i < max ; ++i)
            {
                fds[i - min].fd = i;
            }

            if (poll(fds, max - min, 0) >= 0)
            {
                for (i = min ; i < max ; ++i)
                {
                    if (fds[i - min].revents != POLLNVAL)
                    {
                        // Descriptor is open
                        if (!list_add_item(result, i))
                        {
                            goto nomem;
                        }
                    }
                }
            }
            g_free(fds);
        }
    }

    return result;

nomem:
    list_delete(result);
    return NULL;
}

/*****************************************************************************/
int
g_file_map(int fd, int aread, int awrite, size_t length, void **addr)
{
    int prot = 0;
    void *laddr;

    if (aread)
    {
        prot |= PROT_READ;
    }
    if (awrite)
    {
        prot |= PROT_WRITE;
    }
    laddr = mmap(NULL, length, prot, MAP_SHARED, fd, 0);
    if (laddr == MAP_FAILED)
    {
        return 1;
    }
    *addr = laddr;
    return 0;
}

/*****************************************************************************/
int
g_munmap(void *addr, size_t length)
{
    return munmap(addr, length);
}

/*****************************************************************************/
/* Converts a hex mask to a mode_t value */
#if !defined(_WIN32)
static mode_t
hex_to_mode_t(int hex)
{
    mode_t mode = 0;

    mode |= (hex & 0x4000) ? S_ISUID : 0;
    mode |= (hex & 0x2000) ? S_ISGID : 0;
    mode |= (hex & 0x1000) ? S_ISVTX : 0;
    mode |= (hex & 0x0400) ? S_IRUSR : 0;
    mode |= (hex & 0x0200) ? S_IWUSR : 0;
    mode |= (hex & 0x0100) ? S_IXUSR : 0;
    mode |= (hex & 0x0040) ? S_IRGRP : 0;
    mode |= (hex & 0x0020) ? S_IWGRP : 0;
    mode |= (hex & 0x0010) ? S_IXGRP : 0;
    mode |= (hex & 0x0004) ? S_IROTH : 0;
    mode |= (hex & 0x0002) ? S_IWOTH : 0;
    mode |= (hex & 0x0001) ? S_IXOTH : 0;
    return mode;
}
#endif

/*****************************************************************************/
/* Converts a mode_t value to a hex mask */
#if !defined(_WIN32)
static int
mode_t_to_hex(mode_t mode)
{
    int hex = 0;

    hex |= (mode & S_ISUID) ?  0x4000 : 0;
    hex |= (mode & S_ISGID) ?  0x2000 : 0;
    hex |= (mode & S_ISVTX) ?  0x1000 : 0;
    hex |= (mode & S_IRUSR) ?  0x0400 : 0;
    hex |= (mode & S_IWUSR) ?  0x0200 : 0;
    hex |= (mode & S_IXUSR) ?  0x0100 : 0;
    hex |= (mode & S_IRGRP) ?  0x0040 : 0;
    hex |= (mode & S_IWGRP) ?  0x0020 : 0;
    hex |= (mode & S_IXGRP) ?  0x0010 : 0;
    hex |= (mode & S_IROTH) ?  0x0004 : 0;
    hex |= (mode & S_IWOTH) ?  0x0002 : 0;
    hex |= (mode & S_IXOTH) ?  0x0001 : 0;

    return hex;
}
#endif

/*****************************************************************************/
/* Duplicates a file descriptor onto another one using the semantics
 * of dup2() */
/* return boolean */
int
g_file_duplicate_on(int fd, int target_fd)
{
    int rv = (dup2(fd, target_fd) >= 0);

    if (rv < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't clone file %d as file %d [%s]",
            fd, target_fd, g_get_strerror());
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
int
g_chmod_hex(const char *filename, int flags)
{
#if defined(_WIN32)
    return 0;
#else
    mode_t m = hex_to_mode_t(flags);
    return chmod(filename, m);
#endif
}

/*****************************************************************************/
/* returns error */
int
g_umask_hex(int flags)
{
#if defined(_WIN32)
    return flags;
#else
    mode_t m = hex_to_mode_t(flags);
    m = umask(m);
    return mode_t_to_hex(m);
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
/* returns boolean, non zero if the file exists  and is a readable executable */
int
g_executable_exist(const char *exename)
{
    return access(exename, R_OK | X_OK) == 0;
}

/*****************************************************************************/
/* returns boolean */
int
g_create_dir(const char *dirname)
{
#if defined(_WIN32)
    return CreateDirectoryA(dirname, 0); // test this
#else
    return mkdir(dirname, 0777) == 0;
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
/* returns device number, -1 on error */
int
g_file_get_device_number(const char *filename)
{
#if defined(_WIN32)
    return -1;
#else
    struct stat st;

    if (stat(filename, &st) == 0)
    {
        return (int)(st.st_dev);
    }
    else
    {
        return -1;
    }

#endif
}

/*****************************************************************************/
/* returns inode number, -1 on error */
int
g_file_get_inode_num(const char *filename)
{
#if defined(_WIN32)
    return -1;
#else
    struct stat st;

    if (stat(filename, &st) == 0)
    {
        return (int)(st.st_ino);
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
g_system(const char *aexec)
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

    rv = execvp(p1, args);

    /* should not get here */
    int saved_errno = errno;

    LOG(LOG_LEVEL_ERROR,
        "Error calling exec (excutable: %s, arguments: %s) "
        "returned errno: %d, description: %s",
        p1, args_str, g_get_errno(), g_get_strerror());

    errno = saved_errno;
    return rv;
#endif
}

/*****************************************************************************/
int
g_execvp_list(const char *file, struct list *argv)
{
    int rv = -1;

    /* Push a terminating NULL onto the list for the system call */
    if (!list_add_item(argv, (tintptr)NULL))
    {
        LOG(LOG_LEVEL_ERROR, "No memory for exec to terminate list");
        errno = ENOMEM;
    }
    else
    {
        /* Read the argv argument straight from the list */
        rv = g_execvp(file, (char **)argv->items);

        /* should not get here */
        list_remove_item(argv, argv->count - 1); // Lose terminating NULL
    }
    return rv;
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
        "Calling exec (executable: %s, arguments: %s)",
        a1, args_str);

    g_rm_temp_dir();
    rv = execlp(a1, a2, a3, (void *)0);

    /* should not get here */
    LOG(LOG_LEVEL_ERROR,
        "Error calling exec (executable: %s, arguments: %s) "
        "returned errno: %d, description: %s",
        a1, args_str, g_get_errno(), g_get_strerror());

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32 */
unsigned int
g_set_alarm(void (*func)(int), unsigned int secs)
{
#if defined(_WIN32)
    return 0;
#else
    struct sigaction action;

    /* Cancel any previous alarm to prevent a race */
    unsigned int rv = alarm(0);

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGALRM, &action, NULL);
    if (func != NULL && secs > 0)
    {
        (void)alarm(secs);
    }
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
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        // Don't need to know when children are stopped or started
        action.sa_flags = (SA_RESTART | SA_NOCLDSTOP);
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGCHLD, &action, NULL);
#endif
}

/*****************************************************************************/

void
g_signal_segfault(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESETHAND; // This is a one-shot
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGSEGV, &action, NULL);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_hang_up(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGHUP, &action, NULL);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_user_interrupt(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_terminate(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGTERM, &action, NULL);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_pipe(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGPIPE, &action, NULL);
#endif
}

/*****************************************************************************/
/* does not work in win32 */
void
g_signal_usr1(void (*func)(int))
{
#if defined(_WIN32)
#else
    struct sigaction action;

    if (func == NULL)
    {
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
    }
    else
    {
        action.sa_handler = func;
        action.sa_flags = SA_RESTART;
    }
    sigemptyset (&action.sa_mask);

    sigaction(SIGUSR1, &action, NULL);
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

    if (rv == -1) /* error */
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
g_initgroups(const char *username)
{
#if defined(_WIN32)
    return 0;
#else
    int gid;
    int error = g_getuser_info_by_name(username, NULL, &gid, NULL, NULL, NULL);
    if (error == 0)
    {
        error = initgroups(username, gid);
    }
    return error;
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
#ifdef HAVE_SETUSERCONTEXT
int
g_set_allusercontext(int uid)
{
    int rv;
    struct passwd *pwd = getpwuid(uid);
    if (pwd == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No password entry for UID %d", uid);
        rv = 1;
    }
    else
    {
        rv = setusercontext(NULL, pwd, uid, LOGIN_SETALL);
        if (rv != 0)
        {
            LOG(LOG_LEVEL_ERROR, "setusercontext(%d) failed [%s]",
                uid, g_get_strerror());
        }
    }

    return (rv != 0);  /* Return 0 or 1 */
}
#endif
/*****************************************************************************/
/* does not work in win32
   returns pid of process that exits or zero if signal occurred
   a proc_exit_status struct can optionally be passed in to get the
   exit status of the child */
int
g_waitchild(struct proc_exit_status *e)
{
#if defined(_WIN32)
    return 0;
#else
    int wstat;
    int rv;

    struct proc_exit_status dummy;

    if (e == NULL)
    {
        e = &dummy;  // Set this, then throw it away
    }

    e->reason = E_PXR_UNEXPECTED;
    e->val = 0;

    rv = waitpid(-1, &wstat, WNOHANG);

    if (rv == -1)
    {
        if (errno == EINTR)
        {
            /* This shouldn't happen as signal handlers use SA_RESTART */
            rv = 0;
        }
    }
    else if (WIFEXITED(wstat))
    {
        e->reason = E_PXR_STATUS_CODE;
        e->val = WEXITSTATUS(wstat);
    }
    else if (WIFSIGNALED(wstat))
    {
        e->reason = E_PXR_SIGNAL;
        e->val = WTERMSIG(wstat);
    }

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32
   returns pid of process that exits or <= 0 if no process was found

   Note that signal handlers are established with BSD-style semantics,
   so this call is NOT interrupted by a signal  */
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
    }

    return rv;
#endif
}

/*****************************************************************************/
/* does not work in win32
   returns exit status code of child process with pid

   Note that signal handlers are established with BSD-style semantics,
   so this call is NOT interrupted by a signal  */
struct proc_exit_status
g_waitpid_status(int pid)
{
    struct proc_exit_status exit_status =
    {
        .reason = E_PXR_UNEXPECTED,
        .val = 0
    };

#if !defined(_WIN32)
    if (pid > 0)
    {
        int rv;
        int status;

        LOG(LOG_LEVEL_DEBUG, "waiting for pid %d to exit", pid);
        rv = waitpid(pid, &status, 0);

        if (rv != -1)
        {
            if (WIFEXITED(status))
            {
                exit_status.reason = E_PXR_STATUS_CODE;
                exit_status.val = WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status))
            {
                exit_status.reason = E_PXR_SIGNAL;
                exit_status.val = WTERMSIG(status);
            }
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "wait for pid %d returned unknown result", pid);
        }
    }

#endif
    return exit_status;
}

/*****************************************************************************/
int
g_setpgid(int pid, int pgid)
{
    int rv = setpgid(pid, pgid);
    if (rv < 0)
    {
        if (pid == 0)
        {
            pid = getpid();
        }
        LOG(LOG_LEVEL_ERROR, "Can't set process group ID of %d to %d [%s]",
            pid, pgid, g_get_strerror());
    }

    return rv;
}

/*****************************************************************************/
/* does not work in win32 */
void
g_clearenv(void)
{
#if defined(HAVE_CLEARENV)
    clearenv();
#elif defined(_WIN32)
#elif defined(BSD)
    extern char **environ;
    environ[0] = 0;
#else
    extern char **environ;
    environ = 0;
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
/* does not work in win32 */
int
g_sighup(int pid)
{
#if defined(_WIN32)
    return 0;
#else
    return kill(pid, SIGHUP);
#endif
}

/*****************************************************************************/
/* returns 0 if ok */
/* the caller is responsible to free the buffs */
/* does not work in win32 */
int
g_getuser_info_by_name(const char *username, int *uid, int *gid,
                       char **shell, char **dir, char **gecos)
{
    int rv = 1;
#if !defined(_WIN32)

    if (username == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "g_getuser_info_by_name() called for NULL user");
    }
    else
    {
        struct passwd *pwd_1 = getpwnam(username);

        if (pwd_1 != 0)
        {
            rv = 0;

            if (uid != 0)
            {
                *uid = pwd_1->pw_uid;
            }

            if (gid != 0)
            {
                *gid = pwd_1->pw_gid;
            }

            if (shell != 0)
            {
                *shell = g_strdup(pwd_1->pw_shell);
            }

            if (dir != 0)
            {
                *dir = g_strdup(pwd_1->pw_dir);
            }

            if (gecos != 0)
            {
                *gecos = g_strdup(pwd_1->pw_gecos);
            }
        }
    }
#endif
    return rv;
}


/*****************************************************************************/
/* returns 0 if ok */
/* the caller is responsible to free the buffs */
/* does not work in win32 */
int
g_getuser_info_by_uid(int uid, char **username, int *gid,
                      char **shell, char **dir, char **gecos)
{
#if defined(_WIN32)
    return 1;
#else
    struct passwd *pwd_1;

    pwd_1 = getpwuid(uid);

    if (pwd_1 != 0)
    {
        if (username != NULL)
        {
            *username = g_strdup(pwd_1->pw_name);
        }

        if (gid != 0)
        {
            *gid = pwd_1->pw_gid;
        }

        if (shell != 0)
        {
            *shell = g_strdup(pwd_1->pw_shell);
        }

        if (dir != 0)
        {
            *dir = g_strdup(pwd_1->pw_dir);
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
#ifdef HAVE_GETGROUPLIST
int
g_check_user_in_group(const char *username, int gid, int *ok)
{
    int rv = 1;
    struct passwd *pwd_1 = getpwnam(username);
    if (pwd_1 != NULL)
    {
        // Get number of groups for user
        //
        // Some implementations of getgrouplist() (i.e. muslc) don't
        // allow ngroups to be <1 on entry
        int ngroups = 1;
        GETGROUPS_T dummy;
        getgrouplist(username, pwd_1->pw_gid, &dummy, &ngroups);

        if (ngroups > 0) // Should always be true
        {
            GETGROUPS_T *grouplist;
            grouplist = (GETGROUPS_T *)malloc(ngroups * sizeof(grouplist[0]));
            if (grouplist != NULL)
            {
                // Now get the actual groups. The number of groups returned
                // by this call is not necessarily the same as the number
                // returned by the first call.
                int allocgroups = ngroups;
                getgrouplist(username, pwd_1->pw_gid, grouplist, &ngroups);
                ngroups = MIN(ngroups, allocgroups);

                rv = 0;
                *ok = 0;

                int i;
                for (i = 0 ; i < ngroups; ++i)
                {
                    if (grouplist[i] == (GETGROUPS_T)gid)
                    {
                        *ok = 1;
                        break;
                    }
                }
                free(grouplist);
            }
        }
    }
    return rv;
}
/*****************************************************************************/
#else // HAVE_GETGROUPLIST
int
g_check_user_in_group(const char *username, int gid, int *ok)
{
#if defined(_WIN32)
    return 1;
#else
    int i;

    struct passwd *pwd_1 = getpwnam(username);
    struct group *groups = getgrgid(gid);
    if (pwd_1 == NULL || groups == NULL)
    {
        return 1;
    }

    if (pwd_1->pw_gid == gid)
    {
        *ok = 1;
    }
    else
    {
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
    }

    return 0;
#endif
}
#endif // HAVE_GETGROUPLIST

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

/*****************************************************************************/
/* returns error, zero is success, non zero is error */
/* only works in linux */
int
g_no_new_privs(void)
{
#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_NO_NEW_PRIVS)
    /*
     * PR_SET_NO_NEW_PRIVS requires Linux kernel 3.5 and newer.
     */
    return prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#else
    return 0;
#endif
}

/*****************************************************************************/
void
g_qsort(void *base, size_t nitems, size_t size,
        int (*compar)(const void *, const void *))
{
    qsort(base, nitems, size, compar);
}
