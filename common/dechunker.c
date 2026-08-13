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
 * @file dechunker.c
 * @brief Dechunker functions for chunks on virtual channels - definitions
 * @author Matt Burt
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "dechunker.h"
#include "parse.h"
#include "os_calls.h"
#include "string_calls.h"

enum vc_dechunker_state
{
    E_NO_DATA = 0,
    E_READING,
    E_DATA,
    E_SKIPPING
};

enum vc_chunk_type
{
    CT_INTERMEDIATE = 0,
    CT_FIRST = 1, // CHANNEL_FLAG_FIRST
    CT_LAST = 2,  // CHANNEL_FLAG_LAST
    CT_FIRST_LAST = 3 // CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST
};

struct vc_dechunker
{
    char name[64];
    int max_chunk_size;
    enum vc_dechunker_state state;
    struct stream *reassembly_s;
};

struct dyn_dechunker
{
    char name[64];
    struct stream *reassembly_s;
};

enum
{
    // This is a rather arbitrary figure, but one we are unlikely to
    // go below.  It's a sanity check for vc_dechunker_init()
    E_MAX_VC_CHUNK_SIZE_LOWER_LIMIT = 50
};
/*****************************************************************************/
struct vc_dechunker *
vc_dechunker_init(const char *chan_name, int max_chunk_size)
{
    struct vc_dechunker *self = NULL;
    if (chan_name == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "vc_dechunker_init() called with no channel name");
    }
    else if (max_chunk_size < E_MAX_VC_CHUNK_SIZE_LOWER_LIMIT)
    {
        LOG(LOG_LEVEL_ERROR, "Dechunker: Max chunk size for %s is too small",
            chan_name);
    }
    else if ((self = g_new(struct vc_dechunker, 1)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Dechunker: no memory for %s", chan_name);
    }
    else
    {
        strlcpy(self->name, chan_name, sizeof(self->name));
        self->max_chunk_size = max_chunk_size;
        self->state = E_NO_DATA;
        self->reassembly_s = NULL;
    }

    return self;
}

/*****************************************************************************/
void
vc_dechunker_free(struct vc_dechunker *self)
{
    if (self != NULL)
    {
        free_stream(self->reassembly_s);
        free(self);
    }
}

/*****************************************************************************/
static void
vc_dechunker_reset(struct vc_dechunker *self)
{
    if (self != NULL)
    {
        free_stream(self->reassembly_s);
        self->reassembly_s = NULL;
        self->state = E_NO_DATA;
    }
}

/*****************************************************************************/
static void
log_unexpected_vc_chunk_type(struct vc_dechunker *self,
                             enum vc_chunk_type ct)
{
    static const char *chunk_type_str[4] =
    {
        "CT_INTERMEDIATE",
        "CT_FIRST",
        "CT_LAST",
        "CT_FIRST_LAST"
    };

    int index = (int)ct & 3; // Guarantee to be 0..3
    LOG (LOG_LEVEL_ERROR,
         "Dechunker: unexpected chunk type %s received on %s",
         chunk_type_str[index], self->name);
    self->state = E_SKIPPING; // Look for the next PDU
}

/*****************************************************************************/
static enum vc_dechunker_status
handle_no_data_state(struct vc_dechunker *self,
                     struct stream *s,
                     int chunk_size,
                     int total_size,
                     enum vc_chunk_type ct)
{
    enum vc_dechunker_status rv = E_VC_ERROR;
    switch (ct)
    {
        case CT_FIRST:
            // See [MS-RDPBCGR] 3.1.5.2.1 Sending of Virtual Channel PDU
            if (total_size <= self->max_chunk_size)
            {
                LOG (LOG_LEVEL_ERROR,
                     "Dechunker: short first chunk received on %s",
                     self->name);
                self->state = E_SKIPPING;
            }
            else if (chunk_size >= total_size)
            {
                LOG (LOG_LEVEL_ERROR,
                     "Dechunker: malformed first chunk received on %s",
                     self->name);
                self->state = E_SKIPPING;
            }
            else
            {
                make_stream(self->reassembly_s);
                if (self->reassembly_s)
                {
                    init_stream(self->reassembly_s, total_size);
                }
                if (self->reassembly_s == NULL ||
                        self->reassembly_s->data == NULL)
                {
                    LOG (LOG_LEVEL_ERROR,
                         "Dechunker: out-of-memory on %s",
                         self->name);
                    self->state = E_SKIPPING;
                }
                else
                {
                    out_uint8p(self->reassembly_s, s->p, chunk_size);
                    in_uint8s(s, chunk_size);
                    self->state = E_READING;
                    rv = E_VC_IN_PROGRESS;
                }
            }
            break;

        case CT_FIRST_LAST:
            rv = E_VC_INLINE_CHUNK;
            break;

        default:
            log_unexpected_vc_chunk_type(self, ct);
            self->state = E_SKIPPING;
    }

