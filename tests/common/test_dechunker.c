/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2006-2026
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
 */

/**
 * @file test_dechunker.c
 * @brief Test functiond for dechunker code
 * @author Matt Burt
 *
 */


#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "dechunker.h"
#include "ms-rdpbcgr.h"
#include "os_calls.h"
#include "parse.h"

#include "test_common.h"

// Chapter 1 of Mary Shelley's Frankenstein (thanks to Project Gutenberg)
static const char frankenstein[] =
    "Letter 1\n\n"
    "To Mrs. Saville, England.\n\n"
    "St. Petersburgh, Dec. 11th, 17—.\n"
    "You will rejoice to hear that no disaster has accompanied the "
    "commencement of an enterprise which you have regarded with such evil "
    "forebodings. I arrived here yesterday, and my first task is to assure "
    "my dear sister of my welfare and increasing confidence in the success "
    "of my undertaking.\n"
    "I am already far north of London, and as I walk in the streets of "
    "Petersburgh, I feel a cold northern breeze play upon my cheeks, which "
    "braces my nerves and fills me with delight. Do you understand this "
    "feeling? This breeze, which has travelled from the regions towards which "
    "I am advancing, gives me a foretaste of those icy climes. Inspirited "
    "by this wind of promise, my daydreams become more fervent and vivid. I "
    "try in vain to be persuaded that the pole is the seat of frost and "
    "desolation; it ever presents itself to my imagination as the region "
    "of beauty and delight. There, Margaret, the sun is for ever visible, "
    "its broad disk just skirting the horizon and diffusing a perpetual "
    "splendour. There—for with your leave, my sister, I will put some trust "
    "in preceding navigators—there snow and frost are banished; and, sailing "
    "over a calm sea, we may be wafted to a land surpassing in wonders and "
    "in beauty every region hitherto discovered on the habitable globe. Its "
    "productions and features may be without example, as the phenomena of the "
    "heavenly bodies undoubtedly are in those undiscovered solitudes. What "
    "may not be expected in a country of eternal light? I may there discover "
    "the wondrous power which attracts the needle and may regulate a thousand "
    "celestial observations that require only this voyage to render their "
    "seeming eccentricities consistent for ever. I shall satiate my ardent "
    "curiosity with the sight of a part of the world never before visited, "
    "and may tread a land never before imprinted by the foot of man. These "
    "are my enticements, and they are sufficient to conquer all fear of danger "
    "or death and to induce me to commence this laborious voyage with the joy "
    "a child feels when he embarks in a little boat, with his holiday mates, "
    "on an expedition of discovery up his native river. But supposing all "
    "these conjectures to be false, you cannot contest the inestimable benefit "
    "which I shall confer on all mankind, to the last generation, by "
    "discovering a passage near the pole to those countries, to reach which at "
    "present so many months are requisite; or by ascertaining the secret of "
    "the magnet, which, if at all possible, can only be effected by an "
    " undertaking such as mine.\n"
    "These reflections have dispelled the agitation with which I began my "
    "letter, and I feel my heart glow with an enthusiasm which elevates me "
    "to heaven, for nothing contributes so much to tranquillise the mind as "
    "a steady purpose—a point on which the soul may fix its intellectual "
    "eye. This expedition has been the favourite dream of my early years. I "
    "have read with ardour the accounts of the various voyages which have "
    "been made in the prospect of arriving at the North Pacific Ocean through "
    "the seas which surround the pole. You may remember that a history of "
    "all the voyages made for purposes of discovery composed the whole of "
    "our good Uncle Thomas’ library. My education was neglected, yet I was "
    "passionately fond of reading. These volumes were my study day and night, "
    "and my familiarity with them increased that regret which I had felt, as "
    "a child, on learning that my father’s dying injunction had forbidden "
    "my uncle to allow me to embark in a seafaring life.\n"
    "These visions faded when I perused, for the first time, those poets whose "
    "effusions entranced my soul and lifted it to heaven. I also became a poet "
    "and for one year lived in a paradise of my own creation; I imagined that "
    "I also might obtain a niche in the temple where the names of Homer and "
    "Shakespeare are consecrated. You are well acquainted with my failure and "
    "how heavily I bore the disappointment. But just at that time I inherited "
    "the fortune of my cousin, and my thoughts were turned into the channel "
    "of their earlier bent.\n"
    "Six years have passed since I resolved on my present undertaking. I can, "
    "even now, remember the hour from which I dedicated myself to this great "
    "enterprise. I commenced by inuring my body to hardship. I accompanied "
    "the whale-fishers on several expeditions to the North Sea; I voluntarily "
    "endured cold, famine, thirst, and want of sleep; I often worked harder "
    "than the common sailors during the day and devoted my nights to the "
    "study of mathematics, the theory of medicine, and those branches of "
    "physical science from which a naval adventurer might derive the greatest "
    "practical advantage. Twice I actually hired myself as an under-mate in "
    "a Greenland whaler, and acquitted myself to admiration. I must own I "
    "felt a little proud when my captain offered me the second dignity in "
    "the vessel and entreated me to remain with the greatest earnestness, "
    "so valuable did he consider my services.\n"
    "And now, dear Margaret, do I not deserve to accomplish some great "
    "purpose? My life might have been passed in ease and luxury, but I "
    "preferred glory to every enticement that wealth placed in my path. Oh, "
    "that some encouraging voice would answer in the affirmative! My courage "
    "and my resolution is firm; but my hopes fluctuate, and my spirits are "
    "often depressed. I am about to proceed on a long and difficult voyage, "
    "the emergencies of which will demand all my fortitude: I am required not "
    "only to raise the spirits of others, but sometimes to sustain my own, "
    "when theirs are failing.\n"
    "This is the most favourable period for travelling in Russia. They fly "
    "quickly over the snow in their sledges; the motion is pleasant, and, in "
    "my opinion, far more agreeable than that of an English stagecoach. The "
    "cold is not excessive, if you are wrapped in furs—a dress which I have "
    "already adopted, for there is a great difference between walking the deck "
    "and remaining seated motionless for hours, when no exercise prevents "
    "the blood from actually freezing in your veins. I have no ambition to "
    "lose my life on the post-road between St. Petersburgh and Archangel.\n"
    "I shall depart for the latter town in a fortnight or three weeks; and my "
    "intention is to hire a ship there, which can easily be done by paying "
    "the insurance for the owner, and to engage as many sailors as I think "
    "necessary among those who are accustomed to the whale-fishing. I do not "
    "intend to sail until the month of June; and when shall I return? Ah, "
    "dear sister, how can I answer this question? If I succeed, many, many "
    "months, perhaps years, will pass before you and I may meet. If I fail, "
    "you will see me again soon, or never.\n"
    "Farewell, my dear, excellent Margaret. Heaven shower down blessings on "
    "you, and save me, that I may again and again testify my gratitude for "
    "all your love and kindness.\n\n"
    "Your affectionate brother,\nR. Walton ";

