/*
   Copyright (c) 2004-2007 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   generic operating system calls
*/

#if !defined(OS_CALLS_H)
#define OS_CALLS_H

#include "arch.h"

void* APP_CC
g_malloc(int size, int zero);
void APP_CC
g_free(void* ptr);
void DEFAULT_CC
g_printf(const char *format, ...);
void DEFAULT_CC
g_sprintf(char* dest, const char* format, ...);
void DEFAULT_CC
g_snprintf(char* dest, int len, const char* format, ...);
void DEFAULT_CC
g_writeln(const char* format, ...);
void DEFAULT_CC
g_write(const char* format, ...);
void APP_CC
g_hexdump(char* p, int len);
void APP_CC
g_memset(void* ptr, int val, int size);
void APP_CC
g_memcpy(void* d_ptr, const void* s_ptr, int size);
int APP_CC
g_getchar(void);
int APP_CC
g_tcp_set_no_delay(int sck);
int APP_CC
g_tcp_socket(void);
int APP_CC
g_tcp_local_socket(void);
void APP_CC
g_tcp_close(int sck);
int APP_CC
g_tcp_connect(int sck, const char* address, const char* port);
int APP_CC
g_tcp_force_send(int sck, char* data, int len);
int APP_CC
g_tcp_force_recv(int sck, char* data, int len);
int APP_CC
g_tcp_set_non_blocking(int sck);
int APP_CC
g_tcp_bind(int sck, char* port);
int APP_CC
g_tcp_local_bind(int sck, char* port);
int APP_CC
g_tcp_listen(int sck);
int APP_CC
g_tcp_accept(int sck);
int APP_CC
g_tcp_recv(int sck, void* ptr, int len, int flags);
int APP_CC
g_tcp_send(int sck, const void* ptr, int len, int flags);
int APP_CC
g_tcp_last_error_would_block(int sck);
int APP_CC
g_tcp_socket_ok(int sck);
int APP_CC
g_tcp_can_send(int sck, int millis);
int APP_CC
g_tcp_can_recv(int sck, int millis);
int APP_CC
g_tcp_select(int sck1, int sck2);
void APP_CC
g_sleep(int msecs);
void APP_CC
g_random(char* data, int len);
int APP_CC
g_abs(int i);
int APP_CC
g_memcmp(const void* s1, const void* s2, int len);
int APP_CC
g_file_open(const char* file_name);
int APP_CC
g_file_close(int fd);
int APP_CC
g_file_read(int fd, char* ptr, int len);
int APP_CC
g_file_write(int fd, char* ptr, int len);
int APP_CC
g_file_seek(int fd, int offset);
int APP_CC
g_file_lock(int fd, int start, int len);
int APP_CC
g_set_file_rights(const char* filename, int read, int write);
int APP_CC
g_chmod_hex(const char* filename, int flags);
int APP_CC
g_mkdir(const char* dirname);
char* APP_CC
g_get_current_dir(char* dirname, int maxlen);
int APP_CC
g_set_current_dir(char* dirname);
int APP_CC
g_file_exist(const char* filename);
int APP_CC
g_directory_exist(const char* dirname);
int APP_CC
g_create_dir(const char* dirname);
int APP_CC
g_remove_dir(const char* dirname);
int APP_CC
g_file_delete(const char* filename);
int APP_CC
g_strlen(const char* text);
char* APP_CC
g_strcpy(char* dest, const char* src);
char* APP_CC
g_strncpy(char* dest, const char* src, int len);
char* APP_CC
g_strcat(char* dest, const char* src);
char* APP_CC
g_strdup(const char* in);
int APP_CC
g_strcmp(const char* c1, const char* c2);
int APP_CC
g_strncmp(const char* c1, const char* c2, int len);
int APP_CC
g_strcasecmp(const char* c1, const char* c2);
int APP_CC
g_strncasecmp(const char* c1, const char* c2, int len);
int APP_CC
g_atoi(char* str);
int APP_CC
g_pos(char* str, const char* to_find);
long APP_CC
g_load_library(char* in);
int APP_CC
g_free_library(long lib);
void* APP_CC
g_get_proc_address(long lib, const char* name);
int APP_CC
g_system(char* aexec);
char* APP_CC
g_get_strerror(void);
int APP_CC
g_execvp(const char* p1, char* args[]);
int APP_CC
g_execlp3(const char* a1, const char* a2, const char* a3);
void APP_CC
g_signal(int sig_num, void (*func)(int));
void APP_CC
g_signal_child_stop(void (*func)(int));
void APP_CC
g_unset_signals(void);
int APP_CC
g_fork(void);
int APP_CC
g_setgid(int pid);
int APP_CC
g_initgroups(const char* user, int gid);
int APP_CC
g_setuid(int pid);
int APP_CC
g_waitchild(void);
int APP_CC
g_waitpid(int pid);
void APP_CC
g_clearenv(void);
int APP_CC
g_setenv(const char* name, const char* value, int rewrite);
char* APP_CC
g_getenv(const char* name);
int APP_CC
g_exit(int exit_code);
int APP_CC
g_getpid(void);
int APP_CC
g_sigterm(int pid);
int APP_CC
g_getuser_info(const char* username, int* gid, int* uid, char* shell,
               char* dir, char* gecos);
int APP_CC
g_getgroup_info(const char* groupname, int* gid);
int APP_CC
g_check_user_in_group(const char* username, int gid, int* ok);
int APP_CC
g_time1(void);

#endif