    return rv;
}

/*****************************************************************************/
static enum vc_dechunker_status
handle_reading_state(struct vc_dechunker *self,
                     struct stream *s,
                     int chunk_size,
                     int total_size,
                     enum vc_chunk_type ct)
{
    enum vc_dechunker_status rv = E_VC_ERROR;
    if (ct == CT_INTERMEDIATE || ct == CT_LAST)
    {
        /* Data to add to the reassembly stream
         *
         * [MS-RDPBCGR] 3.1.5.2.2.1 imposes no requirement to check
         * the total_size field is consistent between chunks. Only
         * the total_size value for CT_FIRST is important
         */
        if (chunk_size > s_rem_out(self->reassembly_s))
        {
            LOG (LOG_LEVEL_ERROR,
                 "Dechunker: oversized chunk received on %s",
                 self->name);
            self->state = E_SKIPPING;
        }
        else
        {
            out_uint8p(self->reassembly_s, s->p, chunk_size);
            in_uint8s(s, chunk_size);
            if (ct == CT_LAST)
            {
                if (s_rem_out(self->reassembly_s) == 0)
                {
                    // Make the stream ready for reading
                    s_mark_end(self->reassembly_s);
                    self->reassembly_s->p = self->reassembly_s->data;
                    // Tell the caller the stream is available.
                    self->state = E_DATA;
                    rv = E_VC_READY;
                    LOG_DEVEL(LOG_LEVEL_INFO,
                              "Dechunker: Reassembled PDU of size %d on %s",
                              self->reassembly_s->size, self->name);
                }
                else
                {
                    LOG (LOG_LEVEL_ERROR,
                         "Dechunker: undersized last chunk received on %s",
                         self->name);
                    self->state = E_SKIPPING;
                }
            }
            else
            {
                rv = E_VC_IN_PROGRESS;
            }
        }
    }
    else
    {
        log_unexpected_vc_chunk_type(self, ct);
    }

    return rv;
}

/*****************************************************************************/
enum vc_dechunker_status
vc_dechunker_process_chunk(struct vc_dechunker *self,
                           struct stream *s, int flags,
                           int total_size)
{
    enum vc_dechunker_status rv = E_VC_ERROR;
    enum vc_chunk_type ct = (enum vc_chunk_type)(flags & 3);
    int chunk_size = s ? s_rem(s) : 0; // Chunk is remainder of stream

    if (self == NULL || s == NULL)
    {
        ; // Nothing to do
    }
    else if (total_size < 0)
    {
        LOG (LOG_LEVEL_ERROR,
             "Dechunker: out-of-range length received on %s",
             self->name);
        self->state = E_SKIPPING;
    }
    else if (chunk_size > self->max_chunk_size)
    {
        // [MS-RDPBCGR] 2.2.6.1
        // > ... This field MUST NOT be larger than CHANNEL_CHUNK_size
        // > (1600) bytes in size unless the maximum virtual channel
        // > chunk size is specified in the optional VCChunkSize field
        // > of the Virtual Channel Capability Set (section 2.2.7.1.10).
        LOG (LOG_LEVEL_ERROR,
             "Dechunker: oversize chunk received on %s (%d octets)",
             self->name, chunk_size);
        self->state = E_SKIPPING;
    }
    else
    {
        // Check for a restart after an error
        if (self->state == E_SKIPPING)
        {
            switch (ct)
            {
                case CT_FIRST:
                case CT_FIRST_LAST:
                    // Clean up the dechunker and start again
                    vc_dechunker_reset(self);
                    break;
                default:
                    break;
            }
        }

        switch (self->state)
        {
            case E_NO_DATA:
                rv = handle_no_data_state(self, s, chunk_size,
                                          total_size, ct);
                break;

            case E_READING:
                rv = handle_reading_state(self, s, chunk_size,
                                          total_size, ct);
                break;

            case E_DATA:
                // If we get here, we've not cleared the existing buffer.
                // This is a serious problem and we continue returning
                // a error until the buffer is cleared.
                LOG(LOG_LEVEL_ALWAYS,
                    "Dechunker: unprocessed PDU on %s", self->name);
                break;

            case E_SKIPPING:
                rv = E_VC_IN_PROGRESS; // Ignore this chunk
                break;

            default:
                // Shouldn't get here.
                LOG (LOG_LEVEL_ERROR,
                     "Dechunker: called when %s has an unknown state %d",
                     self->name, (int)self->state);
                self->state = E_SKIPPING;
        }
    }

    return rv;
}

