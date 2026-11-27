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
 * Tests for the [MS-RDPBCGR] 2.2.8.2.1 Server Set Keyboard Indicators PDU
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <sys/socket.h>
#include <unistd.h>

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "os_calls.h"
#include "trans.h"

#include "test_libxrdp.h"

/* A real libxrdp session whose transport is one end of a socketpair,
 * so the test can read back exactly what was put on the wire.
 * No client has connected, so there is no encryption and no compression. */
struct kbd_fixture
{
    struct xrdp_session *session;
    struct trans *trans;
    int peer_sck;
};

static void
init_fixture(struct kbd_fixture *f)
{
    int sck[2];

    ck_assert_int_eq(g_sck_local_socketpair(sck), 0);
    f->trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
    f->trans->sck = sck[0];
    f->trans->status = TRANS_STATUS_UP;
    f->peer_sck = sck[1];
    f->session = libxrdp_init(NULL, f->trans, "/nonexistent/xrdp.ini");
    f->session->up_and_running = 1;
}

static void
free_fixture(struct kbd_fixture *f)
{
    libxrdp_exit(f->session);
    trans_delete(f->trans); /* closes sck[0] */
    g_file_close(f->peer_sck);
}

/* Returns the number of bytes the session has written, 0 if none */
static int
read_sent(struct kbd_fixture *f, char *buf, int size)
{
    int n = recv(f->peer_sck, buf, size, MSG_DONTWAIT);
    return (n < 0) ? 0 : n;
}

/******************************************************************************/
START_TEST(test_set_keyboard_indicators__encodes_pdu)
{
    struct kbd_fixture f;
    char buf[256];
    int n;
    int mcs_channel;
    /* TS_SHARECONTROLHEADER + TS_SHAREDATAHEADER + payload */
    char expected[22] =
    {
        0x16, 0x00, /* totalLength 22 */
        0x17, 0x00, /* pduType : PDUTYPE_DATAPDU, version 1 */
        0x00, 0x00, /* pduSource : filled in below */
        (char)0xea, 0x03, 0x01, 0x00, /* shareID 66538 */
        0x00,       /* pad1 */
        0x01,       /* streamID : STREAM_LOW */
        0x16, 0x00, /* uncompressedLength 22 */
        PDUTYPE2_SET_KEYBOARD_INDICATORS, /* pduType2 */
        0x00,       /* compressedType */
        0x16, 0x00, /* compressedLength 22 */
        0x00, 0x00, /* UnitId 0 */
        TS_SYNC_CAPS_LOCK | TS_SYNC_NUM_LOCK, 0x00 /* LedFlags */
    };

    init_fixture(&f);
    mcs_channel = f.session->rdp->mcs_channel;
    expected[4] = mcs_channel & 0xff;
    expected[5] = (mcs_channel >> 8) & 0xff;

    /* Bits outside the four TS_SYNC_* lock bits must not reach the wire */
    ck_assert_int_eq(libxrdp_send_set_keyboard_indicators(
                         f.session,
                         0x100 | TS_SYNC_CAPS_LOCK | TS_SYNC_NUM_LOCK), 0);

    n = read_sent(&f, buf, sizeof(buf));
    /* TPKT(4) + X.224 DT(3) + MCS SDin(7) + headers and payload (22) */
    ck_assert_int_eq(n, 36);
    ck_assert_mem_eq(buf + n - sizeof(expected), expected, sizeof(expected));

    free_fixture(&f);
}
END_TEST

/******************************************************************************/
START_TEST(test_set_keyboard_indicators__not_sent_before_up_and_running)
{
    struct kbd_fixture f;
    char buf[256];

    init_fixture(&f);
    f.session->up_and_running = 0;

    ck_assert_int_eq(libxrdp_send_set_keyboard_indicators(
                         f.session, TS_SYNC_CAPS_LOCK), 0);
    ck_assert_int_eq(read_sent(&f, buf, sizeof(buf)), 0);

    free_fixture(&f);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_xrdp_keyboard_indicators(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("test_xrdp_keyboard_indicators");

    tc = tcase_create("libxrdp_send_set_keyboard_indicators");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_set_keyboard_indicators__encodes_pdu);
    tcase_add_test(tc,
                   test_set_keyboard_indicators__not_sent_before_up_and_running);

    return s;
}
