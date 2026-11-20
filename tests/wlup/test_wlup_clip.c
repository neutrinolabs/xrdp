/**
 * xrdp: A Remote Desktop Protocol server.
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
 * wlup unit tests: the text clipboard ([MS-RDPECLIP], wlup_clip.c)
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <string.h>
#include <unistd.h>

#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"
#include "os_calls.h"
#include "wlup_clip.h"
#include "xrdp_constants.h"

#include "test_wlup.h"

#define CHANNEL 7

static struct wlup *v;

/******************************************************************************/
static void
setup(void)
{
    v = mock_module_create(64, 48);
    server.channel_id = CHANNEL;
    ck_assert_int_eq(v->mod_connect(v, -1), 0);
}

static void
teardown(void)
{
    mock_module_destroy(v);
}

/******************************************************************************/
/* A CLIPRDR PDU from the client: header, then body */
static int
make_pdu(char *pdu, int type, int flags, const char *body, int body_len)
{
    pdu[0] = type & 0xff;
    pdu[1] = type >> 8;
    pdu[2] = flags & 0xff;
    pdu[3] = flags >> 8;
    pdu[4] = body_len & 0xff;
    pdu[5] = (body_len >> 8) & 0xff;
    pdu[6] = 0;
    pdu[7] = 0;
    memcpy(pdu + 8, body, body_len);
    return 8 + body_len;
}

static void
send_from_client(int type, int flags, const char *body, int body_len)
{
    char pdu[512];
    int len = make_pdu(pdu, type, flags, body, body_len);

    ck_assert_int_eq(wlup_clip_process_channel_data(v, pdu, len, len,
                     XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST), 0);
}

/* The client offers CF_UNICODETEXT, with a long (empty) format name */
static void
client_format_list(void)
{
    static const char list[] = { CF_UNICODETEXT, 0, 0, 0, 0, 0 };

    send_from_client(CB_FORMAT_LIST, 0, list, sizeof(list));
}

/******************************************************************************/
START_TEST(test_open_starts_exchange)
{
    ck_assert_int_eq(v->clip_chanid, CHANNEL);
    ck_assert_int_eq(server.pdus, 2);
    ck_assert_int_eq(server.pdu_types[0], CB_CLIP_CAPS);
    ck_assert_int_eq(server.pdu_types[1], CB_MONITOR_READY);
}
END_TEST

/******************************************************************************/
/* UTF-16 text from the client reaches the session as UTF-8, with CR LF
 * as LF, and characters outside the BMP from their surrogate pairs */
START_TEST(test_client_text_to_session)
{
    /* "a" CR LF "b" e-acute U+1F600, then the terminator */
    static const char text[] =
    {
        'a', 0, '\r', 0, '\n', 0, 'b', 0, (char)0xe9, 0,
        0x3d, (char)0xd8, 0x00, (char)0xde, 0, 0
    };

    client_format_list();
    ck_assert_int_eq(server.pdus, 4);
    ck_assert_int_eq(server.pdu_types[2], CB_FORMAT_LIST_RESPONSE);
    ck_assert_int_eq(server.pdu_types[3], CB_FORMAT_DATA_REQUEST);

    send_from_client(CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, text,
                     sizeof(text));
    ck_assert_str_eq(mock.clip_text, "a\nb\xc3\xa9\xf0\x9f\x98\x80");
}
END_TEST

/******************************************************************************/
/* UTF-8 text from the session reaches the client as UTF-16, LF as CR LF */
START_TEST(test_session_text_to_client)
{
    static const char want[] =
    {
        'x', 0, '\r', 0, '\n', 0, 'y', 0, (char)0xac, 0x20, 0, 0
    };
    static const char request[] = { CF_UNICODETEXT, 0, 0, 0 };

    client_format_list();  /* the exchange has started */
    wlup_clip_session_text(v, "x\ny\xe2\x82\xac", 6);
    ck_assert_int_eq(server.pdu_types[server.pdus - 1], CB_FORMAT_LIST);

    send_from_client(CB_FORMAT_DATA_REQUEST, 0, request, sizeof(request));
    ck_assert_int_eq(server.pdu_types[server.pdus - 1],
                     CB_FORMAT_DATA_RESPONSE);
    ck_assert_int_eq(server.last_pdu_len, 8 + (int)sizeof(want));
    ck_assert_int_eq(memcmp(server.last_pdu + 8, want, sizeof(want)), 0);
}
END_TEST

START_TEST(test_unchanged_text_announced_once)
{
    int pdus;

    client_format_list();
    wlup_clip_session_text(v, "same", 4);
    pdus = server.pdus;
    wlup_clip_session_text(v, "same", 4);
    ck_assert_int_eq(server.pdus, pdus);
}
END_TEST

/******************************************************************************/
/* A PDU split over two channel chunks */
START_TEST(test_chunked_pdu)
{
    static const char list[] = { CF_UNICODETEXT, 0, 0, 0, 0, 0 };
    char pdu[64];
    int len = make_pdu(pdu, CB_FORMAT_LIST, 0, list, sizeof(list));

    ck_assert_int_eq(wlup_clip_process_channel_data(v, pdu, 5, len,
                     XR_CHANNEL_FLAG_FIRST), 0);
    ck_assert_int_eq(server.pdus, 2);
    ck_assert_int_eq(wlup_clip_process_channel_data(v, pdu + 5, len - 5, len,
                     XR_CHANNEL_FLAG_LAST), 0);
    ck_assert_int_eq(server.pdus, 4);
    ck_assert_int_eq(server.pdu_types[3], CB_FORMAT_DATA_REQUEST);
}
END_TEST

/******************************************************************************/
START_TEST(test_read_fd)
{
    int fds[2];
    char *data;
    int len = -1;

    ck_assert_int_eq(pipe(fds), 0);
    ck_assert_int_eq(write(fds[1], "hello", 5), 5);
    close(fds[1]);
    data = wlup_clip_read_fd(fds[0], &len);
    ck_assert_int_eq(len, 5);
    ck_assert_int_eq(memcmp(data, "hello", 5), 0);
    g_free(data);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_wlup_clip(void)
{
    Suite *s = suite_create("wlup clipboard");
    TCase *tc = tcase_create("text");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_open_starts_exchange);
    tcase_add_test(tc, test_client_text_to_session);
    tcase_add_test(tc, test_session_text_to_client);
    tcase_add_test(tc, test_unchanged_text_announced_once);
    tcase_add_test(tc, test_chunked_pdu);
    tcase_add_test(tc, test_read_fd);
    suite_add_tcase(s, tc);
    return s;
}
