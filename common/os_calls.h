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
 */

#if !defined(OS_CALLS_H)
#define OS_CALLS_H

#include "arch.h"

enum proc_exit_reason
{
    E_PXR_STATUS_CODE = 0, ///< 'val' contains exit status
    E_PXR_SIGNAL, ///< 'val' contains a signal number
    E_PXR_UNEXPECTED
};

struct proc_exit_status
{
    enum proc_exit_reason reason;
    int val;
};

struct list;

#define g_tcp_can_recv g_sck_can_recv
#define g_tcp_can_send g_sck_can_send
#define g_tcp_recv g_sck_recv
#define g_tcp_send g_sck_send
#define g_tcp_close g_sck_close
#define g_tcp_last_error_would_block g_sck_last_error_would_block
#define g_tcp_set_non_blocking g_sck_set_non_blocking
#define g_tcp_local_socket g_sck_local_socket
#define g_tcp_local_connect g_sck_local_connect
#define g_tcp_listen g_sck_listen
#define g_tcp_local_bind g_sck_local_bind
#define g_tcp_select g_sck_select
#define g_close_wait_obj g_delete_wait_obj

int      g_rm_temp_dir(void);
void     g_init(const char *app_name);
void     g_deinit(void);
void g_printf(const char *format, ...) printflike(1, 2);
void g_sprintf(char *dest, const char *format, ...) \
printflike(2, 3);
int  g_snprintf(char *dest, int len, const char *format, ...) \
printflike(3, 4);
void g_writeln(const char *format, ...) printflike(1, 2);
void g_write(const char *format, ...) printflike(1, 2);
void     g_hexdump(const char *p, int len);
int      g_getchar(void);
int      g_tcp_set_no_delay(int sck);
int      g_tcp_set_keepalive(int sck);
int      g_tcp_socket(void);
int      g_sck_set_send_buffer_bytes(int sck, int bytes);
int      g_sck_get_send_buffer_bytes(int sck, int *bytes);
int      g_sck_set_recv_buffer_bytes(int sck, int bytes);
int      g_sck_get_recv_buffer_bytes(int sck, int *bytes);
int      g_sck_local_socket(void);
int      g_sck_local_socketpair(int sck[2]);
int      g_sck_vsock_socket(void);
int      g_sck_get_peer_cred(int sck, int *pid, int *uid, int *gid);
void     g_sck_close(int sck);
int      g_tcp_connect(int sck, const char *address, const char *port);
int      g_sck_local_connect(int sck, const char *port);
int      g_sck_set_non_blocking(int sck);
int      g_tcp_bind(int sck, const char *port);
int      g_sck_local_bind(int sck, const char *port);
int      g_sck_vsock_bind(int sck, const char *port);
int      g_sck_vsock_bind_address(int sck, const char *port, const char *address);
int      g_tcp_bind_address(int sck, const char *port, const char *address);
int      g_sck_listen(int sck);
int      g_sck_accept(int sck);
int      g_sck_recv(int sck, void *ptr, unsigned int len, int flags);
int      g_sck_send(int sck, const void *ptr, unsigned int len, int flags);
/**
 * Receives data and file descriptors on a unix domain socket
 *
 * @param sck - Socket to receive data + file descriptors from
 * @param ptr - Pointer to buffer for incoming data
 * @param len - Length of data. Must be > 0
 * @param[out] fds - Array of file descriptors
 * @param [in] maxfd - Max number of elements in fds
 * @param[out] fdcount - Actual number of file descriptors received
 * @return Bytes received, or < 0 for error.
 *
 * If the result is > 0 but less than len, the file descriptors have
 * been received. Get the rest of the data with normal g_sck_recv() calls.
 *
 * fdcount may be more that maxfd. This indicates that more file descriptors
 * were received than there was space for. The excess file descriptors
 * are closed and discarded.
 */
int      g_sck_recv_fd_set(int sck, void *ptr, unsigned int len,
                           int fds[], unsigned int maxfd,
                           unsigned int *fdcount);
/**
 * Sends data and file descriptors on a unix domain socket
 *
 * @param sck - Socket to send data + file descriptors on
 * @param ptr - Data to send
 * @param len - Length of data. Must be > 0
 * @param fds - Array of file descriptors
 * @param fdcount - Number of file descriptors
 * @return Bytes sent, or < 0 for error.
 *
 * If the result is > 0 but less than len, the file descriptors have
 * been sent. Send the rest of the data with normal g_sck_send() calls.
 */
int      g_sck_send_fd_set(int sck, const void *ptr, unsigned int len,
                           int fds[], unsigned int fdcount);
int      g_sck_last_error_would_block(int sck);
int      g_sck_socket_ok(int sck);
/**
 * Checks socket writeability with an optional wait
 *
 * @param sck - Socket to check
 * @param millis - Maximum milliseconds to wait for writeability to be true
 *
 * @note The wait time may not be reached in the event of an incoming signal
 *       so do not use this call to impose a hard timeout */
