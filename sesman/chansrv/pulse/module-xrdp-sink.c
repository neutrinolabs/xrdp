/**
 * xrdp: A Remote Desktop Protocol server.
 * pulse sink
 *
 * Copyright (C) Jay Sorg 2013
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
 */

/*
 * see pulse-notes.txt
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>
//#include <pulse/i18n.h>

#include <pulsecore/core-error.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>

#include "module-xrdp-sink-symdef.h"

PA_MODULE_AUTHOR("Jay Sorg");
PA_MODULE_DESCRIPTION("xrdp sink");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "format=<sample format> "
        "rate=<sample rate>"
        "channels=<number of channels> "
        "channel_map=<channel map>");

#define DEFAULT_SINK_NAME "xrdp"
#define BLOCK_USEC 30000
//#define BLOCK_USEC (PA_USEC_PER_SEC * 2)
#define CHANSRV_PORT_STR "/tmp/.xrdp/xrdp_chansrv_audio_socket_%d"

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_sink *sink;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    pa_usec_t block_usec;
    pa_usec_t timestamp;
    pa_usec_t failed_connect_time;
    pa_usec_t last_send_time;

    int fd; /* unix domain socket connection to xrdp chansrv */
    int display_num;
    int skip_bytes;
    int got_max_latency;

};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "format",
    "rate",
    "channels",
    "channel_map",
    NULL
};

static int close_send(struct userdata *u);

