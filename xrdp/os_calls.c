/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Copyright (C) Jay Sorg 2004

   generic operating system calls

*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bn.h>

static pthread_mutex_t g_term_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_term = 0;

/*****************************************************************************/
void g_printf(char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
}

/*****************************************************************************/
/* produce a hex dump */
void g_hexdump(char* p, int len)
{
  unsigned char* line;
  int i;
  int thisline;
  int offset;

  line = (unsigned char*)p;
  offset = 0;
  while (offset < len)
  {
    printf("%04x ", offset);
    thisline = len - offset;
    if (thisline > 16)
      thisline = 16;
    for (i = 0; i < thisline; i++)
      printf("%02x ", line[i]);
    for (; i < 16; i++)
      printf("   ");
    for (i = 0; i < thisline; i++)
      printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
    printf("\n");
    offset += thisline;
    line += thisline;
  }
}

/*****************************************************************************/
void* g_malloc(int size, int zero)
{
  void* rv;

  rv = malloc(size);
  if (zero)
    memset(rv, 0, size);
  return rv;
}

/*****************************************************************************/
void g_free(void* ptr)
{
  if (ptr != 0)
    free(ptr);
}

/*****************************************************************************/
void g_memset(void* ptr, int val, int size)
{
  memset(ptr, val, size);
}

/*****************************************************************************/
void g_memcpy(void* d_ptr, const void* s_ptr, int size)
{
  memcpy(d_ptr, s_ptr, size);
}

/*****************************************************************************/
int g_getchar(void)
{
  return getchar();
}

/*****************************************************************************/
int g_tcp_socket(void)
{
  return socket(PF_INET, SOCK_STREAM, 0);
}

/*****************************************************************************/
void g_tcp_close(int sck)
{
  close(sck);
}

/*****************************************************************************/
int g_tcp_set_non_blocking(int sck)
{
  int i;

  i = fcntl(sck, F_GETFL);
  i = i | O_NONBLOCK;
  fcntl(sck, F_SETFL, i);
  return 0;
}

/*****************************************************************************/
int g_tcp_bind(int sck, char* port)
{
  struct sockaddr_in s;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_port = htons(atoi(port));
  s.sin_addr.s_addr = INADDR_ANY;
  return bind(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

/*****************************************************************************/
int g_tcp_listen(int sck)
{
  return listen(sck, 2);
}

/*****************************************************************************/
int g_tcp_accept(int sck)
{
  struct sockaddr_in s;
  int i;

  i = sizeof(struct sockaddr_in);
  memset(&s, 0, i);
  return accept(sck, (struct sockaddr*)&s, (socklen_t*)&i);
}

/*****************************************************************************/
int g_tcp_recv(int sck, void* ptr, int len, int flags)
{
  return recv(sck, ptr, len, flags);
}

/*****************************************************************************/
int g_tcp_send(int sck, void* ptr, int len, int flags)
{
  return send(sck, ptr, len, flags);
}

/*****************************************************************************/
int g_tcp_last_error_would_block(int sck)
{
  return errno == EWOULDBLOCK;
}

/*****************************************************************************/
int g_tcp_select(int sck)
{
  fd_set rfds;

  FD_ZERO(&rfds);
  FD_SET(sck, &rfds);
  return select(sck + 1, &rfds, 0, 0, 0);
}

/*****************************************************************************/
int g_is_term(void)
{
  int rv;

  pthread_mutex_lock(&g_term_mutex);
  rv = g_term;
  pthread_mutex_unlock(&g_term_mutex);
  return rv;
}

/*****************************************************************************/
void g_set_term(int in_val)
{
  pthread_mutex_lock(&g_term_mutex);
  g_term = in_val;
  pthread_mutex_unlock(&g_term_mutex);
}

/*****************************************************************************/
void g_sleep(int msecs)
{
  usleep(msecs * 1000);
}

/*****************************************************************************/
int g_thread_create(void* (*start_routine)(void*), void* arg)
{
  pthread_t thread;

  return pthread_create(&thread, 0, start_routine, arg);
}

/* rc4 stuff */

/*****************************************************************************/
void* g_rc4_info_create(void)
{
  return g_malloc(sizeof(RC4_KEY), 1);;
}

/*****************************************************************************/
void g_rc4_info_delete(void* rc4_info)
{
  g_free(rc4_info);
}

/*****************************************************************************/
void g_rc4_set_key(void* rc4_info, char* key, int len)
{
  RC4_set_key((RC4_KEY*)rc4_info, len, (unsigned char*)key);
}

/*****************************************************************************/
void g_rc4_crypt(void* rc4_info, char* data, int len)
{
  RC4((RC4_KEY*)rc4_info, len, (unsigned char*)data, (unsigned char*)data);
}

/* sha1 stuff */

/*****************************************************************************/
void* g_sha1_info_create(void)
{
  return g_malloc(sizeof(SHA_CTX), 1);
}

/*****************************************************************************/
void g_sha1_info_delete(void* sha1_info)
{
  g_free(sha1_info);
}

/*****************************************************************************/
void g_sha1_clear(void* sha1_info)
{
  SHA1_Init((SHA_CTX*)sha1_info);
}

/*****************************************************************************/
void g_sha1_transform(void* sha1_info, char* data, int len)
{
  SHA1_Update((SHA_CTX*)sha1_info, data, len);
}

/*****************************************************************************/
void g_sha1_complete(void* sha1_info, char* data)
{
  SHA1_Final((unsigned char*)data, (SHA_CTX*)sha1_info);
}

/* md5 stuff */

/*****************************************************************************/
void* g_md5_info_create(void)
{
  return g_malloc(sizeof(MD5_CTX), 1);
}

/*****************************************************************************/
void g_md5_info_delete(void* md5_info)
{
  g_free(md5_info);
}

/*****************************************************************************/
void g_md5_clear(void* md5_info)
{
  MD5_Init((MD5_CTX*)md5_info);
}

/*****************************************************************************/
void g_md5_transform(void* md5_info, char* data, int len)
{
  MD5_Update((MD5_CTX*)md5_info, data, len);
}

/*****************************************************************************/
void g_md5_complete(void* md5_info, char* data)
{
  MD5_Final((unsigned char*)data, (MD5_CTX*)md5_info);
}

/*****************************************************************************/
int g_mod_exp(char* out, char* in, char* mod, char* exp)
{
  BN_CTX* ctx;
  BIGNUM lmod;
  BIGNUM lexp;
  BIGNUM lin;
  BIGNUM lout;
  int rv;

  ctx = BN_CTX_new();
  BN_init(&lmod);
  BN_init(&lexp);
  BN_init(&lin);
  BN_init(&lout);
  BN_bin2bn((unsigned char*)mod, 64, &lmod);
  BN_bin2bn((unsigned char*)exp, 64, &lexp);
  BN_bin2bn((unsigned char*)in, 64, &lin);
  BN_mod_exp(&lout, &lin, &lexp, &lmod, ctx);
  rv = BN_bn2bin(&lout, (unsigned char*)out);
  BN_free(&lin);
  BN_free(&lout);
  BN_free(&lexp);
  BN_free(&lmod);
  BN_CTX_free(ctx);
  return rv;
}

/*****************************************************************************/
void g_random(char* data, int len)
{
  int fd;

  memset(data, 0x44, len);
  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1)
    fd = open("/dev/random", O_RDONLY);
  if (fd != -1)
  {
    read(fd, data, len);
    close(fd);
  }
}