int      g_sck_can_send(int sck, int millis);
/**
 * Checks socket readability with an optional wait
 *
 * @param sck - Socket to check
 * @param millis - Maximum milliseconds to wait for readability to be true
 *
 * @note The wait time may not be reached in the event of an incoming signal
 *       so do not use this call to impose a hard timeout */
int      g_sck_can_recv(int sck, int millis);
int      g_sck_select(int sck1, int sck2);

/**
 * Gets the IP address of a connected peer, if it has one
 * @param sck File descriptor for peer
 * @param ip buffer to write IP address to
 * @param bytes Size of ip buffer. Should be at least MAX_IP_ADDRSTRLEN
 * @param[out] portptr Optional variable to receive the port number
 * @return Pointer to IP for convenience
 *
 * If the peer has no IP address (for example, it is a Unix Domain Socket),
 * or the specified buffer is too small, the returned string is ""
 */
const char *
g_sck_get_peer_ip_address(int sck,
                          char *ip, unsigned int bytes,
                          unsigned short *port);
/**
 * Gets a description for a connected peer
 * @param sck File descriptor for peer
 * @param desc buffer to write description to
 * @param bytes Size of description buffer. Should be at least
 *              MAX_PEER_DESCSTRLEN
 * @return Pointer to desc for convenience
 *
 * Unlike g_sck_get_peer_ip_address(), this will return a
 * description of some sort for any socket type.
 */
const char *
g_sck_get_peer_description(int sck,
                           char *desc, unsigned int bytes);
/**
 * Sleep for the specified number of milli-seconds
 * @param msecs Milli-seconds
 *
 * If a signal is processed, it is possible that this call will
 * sleep for less than the specified number of milli-seconds. This
 * is platform-specific
 */
void     g_sleep(int msecs);
int      g_pipe(int fd[2]);

tintptr  g_create_wait_obj(const char *name);
tintptr  g_create_wait_obj_from_socket(tintptr socket, int write);
void     g_delete_wait_obj_from_socket(tintptr wait_obj);
int      g_set_wait_obj(tintptr obj);
int      g_reset_wait_obj(tintptr obj);
int      g_is_wait_obj_set(tintptr obj);
int      g_delete_wait_obj(tintptr obj);
/**
 * Wait for the specified readable and writeable objs
 *
 * The wait finishes when at least one of the objects becomes
 * readable or writeable
 *
 * @param read_objs Array of read objects
 * @param rcount Number of elements in read_objs
 * @param write_objs Array of write objects
 * @param rcount Number of elements in write_objs
 * @param mstimeout Timeout in milliseconds. < 0 means an infinite timeout.
 *
 * @return 0 for success. The objects will need to be polled to
 * find out what is readable or writeable.
 *
 * An mstimeout of zero will return immediately, although
 * error conditions may be checked for.
 */
int      g_obj_wait(tintptr *read_objs, int rcount, tintptr *write_objs,
                    int wcount, int mstimeout);
void     g_random(char *data, int len);
int      g_abs(int i);
int      g_memcmp(const void *s1, const void *s2, int len);
int      g_file_open_rw(const char *file_name);
int      g_file_open_ex(const char *file_name, int aread, int awrite,
                        int acreate, int atrunc);
int      g_file_open_ro(const char *file_name);
int      g_file_close(int fd);
/**
 * Returns 1 if a file is open (i.e. the file descriptor is valid)
 * @param fd File descriptor
 * @return 1 for file open, 0 for not open
 */
int      g_file_is_open(int fd);
int      g_file_read(int fd, char *ptr, int len);
int      g_file_write(int fd, const char *ptr, int len);
int      g_file_seek(int fd, int offset);
int      g_file_lock(int fd, int start, int len);
int
g_file_map(int fd, int aread, int awrite, size_t length, void **addr);
int
g_munmap(void *addr, size_t length);
int      g_file_duplicate_on(int fd, int target_fd);
int      g_file_get_cloexec(int fd);
int      g_file_set_cloexec(int fd, int status);
/**
 * Get a list of open file descriptors
 *
 * @param min Min FD to consider
 * @param max Max FD to consider (+1), or -1 for no limit
 * @result Array of file descriptors, in ascending order.
 *
 * Call delete_list() on the result when you've finished with it.
 */
struct list *g_get_open_fds(int min, int max);
int      g_chmod_hex(const char *filename, int flags);
int      g_umask_hex(int flags);
int      g_chown(const char *name, int uid, int gid);
int      g_mkdir(const char *dirname);
char    *g_get_current_dir(char *dirname, int maxlen);
int      g_set_current_dir(const char *dirname);
int      g_file_exist(const char *filename);
int      g_file_readable(const char *filename);
int      g_directory_exist(const char *dirname);
int      g_executable_exist(const char *dirname);
int      g_create_dir(const char *dirname);
int      g_create_path(const char *path);
int      g_remove_dir(const char *dirname);
int      g_file_delete(const char *filename);
int      g_file_get_size(const char *filename);
int      g_file_get_device_number(const char *filename);
int      g_file_get_inode_num(const char *filename);
long     g_load_library(char *in);
int      g_free_library(long lib);
void    *g_get_proc_address(long lib, const char *name);
int      g_system(const char *aexec);
char    *g_get_strerror(void);
int      g_get_errno(void);
int      g_execvp(const char *p1, char *args[]);
/**
 * Issues an execvp() call
 *
 * @param file Executable
 * @param argv Argument list for executable.
 *
 * argv does not need to be NULL terminated - the call takes care
 * of this.
 *
 * @return Only if an error has occurred - use g_get_errno() or equivalent
 */
