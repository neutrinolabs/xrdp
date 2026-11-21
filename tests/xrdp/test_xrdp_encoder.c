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
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <string.h>

#include "test_xrdp.h"
#include "xrdp.h"
#include "xrdp_egfx.h"
#include "xrdp_encoder.h"
#include "thread_calls.h"

/* Forward declarations for the linker's thread-creation wrapper. */
int
__real_tc_thread_create(THREAD_RV (THREAD_CC *start_routine)(void *), void *arg);
int
__wrap_tc_thread_create(THREAD_RV (THREAD_CC *start_routine)(void *), void *arg);

#if defined(XRDP_OPENH264) || defined(XRDP_X264)
static int suppress_encoder_thread;
static struct
{
    struct xrdp_encoder *encoder;
    char *scratch;
    int creates;
    int encodes;
    int deletes;
} codec;
#endif

int
__wrap_tc_thread_create(THREAD_RV (THREAD_CC *start_routine)(void *), void *arg)
{
#if defined(XRDP_OPENH264) || defined(XRDP_X264)
    if (suppress_encoder_thread && start_routine == proc_enc_msg)
    {
        /* Invoke the real process_enc callback synchronously in this test. */
        codec.encoder = (struct xrdp_encoder *) arg;
        return 0;
    }
#endif
    return __real_tc_thread_create(start_routine, arg);
}

#if defined(XRDP_OPENH264) || defined(XRDP_X264)

/* One full-frame region produces a 14-byte AVC420 metadata block. */
#define AVC420_METADATA_BYTES 14
#define TEST_WIDTH 16
#define TEST_HEIGHT 16

static void *
codec_create(void)
{
    ++codec.creates;
    return &codec;
}

static int
codec_delete(void *handle)
{
    ck_assert_ptr_eq(handle, &codec);
    ++codec.deletes;
    return 0;
}

static int
codec_encode(void *handle, int session, int left, int top,
             int width, int height, int twidth, int theight,
             int format, const char *data, short *crects, int num_crects,
             char *cdata, int *cdata_bytes, int connection_type, int *flags_ptr)
{
    static const int lengths[] = {9, 23, 5};
    static const unsigned char values[] = {0x31, 0xee, 0x72};
    int call = codec.encodes++;

    ck_assert_ptr_eq(handle, &codec);
    ck_assert_int_lt(call, 3);
    ck_assert_int_eq(width, TEST_WIDTH);
    ck_assert_int_eq(height, TEST_HEIGHT);
    ck_assert_int_eq(num_crects, 1);
    ck_assert_int_eq(crects[0], 0);
    ck_assert_int_eq(crects[1], 0);
    ck_assert_int_eq(crects[2], TEST_WIDTH);
    ck_assert_int_eq(crects[3], TEST_HEIGHT);
    ck_assert_ptr_nonnull(codec.encoder->gfx_h264_buffer);
    if (call == 0)
    {
        codec.scratch = codec.encoder->gfx_h264_buffer;
    }
    ck_assert_ptr_eq(codec.encoder->gfx_h264_buffer, codec.scratch);
    ck_assert_ptr_eq(cdata, codec.scratch + AVC420_METADATA_BYTES);
    ck_assert_int_eq(*cdata_bytes,
                     codec.encoder->max_compressed_bytes - AVC420_METADATA_BYTES);

    /* The failed call dirties the scratch buffer and changes the length.
     * The shorter retry must not publish any of these failed bytes. */
    memset(cdata, values[call], lengths[call]);
    *cdata_bytes = lengths[call];
    return call == 1 ? 3 : 0;
}

static void
make_command(struct stream *s)
{
    out_uint16_le(s, XR_RDPGFX_CMDID_WIRETOSURFACE_1);
    out_uint16_le(s, 0); /* command flags */
    out_uint32_le(s, 45); /* command bytes */
    out_uint16_le(s, 1); /* surface ID */
    out_uint16_le(s, XR_RDPGFX_CODECID_AVC420);
    out_uint8(s, XR_PIXEL_FORMAT_XRGB_8888);
    out_uint32_le(s, 0); /* raw NV12 input, monitor zero */
    out_uint16_le(s, 1); /* dirty rectangle count */
    out_uint16_le(s, 0);
    out_uint16_le(s, 0);
    out_uint16_le(s, TEST_WIDTH);
    out_uint16_le(s, TEST_HEIGHT);
    out_uint16_le(s, 1); /* copy rectangle count */
    out_uint16_le(s, 0);
    out_uint16_le(s, 0);
    out_uint16_le(s, TEST_WIDTH);
    out_uint16_le(s, TEST_HEIGHT);
    out_uint16_le(s, 0); /* frame left */
    out_uint16_le(s, 0); /* frame top */
    out_uint16_le(s, TEST_WIDTH);
    out_uint16_le(s, TEST_HEIGHT);
    s_mark_end(s);
}

static void
check_output(const XRDP_ENC_DATA_DONE *done, int payload_bytes,
             unsigned char payload_value)
{
    static const unsigned char metadata[AVC420_METADATA_BYTES] =
    {
        1, 0, 0, 0, /* region count */
        0, 0, 0, 0, TEST_WIDTH, 0, TEST_HEIGHT, 0,
        23, 100 /* quantization parameter and quality */
    };
    int i;

    ck_assert_ptr_nonnull(done);
    ck_assert_int_eq(done->last, 1);
    ck_assert_int_eq(done->pad_bytes, 0);
    ck_assert(ENC_IS_BIT_SET(done->flags, ENC_DONE_FLAGS_GFX_BIT));
    /* The segmented GFX headers occupy 42 bytes for these small payloads. */
    ck_assert_int_eq(done->comp_bytes, 42 + AVC420_METADATA_BYTES + payload_bytes);
    ck_assert_mem_eq(done->comp_pad_data + 42, metadata, sizeof(metadata));
    for (i = done->comp_bytes - payload_bytes; i < done->comp_bytes; ++i)
    {
        ck_assert_int_eq((unsigned char) done->comp_pad_data[i], payload_value);
    }
}

