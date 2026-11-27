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
 *
 * Tests for the keyboard toggle-key (indicator) synchronisation events a
 * client sends, [MS-RDPBCGR] 2.2.8.1.1.3.1.1.5 (slow-path TS_SYNC_EVENT) and
 * 2.2.8.1.2.2.5 (fast-path TS_FP_SYNC_EVENT). Both must reach the session
 * callback as RDP_INPUT_SYNCHRONIZE with the toggleFlags in param1.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "os_calls.h"

#include "test_libxrdp.h"

static int g_calls;
static int g_msg;
static intptr_t g_param1;

static int
record_callback(struct xrdp_process *id, int msg, intptr_t param1,
                intptr_t param2, intptr_t param3, intptr_t param4)
{
    g_calls++;
    g_msg = msg;
    g_param1 = param1;
    return 0;
}

static struct xrdp_session g_session;

static void
setup(void)
{
    g_memset(&g_session, 0, sizeof(g_session));
    g_session.callback = record_callback;
    g_calls = 0;
    g_msg = -1;
    g_param1 = -1;
}

/* TS_FP_SYNC_EVENT is just the eventHeader: eventCode in the top 3 bits,
 * toggle flags in the low 5 bits, no payload */
START_TEST(test_fastpath_sync_event__passes_toggle_flags)
{
    struct xrdp_fastpath fp;
    struct stream *s;
    int flags;

    g_memset(&fp, 0, sizeof(fp));
    fp.session = &g_session;
    fp.numEvents = 1;

    for (flags = 0; flags <= 0x1f; ++flags)
    {
        make_stream(s);
        init_stream(s, 8);
        out_uint8(s, (FASTPATH_INPUT_EVENT_SYNC << 5) | flags);
        s_mark_end(s);
        s->p = s->data;

        g_calls = 0;
        ck_assert_int_eq(xrdp_fastpath_process_input_event(&fp, s), 0);
        ck_assert_int_eq(g_calls, 1);
        ck_assert_int_eq(g_msg, RDP_INPUT_SYNCHRONIZE);
        ck_assert_int_eq(g_param1, flags);
        free_stream(s);
    }
}
END_TEST

/* Slow-path TS_SYNC_EVENT inside a TS_INPUT_PDU_DATA: 2 bytes of padding
 * then a 4 byte toggleFlags field */
START_TEST(test_slowpath_sync_event__passes_toggle_flags)
{
    struct xrdp_rdp *rdp;
    struct stream *s;
    int flags;

    rdp = (struct xrdp_rdp *)g_malloc(sizeof(struct xrdp_rdp), 1);
    rdp->session = &g_session;

    for (flags = 0; flags <= 0xf; ++flags)
    {
        make_stream(s);
        init_stream(s, 64);
        /* TS_SHAREDATAHEADER */
        out_uint32_le(s, 0x1234); /* shareId */
        out_uint8(s, 0); /* pad1 */
        out_uint8(s, 1); /* streamId STREAM_LOW */
        out_uint16_le(s, 16); /* uncompressedLength */
        out_uint8(s, PDUTYPE2_INPUT);
        out_uint8(s, 0); /* compressedType */
        out_uint16_le(s, 0); /* compressedLength */
        /* TS_INPUT_PDU_DATA */
        out_uint16_le(s, 1); /* numEvents */
        out_uint16_le(s, 0); /* pad2Octets */
        /* TS_INPUT_EVENT */
        out_uint32_le(s, 0); /* eventTime */
        out_uint16_le(s, RDP_INPUT_SYNCHRONIZE); /* messageType */
        /* TS_SYNC_EVENT */
        out_uint16_le(s, 0); /* pad2Octets */
        out_uint32_le(s, flags); /* toggleFlags */
        s_mark_end(s);
        s->p = s->data;

        g_calls = 0;
        ck_assert_int_eq(xrdp_rdp_process_data(rdp, s), 0);
        ck_assert_int_eq(g_calls, 1);
        ck_assert_int_eq(g_msg, RDP_INPUT_SYNCHRONIZE);
        ck_assert_int_eq(g_param1, flags);
        free_stream(s);
    }

    g_free(rdp);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_input_sync(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("InputSync");

    tc = tcase_create("input_sync");
    tcase_add_checked_fixture(tc, setup, NULL);
    tcase_add_test(tc, test_fastpath_sync_event__passes_toggle_flags);
    tcase_add_test(tc, test_slowpath_sync_event__passes_toggle_flags);
    suite_add_tcase(s, tc);

    return s;
}