// Number of CHANNEL_CHUNK_LENGTH (1600) chunks required to send the
// above text into the vc dechunker
#define FRANKENSTEIN_VC_CHUNK_COUNT \
    ((sizeof(frankenstein) + (CHANNEL_CHUNK_LENGTH -1)) \
     / CHANNEL_CHUNK_LENGTH)

// The dynamic dechunker works on total data block sizes of 1600 bytes,
// including the block header as well.
// The FIRST block header is 6-12 bytes long, and the DATA block header
// is 5-8 bytes long. For simplicity we're assume a header size of 8
// bytes, and hence a data size of 1592 bytes.
#define FRANKENSTEIN_DYN_CHUNK_SIZE 1592

#define FRANKENSTEIN_DYN_CHUNK_COUNT \
    ((sizeof(frankenstein) + (FRANKENSTEIN_DYN_CHUNK_SIZE -1)) \
     / FRANKENSTEIN_DYN_CHUNK_SIZE)

// See the private E_MAX_VC_CHUNK_SIZE_LOWER_LIMIT in dechunker.c
#define PAD50 "                                                  "

/******************************************************************************/
/*
  * Constructs a stream from static data
  *
  * The returned stream is suitable for reading.
  */
static struct stream *
make_stream_from_data(const char *data, int data_len)
{
    struct stream *s;
    make_stream(s);
    init_stream(s, data_len);
    s_push_layer(s, iso_hdr, 0);
    out_uint8p(s, data, data_len);
    s_mark_end(s);
    s_pop_layer(s, iso_hdr);
    return s;
}

