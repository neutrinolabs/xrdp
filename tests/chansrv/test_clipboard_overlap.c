#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

/* Exercise the real clipboard handlers, with Xvfb and synthetic RDP replies. */
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include "../../sesman/chansrv/clipboard.c"

Display *g_display;
int g_cliprdr_chan_id = 1;
struct config_chansrv *g_cfg;
static int requests;
static int fail_send;
static int formats[64];
static Window first_requestor;
static unsigned int clock_ms;
static void (*timer_callback)(void *);
static void *timer_data;
static int timer_delay;
static int timer_calls;

unsigned int
g_get_elapsed_ms(void)
{
    return clock_ms;
}

int
add_timeout(int delay, void (*callback)(void *), void *data)
{
    timer_delay = delay;
    timer_callback = callback;
    timer_data = data;
    ++timer_calls;
    return 0;
}

int
xfuse_deinit(void)
{
    return 0;
}

int
xfuse_clear_clip_dir(void)
{
    return 0;
}

int
clipboard_send_data_response_for_file(const char *data, int size)
{
    assert(!"File output is outside this fixture");
    return 1;
}

int
send_channel_data(int chan_id, const char *data, int size)
{
    assert(chan_id == 1 && size >= 8);
    if ((unsigned char)data[0] == CB_FORMAT_DATA_REQUEST)
    {
        assert(requests < sizeof(formats) / sizeof(formats[0]) && size >= 12);
        formats[requests] = (unsigned char)data[8] |
                            ((unsigned char)data[9] << 8);
        ++requests;
    }
    return fail_send;
}

int
clipboard_c2s_in_files(struct stream *s, char *out, int size, const char *prefix)
{
    assert(!"File conversion is outside this text fixture");
    return 1;
}

static void
request_target(Display *client, Window window, Atom target)
{
    XEvent event;
    XConvertSelection(client, g_clipboard_atom, target,
                      g_clip_property_atom, window, CurrentTime);
    XSync(client, False);
    XNextEvent(g_display, &event);
    assert(event.type == SelectionRequest);
    clipboard_event_selection_request_or_queue(&event);
}

static void
reply_text(int status)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, 64);
    if (g_clip_c2s.xrdp_clip_type == XRDP_CB_BITMAP)
    {
        out_uint32_le(s, 40);
        out_uint32_le(s, 1);
        out_uint32_le(s, 1);
        out_uint16_le(s, 1);
        out_uint16_le(s, 24);
        memset(s->p, 0, 24);
        out_uint8s(s, 24);
    }
    else
    {
        out_uint16_le(s, requests == 1 ? 'A' : 'B');
        out_uint16_le(s, 'e');
        out_uint16_le(s, 's');
        out_uint16_le(s, 't');
        out_uint16_le(s, 0);
    }
    s_mark_end(s);
    s->p = s->data;
    clipboard_process_data_response(s, status, (int)(s->end - s->data));
    free_stream(s);
    XSync(g_display, False);
}

static int
received(Display *client, Window window, Atom property, int check_text)
{
    XEvent event;
    int count = 0;
    XSync(client, False);
    if (!XPending(client))
    {
        struct pollfd connection = {ConnectionNumber(client), POLLIN, 0};
        /* The other connection's XFlush does not order this client's XSync. */
        assert(poll(&connection, 1, 1000) >= 0);
    }
    while (XPending(client))
    {
        XNextEvent(client, &event);
        assert(event.type == SelectionNotify);
        assert(event.xselection.requestor == window);
        assert(event.xselection.property == property);
        ++count;
    }
    if (count && check_text)
    {
        Atom type;
        int format;
        unsigned long size;
        unsigned long rest;
        unsigned char *text;
        assert(XGetWindowProperty(client, window, property, 0, 64, False,
                                  AnyPropertyType, &type, &format, &size,
                                  &rest, &text) == Success);
        assert(format == 8 && rest == 0);
        if (check_text == 2)
        {
            assert(type == g_image_bmp_atom && size == 54);
            assert(text[0] == 'B' && text[1] == 'M');
        }
        else
        {
            assert(type == g_utf8_atom && size == 4);
            assert(memcmp(text, window == first_requestor ? "Aest" : "Best", 4) == 0);
        }
        XFree(text);
    }
    return count;
}

