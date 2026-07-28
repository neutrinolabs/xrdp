/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2026
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
 * @file dechunker.h
 * @brief Dechunker functions for chunks on virtual channels - declarations
 *
 * Many places in xrdp need to dechunk data received from virtual channels.
 * This process is described in [MS-RDPBCGR] 3.1.5.2.1 (sending) and
 * 3.1.5.2.2.1 (reassembly).
 *
 *
 * @author Matt Burt
 */

#if !defined(DECHUNKER_H)
#define DECHUNKER_H

struct stream;

/* Private type */
struct vc_dechunker;

/**
 * Returned from dechunker_process_vc_chunk()
 */
enum vc_dechunker_status
{
    E_VC_INLINE_CHUNK = 0, ///< This chunk is complete in itself
    E_VC_IN_PROGRESS,      ///< The dechunker is processing chunks
    E_VC_READY,            ///< A dechunked stream is now complete
    E_VC_ERROR             ///< An error occurred (logged)
};

/**
 * Initialise a virtual channel dechunker
 *
 * @param chan_name - Name of channel
 * @param max_chunk_size - Max size of chunks allowed on channel
 * @return vc_dechunker
 */
struct vc_dechunker *
vc_dechunker_init(const char *chan_name, int max_chunk_size);

/**
 * Free a virtual channel dechunker
 * @param self vc dechunker to free
 */
void
vc_dechunker_free(struct vc_dechunker *self);

/**
 * Process a virtual channel chunk
 *
 * @param self dechunker
 * @param s Stream for chunk, positioned at start of chunk
 * @param flags from CHANNEL_PDU_HEADER
 * @param total_size length from CHANNEL_PDU_HEADER
 * @return status of dechunker
 *
 * If E_VC_ERROR is returned, the dechunker will ignore further PDUs
 * until the start of the next one is detected. This can be used to
 * recover from chunking errors without losing the channel entirely. It
 * is up to the caller whether to treat a dechunking error as fatal for
 * the channel or not.
 */
enum vc_dechunker_status
vc_dechunker_process_chunk(struct vc_dechunker *self,
                           struct stream *s, int flags, int total_size);
/**
 * Get the stream from a ready dechunker
 *
 * @param self virtual channel dechunker
 * @return input stream containing completed chunk
 *
 * Ownership of the stream passes to the caller
 *
 * Resets the dechunker state so that further chunks can be processed.
 */
struct stream *
vc_dechunker_get_stream(struct vc_dechunker *self);


#endif // DECHUNKER_H