static int sink_process_msg(pa_msgobject *o, int code, void *data,
                            int64_t offset, pa_memchunk *chunk) {

    struct userdata *u = PA_SINK(o)->userdata;
    pa_usec_t now;
    long lat;

    pa_log_debug("sink_process_msg: code %d", code);

    switch (code) {

        case PA_SINK_MESSAGE_SET_VOLUME: /* 3 */
            break;

        case PA_SINK_MESSAGE_SET_MUTE: /* 6 */
            break;

        case PA_SINK_MESSAGE_GET_LATENCY: /* 7 */
            now = pa_rtclock_now();
            lat = u->timestamp > now ? u->timestamp - now : 0ULL;
            pa_log_debug("sink_process_msg: lat %ld", lat);
            *((pa_usec_t*) data) = lat;
            return 0;

        case PA_SINK_MESSAGE_GET_REQUESTED_LATENCY: /* 8 */
            break;

        case PA_SINK_MESSAGE_SET_STATE: /* 9 */
            if (PA_PTR_TO_UINT(data) == PA_SINK_RUNNING) /* 0 */ {
                pa_log("sink_process_msg: running");
                u->timestamp = pa_rtclock_now();
            } else {
                pa_log("sink_process_msg: not running");
                close_send(u);
            }
            break;

    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;
    size_t nbytes;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    u->block_usec = BLOCK_USEC;
    //u->block_usec = pa_sink_get_requested_latency_within_thread(s);
    pa_log("1 block_usec %d", u->block_usec);

    u->got_max_latency = 0;
    if (u->block_usec == (pa_usec_t) -1) {
        u->block_usec = s->thread_info.max_latency;
        pa_log("2 block_usec %d", u->block_usec);
        u->got_max_latency = 1;
    }

    nbytes = pa_usec_to_bytes(u->block_usec, &s->sample_spec);
    pa_sink_set_max_rewind_within_thread(s, nbytes);
    pa_sink_set_max_request_within_thread(s, nbytes);
}

static void process_rewind(struct userdata *u, pa_usec_t now) {
    size_t rewind_nbytes, in_buffer;
    pa_usec_t delay;

    pa_assert(u);

    /* Figure out how much we shall rewind and reset the counter */
    rewind_nbytes = u->sink->thread_info.rewind_nbytes;
    u->sink->thread_info.rewind_nbytes = 0;

    pa_assert(rewind_nbytes > 0);
    pa_log_debug("Requested to rewind %lu bytes.",
                 (unsigned long) rewind_nbytes);

    if (u->timestamp <= now)
        goto do_nothing;

    delay = u->timestamp - now;
    in_buffer = pa_usec_to_bytes(delay, &u->sink->sample_spec);

    if (in_buffer <= 0)
        goto do_nothing;

    if (rewind_nbytes > in_buffer)
        rewind_nbytes = in_buffer;

    pa_sink_process_rewind(u->sink, rewind_nbytes);
    u->timestamp -= pa_bytes_to_usec(rewind_nbytes, &u->sink->sample_spec);
    u->skip_bytes += rewind_nbytes;

    pa_log_debug("Rewound %lu bytes.", (unsigned long) rewind_nbytes);
    return;

do_nothing:

    pa_sink_process_rewind(u->sink, 0);
}

struct header {
    int code;
    int bytes;
};

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

static int data_send(struct userdata *u, pa_memchunk *chunk) {
    char *data;
    int bytes;
    int sent;
    int fd;
    struct header h;
    struct sockaddr_un s;

    if (u->fd == 0) {
        if (u->failed_connect_time != 0) {
            if (pa_rtclock_now() - u->failed_connect_time < 1000000) {
                return 0;
            }
        }
        fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        memset(&s, 0, sizeof(s));
        s.sun_family = AF_UNIX;
        bytes = sizeof(s.sun_path) - 1;
        snprintf(s.sun_path, bytes, CHANSRV_PORT_STR, u->display_num);
        pa_log_debug("trying to conenct to %s", s.sun_path);
        if (connect(fd, (struct sockaddr *)&s,
                    sizeof(struct sockaddr_un)) != 0) {
            u->failed_connect_time = pa_rtclock_now();
            pa_log_debug("Connected failed");
            close(fd);
            return 0;
        }
        u->failed_connect_time = 0;
        pa_log("Connected ok fd %d", fd);
        u->fd = fd;
    }

    bytes = chunk->length;
    pa_log_debug("bytes %d", bytes);

    /* from rewind */
    if (u->skip_bytes > 0) {
        if (bytes > u->skip_bytes) {
            bytes -= u->skip_bytes;
            u->skip_bytes = 0;
        } else  {
            u->skip_bytes -= bytes;
            return bytes;
        }
    }

    h.code = 0;
    h.bytes = bytes + 8;
    if (send(u->fd, &h, 8, 0) != 8) {
        pa_log("data_send: send failed");
        close(u->fd);
        u->fd = 0;
        return 0;
    } else {
        pa_log_debug("data_send: sent header ok bytes %d", bytes);
    }

    data = (char*)pa_memblock_acquire(chunk->memblock);
    data += chunk->index;
    sent = send(u->fd, data, bytes, 0);
    pa_memblock_release(chunk->memblock);

    if (sent != bytes) {
        pa_log("data_send: send failed sent %d bytes %d", sent, bytes);
        close(u->fd);
        u->fd = 0;
        return 0;
    }

    return sent;
}

static int close_send(struct userdata *u) {
    struct header h;

    pa_log("close_send:");
    if (u->fd == 0) {
        return 0;
    }
    h.code = 1;
    h.bytes = 8;
    if (send(u->fd, &h, 8, 0) != 8) {
        pa_log("close_send: send failed");
        close(u->fd);
        u->fd = 0;
        return 0;
    } else {
        pa_log_debug("close_send: sent header ok");
    }
    return 8;
}

static void process_render(struct userdata *u, pa_usec_t now) {
    pa_memchunk chunk;
    int request_bytes;

    pa_assert(u);
    if (u->got_max_latency) {
        return;
    }
    pa_log_debug("process_render: u->block_usec %d", u->block_usec);
    while (u->timestamp < now + u->block_usec) {
        request_bytes = u->sink->thread_info.max_request;
        request_bytes = MIN(request_bytes, 16 * 1024);
        pa_sink_render(u->sink, request_bytes, &chunk);
        data_send(u, &chunk);
        pa_memblock_unref(chunk.memblock);
        u->timestamp += pa_bytes_to_usec(chunk.length, &u->sink->sample_spec);
    }
}

static void thread_func(void *userdata) {

    struct userdata *u = userdata;
    int ret;
    pa_usec_t now;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    pa_thread_mq_install(&u->thread_mq);

    u->timestamp = pa_rtclock_now();

    for (;;) {

        if (u->sink->thread_info.state == PA_SINK_RUNNING) {

            now = pa_rtclock_now();

            if (u->sink->thread_info.rewind_requested) {
                if (u->sink->thread_info.rewind_nbytes > 0) {
                    process_rewind(u, now);
                } else {
                    pa_sink_process_rewind(u->sink, 0);
                }
            }

            if (u->timestamp <= now) {
                pa_log_debug("thread_func: calling process_render");
                process_render(u, now);
            }

            pa_rtpoll_set_timer_absolute(u->rtpoll, u->timestamp);

        } else {
            pa_rtpoll_set_timer_disabled(u->rtpoll);
        }

        if ((ret = pa_rtpoll_run(u->rtpoll, TRUE)) < 0) {
            goto fail;
        }

        if (ret == 0) {
            goto finish;
        }
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core),
                      PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0,
                      NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

int pa__init(pa_module*m) {
    struct userdata *u = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    pa_sink_new_data data;
    size_t nbytes;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map,
                         PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    pa_sink_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_sink_new_data_set_name(&data,
          pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
    pa_sink_new_data_set_sample_spec(&data, &ss);
    pa_sink_new_data_set_channel_map(&data, &map);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "xrdp sink");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "abstract");

    if (pa_modargs_get_proplist(ma, "sink_properties", data.proplist,
                                PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&data);
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &data,
                          PA_SINK_LATENCY | PA_SINK_DYNAMIC_LATENCY);
    pa_sink_new_data_done(&data);

    if (!u->sink) {
        pa_log("Failed to create sink object.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(u->sink, u->rtpoll);

    u->block_usec = BLOCK_USEC;
    pa_log("3 block_usec %d", u->block_usec);
    nbytes = pa_usec_to_bytes(u->block_usec, &u->sink->sample_spec);
    pa_sink_set_max_rewind(u->sink, nbytes);
    pa_sink_set_max_request(u->sink, nbytes);

    u->display_num = get_display_num_from_display(getenv("DISPLAY"));

#if defined(PA_CHECK_VERSION)
#if PA_CHECK_VERSION(0, 9, 22)
    if (!(u->thread = pa_thread_new("xrdp-sink", thread_func, u))) {
#else
    if (!(u->thread = pa_thread_new(thread_func, u))) {
#endif
#else
    if (!(u->thread = pa_thread_new(thread_func, u))) {
#endif
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_sink_put(u->sink);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma) {
        pa_modargs_free(ma);
    }

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink);
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata)) {
        return;
    }

    if (u->sink) {
        pa_sink_unlink(u->sink);
    }

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN,
                          NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->sink) {
        pa_sink_unref(u->sink);
    }

    if (u->rtpoll) {
        pa_rtpoll_free(u->rtpoll);
    }

    pa_xfree(u);
}
