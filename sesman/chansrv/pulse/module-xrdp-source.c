/***
  This file is part of PulseAudio.

  Copyright 2004-2008 Lennart Poettering
  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/modargs.h>
#include <pulsecore/module.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/source.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/thread.h>

/* defined in pulse/version.h */
#if PA_PROTOCOL_VERSION > 28
/* these used to be defined in pulsecore/macro.h */
typedef bool pa_bool_t;
#define FALSE ((pa_bool_t) 0)
#define TRUE (!FALSE)
#else
#endif

#include "module-xrdp-source-symdef.h"
#include "../../../common/xrdp_sockets.h"

PA_MODULE_AUTHOR("Laxmikant Rashinkar");
PA_MODULE_DESCRIPTION("xrdp source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
        "format=<sample format> "
        "channels=<number of channels> "
        "rate=<sample rate> "
        "source_name=<name of source> "
        "channel_map=<channel map> "
        "description=<description for the source> "
        "latency_time=<latency time in ms>");

#define DEFAULT_SOURCE_NAME "xrdp-source"
#define DEFAULT_LATENCY_TIME 10
#define MAX_LATENCY_USEC 1000

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_source *source;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    size_t block_size;

    pa_usec_t block_usec;
    pa_usec_t timestamp;
    pa_usec_t latency_time;

    /* xrdp stuff */
    int fd;            /* UDS connection to xrdp chansrv */
    int display_num;   /* X display number */
    int want_src_data;
};

static const char* const valid_modargs[] = {
    "rate",
    "format",
    "channels",
    "source_name",
    "channel_map",
    "description",
    "latency_time",
    NULL
};

static int get_display_num_from_display(char *display_text) ;

static int source_process_msg(pa_msgobject *o, int code, void *data,
                              int64_t offset, pa_memchunk *chunk) {

    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {
        case PA_SOURCE_MESSAGE_SET_STATE:

            if (PA_PTR_TO_UINT(data) == PA_SOURCE_RUNNING)
                u->timestamp = pa_rtclock_now();

            break;

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            pa_usec_t now;

            now = pa_rtclock_now();
            *((pa_usec_t*) data) = u->timestamp > now ? u->timestamp - now : 0;

            return 0;
        }
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

static void source_update_requested_latency_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    u = s->userdata;
    pa_assert(u);

    u->block_usec = pa_source_get_requested_latency_within_thread(s);
}

static int lsend(int fd, char *data, int bytes) {
    int sent = 0;
    int error;
    while (sent < bytes) {
        error = send(fd, data + sent, bytes - sent, 0);
        if (error < 1) {
            return error;
        }
        sent += error;
    }
    return sent;
}

static int lrecv(int fd, char *data, int bytes) {
    int recved = 0;
    int error;
    while (recved < bytes) {
        error = recv(fd, data + recved, bytes - recved, 0);
        if (error < 1) {
            return error;
        }
        recved += error;
    }
    return recved;
}