START_TEST(test_gfx_h264_scratch_reuse_after_encode_error)
{
    struct xrdp_client_info client_info = {0};
    struct xrdp_wm wm = {0};
    struct xrdp_mm mm = {0};
    struct xrdp_egfx egfx = {0};
    struct xrdp_egfx_bulk bulk = {0};
    struct xrdp_encoder *encoder;
    struct stream command = {0};
    XRDP_ENC_DATA input = {0};
    XRDP_ENC_DATA_DONE *first;
    XRDP_ENC_DATA_DONE *retry;
    char command_data[45];
    char nv12[TEST_WIDTH * TEST_HEIGHT * 3 / 2] = {0};
    char first_copy[42 + AVC420_METADATA_BYTES + 9];

    memset(&codec, 0, sizeof(codec));
    client_info.bpp = 32;
    client_info.gfx = 1;
    client_info.mcs_connection_type = CONNECTION_TYPE_LAN;
    wm.client_info = &client_info;
    mm.wm = &wm;
    mm.libh264_loaded = 1;
    mm.egfx_flags = XRDP_EGFX_H264;
    mm.egfx = &egfx;
    egfx.bulk = &bulk;

    suppress_encoder_thread = 1;
    encoder = xrdp_encoder_create(&mm);
    suppress_encoder_thread = 0;
    ck_assert_ptr_nonnull(encoder);
    ck_assert_ptr_eq(codec.encoder, encoder);
    encoder->xrdp_encoder_h264_create = codec_create;
    encoder->xrdp_encoder_h264_encode = codec_encode;
    encoder->xrdp_encoder_h264_delete = codec_delete;

    command.data = command.p = command_data;
    command.size = sizeof(command_data);
    make_command(&command);
    ck_assert_int_eq(command.end - command.data, sizeof(command_data));
    ENC_SET_BIT(input.flags, ENC_FLAGS_GFX_BIT);
    input.u.gfx.cmd = command_data;
    input.u.gfx.cmd_bytes = sizeof(command_data);
    input.u.gfx.data = nv12;
    input.u.gfx.data_bytes = sizeof(nv12);

    ck_assert_int_eq(encoder->process_enc(encoder, &input), 0);
    ck_assert_int_eq(codec.encodes, 1);
    first = (XRDP_ENC_DATA_DONE *) fifo_remove_item(encoder->fifo_processed);
    check_output(first, 9, 0x31);
    memcpy(first_copy, first->comp_pad_data, sizeof(first_copy));
    ck_assert(fifo_is_empty(encoder->fifo_processed));
    ck_assert(fifo_add_item(encoder->fifo_processed, first));

    /* Retain the successful output while a later encode fails. The dispatcher
     * returns zero on this error; no new output or completion signal is valid. */
    g_reset_wait_obj(encoder->xrdp_encoder_event_processed);
    ck_assert_int_eq(encoder->process_enc(encoder, &input), 0);
    ck_assert_int_eq(codec.encodes, 2);
    ck_assert_ptr_eq(encoder->gfx_h264_buffer, codec.scratch);
    ck_assert(!g_is_wait_obj_set(encoder->xrdp_encoder_event_processed));
    ck_assert_ptr_eq(fifo_remove_item(encoder->fifo_processed), first);
    ck_assert(fifo_is_empty(encoder->fifo_processed));
    ck_assert_mem_eq(first->comp_pad_data, first_copy, sizeof(first_copy));
    ck_assert(fifo_add_item(encoder->fifo_processed, first));

    ck_assert_int_eq(encoder->process_enc(encoder, &input), 0);
    ck_assert_int_eq(codec.encodes, 3);
    ck_assert_int_eq(codec.creates, 1);
    ck_assert(g_is_wait_obj_set(encoder->xrdp_encoder_event_processed));
    ck_assert_ptr_eq(fifo_remove_item(encoder->fifo_processed), first);
    ck_assert_mem_eq(first->comp_pad_data, first_copy, sizeof(first_copy));
    retry = (XRDP_ENC_DATA_DONE *) fifo_remove_item(encoder->fifo_processed);
    check_output(retry, 5, 0x72);
    ck_assert_ptr_ne(retry->comp_pad_data, first->comp_pad_data);
    ck_assert_ptr_ne(retry->comp_pad_data, encoder->gfx_h264_buffer);
    ck_assert(fifo_is_empty(encoder->fifo_processed));

    /* Exercise the real destructor, including both pending output buffers. */
    ck_assert(fifo_add_item(encoder->fifo_processed, first));
    ck_assert(fifo_add_item(encoder->fifo_processed, retry));
    g_set_wait_obj(encoder->xrdp_encoder_term_done);
    xrdp_encoder_delete(encoder);
    ck_assert_int_eq(codec.deletes, 1);
}
END_TEST

Suite *
make_suite_encoder(void)
{
    Suite *s = suite_create("test_xrdp_encoder");
    TCase *tc = tcase_create("gfx_h264_scratch");

    tcase_add_test(tc, test_gfx_h264_scratch_reuse_after_encode_error);
    suite_add_tcase(s, tc);
    return s;
}

#endif