/******************************************************************************/
/*
 * Check bad parameters passed to the dechunker functions
 */
START_TEST(test_vc_dechunker_bad_params)
{
    struct vc_dechunker *dc;
    const char data[] = "Some stream data";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    // vc_dechunker_init
    dc = vc_dechunker_init(NULL, 1000); // No channel name
    ck_assert_ptr_eq(dc, NULL);
    dc = vc_dechunker_init("test", 1); // max chunk size too small
    ck_assert_ptr_eq(dc, NULL);
    dc = vc_dechunker_init("test", 1000); // Should be OK
    ck_assert_ptr_ne(dc, NULL);

    // vc_dechunker_free
    vc_dechunker_free(NULL);   // Musn't crash!

    // vc_dechunker_get_stream
    vc_dechunker_get_stream(NULL);   // Musn't crash!

    // vc_dechunker_process_chunk
    stat = vc_dechunker_process_chunk(NULL, s, 0, 1600); // No dechunker
    ck_assert_int_eq(stat, E_VC_ERROR);
    stat = vc_dechunker_process_chunk(dc, NULL, 0, 1600); // No stream
    ck_assert_int_eq(stat, E_VC_ERROR);
    stat = vc_dechunker_process_chunk(dc, s, 0, -1); // bad total_size
    ck_assert_int_eq(stat, E_VC_ERROR);

    free_stream(s);
    vc_dechunker_free(dc);
}

/******************************************************************************/
/*
 * Check passthrough chunks (i.e. those with FIRST and LAST bits set)
 *
 * When the dechunker is in normal operation, these chunks are
 * immediately returned to the caller with E_VC_INLINE_CHUNK. If
 * however, we are currently dechunking, a passthrough chunk is not allowed
 */
START_TEST(test_vc_dechunker_passthrough)
{
    const char data[] = "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    struct vc_dechunker *dc = vc_dechunker_init("test", 1000);
    ck_assert_ptr_ne(dc, NULL);

    // Save the stream pointer so we can reset the stream in between calls
    s_push_layer(s, iso_hdr, 0);

    // Check a passthrough chunk is normally recognised immediately
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST,
               s->size);
    ck_assert_int_eq(stat, E_VC_INLINE_CHUNK);

    // Check a passthrough chunk after a first chunk generates an error
    // The total size passed for the first chunk must be bigger than the
    // dechunker chunking_size
    s_pop_layer(s, iso_hdr);  // Restore the stream pointer
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST,
               2000);
    ck_assert_int_eq(stat, E_VC_IN_PROGRESS);

    s_pop_layer(s, iso_hdr);
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST,
               s->size);
    ck_assert_int_eq(stat, E_VC_ERROR);

    // Check a passthrough chunk is accepted after the error
    // (i.e. the dechunker can still be used if required)
    s_pop_layer(s, iso_hdr);
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST,
               s->size);
    ck_assert_int_eq(stat, E_VC_INLINE_CHUNK);

    vc_dechunker_free(dc);
    free_stream(s);
}
END_TEST


/******************************************************************************/
/*
 * An intermediate chunk (neither first of last) must be rejected if we
 * are not dechunking
 */
START_TEST(test_vc_dechunker_intermediate)
{
    const char data[] = PAD50 "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    struct vc_dechunker *dc = vc_dechunker_init("test", sizeof(data));
    ck_assert_ptr_ne(dc, NULL);

    // Check an intermediate chunk is immediately rejected
    stat = vc_dechunker_process_chunk(
               dc, s,
               0,
               s->size + 1);
    ck_assert_int_eq(stat, E_VC_ERROR);

    vc_dechunker_free(dc);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/*
 * A LAST chunk must be rejected if we are not dechunking
 */
START_TEST(test_vc_dechunker_last)
{
    const char data[] = PAD50 "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    struct vc_dechunker *dc = vc_dechunker_init("test", sizeof(data));
    ck_assert_ptr_ne(dc, NULL);

    // Check a LAST chunk is immediately rejected
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_LAST,
               s->size + 1);
    ck_assert_int_eq(stat, E_VC_ERROR);

    vc_dechunker_free(dc);
    free_stream(s);
}
END_TEST

