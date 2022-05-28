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
 * generic transport
 */

#if !defined(TRANS_H)
#define TRANS_H

#include "arch.h"
#include "parse.h"

#define TRANS_MODE_TCP 1 /* tcp6 if defined, else tcp4 */
#define TRANS_MODE_UNIX 2
#define TRANS_MODE_VSOCK 3
#define TRANS_MODE_TCP4 4 /* tcp4 only */
#define TRANS_MODE_TCP6 6 /* tcp6 only */

#define TRANS_TYPE_LISTENER 1
#define TRANS_TYPE_SERVER 2
#define TRANS_TYPE_CLIENT 3

#define TRANS_STATUS_DOWN 0
#define TRANS_STATUS_UP 1

struct trans; /* forward declaration */
struct xrdp_tls;

typedef int (*ttrans_data_in)(struct trans *self);
typedef int (*ttrans_conn_in)(struct trans *self,
                              struct trans *new_self);
typedef int (*tis_term)(void);
typedef int (*trans_recv_proc) (struct trans *self, char *ptr, int len);
typedef int (*trans_send_proc) (struct trans *self, const char *data, int len);
typedef int (*trans_can_recv_proc) (struct trans *self, int sck, int millis);

/* optional source info */

enum xrdp_source
{
    XRDP_SOURCE_NONE = 0,
    XRDP_SOURCE_CLIENT,
    XRDP_SOURCE_SESMAN,
    XRDP_SOURCE_CHANSRV,
    XRDP_SOURCE_MOD,
    XORGXRDP_SOURCE_XORG,
    XORGXRDP_SOURCE_XRDP,

    XRDP_SOURCE_MAX_COUNT
};

/*
 * @brief Provide flow control mechanism for (primarily) xrdp
 *
 * There is one of these data structures per-program.
 *
 * While input is being read from a 'struct trans' and processed, the
 * cur_source member is set to the my_source member from the transport.
 * During this processing, trans_write_copy() may be called to send output
 * on another struct trans. If this happens, and the ouput needs to be
 * buffered, trans_write_copy() can add the number of bytes generated by
 * the input trans to the source field for the cur_source. This allows us to
 * see how much output has been buffered for each input source.
 *
 * When the program assembles 'struct trans' objects to scan for input
 * (normally in trans_get_wait_objs()), it is able to see how much buffered
 * output is registered for each input. Inputs which have too much buffered
 * output owing are skipped, and not considered for input.
 *
 * This provides a simple means of providing back-pressure on an input
 * where the data it is providing is being processed and then sent out on
 * a much slower link.
 */
struct source_info
{
    enum xrdp_source cur_source;
    int source[XRDP_SOURCE_MAX_COUNT];
};

struct trans
{
    tbus sck; /* socket handle */
    int mode; /* 1 tcp, 2 unix socket, 3 vsock */
    int status;
    int type1; /* 1 listener 2 server 3 client */
    ttrans_data_in trans_data_in;
    ttrans_conn_in trans_conn_in;
    void *callback_data;
    int header_size;
    struct stream *in_s;
    struct stream *out_s;
    char *listen_filename;
    tis_term is_term; /* used to test for exit */
    struct stream *wait_s;
    int no_stream_init_on_data_in;
    int extra_flags; /* user defined */
    void *extra_data; /* user defined */
    void (*extra_destructor)(struct trans *); /* user defined */

    struct ssl_tls *tls;
    const char *ssl_protocol; /* e.g. TLSv1, TLSv1.1, TLSv1.2, unknown */
    const char *cipher_name;  /* e.g. AES256-GCM-SHA384 */
    trans_recv_proc trans_recv;
    trans_send_proc trans_send;
    trans_can_recv_proc trans_can_recv;
    struct source_info *si;
    enum xrdp_source my_source;
};

struct trans *
trans_create(int mode, int in_size, int out_size);
void
trans_delete(struct trans *self);
void
trans_delete_from_child(struct trans *self);
int
trans_get_wait_objs(struct trans *self, tbus *objs, int *count);
int
trans_get_wait_objs_rw(struct trans *self,
                       tbus *robjs, int *rcount,
                       tbus *wobjs, int *wcount, int *timeout);
int
trans_check_wait_objs(struct trans *self);
int
trans_force_read_s(struct trans *self, struct stream *in_s, int size);
int
trans_force_write_s(struct trans *self, struct stream *out_s);
int
trans_force_read(struct trans *self, int size);
int
trans_force_write(struct trans *self);
int
trans_write_copy(struct trans *self);
int
trans_write_copy_s(struct trans *self, struct stream *out_s);
/**
 * Connect the transport to the specified destination
 *
 * @param self Transport
 * @param server Destination server (TCP transports only)
 * @param port TCP port, or UNIX socket to connect to
 * @param timeout in milli-seconds for the operation
 * @return 0 for success
 *
 * Multiple connection attempts may be made within the timeout period.
 *
 * If the operation is successful, 0 is returned and self->status will
 * be TRANS_STATUS_UP
 */
int
trans_connect(struct trans *self, const char *server, const char *port,
              int timeout);
int
trans_listen_address(struct trans *self, const char *port, const char *address);
int
trans_listen(struct trans *self, const char *port);
struct stream *
trans_get_in_s(struct trans *self);
struct stream *
trans_get_out_s(struct trans *self, int size);
int
trans_set_tls_mode(struct trans *self, const char *key, const char *cert,
                   long ssl_protocols, const char *tls_ciphers);
int
trans_shutdown_tls_mode(struct trans *self);
int
trans_tcp_force_read_s(struct trans *self, struct stream *in_s, int size);
int
trans_use_helper();

#endif
