/*
   Copyright (c) 2004-2006 Jay Sorg

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

void*
g_malloc(int size, int zero);
void
g_free(void* ptr);
void
g_printf(const char *format, ...);
void
g_sprintf(char* dest, const char* format, ...);
void
g_writeln(const char* format, ...);
void
g_write(const char* format, ...);
void
g_hexdump(char* p, int len);
void
g_memset(void* ptr, int val, int size);
void
g_memcpy(void* d_ptr, const void* s_ptr, int size);
int
g_getchar(void);
int
g_tcp_set_no_delay(int sck);
int
g_tcp_socket(void);
int
g_tcp_local_socket(void);
void
g_tcp_close(int sck);
int
g_tcp_connect(int sck, const char* address, const char* port);
int
g_tcp_force_send(int sck, char* data, int len);
int
g_tcp_force_recv(int sck, char* data, int len);
int
g_tcp_set_non_blocking(int sck);
int
g_tcp_bind(int sck, char* port);
int
g_tcp_local_bind(int sck, char* port);
int
g_tcp_listen(int sck);
int
g_tcp_accept(int sck);
int
g_tcp_recv(int sck, void* ptr, int len, int flags);
int
g_tcp_send(int sck, const void* ptr, int len, int flags);
int
g_tcp_last_error_would_block(int sck);
int
g_tcp_select(int sck1, int sck2);
void
g_sleep(int msecs);
void
g_random(char* data, int len);
int
g_abs(int i);
int
g_memcmp(const void* s1, const void* s2, int len);
int
g_file_open(const char* file_name);
int
g_file_close(int fd);
int
g_file_read(int fd, char* ptr, int len);
int
g_file_write(int fd, char* ptr, int len);
int
g_file_seek(int fd, int offset);
int
g_file_lock(int fd, int start, int len);
int
g_set_file_rights(const char* filename, int read, int write);
int
g_chmod_hex(const char* filename, int flags);
int
g_mkdir(const char* dirname);
char*
g_get_current_dir(char* dirname, int maxlen);
int
g_set_current_dir(char* dirname);
int
g_file_exist(const char* filename);
int
g_directory_exist(const char* dirname);
int
g_create_dir(const char* dirname);
int
g_remove_dir(const char* dirname);
int
g_file_delete(const char* filename);
int
g_strlen(const char* text);
char*
g_strcpy(char* dest, const char* src);
char*
g_strncpy(char* dest, const char* src, int len);
char*
g_strcat(char* dest, const char* src);
char*
g_strdup(const char* in);
int
g_strcmp(const char* c1, const char* c2);
int
g_strncmp(const char* c1, const char* c2, int len);
int
g_strcasecmp(const char* c1, const char* c2);
int
g_strncasecmp(const char* c1, const char* c2, int len);
int
g_atoi(char* str);
int
g_pos(char* str, const char* to_find);
long
g_load_library(char* in);
int
g_free_library(long lib);
void*
g_get_proc_address(long lib, const char* name);
int
g_system(char* aexec);
char*
g_get_strerror(void);
int
g_execvp(const char* p1, char* args[]);
int
g_execlp3(const char* a1, const char* a2, const char* a3);
void
g_signal(int sig_num, void (*func)(int));
void
g_signal_child_stop(void (*func)(int));
void
g_unset_signals(void);
int
g_fork(void);
int
g_setgid(int pid);
int
g_initgroups(const char* user, int gid);
int
g_setuid(int pid);
int
g_waitchild(void);
int
g_waitpid(int pid);
void
g_clearenv(void);
int
g_setenv(const char* name, const char* value, int rewrite);
char*
g_getenv(const char* name);
int
g_exit(int exit_code);
int
g_getpid(void);
int
g_sigterm(int pid);
int
g_getuser_info(const char* username, int* gid, int* uid, char* shell,
               char* dir, char* gecos);
int
g_getgroup_info(const char* groupname, int* gid);
int
g_check_user_in_group(const char* username, int gid, int* ok);
int
g_time1(void);

#endif