static void
stress_requests(void)
{
    Display *clients[64];
    Window windows[64];
    int round;
    int i;
    int refusals = 0;
    for (i = 0; i < 64; ++i)
    {
        clients[i] = XOpenDisplay(NULL);
        assert(clients[i]);
        windows[i] = XCreateSimpleWindow(clients[i], DefaultRootWindow(clients[i]),
                                         0, 0, 1, 1, 0, 0, 0);
    }
    first_requestor = windows[0];
    for (round = 0; round < 100; ++round)
    {
        requests = 0;
        for (i = 0; i < 64; ++i)
        {
            request_target(clients[i], windows[i],
                           i % 2 ? g_image_bmp_atom : g_utf8_atom);
            assert(requests == 1);
        }
        for (i = 0; i < 64; ++i)
        {
            int failed = (i + round) % 13 == 0;
            assert(g_saved_selection_req_event.requestor == windows[i]);
            assert(formats[i] == (i % 2 ? CF_DIB : CF_UNICODETEXT));
            reply_text(failed ? CB_RESPONSE_FAIL : CB_RESPONSE_OK);
            assert(requests == (i == 63 ? 64 : i + 2));
            assert(received(clients[i], windows[i],
                            failed ? None : g_clip_property_atom,
                            failed ? 0 : (i % 2 ? 2 : 1)) == 1);
            refusals += failed;
        }
        assert(!g_clip_c2s.in_request && !g_clip_c2s.incr_in_progress);
        assert(fifo_is_empty(g_pending_selection_requests));
        for (i = 0; i < 64; ++i)
        {
            XSync(clients[i], False);
            assert(!XPending(clients[i]));
        }
    }
    for (i = 0; i < 64; ++i)
    {
        XCloseDisplay(clients[i]);
    }
    printf("stress: 100 bursts, 6400 requests, %d refusals, no lost/duplicate replies\n",
           refusals);
}

