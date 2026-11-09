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
 * Tests that deleting an encoder releases the shared memory frames that are
 * still queued in it.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "arch.h"
#include "os_calls.h"
#include "fifo.h"
#include "thread_calls.h"
#include "ms-rdpbcgr.h"
#include "xrdp.h"
#include "xrdp_encoder.h"
#include "test_xrdp.h"

#define NUM_FAKE_FRAMES 4

/* Stand-ins for the shared memory frames xorgxrdp hands to xrdp. They
 * are never mapped; the wrapper below only records the addresses. */
static char g_fake_frames[NUM_FAKE_FRAMES][16];
static void *g_unmapped[NUM_FAKE_FRAMES];
static size_t g_unmapped_length[NUM_FAKE_FRAMES];
static int g_unmapped_count;   /* every call, including ones not stored */

static int
is_fake_frame(const void *addr)
{
    int i;

    for (i = 0; i < NUM_FAKE_FRAMES; ++i)
    {
        if (addr == (const void *)g_fake_frames[i])
        {
            return 1;
        }
    }
    return 0;
}

static int
unmap_count_for(int frame)
{
    int i;
    int n = 0;

    for (i = 0; i < g_unmapped_count && i < NUM_FAKE_FRAMES; ++i)
    {
        if (g_unmapped[i] == (void *)g_fake_frames[frame])
        {
            /* a mapping is only released by unmapping all of it */
            ck_assert_uint_eq(g_unmapped_length[i],
                              sizeof(g_fake_frames[frame]));
            ++n;
        }
    }
    return n;
}

/* Linked in with -Wl,--wrap=g_munmap (see Makefile.am) */
extern int __real_g_munmap(void *addr, size_t length);
extern int __wrap_g_munmap(void *addr, size_t length);
int
__wrap_g_munmap(void *addr, size_t length)
{
    if (!is_fake_frame(addr))
    {
        return __real_g_munmap(addr, length);
    }
    if (g_unmapped_count < NUM_FAKE_FRAMES)
    {
        g_unmapped_length[g_unmapped_count] = length;
        g_unmapped[g_unmapped_count] = addr;
    }
    ++g_unmapped_count;
    return 0;
}

struct encoder_fixture
{
    struct xrdp_mm mm;
    struct xrdp_wm wm;
    struct xrdp_client_info client_info;
    struct xrdp_encoder *encoder;
};

static void
encoder_fixture_setup(struct encoder_fixture *f)
{
    g_memset(f, 0, sizeof(*f));
    f->mm.wm = &f->wm;
    f->wm.client_info = &f->client_info;
    f->client_info.bpp = 32;
    f->client_info.jpeg_codec_id = 1;
    f->client_info.mcs_connection_type = CONNECTION_TYPE_LAN;
    g_unmapped_count = 0;

    /* The jpeg codec is used because creating it does not read mm->wm->screen,
     * which the fixture does not have. The encoder thread is started but never
     * signalled, so it does not touch the fake commands queued by the tests;
     * processing them would need a real mm. */
    f->encoder = xrdp_encoder_create(&f->mm);
    ck_assert_ptr_nonnull(f->encoder);
}

static XRDP_ENC_DATA *
new_command(int frame, int gfx)
{
    XRDP_ENC_DATA *enc = g_new0(XRDP_ENC_DATA, 1);

    ck_assert_ptr_nonnull(enc);
    enc->shmem_ptr = g_fake_frames[frame];
    enc->shmem_bytes = (int)sizeof(g_fake_frames[frame]);
    if (gfx)
    {
        ENC_SET_BIT(enc->flags, ENC_FLAGS_GFX_BIT);
        enc->u.gfx.cmd = g_new0(char, 16);
        ck_assert_ptr_nonnull(enc->u.gfx.cmd);
    }
    else
    {
        enc->u.sc.drects = g_new0(short, 4);
        enc->u.sc.crects = g_new0(short, 4);
        ck_assert_ptr_nonnull(enc->u.sc.drects);
        ck_assert_ptr_nonnull(enc->u.sc.crects);
    }
    return enc;
}

