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
#include <pulse/i18n.h>

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
#define BLOCK_USEC (PA_USEC_PER_SEC * 2)
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

    int fd; /* unix domain socket connection to chansrv */
    pa_memchunk memchunk;

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

static int sink_process_msg(pa_msgobject *o, int code, void *data,
                            int64_t offset, pa_memchunk *chunk) {

    struct userdata *u = PA_SINK(o)->userdata;
    pa_usec_t now;
    long lat;

    pa_log("sink_process_msg: code %d", code);

    switch (code) {
        case PA_SINK_MESSAGE_SET_STATE: /* 9 */

            if (PA_PTR_TO_UINT(data) == PA_SINK_RUNNING) /* 0 */ {
                pa_log("sink_process_msg: running");
                u->timestamp = pa_rtclock_now();
            } else {
                pa_log("sink_process_msg: not running");
            }

            break;

        case PA_SINK_MESSAGE_GET_LATENCY:

            now = pa_rtclock_now();
            lat = u->timestamp > now ? u->timestamp - now : 0ULL;
            pa_log("sink_process_msg: lat %ld", lat);
            *((pa_usec_t*) data) = lat;

            return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;
    size_t nbytes;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    u->block_usec = pa_sink_get_requested_latency_within_thread(s);

    if (u->block_usec == (pa_usec_t) -1)
        u->block_usec = s->thread_info.max_latency;

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
    pa_log_debug("Requested to rewind %lu bytes.", (unsigned long) rewind_nbytes);

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

    pa_log_debug("Rewound %lu bytes.", (unsigned long) rewind_nbytes);
    return;

do_nothing:

    pa_sink_process_rewind(u->sink, 0);
}

struct header
{
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

static int data_send(struct userdata *u) {
    char *data;
    int bytes;
    int sent;
    int display_num;
    struct header h;

    if (u->fd == 0) {
        int fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        struct sockaddr_un s = { 0 };
        s.sun_family = AF_UNIX;
        display_num = get_display_num_from_display(getenv("DISPLAY"));
        bytes = sizeof(s.sun_path) - 1;
        snprintf(s.sun_path, bytes, CHANSRV_PORT_STR, display_num);
        if (connect(fd, (struct sockaddr *)&s,
                    sizeof(struct sockaddr_un)) != 0) {
            //pa_log("Connected failed");
            close(fd);
            return 0;
        }
        u->fd = fd;
        pa_log("Connected ok fd %d", fd);
    }

    bytes = u->memchunk.length;
    pa_log("bytes %d", bytes);

    h.code = 0;
    h.bytes = bytes + 8;
    if (send(u->fd, &h, 8, 0) != 8) {
        pa_log("data_send: send failed");
        close(u->fd);
        u->fd = 0;
        return 0;
    } else {
        pa_log("data_send: sent header ok bytes %d", bytes);
    }

    data = (char*)pa_memblock_acquire(u->memchunk.memblock);
    data += u->memchunk.index;
    sent = send(u->fd, data, bytes, 0);
    pa_memblock_release(u->memchunk.memblock);

    if (sent != bytes) {
        pa_log("data_send: send failed sent %d bytes %d", sent, bytes);
        close(u->fd);
        u->fd = 0;
        return 0;
    }

    u->memchunk.index += sent;
    u->memchunk.length -= sent;

    if (u->memchunk.length <= 0) {
        pa_memblock_unref(u->memchunk.memblock);
        pa_memchunk_reset(&u->memchunk);
    }

    return sent;
}

static void process_render(struct userdata *u, pa_usec_t now) {

    pa_memchunk chunk;
    int request_bytes;
    int index;
    size_t ate = 0;

    pa_assert(u);

    /* This is the configured latency. Sink inputs connected to us
    might not have a single frame more than the maxrequest value
    queed. Hence: at maximum read this many bytes from the sink
    inputs. */

    index = 0;
    /* Fill the buffer up the the latency size */
    while (u->timestamp < now + u->block_usec) {

        request_bytes = u->sink->thread_info.max_request;
        request_bytes = MIN(request_bytes, 8192);
        pa_sink_render(u->sink, request_bytes, &chunk);
        index++;
        pa_log("bytes %d index %d", chunk.length, index);
        pa_memblock_unref(chunk.memblock);

/*         pa_log_debug("Ate %lu bytes.", (unsigned long) chunk.length); */
        u->timestamp += pa_bytes_to_usec(chunk.length, &u->sink->sample_spec);

        ate += chunk.length;

        //if (ate >= u->sink->thread_info.max_request)
        //    break;
    }

/*     pa_log_debug("Ate in sum %lu bytes (of %lu)", (unsigned long) ate, (unsigned long) nbytes); */


    //pa_log("%d", u->memchunk.length);

    //pa_log("a");

    //if (u->memchunk.length <= 0)
    //    pa_sink_render(u->sink, 8192, &u->memchunk);

    //pa_log("b");
    //data_send(u);
    //pa_log("c");

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
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
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
    pa_sink_new_data_set_name(&data, pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
    pa_sink_new_data_set_sample_spec(&data, &ss);
    pa_sink_new_data_set_channel_map(&data, &map);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "xrdp sink");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "abstract");

    if (pa_modargs_get_proplist(ma, "sink_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&data);
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &data, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY);
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
    nbytes = pa_usec_to_bytes(u->block_usec, &u->sink->sample_spec);
    pa_sink_set_max_rewind(u->sink, nbytes);
    pa_sink_set_max_request(u->sink, nbytes);

    pa_memchunk_reset(&u->memchunk);

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
    if (ma)
        pa_modargs_free(ma);

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

    if (!(u = m->userdata))
        return;

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->memchunk.memblock)
        pa_memblock_unref(u->memchunk.memblock);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    pa_xfree(u);
}