int
main(int argc, char **argv)
{
    struct config_chansrv config = {0};
    Display *first;
    Display *second;
    Window a;
    Window b;
    int a_count;
    int b_count;
    const char *mode = argc > 1 ? argv[1] : "overlap";
    struct log_config *logging;
    Atom a_property;
    int a_text = 1;
    int b_text = 1;
    if (getenv("XRDP_CLIPBOARD_TEST_DISPLAY") == NULL ||
            getenv("DISPLAY") == NULL ||
            strcmp(getenv("XRDP_CLIPBOARD_TEST_DISPLAY"), getenv("DISPLAY")) != 0)
    {
        fprintf(stderr, "Run through test_clipboard_overlap.sh on Xvfb\n");
        return 77;
    }
    g_init("clipboard-overlap-test");
    logging = log_config_init_for_console(LOG_LEVEL_ERROR, NULL);
    assert(logging && log_start_from_param(logging) == LOG_STARTUP_OK);
    g_cfg = &config;
    g_display = XOpenDisplay(NULL);
    first = XOpenDisplay(NULL);
    second = XOpenDisplay(NULL);
    assert(g_display && first && second);
    g_clipboard_atom = XInternAtom(g_display, "CLIPBOARD", False);
    g_utf8_atom = XInternAtom(g_display, "UTF8_STRING", False);
    g_clip_property_atom = XInternAtom(g_display, "TEST_CLIPBOARD", False);
    g_incr_atom = XInternAtom(g_display, "INCR", False);
    g_image_bmp_atom = XInternAtom(g_display, "image/bmp", False);
    g_incr_max_req_size = 32768;
    g_formatIds[0] = CF_UNICODETEXT;
    g_num_formatIds = 1;
    g_wnd = XCreateSimpleWindow(g_display, DefaultRootWindow(g_display),
                                0, 0, 1, 1, 0, 0, 0);
    g_clip_up = 1;
    a = XCreateSimpleWindow(first, DefaultRootWindow(first), 0, 0, 1, 1, 0, 0, 0);
    first_requestor = a;
    b = XCreateSimpleWindow(second, DefaultRootWindow(second), 0, 0, 1, 1, 0, 0, 0);
    XSetSelectionOwner(g_display, g_clipboard_atom, g_wnd, CurrentTime);
    XSync(g_display, False);
    if (strcmp(mode, "stress") == 0)
    {
        stress_requests();
        a_count = b_count = 1;
        goto cleanup;
    }
    a_property = g_clip_property_atom;
    request_target(first, a, g_utf8_atom);
    request_target(second, b, strcmp(mode, "mixed") == 0 ? g_image_bmp_atom : g_utf8_atom);
    assert(requests == 1);
    if (strcmp(mode, "failure") == 0 || strcmp(mode, "malformed") == 0)
    {
        reply_text(strcmp(mode, "failure") == 0 ? CB_RESPONSE_FAIL : 0);
        a_property = None;
        a_text = 0;
        assert(requests == 2);
        reply_text(CB_RESPONSE_OK);
    }
    else if (strcmp(mode, "clear") == 0 || strcmp(mode, "deinit") == 0)
    {
        XEvent event = {0};
        if (strcmp(mode, "clear") == 0)
        {
            clipboard_event_selection_clear(&event);
            assert(requests == 1);
            reply_text(CB_RESPONSE_OK);
        }
        else
        {
            clipboard_deinit();
            a_property = None;
            a_text = 0;
            assert(!g_clip_c2s.in_request && !g_clip_c2s.incr_in_progress);
        }
        b_text = 0;
        assert(requests == 1);
    }
    else if (strcmp(mode, "send-failure") == 0)
    {
        fail_send = 1;
        reply_text(CB_RESPONSE_OK);
        assert(!g_clip_c2s.in_request && requests == 2);
        b_text = 0;
    }
    else if (strcmp(mode, "idle") == 0 || strcmp(mode, "idle-refuse") == 0 ||
             strcmp(mode, "incr-deinit") == 0 || strcmp(mode, "destroy-incr") == 0 ||
             strcmp(mode, "reconnect-timer") == 0)
    {
        void *token;
        int calls;
        g_incr_max_req_size = 2;
        reply_text(CB_RESPONSE_OK);
        assert(g_clip_c2s.incr_in_progress && requests == 1);
        assert(timer_calls == 1 && timer_delay == CLIPBOARD_INCR_IDLE_MS);
        token = timer_data;
        a_text = 0;
        if (strcmp(mode, "incr-deinit") == 0 || strcmp(mode, "reconnect-timer") == 0)
        {
            clipboard_deinit();
            calls = timer_calls;
            timer_callback(token);
            assert(!g_clip_up && !g_clip_c2s.incr_in_progress && requests == 1);
            assert(timer_calls == calls);
            b_text = 0;
            if (strcmp(mode, "reconnect-timer") == 0)
            {
                XEvent event;
                assert(received(first, a, g_clip_property_atom, 0) == 1);
                assert(received(second, b, None, 0) == 1);
                while (XPending(g_display))
                {
                    XNextEvent(g_display, &event);
                }
                g_wnd = XCreateSimpleWindow(g_display, DefaultRootWindow(g_display),
                                            0, 0, 1, 1, 0, 0, 0);
                g_clip_up = 1;
                XSetSelectionOwner(g_display, g_clipboard_atom, g_wnd, CurrentTime);
                XSync(g_display, False);
                requests = 0;
                request_target(first, a, g_utf8_atom);
                request_target(second, b, g_utf8_atom);
                reply_text(CB_RESPONSE_OK);
                assert(g_clip_c2s.incr_in_progress && token != timer_data);
                calls = timer_calls;
                clock_ms += CLIPBOARD_INCR_IDLE_MS;
                timer_callback(token);
                assert(g_clip_c2s.incr_in_progress && timer_calls == calls);
                g_incr_max_req_size = 32768;
                timer_callback(timer_data);
                assert(!g_clip_c2s.incr_in_progress && requests == 2);
                reply_text(CB_RESPONSE_OK);
                b_text = 1;
            }
        }
        else
        {
            g_incr_max_req_size = 32768;
            if (strcmp(mode, "destroy-incr") == 0)
            {
                XEvent event;
                XDestroyWindow(first, a);
                XSync(first, False);
                do
                {
                    XNextEvent(g_display, &event);
                }
                while (event.type != DestroyNotify);
                clipboard_xevent(&event);
            }
            else
            {
                clock_ms += CLIPBOARD_INCR_IDLE_MS - 1;
                timer_callback(token);
                assert(g_clip_c2s.incr_in_progress && requests == 1 && timer_delay == 1);
                if (strcmp(mode, "idle-refuse") == 0)
                {
                    config.restrict_inbound_clipboard = CLIP_RESTRICT_TEXT;
                    b_text = 0;
                }
                ++clock_ms;
                timer_callback(token);
            }
            assert(!g_clip_c2s.incr_in_progress && !g_clip_c2s.converted);
            assert(fifo_is_empty(g_pending_selection_requests));
            if (b_text)
            {
                assert(requests == 2 && g_clip_c2s.in_request);
                reply_text(CB_RESPONSE_OK);
            }
            else
            {
                assert(requests == 1 && !g_clip_c2s.in_request);
            }
        }
    }
    else if (strcmp(mode, "incr") == 0 || strcmp(mode, "pending-ack") == 0 ||
             strcmp(mode, "stale-timer") == 0 || strcmp(mode, "clock-wrap") == 0)
    {
        XEvent event;
        int chunks = 0;
        char assembled[4];
        unsigned long used = 0;
        if (strcmp(mode, "clock-wrap") == 0)
        {
            clock_ms = 0xffffffffU - 2000;
        }
        g_incr_max_req_size = 2;
        reply_text(CB_RESPONSE_OK);
        assert(g_clip_c2s.incr_in_progress && requests == 1);
        if (strcmp(mode, "clock-wrap") == 0)
        {
            clock_ms += 4000;
            timer_callback(timer_data);
            assert(g_clip_c2s.incr_in_progress && timer_delay == 1000);
        }
        if (strcmp(mode, "pending-ack") == 0)
        {
            XDeleteProperty(first, a, g_clip_property_atom);
            XSync(first, False);
            clock_ms += CLIPBOARD_INCR_IDLE_MS;
            timer_callback(timer_data);
            assert(g_clip_c2s.incr_in_progress && requests == 1);
            assert(timer_delay == CLIPBOARD_INCR_IDLE_MS);
        }
        while (g_clip_c2s.incr_in_progress)
        {
            int calls;
            if (strcmp(mode, "clock-wrap") != 0 || chunks != 0)
            {
                clock_ms += 4000;
            }
            XDeleteProperty(first, a, g_clip_property_atom);
            XSync(first, False);
            do
            {
                XNextEvent(g_display, &event);
            }
            while (event.type != PropertyNotify ||
                    event.xproperty.state != PropertyDelete);
            clipboard_event_property_notify(&event);
            calls = timer_calls;
            timer_callback(timer_data);
            assert(timer_calls == calls + (g_clip_c2s.incr_in_progress ? 1 : 0));
            if (g_clip_c2s.incr_in_progress)
            {
                assert(timer_delay == CLIPBOARD_INCR_IDLE_MS);
            }
            XSync(g_display, False);
            {
                Atom type;
                int format;
                unsigned long size;
                unsigned long rest;
                unsigned char *text;
                assert(XGetWindowProperty(first, a, g_clip_property_atom, 0, 64,
                                          False, AnyPropertyType, &type, &format,
                                          &size, &rest, &text) == Success);
                assert(type == g_utf8_atom && format == 8 && rest == 0);
                assert(used + size <= sizeof(assembled));
                memcpy(assembled + used, text, size);
                used += size;
                XFree(text);
            }
            assert(++chunks <= 3);
            if (chunks < 3)
            {
                assert(requests == 1);
            }
        }
        assert(chunks == 3 && requests == 2);
        assert(used == 4 && memcmp(assembled, "Aest", 4) == 0);
        if (strcmp(mode, "stale-timer") == 0)
        {
            void *old_token = timer_data;
            int calls;
            reply_text(CB_RESPONSE_OK);
            assert(g_clip_c2s.incr_in_progress && old_token != timer_data);
            clock_ms += CLIPBOARD_INCR_IDLE_MS;
            calls = timer_calls;
            timer_callback(old_token);
            assert(g_clip_c2s.incr_in_progress && timer_calls == calls);
            timer_callback(timer_data);
            assert(!g_clip_c2s.incr_in_progress && !g_clip_c2s.converted);
            b_text = 0;
        }
        else
        {
            g_incr_max_req_size = 32768;
            reply_text(CB_RESPONSE_OK);
        }
        a_text = 0;
    }
    else
    {
        if (strcmp(mode, "slow-windows") == 0)
        {
            clock_ms += 60000;
            assert(timer_calls == 0 && g_clip_c2s.in_request && requests == 1);
        }
        reply_text(CB_RESPONSE_OK);
        if (strcmp(mode, "mixed") == 0)
        {
            assert(formats[0] == CF_UNICODETEXT && formats[1] == CF_DIB);
            b_text = 2;
        }
        assert(requests == 2);
        reply_text(CB_RESPONSE_OK);
    }
    if (strcmp(mode, "deinit") != 0 && strcmp(mode, "incr-deinit") != 0 &&
            strcmp(mode, "idle-refuse") != 0)
    {
        XSync(g_display, False);
    }
    a_count = received(first, a, a_property, a_text);
    b_count = received(second, b,
                       b_text || strcmp(mode, "stale-timer") == 0 ?
                       g_clip_property_atom : None, b_text);
    printf("%s: requests=%d first_replies=%d second_replies=%d\n",
           mode, requests, a_count, b_count);
cleanup:
    XCloseDisplay(first);
    XCloseDisplay(second);
    XCloseDisplay(g_display);
    g_free(g_clip_c2s.data);
    if (g_pending_selection_requests)
    {
        fifo_delete(g_pending_selection_requests, NULL);
    }
    log_end();
    log_config_free(logging);
    g_deinit();
    return a_count == 1 && b_count == 1 ? 0 : 1;
}