/******************************************************************************/
/**
 * Two consecutive FIRST chunks are not allowed
 */
START_TEST(test_vc_dechunker_first_first)
{
    const char data[] = PAD50 "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    struct vc_dechunker *dc = vc_dechunker_init("test", sizeof(data));
    ck_assert_ptr_ne(dc, NULL);

    // Save the stream pointer so we can reset the stream in between calls
    s_push_layer(s, iso_hdr, 0);

    // Check a FIRST chunk is accepted...
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST,
               s->size + 1);
    ck_assert_int_eq(stat, E_VC_IN_PROGRESS);

    // ... and another FIRST chunk is an error
    s_pop_layer(s, iso_hdr);
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST,
               s->size + 1);
    ck_assert_int_eq(stat, E_VC_ERROR);

    vc_dechunker_free(dc);
    free_stream(s);
}
END_TEST


/******************************************************************************/
/**
 * Checks that a FIRST chunk cannot be the total_size. This follows from
 * [MS-RDPBCGR] 3.1.5.2.1:
 *
 * > If the total size of the virtual channel data is larger than
 * > the chunk size, then each chunk MUST be sent in a separate Virtual
 * > Channel PDU.
*
*  > Virtual channel data that fits in a single Virtual Channel PDU MUST
*  > specify both flags
 */
START_TEST(test_vc_dechunker_chunk_overflow)
{
    const char data[] = PAD50 "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum vc_dechunker_status stat;

    struct vc_dechunker *dc = vc_dechunker_init("test", sizeof(data));
    ck_assert_ptr_ne(dc, NULL);

    // Save the stream pointer so we can reset the stream in between calls
    s_push_layer(s, iso_hdr, 0);

    // Check a FIRST chunk the same size as the total size is rejected
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST,
               s->size);
    ck_assert_int_eq(stat, E_VC_ERROR);

    // Check a FIRST chunk bigger than the total size is rejected
    // [MS-RDPBCGR] 3.1.5.2.1
    vc_dechunker_free(dc);
    dc = vc_dechunker_init("test", sizeof(data));
    ck_assert_ptr_ne(dc, NULL);
    s_pop_layer(s, iso_hdr);
    stat = vc_dechunker_process_chunk(
               dc, s,
               XR_CHANNEL_FLAG_FIRST,
               s->size - 1);
    ck_assert_int_eq(stat, E_VC_ERROR);

    vc_dechunker_free(dc);
    free_stream(s);
}
END_TEST

/******************************************************************************/
// Returns a stream with some random data, then a chunk of
// up to CHANNEL_CHUNK_LENGTH bytes from Frankenstein chapter 1
// The stream pointer will be positioned at the start of the text.
static struct stream *
make_vc_bigtest_chunk(unsigned int chunk_num, int *flags)
{
    struct stream *s = NULL;

    if (chunk_num < FRANKENSTEIN_VC_CHUNK_COUNT)
    {
        int chunk_size;
        // Work out the size of this chunk
        if (chunk_num == (FRANKENSTEIN_VC_CHUNK_COUNT - 1))
        {
            chunk_size = sizeof(frankenstein) % CHANNEL_CHUNK_LENGTH;
            if (chunk_size == 0)
            {
                chunk_size = CHANNEL_CHUNK_LENGTH;
            }
        }
        else
        {
            chunk_size = CHANNEL_CHUNK_LENGTH;
        }

        // Add some random data at the start of the stream, so we
        // can check the dechunker works for non-zero positioned
        // streams
        int rand_size = 4 * 4 * chunk_num;

        make_stream(s);
        init_stream(s, rand_size + chunk_size);

        // Write the random data
        int i;
        for (i = 0 ; i < rand_size; ++i)
        {
            out_uint8(s, rand() & 255);
        }
        s_push_layer(s, iso_hdr, 0);

        // Copy the chapter data
        out_uint8a(s, &frankenstein[chunk_num * CHANNEL_CHUNK_LENGTH],
                   chunk_size);

        // Get the stream ready for reading from the Frankenstein text
        s_mark_end(s);
        s_pop_layer(s, iso_hdr);

        // Sort out the flags
        *flags =
            (chunk_num == 0) ? XR_CHANNEL_FLAG_FIRST :
            (chunk_num == (FRANKENSTEIN_VC_CHUNK_COUNT - 1)) ? XR_CHANNEL_FLAG_LAST :
            0;
    }

    return s;
}

