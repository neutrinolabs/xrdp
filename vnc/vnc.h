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

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004

   libvnc

*/

/* check endianess */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define L_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
#define B_ENDIAN
#endif
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__)
#define NEED_ALIGN
#endif
/* include other h files */
#include "parse.h"

#ifdef _WIN32
#define THREAD_RV unsigned long
#define THREAD_CC __stdcall
#else
#define THREAD_RV void*
#define THREAD_CC
#endif

void rfbEncryptBytes(unsigned char *bytes, char *passwd);

/* os_calls.c */
int g_init_system(void);
int g_exit_system(void);
void g_printf(char *format, ...);
void g_hexdump(char* p, int len);
void* g_malloc(int size, int zero);
void* g_malloc1(int size, int zero);
void g_free(void* ptr);
void g_free1(void* ptr);
void g_memset(void* ptr, int val, int size);
void g_memcpy(void* d_ptr, const void* s_ptr, int size);
int g_getchar(void);
int g_tcp_socket(void);
int g_tcp_local_socket(void);
void g_tcp_close(int sck);
int g_tcp_connect(int sck, char* address, char* port);
int g_tcp_force_send(int sck, char* data, int len);
int g_tcp_force_recv(int sck, char* data, int len);
int g_tcp_set_non_blocking(int sck);
int g_tcp_bind(int sck, char* port);
int g_tcp_local_bind(int sck, char* port);
int g_tcp_listen(int sck);
int g_tcp_accept(int sck);
int g_tcp_recv(int sck, void* ptr, int len, int flags);
int g_tcp_send(int sck, void* ptr, int len, int flags);
int g_tcp_last_error_would_block(int sck);
int g_tcp_select(int sck1, int sck2);
int g_is_term(void);
void g_set_term(int in_val);
void g_sleep(int msecs);
int g_thread_create(THREAD_RV (THREAD_CC * start_routine)(void*), void* arg);
void* g_rc4_info_create(void);
void g_rc4_info_delete(void* rc4_info);
void g_rc4_set_key(void* rc4_info, char* key, int len);
void g_rc4_crypt(void* rc4_info, char* data, int len);
void* g_sha1_info_create(void);
void g_sha1_info_delete(void* sha1_info);
void g_sha1_clear(void* sha1_info);
void g_sha1_transform(void* sha1_info, char* data, int len);
void g_sha1_complete(void* sha1_info, char* data);
void* g_md5_info_create(void);
void g_md5_info_delete(void* md5_info);
void g_md5_clear(void* md5_info);
void g_md5_transform(void* md5_info, char* data, int len);
void g_md5_complete(void* md5_info, char* data);
int g_mod_exp(char* out, char* in, char* mod, char* exp);
void g_random(char* data, int len);
int g_abs(int i);
int g_memcmp(void* s1, void* s2, int len);
int g_file_open(char* file_name);
int g_file_close(int fd);
int g_file_read(int fd, char* ptr, int len);
int g_file_write(int fd, char* ptr, int len);
int g_file_seek(int fd, int offset);
int g_file_lock(int fd, int start, int len);
int g_strlen(char* text);
char* g_strcpy(char* dest, char* src);
char* g_strncpy(char* dest, char* src, int len);
char* g_strcat(char* dest, char* src);
char* g_strdup(char* in);
int g_load_library(char* in);
int g_free_library(int lib);
void* g_get_proc_address(int lib, char* name);

#define COLOR16(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

struct vnc
{
  int size; /* size of this struct */
  /* client functions */
  int (*mod_start)(int handle, int w, int h, int bpp);
  int (*mod_connect)(int handle, char* ip, char* port,
                     char* username, char* password);
  int (*mod_event)(int handle, int msg, int param1, int param2);
  int (*mod_signal)(int handle);
  int (*mod_invalidate)(int handle, int x, int y, int cx, int cy);
  int d1[95];
  /* server functions */
  int (*server_begin_update)(int handle);
  int (*server_end_update)(int handle);
  int (*server_fill_rect)(int handle, int x, int y, int cx, int cy,
                          int color);
  int (*server_screen_blt)(int handle, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(int handle, int x, int y, int cx, int cy,
                           char* data);
  int (*server_set_cursor)(int handle, int x, int y, char* data, char* mask);
  int (*server_palette)(int handle, int* palette);
  int (*server_error_popup)(int handle, char* error, char* caption);
  int d2[92];
  /* common */
  int handle; /* pointer to self as int */
  int wm;
  int painter;
  int sck;
  /* mod data */
  int server_width;
  int server_height;
  int server_bpp;
  int mod_width;
  int mod_height;
  int mod_bpp;
  char mod_name[256];
  int mod_mouse_state;
  int palette[256];
};
