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
   Copyright (C) Jay Sorg 2004-2005

   generic operating system calls

*/

int g_init_system(void);
int g_exit_system(void);
void g_printf(char *format, ...);
void g_sprintf(char* dest, char* format, ...);
void g_hexdump(char* p, int len);
void* g_malloc(int size, int zero);
void* g_malloc1(int size, int zero);
void g_free(void* ptr);
void g_free1(void* ptr);
void g_memset(void* ptr, int val, int size);
void g_memcpy(void* d_ptr, const void* s_ptr, int size);
int g_getchar(void);
int g_tcp_set_no_delay(int sck);
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
int g_strcmp(char* c1, char* c2);
int g_load_library(char* in);
int g_free_library(int lib);
void* g_get_proc_address(int lib, char* name);
int g_system(char* aexec);
void g_signal(int sig_num, void (*func)(int));