/******************************************************************************/
/*
 * Streams a lot of data through the dechunker and checks it's all
 * assembled correctly at the end
 */
START_TEST(test_vc_dechunker_big_test)
{
    enum vc_dechunker_status stat;
    struct vc_dechunker *dc = vc_dechunker_init("test", CHANNEL_CHUNK_LENGTH);
    ck_assert_ptr_ne(dc, NULL);
    struct stream *s;

    int i;
    for (i = 0 ; i < FRANKENSTEIN_VC_CHUNK_COUNT; ++i)
    {
        int flags;
        s = make_vc_bigtest_chunk(i, &flags);
        stat = vc_dechunker_process_chunk(
                   dc, s,
                   flags,
                   sizeof(frankenstein));
        if (i < FRANKENSTEIN_VC_CHUNK_COUNT - 1)
        {
            ck_assert_int_eq(stat, E_VC_IN_PROGRESS);
        }
        else
        {
            ck_assert_int_eq(stat, E_VC_READY);
        }
        free_stream(s);
    }

    // Check we have a result
    s = vc_dechunker_get_stream(dc);
    ck_assert_ptr_ne(s, NULL);

    // Is it the right size?
    int stream_size = s_rem(s);
    ck_assert_int_eq(stream_size, sizeof(frankenstein));

    // Check the data
    const char *p;
    in_uint8p(s, p, stream_size);
    ck_assert_mem_eq(frankenstein, p, stream_size);

    free_stream(s);
    vc_dechunker_free(dc);
}

/******************************************************************************/
// Like test_vc_dechunker_big_test, but the last chunk is oversized
START_TEST(test_vc_dechunker_big_test_oversize_fail)
{
    enum vc_dechunker_status stat;
    struct vc_dechunker *dc = vc_dechunker_init("test", CHANNEL_CHUNK_LENGTH);
    ck_assert_ptr_ne(dc, NULL);
    struct stream *s;

    int i;
    for (i = 0 ; i < FRANKENSTEIN_VC_CHUNK_COUNT; ++i)
    {
        int flags;
        s = make_vc_bigtest_chunk(i, &flags);
        if (i == (FRANKENSTEIN_VC_CHUNK_COUNT - 1))
        {
            // Add a byte to the end of the text in the chunk
            struct stream *s2;
            make_stream(s2);
            init_stream(s2, s_rem(s) + 1);
            s_push_layer(s2, iso_hdr, 0);
            out_uint8p(s2, s->p, s_rem(s));
            out_uint8(s2, 'x');
            s_mark_end(s2);
            s_pop_layer(s2, iso_hdr); // Rewind for reading
            // Swap s2 and s and delete the original stream
            struct stream *tmp = s;
            s = s2;
            free_stream(tmp);
        }
        stat = vc_dechunker_process_chunk(
                   dc, s,
                   flags,
                   sizeof(frankenstein));
        if (i < FRANKENSTEIN_VC_CHUNK_COUNT - 1)
        {
            ck_assert_int_eq(stat, E_VC_IN_PROGRESS);
        }
        else
        {
            ck_assert_int_eq(stat, E_VC_ERROR);
        }
        free_stream(s);
    }

    vc_dechunker_free(dc);
}