static void
queue_result(struct encoder_fixture *f, XRDP_ENC_DATA *enc, int last)
{
    XRDP_ENC_DATA_DONE *done = g_new0(XRDP_ENC_DATA_DONE, 1);

    ck_assert_ptr_nonnull(done);
    done->enc = enc;
    done->last = last;
    tc_mutex_lock(f->encoder->mutex);
    fifo_add_item(f->encoder->fifo_processed, done);
    tc_mutex_unlock(f->encoder->mutex);
}

/******************************************************************************/
/* A desktop resize deletes the encoder while frames are still waiting to be
 * encoded. Each of those frames owns a shared memory mapping. */
START_TEST(test_xrdp_encoder_delete__unmaps_queued_frames)
{
    struct encoder_fixture f;

    encoder_fixture_setup(&f);

    /* Both kinds of command: a surface command and an EGFX command */
    tc_mutex_lock(f.encoder->mutex);
    fifo_add_item(f.encoder->fifo_to_proc, new_command(0, 0));
    fifo_add_item(f.encoder->fifo_to_proc, new_command(1, 1));
    tc_mutex_unlock(f.encoder->mutex);

    xrdp_encoder_delete(f.encoder);

    ck_assert_int_eq(g_unmapped_count, 2);
    ck_assert_int_eq(unmap_count_for(0), 1);
    ck_assert_int_eq(unmap_count_for(1), 1);
}
END_TEST

/******************************************************************************/
/* Several results can belong to one command, for example the tiles of a
 * frame. The command is owned by the result marked as the last one. It is
 * released with it and, as the other results only refer to it, only once. */
START_TEST(test_xrdp_encoder_delete__unmaps_shared_command_once)
{
    struct encoder_fixture f;
    XRDP_ENC_DATA *enc;

    encoder_fixture_setup(&f);

    enc = new_command(0, 1);
    queue_result(&f, enc, 0);
    queue_result(&f, enc, 0);
    queue_result(&f, enc, 1);

    xrdp_encoder_delete(f.encoder);

    ck_assert_int_eq(g_unmapped_count, 1);
    ck_assert_int_eq(unmap_count_for(0), 1);
}
END_TEST

/******************************************************************************/
/* The function behind the destructors and xrdp_mm_process_enc_done(). Every
 * kind of command, with and without a shared memory frame, and none. */
START_TEST(test_xrdp_enc_data_delete__all_kinds)
{
    XRDP_ENC_DATA *enc;

    g_unmapped_count = 0;

    xrdp_enc_data_delete(new_command(0, 0));
    xrdp_enc_data_delete(new_command(1, 1));
    ck_assert_int_eq(g_unmapped_count, 2);
    ck_assert_int_eq(unmap_count_for(0), 1);
    ck_assert_int_eq(unmap_count_for(1), 1);

    /* no frame to unmap, as for a command made from a plain paint */
    enc = new_command(2, 0);
    enc->shmem_ptr = NULL;
    xrdp_enc_data_delete(enc);
    enc = new_command(2, 1);
    enc->shmem_ptr = NULL;
    xrdp_enc_data_delete(enc);
    ck_assert_int_eq(g_unmapped_count, 2);

    xrdp_enc_data_delete(NULL);
    ck_assert_int_eq(g_unmapped_count, 2);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_encoder_teardown(void)
{
    Suite *s;
    TCase *tc_teardown;

    s = suite_create("test_xrdp_encoder_teardown");

    tc_teardown = tcase_create("xrdp_encoder_teardown");
    tcase_add_test(tc_teardown,
                   test_xrdp_encoder_delete__unmaps_queued_frames);
    tcase_add_test(tc_teardown,
                   test_xrdp_encoder_delete__unmaps_shared_command_once);
    tcase_add_test(tc_teardown,
                   test_xrdp_enc_data_delete__all_kinds);
    suite_add_tcase(s, tc_teardown);

    return s;
}