static int data_get(struct userdata *u, pa_memchunk *chunk) {

    int fd;
    int bytes;
    int read_bytes;
    struct sockaddr_un s;
    char *data;
    char *socket_dir;
    char buf[11];
    unsigned char ubuf[10];

    if (u->fd == 0) {
        /* connect to xrdp unix domain socket */
        fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        memset(&s, 0, sizeof(s));
        s.sun_family = AF_UNIX;
        bytes = sizeof(s.sun_path) - 1;
        socket_dir = getenv("XRDP_SOCKET_PATH");
        if (socket_dir == NULL || socket_dir[0] == '\0')
        {
            socket_dir = "/tmp/.xrdp";
        }
        snprintf(s.sun_path, bytes, "%s/" CHANSRV_PORT_IN_BASE_STR,
                 socket_dir, u->display_num);
        pa_log_debug("Trying to connect to %s", s.sun_path);

        if (connect(fd, (struct sockaddr *) &s, sizeof(struct sockaddr_un)) != 0) {
            pa_log_debug("Connect failed");
            close(fd);
            return -1;
        }

        pa_log("Connected ok, fd=%d", fd);
        pa_log_debug("###### connected to xrdp audio_in socket");
        u->fd = fd;
    }

    data = (char *) pa_memblock_acquire(chunk->memblock);

    if (!u->want_src_data) {
        char buf[12];

        buf[0]  = 0;
        buf[1]  = 0;
        buf[2]  = 0;
        buf[3]  = 0;
        buf[4]  = 11;
        buf[5]  = 0;
        buf[6]  = 0;
        buf[7]  = 0;
        buf[8]  = 1;
        buf[9]  = 0;
        buf[10] = 0;

        if (lsend(u->fd, buf, 11) != 11) {
            close(u->fd);
            u->fd = 0;
            pa_memblock_release(chunk->memblock);
            return -1;
        }
        u->want_src_data = 1;
        pa_log_debug("###### started recording");
    }

    /* ask for more data */
    buf[0]  = 0;
    buf[1]  = 0;
    buf[2]  = 0;
    buf[3]  = 0;
    buf[4]  = 11;
    buf[5]  = 0;
    buf[6]  = 0;
    buf[7]  = 0;
    buf[8]  = 3;
    buf[9]  = (unsigned char) chunk->length;
    buf[10] = (unsigned char) ((chunk->length >> 8) & 0xff);

    if (lsend(u->fd, buf, 11) != 11) {
        close(u->fd);
        u->fd = 0;
        pa_memblock_release(chunk->memblock);
        u->want_src_data = 0;
        return -1;
    }

    /* read length of data available */
    if (lrecv(u->fd, (char *) ubuf, 2) != 2) {
        close(u->fd);
        u->fd = 0;
        pa_memblock_release(chunk->memblock);
        u->want_src_data = 0;
        return -1;
    }
    bytes = ((ubuf[1] << 8) & 0xff00) | (ubuf[0] & 0xff);

    if (bytes == 0) {
        pa_memblock_release(chunk->memblock);
        return 0;
    }

    /* get data */
    read_bytes = lrecv(u->fd, data, bytes);
    if (read_bytes != bytes) {
        close(u->fd);
        u->fd = 0;
        pa_memblock_release(chunk->memblock);
        u->want_src_data = 0;
        return -1;
    }
    pa_memblock_release(chunk->memblock);

    return read_bytes;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    int bytes;

    pa_assert(u);
    pa_thread_mq_install(&u->thread_mq);
    u->timestamp = pa_rtclock_now();

    for (;;) {
        int ret;

        /* Generate some null data */
        if (u->source->thread_info.state == PA_SOURCE_RUNNING) {
            pa_usec_t now;
            pa_memchunk chunk;

            now = pa_rtclock_now();

            if ((chunk.length = pa_usec_to_bytes(now - u->timestamp, &u->source->sample_spec)) > 0) {
                chunk.length *= 4;
                chunk.memblock = pa_memblock_new(u->core->mempool, chunk.length);
                chunk.index = 0;
                bytes = data_get(u, &chunk);
                if (bytes > 0)
                {
                    chunk.length = bytes;
                    pa_source_post(u->source, &chunk); 
                }
                pa_memblock_unref(chunk.memblock);
                u->timestamp = now;
            }

            pa_rtpoll_set_timer_absolute(u->rtpoll, u->timestamp + u->latency_time * PA_USEC_PER_MSEC);
        } else {
            if (u->want_src_data)
            {
                /* we don't want source data anymore */
                char buf[12];

                buf[0]  = 0;
                buf[1]  = 0;
                buf[2]  = 0;
                buf[3]  = 0;
                buf[4]  = 11;
                buf[5]  = 0;
                buf[6]  = 0;
                buf[7]  = 0;
                buf[8]  = 2;
                buf[9]  = 0;
                buf[10] = 0;

                if (lsend(u->fd, buf, 11) != 11) {
                    close(u->fd);
                    u->fd = 0;
                }
                u->want_src_data = 0;
                pa_log_debug("###### stopped recording");
            }
            pa_rtpoll_set_timer_disabled(u->rtpoll);
        }

        /* Hmm, nothing to do. Let's sleep */
#if defined(PA_CHECK_VERSION) && PA_CHECK_VERSION(6, 0, 0)
        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0) {
#else
        if ((ret = pa_rtpoll_run(u->rtpoll, TRUE)) < 0) {
#endif
            goto fail;
        }

        if (ret == 0)
            goto finish;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("###### thread shutting down");
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    pa_source_new_data data;
    uint32_t latency_time = DEFAULT_LATENCY_TIME;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

#if 1
    ss = m->core->default_sample_spec;
#else
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 22050;
    ss.channels = 2;
#endif

    map = m->core->default_channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    pa_source_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_source_new_data_set_name(&data, pa_modargs_get_value(ma, "source_name", DEFAULT_SOURCE_NAME));
    pa_source_new_data_set_sample_spec(&data, &ss);
    pa_source_new_data_set_channel_map(&data, &map);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, pa_modargs_get_value(ma, "description", "xrdp source"));
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "abstract");

    u->source = pa_source_new(m->core, &data, PA_SOURCE_LATENCY | PA_SOURCE_DYNAMIC_LATENCY);
    pa_source_new_data_done(&data);

    if (!u->source) {
        pa_log("Failed to create source object.");
        goto fail;
    }

    u->latency_time = DEFAULT_LATENCY_TIME;
    if (pa_modargs_get_value_u32(ma, "latency_time", &latency_time) < 0) {
        pa_log("Failed to parse latency_time value.");
        goto fail;
    }
    u->latency_time = latency_time;

    u->source->parent.process_msg = source_process_msg;
    u->source->update_requested_latency = source_update_requested_latency_cb;
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    pa_source_set_latency_range(u->source, 0, MAX_LATENCY_USEC);
    u->block_usec = u->source->thread_info.max_latency;

    u->source->thread_info.max_rewind =
        pa_usec_to_bytes(u->block_usec, &u->source->sample_spec);

    #if defined(PA_CHECK_VERSION)
    #if PA_CHECK_VERSION(0, 9, 22)
        if (!(u->thread = pa_thread_new("xrdp-source", thread_func, u))) {
    #else
        if (!(u->thread = pa_thread_new(thread_func, u))) {
    #endif
    #else
	if (!(u->thread = pa_thread_new(thread_func, u))) 
    #endif
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_source_put(u->source);

    pa_modargs_free(ma);

    u->display_num = get_display_num_from_display(getenv("DISPLAY"));

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->source)
        pa_source_unlink(u->source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->source)
        pa_source_unref(u->source);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    pa_xfree(u);
}

static int get_display_num_from_display(char *display_text) {
    int index;
    int mode;
    int host_index;
    int disp_index;
    int scre_index;
    int display_num;
    char host[256];
    char disp[256];
    char scre[256];

    if (display_text == NULL) {
        return 0;
    }
    memset(host, 0, 256);
    memset(disp, 0, 256);
    memset(scre, 0, 256);

    index = 0;
    host_index = 0;
    disp_index = 0;
    scre_index = 0;
    mode = 0;

    while (display_text[index] != 0) {
        if (display_text[index] == ':') {
            mode = 1;
        } else if (display_text[index] == '.') {
            mode = 2;
        } else if (mode == 0) {
            host[host_index] = display_text[index];
            host_index++;
        } else if (mode == 1) {
            disp[disp_index] = display_text[index];
            disp_index++;
        } else if (mode == 2) {
            scre[scre_index] = display_text[index];
            scre_index++;
        }
        index++;
    }

    host[host_index] = 0;
    disp[disp_index] = 0;
    scre[scre_index] = 0;
    display_num = atoi(disp);
    return display_num;
}