/******************************************************************************/
// Like test_vc_dechunker_big_test, but the last chunk is undersized
START_TEST(test_vc_dechunker_big_test_undersize_fail)
{
    enum vc_dechunker_status stat;
    struct vc_dechunker *dc = vc_dechunker_init("test", CHANNEL_CHUNK_LENGTH);
    ck_assert_ptr_ne(dc, NULL);
    struct stream *s;

    int i;
    for (i = 0 ; i < FRANKENSTEIN_VC_CHUNK_COUNT; ++i)
    {
        int flags;
        s = make_vc_bigtest_chunk(i, &flags);
        if (i == (FRANKENSTEIN_VC_CHUNK_COUNT - 1))
        {
            // Skip a byte in the stream, so there is one
            // less byte than expected
            in_uint8s(s, 1);
        }
        stat = vc_dechunker_process_chunk(
                   dc, s,
                   flags,
                   sizeof(frankenstein));
        if (i < FRANKENSTEIN_VC_CHUNK_COUNT - 1)
        {
            ck_assert_int_eq(stat, E_VC_IN_PROGRESS);
        }
        else
        {
            ck_assert_int_eq(stat, E_VC_ERROR);
        }
        free_stream(s);
    }

    vc_dechunker_free(dc);
}

/******************************************************************************/

/******************************************************************************/
/*
 * Check bad parameters passed to the dechunker functions
 */
START_TEST(test_dyn_dechunker_bad_params)
{
    struct dyn_dechunker *dc;
    const char data[] = "Some stream data";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum dyn_dechunker_status stat;

    // dyn_dechunker_init
    dc = dyn_dechunker_init(NULL); // No channel name
    ck_assert_ptr_eq(dc, NULL);
    dc = dyn_dechunker_init("test"); // Should be OK
    ck_assert_ptr_ne(dc, NULL);

    // dyn_dechunker_free
    dyn_dechunker_free(NULL);   // Musn't crash!

    // dyn_dechunker_get_stream
    dyn_dechunker_get_stream(NULL);   // Musn't crash!

    // dyn_dechunker_process_chunk
    stat = dyn_dechunker_process_first_chunk(NULL, s, 1600); // No dechunker
    ck_assert_int_eq(stat, E_DYN_ERROR);
    stat = dyn_dechunker_process_first_chunk(dc, NULL, 1600); // No stream
    ck_assert_int_eq(stat, E_DYN_ERROR);
    stat = dyn_dechunker_process_first_chunk(dc, s, -1); // bad total_size
    ck_assert_int_eq(stat, E_DYN_ERROR);

    free_stream(s);
    dyn_dechunker_free(dc);
}

/******************************************************************************/
/*
 * Check passthrough DATA chunks (i.e. those not following a FIRST)
 *
 * When the dechunker is in normal operation, these chunks are
 * immediately returned to the caller with E_DYN_INLINE_CHUNK.
 */
START_TEST(test_dyn_dechunker_passthrough)
{
    const char data[] = "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum dyn_dechunker_status stat;

    struct dyn_dechunker *dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);

    // Save the stream pointer so we can reset the stream in between calls
    s_push_layer(s, iso_hdr, 0);

    // Check a DATA chunk is normally recognised immediately
    stat = dyn_dechunker_process_data_chunk(
               dc, s);
    ck_assert_int_eq(stat, E_DYN_INLINE_CHUNK);

    dyn_dechunker_free(dc);
    free_stream(s);
}
END_TEST


/******************************************************************************/
/**
 * Two consecutive FIRST chunks are not allowed
 */
START_TEST(test_dyn_dechunker_first_first)
{
    const char data[] = PAD50 "Some data to dechunk";
    struct stream *s = make_stream_from_data(data, sizeof(data));
    enum dyn_dechunker_status stat;

    struct dyn_dechunker *dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);

    // Save the stream pointer so we can reset the stream in between calls
    s_push_layer(s, iso_hdr, 0);

    // Check a FIRST chunk is accepted...
    stat = dyn_dechunker_process_first_chunk(
               dc, s, 1600);
    ck_assert_int_eq(stat, E_DYN_IN_PROGRESS);

    // ... and another FIRST chunk is an error
    s_pop_layer(s, iso_hdr);
    stat = dyn_dechunker_process_first_chunk(
               dc, s, 1600);
    ck_assert_int_eq(stat, E_DYN_ERROR);

    dyn_dechunker_free(dc);
    free_stream(s);
}
END_TEST


/******************************************************************************/
/**
 * A FIRST chunk bigger than 1590 bytes but less than 1600 is passed
 * through to the application, if it is the total length
 */