/*****************************************************************************/
struct stream *
vc_dechunker_get_stream(struct vc_dechunker *self)
{
    struct stream *s;
    const char *name = (self != NULL) ? self->name : "<unknown>";
    if (self == NULL || self->state != E_DATA)
    {
        LOG (LOG_LEVEL_ERROR,
             "Dechunker: get stream called for %s with no data available",
             name);
        s = NULL;
    }
    else
    {
        // Pass ownership of the stream to the caller
        s = self->reassembly_s;
        self->reassembly_s = NULL; // So we don't free it ourselves!
        vc_dechunker_reset(self);
    }

    return s;
}
/*****************************************************************************/
struct dyn_dechunker *
dyn_dechunker_init(const char *chan_name)
{
    struct dyn_dechunker *self = NULL;
    if (chan_name == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "dyn_dechunker_init() called with no channel name");
    }
    else if ((self = g_new(struct dyn_dechunker, 1)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Dechunker: no memory for %s", chan_name);
    }
    else
    {
        strlcpy(self->name, chan_name, sizeof(self->name));
        self->reassembly_s = NULL;
    }

    return self;
}

/*****************************************************************************/
void
dyn_dechunker_free(struct dyn_dechunker *self)
{
    if (self != NULL)
    {
        free_stream(self->reassembly_s);
        free(self);
    }
}

/*****************************************************************************/
enum dyn_dechunker_status
dyn_dechunker_process_first_chunk(struct dyn_dechunker *self,
                                  struct stream *s, int total_size)
{
    enum dyn_dechunker_status status = E_DYN_ERROR;

    int frag_size = s ? s_rem(s) : 0;
    if (self == NULL || s == NULL)
    {
        ; // Nothing to be done
    }
    else if (total_size <= 1590 || frag_size > total_size)
    {
        // See [MS-RDPEDYC] 2.2.3
        LOG(LOG_LEVEL_ERROR,
            "Badly sized DYNVC_DATA_FIRST PDU received on dynamic channel %s",
            self->name);
    }
    else if (self->reassembly_s != NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "unexpected DYNVC_DATA_FIRST received on dynamic channel %s",
            self->name);
    }
    else if (frag_size == total_size)
    {
        // This chunk contains all the data
        status = E_DYN_INLINE_CHUNK;
    }
    else
    {
        make_stream(self->reassembly_s);
        if (self->reassembly_s)
        {
            init_stream(self->reassembly_s, total_size);
        }
        if (self->reassembly_s == NULL || self->reassembly_s->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Out of memory for dynamic PDU reassembly on %s",
                self->name);
        }
        else
        {
            out_uint8p(self->reassembly_s, s->p, frag_size);
            in_uint8s(s, frag_size);
            status = E_DYN_IN_PROGRESS;
        }
    }

    return status;
}

/*****************************************************************************/
enum dyn_dechunker_status
dyn_dechunker_process_data_chunk(struct dyn_dechunker *self,
                                 struct stream *s)
{
    enum dyn_dechunker_status rv;

    if (self == NULL || s == NULL)
    {
        rv = E_DYN_ERROR;
    }
    else if (self->reassembly_s == NULL)
    {
        rv = E_DYN_INLINE_CHUNK;
    }
    else
    {
        int frag_size = s_rem(s);
        // We're currently reconstructing a data PDU from fragments
        if (!s_check_rem_out(self-> reassembly_s, frag_size))
        {
            LOG(LOG_LEVEL_ERROR,
                "Oversized DYNVC_DATA when reconstructing PDU on %s",
                self->name);
            rv = E_DYN_ERROR;
        }
        else
        {
            out_uint8p(self->reassembly_s, s->p, frag_size);
            in_uint8s(s, frag_size);

            if (s_rem_out(self->reassembly_s) == 0)
            {
                // Finished defragging
                s_mark_end(self->reassembly_s);
                self->reassembly_s->p = self->reassembly_s->data;
                rv = E_DYN_READY;
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "Dechunker: Reassembled PDU of size %d on %s",
                          self->reassembly_s->size, self->name);
            }
            else
            {
                rv = E_DYN_IN_PROGRESS;
            }
        }
    }

    return rv;
}

/*****************************************************************************/
struct stream *
dyn_dechunker_get_stream(struct dyn_dechunker *self)
{
    struct stream *s;
    const char *name = (self != NULL) ? self->name : "<unknown>";
    if (self == NULL || self->reassembly_s == NULL ||
            self->reassembly_s->end == self->reassembly_s->data)
    {
        LOG (LOG_LEVEL_ERROR,
             "Dechunker: get stream called for %s with no data available",
             name);
        s = NULL;
    }
    else
    {
        // Pass ownership of the stream to the caller
        s = self->reassembly_s;
        self->reassembly_s = NULL; // So we don't free it ourselves!
    }

    return s;
}

/*****************************************************************************/
int
dyn_dechunker_pending(struct dyn_dechunker *self)
{
    return (self != NULL && self->reassembly_s != NULL);
}
