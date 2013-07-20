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

#ifndef __RDPMISC_H
#define __RDPMISC_H

int
rdpBitsPerPixel(int depth);
int
g_tcp_recv(int sck, void *ptr, int len, int flags);
void
g_tcp_close(int sck);
int
g_tcp_last_error_would_block(int sck);
void
g_sleep(int msecs);
int
g_tcp_send(int sck, void *ptr, int len, int flags);
void *
g_malloc(int size, int zero);
void
g_free(void *ptr);
void
g_sprintf(char *dest, char *format, ...);
int
g_tcp_socket(void);
int
g_tcp_local_socket_dgram(void);
int
g_tcp_local_socket_stream(void);
void
g_memcpy(void *d_ptr, const void *s_ptr, int size);
void
g_memset(void *d_ptr, const unsigned char chr, int size);
int
g_tcp_set_no_delay(int sck);
int
g_tcp_set_non_blocking(int sck);
int
g_tcp_accept(int sck);
int
g_tcp_select(int sck1, int sck2, int sck3);
int
g_tcp_bind(int sck, char *port);
int
g_tcp_local_bind(int sck, char *port);
int
g_tcp_listen(int sck);
int
g_create_dir(const char *dirname);
int
g_directory_exist(const char *dirname);
int
g_chmod_hex(const char *filename, int flags);
void
g_hexdump(unsigned char *p, unsigned int len);

#endif