START_TEST(test_dyn_dechunker_first_inline)
{
    char data[1591] = {0};

    enum dyn_dechunker_status stat;
    struct dyn_dechunker *dc;
    struct stream *s;

    // Check a FIRST chunk of 1590 bytes with a total of 1590 is rejected
    // (too small to fragment)
    dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);
    s = make_stream_from_data(data, 1590);
    ck_assert_ptr_ne(s, NULL);
    stat = dyn_dechunker_process_first_chunk( dc, s, 1590);
    ck_assert_int_eq(stat, E_DYN_ERROR);
    dyn_dechunker_free(dc);
    free_stream(s);

    // Check a FIRST chunk of 1591 bytes with a total size of 1591 is
    // accepted as inline
    dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);
    s = make_stream_from_data(data, 1591);
    ck_assert_ptr_ne(s, NULL);
    stat = dyn_dechunker_process_first_chunk( dc, s, 1591);
    ck_assert_int_eq(stat, E_DYN_INLINE_CHUNK);
    dyn_dechunker_free(dc);
    free_stream(s);
}
END_TEST


/******************************************************************************/
/**
 * Checks that a FIRST chunk cannot be bigger than the total size
 */
START_TEST(test_dyn_dechunker_chunk_overflow)
{
    char data[1592] = {0};

    enum dyn_dechunker_status stat;
    struct dyn_dechunker *dc;
    struct stream *s;

    // We know a FIRST chunk of size 1591 for a total of 1591 is
    // inline (see test_dyn_dechunker_first_inline()). Check if the
    // first chunk is 1592, it is rejected
    dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);
    s = make_stream_from_data(data, 1592);
    ck_assert_ptr_ne(s, NULL);
    stat = dyn_dechunker_process_first_chunk( dc, s, 1591);
    ck_assert_int_eq(stat, E_DYN_ERROR);
    dyn_dechunker_free(dc);
    free_stream(s);
}
END_TEST

/******************************************************************************/
// Returns a stream with some random data, then a chunk of
// up to FRANKENSTEIN_DYN_CHUNK_COUNT bytes from Frankenstein chapter 1
// The stream pointer will be positioned at the start of the text.
static struct stream *
make_dyn_bigtest_chunk(unsigned int chunk_num)
{
    struct stream *s = NULL;

    if (chunk_num < FRANKENSTEIN_DYN_CHUNK_COUNT)
    {
        int chunk_size;
        // Work out the size of this chunk
        if (chunk_num == (FRANKENSTEIN_DYN_CHUNK_COUNT - 1))
        {
            chunk_size = sizeof(frankenstein) % FRANKENSTEIN_DYN_CHUNK_SIZE;
            if (chunk_size == 0)
            {
                chunk_size = FRANKENSTEIN_DYN_CHUNK_SIZE;
            }
        }
        else
        {
            chunk_size = FRANKENSTEIN_DYN_CHUNK_SIZE;
        }

        // Add some random data at the start of the stream, so we
        // can check the dechunker works for non-zero positioned
        // streams
        int rand_size = 4 * 4 * chunk_num;

        make_stream(s);
        init_stream(s, rand_size + chunk_size);

        // Write the random data
        int i;
        for (i = 0 ; i < rand_size; ++i)
        {
            out_uint8(s, rand() & 255);
        }
        s_push_layer(s, iso_hdr, 0);

        // Copy the chapter data
        out_uint8a(s, &frankenstein[chunk_num * FRANKENSTEIN_DYN_CHUNK_SIZE],
                   chunk_size);

        // Get the stream ready for reading from the Frankenstein text
        s_mark_end(s);
        s_pop_layer(s, iso_hdr);
    }

    return s;
}

/******************************************************************************/
/*
 * Streams a lot of data through the dechunker and checks it's all
 * assembled correctly at the end
 */
