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
 * Tests for input validation of data arriving from the display backend
 * (xorgxrdp over the xup protocol). The backend runs as the session user
 * while xrdp runs with higher privilege, so every size and index in these
 * messages must be checked before use.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "test_xrdp.h"

/* The parsers under test are static */
#include "../../xup/xup.c"

#include "xrdp.h"
#include "trans.h"
#include <sys/ipc.h>
#include <sys/shm.h>

static int g_callback_calls;

static int
stub_set_cursor_ex(struct mod *v, int x, int y, char *data,
                   char *mask, int bpp)
{
    g_callback_calls++;
    return 0;
}

static int
stub_paint_rect(struct mod *v, int x, int y, int cx, int cy,
                char *data, int width, int height, int srcx, int srcy)
{
    g_callback_calls++;
    return 0;
}

static int
stub_add_char(struct mod *v, int font, int character,
              int offset, int baseline, int width, int height, char *data)
{
    g_callback_calls++;
    return 0;
}

static int
stub_paint_rects(struct mod *v, int num_drects, short *drects,
                 int num_crects, short *crects, char *data,
                 int width, int height, int flags, int frame_id)
{
    g_callback_calls++;
    return 0;
}

static struct mod *
make_mod(void)
{
    struct mod *mod = (struct mod *)mod_init();
    mod->server_set_cursor_ex = stub_set_cursor_ex;
    mod->server_paint_rect = stub_paint_rect;
    mod->server_add_char = stub_add_char;
    mod->server_paint_rects = stub_paint_rects;
    g_callback_calls = 0;
    return mod;
}

static void
finish_stream(struct stream *s)
{
    s_mark_end(s);
    s->p = s->data;
}

/******************************************************************************/
START_TEST(test_set_pointer_ex__rejects_out_of_range_bpp)
{
    struct mod *mod = make_mod();
    struct stream *s;
    int rv;

    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, 0);  /* x */
    out_uint16_le(s, 0);  /* y */
    out_uint16_le(s, 40); /* bpp: not a supported depth */
    out_uint8s(s, 64);
    finish_stream(s);

    rv = process_server_set_pointer_ex(mod, s);

    ck_assert_int_ne(rv, 0);
    ck_assert_int_eq(g_callback_calls, 0);
    free_stream(s);
    mod_exit((tintptr)mod);
}
END_TEST

/******************************************************************************/
START_TEST(test_paint_rect__rejects_short_bitmap_data)
{
    struct mod *mod = make_mod();
    struct stream *s;
    int rv;

    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, 0);       /* x */
    out_uint16_le(s, 0);       /* y */
    out_uint16_le(s, 16);      /* cx */
    out_uint16_le(s, 16);      /* cy */
    out_uint32_le(s, 100000);  /* len_bmpdata: more than is present */
    out_uint8s(s, 8);
    finish_stream(s);

    rv = process_server_paint_rect(mod, s);

    ck_assert_int_ne(rv, 0);
    ck_assert_int_eq(g_callback_calls, 0);
    free_stream(s);
    mod_exit((tintptr)mod);
}
END_TEST

/******************************************************************************/
START_TEST(test_add_char__rejects_data_shorter_than_geometry)
{
    struct mod *mod = make_mod();
    struct stream *s;
    int rv;

    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, 0);   /* font */
    out_uint16_le(s, 'A'); /* character */
    out_uint16_le(s, 0);   /* x */
    out_uint16_le(s, 0);   /* y */
    out_uint16_le(s, 64);  /* cx : 64x64 1bpp needs 512 bytes */
    out_uint16_le(s, 64);  /* cy */
    out_uint16_le(s, 4);   /* len_bmpdata */
    out_uint8s(s, 4);
    finish_stream(s);

    rv = process_server_add_char(mod, s);

    ck_assert_int_ne(rv, 0);
    ck_assert_int_eq(g_callback_calls, 0);
    free_stream(s);
    mod_exit((tintptr)mod);
}
END_TEST

/******************************************************************************/
/* The shared memory checks need to see the backend's uid, which comes
 * from the peer credentials of the backend socket */
static struct mod *
make_mod_with_peer(int *peer_sck)
{
    struct mod *mod = make_mod();
    int sck[2];

    ck_assert_int_eq(g_sck_local_socketpair(sck), 0);
    mod->trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
    mod->trans->sck = sck[0];
    *peer_sck = sck[1];
    return mod;
}

static struct stream *
make_shmem_ex_message(int shmid, int width, int height)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, 0);      /* num_drects */
    out_uint16_le(s, 0);      /* num_crects */
    out_uint32_le(s, 0);      /* flags */
    out_uint32_le(s, 1);      /* frame_id */
    out_uint32_le(s, shmid);  /* shmem_id */
    out_uint32_le(s, 0);      /* shmem_offset */
    out_uint16_le(s, width);
    out_uint16_le(s, height);
    finish_stream(s);
    return s;
}

START_TEST(test_paint_rect_shmem_ex__rejects_image_past_segment_end)
{
    int peer_sck;
    struct mod *mod = make_mod_with_peer(&peer_sck);
    struct stream *s;
    int shmid;
    int rv;

    shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    ck_assert_int_ne(shmid, -1);
    s = make_shmem_ex_message(shmid, 64, 64); /* 64x64x4 = 16384 > 4096 */

    rv = process_server_paint_rect_shmem_ex(mod, s);

    ck_assert_int_ne(rv, 0);
    ck_assert_int_eq(g_callback_calls, 0);

    shmctl(shmid, IPC_RMID, NULL);
    free_stream(s);
    g_file_close(peer_sck);
    mod_exit((tintptr)mod);
}
END_TEST

START_TEST(test_paint_rect_shmem_ex__accepts_image_within_segment)
{
    int peer_sck;
    struct mod *mod = make_mod_with_peer(&peer_sck);
    struct stream *s;
    int shmid;
    int rv;

    shmid = shmget(IPC_PRIVATE, 32 * 32 * 4, IPC_CREAT | 0600);
    ck_assert_int_ne(shmid, -1);
    s = make_shmem_ex_message(shmid, 32, 32);

    rv = process_server_paint_rect_shmem_ex(mod, s);

    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(g_callback_calls, 1);

    shmctl(shmid, IPC_RMID, NULL);
    free_stream(s);
    g_file_close(peer_sck);
    mod_exit((tintptr)mod);
}
END_TEST

/******************************************************************************/
START_TEST(test_xrdp_wm_pointer__rejects_pointer_larger_than_cache_entry)
{
    struct xrdp_wm *wm = g_new0(struct xrdp_wm, 1);
    char data[32 * 32 * 4];
    char mask[32 * 32 / 8];
    int rv;

    rv = xrdp_wm_pointer(wm, data, mask, 0, 0, 32, 128, 128);

    ck_assert_int_ne(rv, 0);
    g_free(wm);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_backend_input(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("test_backend_input");

    tc = tcase_create("xup_parsers");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_set_pointer_ex__rejects_out_of_range_bpp);
    tcase_add_test(tc, test_paint_rect__rejects_short_bitmap_data);
    tcase_add_test(tc, test_add_char__rejects_data_shorter_than_geometry);
    tcase_add_test(tc,
                   test_paint_rect_shmem_ex__rejects_image_past_segment_end);
    tcase_add_test(tc,
                   test_paint_rect_shmem_ex__accepts_image_within_segment);

    tc = tcase_create("xrdp_wm_pointer");
    suite_add_tcase(s, tc);
    tcase_add_test(tc,
                   test_xrdp_wm_pointer__rejects_pointer_larger_than_cache_entry);

    return s;
}