int      g_execvp_list(const char *file, struct list *argv);
int      g_execlp3(const char *a1, const char *a2, const char *a3);
/**
 * Set an alarm using SIGALRM
 * @param func Signal handler, or NULL to cancel an alarm
 * @param secs Number of seconds until an alarm is raised
 * @return Number of seconds remaining before a previously requested
 *         alarm is raised
 */
unsigned int g_set_alarm(void (*func)(int), unsigned int secs);
/**
 * Set a handler up for SIGCHLD
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_child_stop(void (*func)(int));
/**
 * Set a handler up for SIGSEGV
 * @param func signal handler, or NULL to restore the default handler
 * The handler can only be called once, at which point the
 * default handler is restored. This is to avoid infinite loops
 */
void     g_signal_segfault(void (*func)(int));
/**
 * Set a handler up for SIGHUP
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_hang_up(void (*func)(int));
/**
 * Set a handler up for SIGINT
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_user_interrupt(void (*func)(int));
/**
 * Set a handler up for SIGTERM
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_terminate(void (*func)(int));
/**
 * Set a handler up for SIGPIPE
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_pipe(void (*func)(int));
/**
 * Set a handler up for SIGUSR1
 * @param func signal handler, or NULL to restore the default handler
 * The handler remains in place until explicitly replaced.
 */
void     g_signal_usr1(void (*func)(int));
int      g_fork(void);
int      g_setgid(int pid);
int      g_initgroups(const char *user);
int      g_getuid(void);
int      g_getgid(void);
int      g_setuid(int pid);
int      g_setsid(void);
int      g_getlogin(char *name, unsigned int len);
int      g_setlogin(const char *name);
#ifdef HAVE_SETUSERCONTEXT
/** Sets the login user context (BSD systems only)
 * @param uid UID of suer
 * @return 0 for success
 */
int      g_set_allusercontext(int uid);
#endif
int      g_waitchild(struct proc_exit_status *e);
int      g_waitpid(int pid);
struct proc_exit_status g_waitpid_status(int pid);
/*
 * Sets the process group ID of the indicated process to the specified value.
 * (POSIX.1)
 *
 * Errors are logged.
 *
 * May do nothing if process groups are not supported
 */
int      g_setpgid(int pid, int pgid);
void     g_clearenv(void);
int      g_setenv(const char *name, const char *value, int rewrite);
char    *g_getenv(const char *name);
int      g_exit(int exit_code);
int      g_getpid(void);
int      g_sigterm(int pid);
int      g_sighup(int pid);
int      g_getuser_info_by_name(const char *username, int *uid, int *gid,
                                char **shell, char **dir, char **gecos);
int      g_getuser_info_by_uid(int uid, char **username, int *gid,
                               char **shell, char **dir, char **gecos);
int      g_getgroup_info(const char *groupname, int *gid);
/**
 * Checks whether a user is in the specified group
 * @param username Name of user
 * @param gid GID of group
 * @param[out] ok Whether user is in group
 * @return Non-zero if a system error occurred. In this instance OK is not set
 *
 * Primary group of username is also checked
 */
int      g_check_user_in_group(const char *username, int gid, int *ok);
int      g_time1(void);
int      g_time2(void);
int      g_time3(void);
int      g_save_to_bmp(const char *filename, char *data, int stride_bytes,
                       int width, int height, int depth, int bits_per_pixel);
void    *g_shmat(int shmid);
int      g_shmdt(const void *shmaddr);
int      g_gethostname(char *name, int len);
int      g_mirror_memcpy(void *dst, const void *src, int len);
int      g_tcp4_socket(void);
int      g_tcp4_bind_address(int sck, const char *port, const char *address);
int      g_tcp6_socket(void);
int      g_tcp6_bind_address(int sck, const char *port, const char *address);
int      g_no_new_privs(void);
void
g_qsort(void *base, size_t nitems, size_t size,
        int (*compar)(const void *, const void *));

/* glib-style wrappers */
#define g_new(struct_type, n_structs) \
    (struct_type *) malloc(sizeof(struct_type) * (n_structs))
#define g_new0(struct_type, n_structs) \
    (struct_type *) calloc((n_structs), sizeof(struct_type))

/* remove these when no longer used */
#define g_malloc(_size, _zero) (_zero ? calloc(1, _size) : malloc(_size))
#define g_free free
#define g_memset memset
#define g_memcpy memcpy
#define g_memmove memmove

#endif