START_TEST(test_dyn_dechunker_big_test)
{
    enum dyn_dechunker_status stat;
    struct dyn_dechunker *dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);
    struct stream *s;

    int i;
    for (i = 0 ; i < FRANKENSTEIN_DYN_CHUNK_COUNT; ++i)
    {
        s = make_dyn_bigtest_chunk(i);
        if (i == 0)
        {
            stat = dyn_dechunker_process_first_chunk(
                       dc, s, sizeof(frankenstein));
        }
        else
        {
            stat = dyn_dechunker_process_data_chunk( dc, s);
        }
        if (i < FRANKENSTEIN_DYN_CHUNK_COUNT - 1)
        {
            ck_assert_int_eq(stat, E_DYN_IN_PROGRESS);
        }
        else
        {
            ck_assert_int_eq(stat, E_DYN_READY);
        }
        free_stream(s);
    }

    // Check we have a result
    s = dyn_dechunker_get_stream(dc);
    ck_assert_ptr_ne(s, NULL);

    // Is it the right size?
    int stream_size = s_rem(s);
    ck_assert_int_eq(stream_size, sizeof(frankenstein));

    // Check the data
    const char *p;
    in_uint8p(s, p, stream_size);
    ck_assert_mem_eq(frankenstein, p, stream_size);

    free_stream(s);
    dyn_dechunker_free(dc);
}

/******************************************************************************/
// Like test_dyn_dechunker_big_test, but the last chunk is oversized
START_TEST(test_dyn_dechunker_big_test_oversize_fail)
{
    enum dyn_dechunker_status stat;
    struct dyn_dechunker *dc = dyn_dechunker_init("test");
    ck_assert_ptr_ne(dc, NULL);
    struct stream *s;

    int i;
    for (i = 0 ; i < FRANKENSTEIN_DYN_CHUNK_COUNT; ++i)
    {
        s = make_dyn_bigtest_chunk(i);
        if (i == 0)
        {
            stat = dyn_dechunker_process_first_chunk(
                       dc, s, sizeof(frankenstein));
        }
        else
        {
            if (i == (FRANKENSTEIN_DYN_CHUNK_COUNT - 1))
            {
                // Add a byte to the end of the text in the chunk
                struct stream *s2;
                make_stream(s2);
                init_stream(s2, s_rem(s) + 1);
                s_push_layer(s2, iso_hdr, 0);
                out_uint8p(s2, s->p, s_rem(s));
                out_uint8(s2, 'x');
                s_mark_end(s2);
                s_pop_layer(s2, iso_hdr); // Rewind for reading
                // Swap s2 and s and delete the original stream
                struct stream *tmp = s;
                s = s2;
                free_stream(tmp);
            }
            stat = dyn_dechunker_process_data_chunk( dc, s);
        }
        if (i < FRANKENSTEIN_DYN_CHUNK_COUNT - 1)
        {
            ck_assert_int_eq(stat, E_DYN_IN_PROGRESS);
        }
        else
        {
            ck_assert_int_eq(stat, E_DYN_ERROR);
        }
        free_stream(s);
    }

    // Check we do not have a result
    s = dyn_dechunker_get_stream(dc);
    ck_assert_ptr_eq(s, NULL);

    dyn_dechunker_free(dc);
}

/******************************************************************************/

Suite *
make_suite_test_dechunker(void)
{
    Suite *s;
    TCase *rc_dechunker;

    s = suite_create("dechunker");

    rc_dechunker = tcase_create("vc_dechunker");
    suite_add_tcase(s, rc_dechunker);
    tcase_add_test(rc_dechunker, test_vc_dechunker_bad_params);
    tcase_add_test(rc_dechunker, test_vc_dechunker_passthrough);
    tcase_add_test(rc_dechunker, test_vc_dechunker_intermediate);
    tcase_add_test(rc_dechunker, test_vc_dechunker_last);
    tcase_add_test(rc_dechunker, test_vc_dechunker_first_first);
    tcase_add_test(rc_dechunker, test_vc_dechunker_chunk_overflow);
    tcase_add_test(rc_dechunker, test_vc_dechunker_big_test);
    tcase_add_test(rc_dechunker, test_vc_dechunker_big_test_oversize_fail);
    tcase_add_test(rc_dechunker, test_vc_dechunker_big_test_undersize_fail);

    rc_dechunker = tcase_create("dyn_dechunker");
    suite_add_tcase(s, rc_dechunker);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_bad_params);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_passthrough);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_first_first);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_first_inline);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_chunk_overflow);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_big_test);
    tcase_add_test(rc_dechunker, test_dyn_dechunker_big_test_oversize_fail);

    return s;
}
