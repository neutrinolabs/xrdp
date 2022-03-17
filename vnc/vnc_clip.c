
/* startup_complete is only ever set if we're using the VNC clip facility *//**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * libvnc
 *
 * The message definitions used in this source file can be found mostly
 * in RFC6143 - "The Remote Framebuffer Protocol".
 *
 * The clipboard messages from the RDP client side are mostly
 * described in [MS-RDPECLIP]
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "vnc.h"
#include "vnc_clip.h"
#include "string_calls.h"
#include "ssl_calls.h"
#include "rfb.h"
#include "log.h"
#include "trans.h"
#include "ms-rdpbcgr.h"
#include "ms-rdpeclip.h"
#include "xrdp_constants.h"

/**
 * The formats we advertise as supported to the RDP client
 */
static const int
g_supported_formats[] =
{
    CF_UNICODETEXT,
    CF_LOCALE,
    CF_TEXT,
    /* Don't advertise CF_OEMTEXT - anything other than ASCII will be broken */
    /* CF_OEMTEXT, */
    0
};

/**
 * Data private to the VNC clipboard
 *
 * Note that this doesn't include the clip channel ID, as vnc.c needs
 * this to redirect virtual channel PDUs to this module */
struct vnc_clipboard_data
{
    struct stream *rfb_clip_s;
    int requested_clip_format; /* Last requested text format */
    int active_data_requests; /* Number of outstanding FORMAT_DATA_REQUESTs */
    struct stream *dechunker_s; /* Dechunker for the RDP clip channel */
    int capability_version;  /* Clipboard virt channel extension version */
    int capability_flags;    /* Channel capability flags */
    bool_t startup_complete; /* is the startup sequence done (1.3.2.1) */
};

/**
 * Summarise a stream contents in a way which allows two streams to
 * be easily compared
 */
struct stream_characteristics
{
    char digest[16];
    int length;
};

#ifdef USE_DEVEL_LOGGING
/***************************************************************************//**
 * Convert a CF_ constant to text
 *
 * @param CF_xxx constant
 * @param buff Scratchpad for storing a temporary string in
 * @param bufflen Length of the above
 *
 * @return string representation
 */
static const char *
cf2text(int cf, char *buff, int bufflen)
{
    const char *result;

    switch (cf)
    {
        case CF_UNICODETEXT:
            result = "CF_UNICODETEXT";
            break;

        case CF_LOCALE:
            result = "CF_LOCALE";
            break;

        case CF_TEXT:
            result = "CF_TEXT";
            break;

        case CF_OEMTEXT:
            result = "CF_OEMTEXT";
            break;

        default:
            g_snprintf(buff, bufflen, "CF_<0x%08x>", cf);
            result = buff;
    };

    return result;
}
#endif /* USE_DEVEL_LOGGING */

/***************************************************************************//**
 * Adds a CLIPRDR_HEADER ([MS-RDPECLIP] 2.2.1) to the data stream
 *
 * The location of the length is stored in the unused 'channel_hdr' field
 * of the data stream. When send_stream_to_clip_channel() is called,
 * we can use update the data length.
 *
 * @param s Output stream
 * @param msg_type Message Type
 * @param msg_flags Message flags
 */
static void
out_cliprdr_header(struct stream *s, int msg_type, int msg_flags)
{
    out_uint16_le(s, msg_type);
    out_uint16_le(s, msg_flags);
    /* Save the datalen location so we can update it later */
    s_push_layer(s, channel_hdr, 4);
}

/***************************************************************************//**
 * Sends the contents of a stream buffer to the clipboard channel
 *
 * Stream contents are chunked appropriately if they are too big to
 * fit in a single PDU
 *
 * The stream object cliprdr datalen header field is updated by this call.
 *
 * @param v VNC object
 * @param s stream buffer
 *
 * @pre stream buffer must have been initialised with a call to
 *      out_cliprdr_header()
 * @pre Stream is terminated with s_mark_end()
 */
static int
send_stream_to_clip_channel(struct vnc *v, struct stream *s)
{
    int rv = 0;
    int datalen = 0; /* cliprdr PDU datalen field */
    int pos = 0;
    int pdu_len = 0; /* Length of channel PDU */
    int total_data_len = (int)(s->end - s->data);
    int flags;
    int msg_type;
    int msg_flags;

    /* Use the pointer saved by out_cliprdr_header() to
     * write the cliprdr PDU length */
    s_pop_layer(s, channel_hdr);
    datalen = (int)(s->end - s->p) - 4;
    out_uint32_le(s, datalen);

    /* Read the other fields of the cliprdr header for logging */
    s->p = s->data;
    in_uint16_le(s, msg_type);
    in_uint16_le(s, msg_flags);
    LOG(LOG_LEVEL_DEBUG, "Sending cliprdr PDU type:%s flags:%d datalen:%d",
        CB_PDUTYPE_TO_STR(msg_type), msg_flags, datalen);


    for ( ; rv == 0 && pos < total_data_len ; pos += pdu_len)
    {
        pdu_len = MIN(CHANNEL_CHUNK_LENGTH, (total_data_len - pos));

        /* Determine chunking flags for this PDU (MS-RDPBCGR 3.1.5.2.1) */
        if (pos == 0)
        {
            if ((pos + pdu_len) == total_data_len)
            {
                /* Only one chunk */
                flags = (XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST);
            }
            else
            {
                /* First chunk of several */
                flags = (XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_SHOW_PROTOCOL);
            }
        }
        else if ((pos + pdu_len) == total_data_len)
        {
            /* Last chunk of several */
            flags = (XR_CHANNEL_FLAG_LAST | XR_CHANNEL_FLAG_SHOW_PROTOCOL);
        }
        else
        {
            /* Intermediate chunk of several */
            flags = XR_CHANNEL_FLAG_SHOW_PROTOCOL;
        }
        rv = v->server_send_to_channel(v, v->clip_chanid,
                                       s->data + pos, pdu_len,
                                       total_data_len, flags);
    }

    return rv;
}

/***************************************************************************//**
 * Counts the occurrences of a character in a stream
 * @param s stream
 * @param c character
 * @return occurrence count
 */
static int
char_count_in(const struct stream *s, char c)
{
    int rv = 0;
    const char *p = s->data;
    const char *end = s->end;

    while ((p = g_strnchr(p, c, end - p)) != NULL)
    {
        ++rv;
        ++p; /* Skip counted character */
    }

    return rv;
}

/***************************************************************************//**
 * Searches a Format List PDU for a preferred text format
 *
 * On entry, the stream contains a formatListData object
 *
 * @param v VNC module
 * @param msg_flags clipHeader msgFlags field
 * @params s formatListData object.
 * @return Preferred text format, or 0 if not found
 */
static int
find_preferred_text_format(struct vnc *v, int msg_flags, struct stream *s)
{
    int seen_cf_unicodetext = 0;
    int seen_cf_text = 0;
    int format_id;
#ifdef USE_DEVEL_LOGGING
    char scratch[64];
#endif

    while (s_check_rem(s, 4))
    {
        in_uint32_le(s, format_id);

        if ((v->vc->capability_flags & CB_USE_LONG_FORMAT_NAMES) == 0)
        {
            /* Short format name */
            int skip = MIN(s_rem(s), 32);
            in_uint8s(s, skip);
        }
        else
        {
            /* Skip a wsz string */
            int wc = 1;
            while (s_check_rem(s, 2) && wc != 0)
            {
                in_uint16_le(s, wc);
            }
        }

        LOG_DEVEL(LOG_LEVEL_INFO, "VNC: Format id %s available"
                  " from RDP client",
                  cf2text(format_id, scratch, sizeof(scratch)));

        switch (format_id)
        {
            case CF_UNICODETEXT:
                seen_cf_unicodetext = 1;
                break;

            case CF_TEXT:
                seen_cf_text = 1;
                break;
        }
    }

    /* Prefer Unicode (UTF-16), as it's most easily converted to
     * the ISO-8859-1 supported by the VNC clipboard */
    return
        (seen_cf_unicodetext != 0 ? CF_UNICODETEXT :
         seen_cf_text != 0 ? CF_TEXT :
         /* Default */ 0);
}

/******************************************************************************/
static int
handle_cb_format_list(struct vnc *v, int msg_flags, struct stream *s)
{
    struct stream *out_s;
    int format;
    int rv = 0;
#ifdef USE_DEVEL_LOGGING
    char scratch[64];
#endif

    /* This is the last stage of the startup sequence in MS-RDPECLIP 1.3.2.1,
     * although it does occur at other times */
    v->vc->startup_complete = 1;

    make_stream(out_s);

    /* Reply to the caller */
    init_stream(out_s, 64);
    out_cliprdr_header(out_s, CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK);
    s_mark_end(out_s);
    send_stream_to_clip_channel(v, out_s);

    /* Send a CB_DATA_REQUEST message to the cliprdr channel,
     * if a suitable text format is available */
    if ((format = find_preferred_text_format(v, msg_flags, s)) != 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO,
                  "Asking RDP client for clip data format=%s",
                  cf2text(format, scratch, sizeof(scratch)));
        v->vc->requested_clip_format = format;
        ++v->vc->active_data_requests;
        init_stream(out_s, 64);
        out_cliprdr_header(out_s, CB_FORMAT_DATA_REQUEST, 0);
        out_uint32_le(out_s, format);
        s_mark_end(out_s);
        send_stream_to_clip_channel(v, out_s);
    }
    free_stream(out_s);

    return rv;
}

/***************************************************************************//**
 * Computes the characteristics of a stream.
 *
 * This can be used to tell if a stream has changed or two streams are
 * the same
 *
 * @param s Stream
 * @param[out] chars Resulting characteristics of stream
 */
static void
compute_stream_characteristics(const struct stream *s,
                               struct stream_characteristics *chars)
{
    void *info = ssl_md5_info_create();
    ssl_md5_clear(info);
    if (s->data != NULL && s->end != NULL)
    {
        chars->length = (int)(s->end - s->data);
        ssl_md5_transform(info, s->data, chars->length);
    }
    else
    {
        chars->length = 0;
    }
    ssl_md5_complete(info, chars->digest);
    ssl_md5_info_delete(info);
}

/***************************************************************************//**
 * Compare two stream characteristics structs for equality
 *
 * @param a characteristics of first stream
 * @param b characteristics of second stream
 * @result != 0 if characteristics are equal
 */
static int
stream_characteristics_equal(const struct stream_characteristics *a,
                             const struct stream_characteristics *b)
{
    return (a->length == b->length &&
            g_memcmp(a->digest, b->digest, sizeof(a->digest)) == 0);
}

/******************************************************************************/
static int
handle_cb_format_data_request(struct vnc *v, struct stream *s)
{
    int format = 0;
    struct stream *out_s;
    int i;
    struct vnc_clipboard_data *vc = v->vc;
    int rv = 0;
    int msg_flags = CB_RESPONSE_OK;
    int lf_count;
    int alloclen = 64;
#ifdef USE_DEVEL_LOGGING
    char scratch[64];
#endif

    if (s_check_rem(s, 4))
    {
        in_uint32_le(s, format);
    }

    LOG_DEVEL(LOG_LEVEL_INFO, "RDP client requested data format=%s",
              cf2text(format, scratch, sizeof(scratch)));

    make_stream(out_s);

    /* For all formats, we need to convert to Windows carriage control,
     * so we need to know how many '\n' characters become '\r\n' */
    lf_count = char_count_in(vc->rfb_clip_s, '\n');

    /* If we're writing a big string, we need to increase the alloclen
     * for the return PDU. We can also vet the requested format here */
    switch (format)
    {
        case CF_TEXT:
            /* We need to allocate enough characters to hold the string
             * with '\n' becoming '\r\n' and also for a terminator. */
            alloclen += vc->rfb_clip_s->size + lf_count + 1;
            break;

        case CF_UNICODETEXT:
            /* As CF_TEXT, but twice as much, as each ANSI character maps to
             * two octets */
            alloclen += (vc->rfb_clip_s->size + lf_count + 1) * 2;
            break;

        case CF_LOCALE:
            break;

        default:
            /* No idea what this is */
            msg_flags = CB_RESPONSE_FAIL;
    }

    /* Allocate the stream and check for failure as the string could be
     * essentially unlimited in length */
    init_stream(out_s, alloclen);
    if (out_s->data == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Memory exhausted allocating %d bytes for clip data response",
            alloclen);
        rv = 1;
    }
    else
    {
        /* Write the packet header.... */
        out_cliprdr_header(out_s, CB_FORMAT_DATA_RESPONSE, msg_flags);

        /* ...and any data */
        switch (format)
        {
            case CF_LOCALE:
                /*
                 * This is undocumented.
                 *
                 * Reverse engineering by firing this request off to
                 * a Microsoft client suggests this is a code from
                 * [MS-LCID]. 0x409 maps to en-us which uses codepage
                 * 1252. This is a close enough match to ISO8859-1 as used
                 * by RFB */
                out_uint32_le(out_s, 0x409);
                break;

            case CF_TEXT:
                for (i = 0; i < vc->rfb_clip_s->size; ++i)
                {
                    char c = vc->rfb_clip_s->data[i];
                    if (c == '\n')
                    {
                        out_uint8(out_s, '\r');
                    }
                    out_uint8(out_s, c);
                }

                out_uint8s(out_s, 1);
                break;

            case CF_UNICODETEXT:
                /* The VNC clipboard format (ISO 8859-1)
                   maps directly to UTF-16LE by moving over the bottom 8 bits,
                   and setting the top 8 bits to zero */
                for (i = 0; i < vc->rfb_clip_s->size; ++i)
                {
                    char c = vc->rfb_clip_s->data[i];
                    if (c == '\n')
                    {
                        out_uint8(out_s, '\r');
                        out_uint8(out_s, 0);
                    }
                    out_uint8(out_s, c);
                    out_uint8(out_s, 0);
                }

                out_uint8s(out_s, 2);
                break;
        }

        s_mark_end(out_s);
        send_stream_to_clip_channel(v, out_s);
        free_stream(out_s);
    }

    return rv;
}

/******************************************************************************/
static int
handle_cb_format_data_response(struct vnc *v, struct stream *s)
{
    int rv = 0;

    struct vnc_clipboard_data *vc = v->vc;

    /* The [MS-RDPECLIP] specification lets a new CB_FORMAT_LIST PDU turn
     * up before we've received a response to a CB_FORMAT_DATA_REQUEST.
     * As a result, there might be more than one CB_FORMAT_DATA_RESPONSE
     * PDUs in-flight. We handle this by ignoring all but the last PDU
     * we're expecting
     */
    if (vc->active_data_requests > 0 && --vc->active_data_requests == 0)
    {
        struct stream *out_s;
        int length;
        char c;
        char lastc;
        int wc;
        unsigned int out_of_range = 0;

        /* We've got a copy of the current VNC paste buffer in
         * vc->rfb_clip_s. Since we're about to change the VNC paste
         * buffer anyway, we'll use this to construct the ISO8859-1
         * text, and then send it to the VNC server
         *
         * We size the buffer as follows:-
         * TEXT Use the same size buffer.
         * UTF-16 - Use half the size
         *
         * In all cases this is big enough, or a little over when removal
         * of `\r` characters is taken into account */
        if  (vc->requested_clip_format == CF_UNICODETEXT)
        {
            length = s_rem(s) / 2;
        }
        else
        {
            length = s_rem(s);
        }

        init_stream(vc->rfb_clip_s, length);
        if (vc->rfb_clip_s->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Memory exhausted allocating %d bytes for clip buffer",
                length);
            rv = 1;
        }
        else
        {
            switch (vc->requested_clip_format)
            {
                case CF_TEXT:
                    lastc = '\0';
                    while (s_check_rem(s, 1))
                    {
                        in_uint8(s, c);
                        if (c == '\n' && lastc == '\r')
                        {
                            /* Overwrite the `\r' */
                            --vc->rfb_clip_s->p;
                        }
                        out_uint8(vc->rfb_clip_s, c);
                        lastc = c;
                    }
                    break;

                case CF_UNICODETEXT:
                    lastc = '\0';
                    while (s_check_rem(s, 2))
                    {
                        in_uint16_le(s, wc);
                        if (wc / 0x100 == 0)
                        {
                            /* Valid ISO8859-1 character in bottom 8 bits */
                            c = wc % 0x100;
                            if (c == '\n' && lastc == '\r')
                            {
                                /* Overwrite the `\r' */
                                --vc->rfb_clip_s->p;
                            }
                            out_uint8(vc->rfb_clip_s, c);
                            lastc = c;
                        }
                        else
                        {
                            /* Character can't be represented in ISO8859-1 */
                            ++out_of_range;
                            if (wc & 0xdc00 && wc <= 0xdfff)
                            {
                                /* Character is start of a surrogate pair */
                                if (s_check_rem(s, 2))
                                {
                                    in_uint16_le(s, wc);
                                }
                            }
                        }
                    }

                    if (out_of_range > 0)
                    {
                        LOG(LOG_LEVEL_WARNING,
                            "VNC: %u out-of-range Unicode characters"
                            " could not be moved to the VNC clipboard",
                            out_of_range);
                    }
                    break;

            }

            /* Remove a terminator at the end, as RFB doesn't need it */
            if (vc->rfb_clip_s->p != vc->rfb_clip_s->data &&
                    *(vc->rfb_clip_s->p - 1) == '\0')
            {
                --vc->rfb_clip_s->p;
            }
            s_mark_end(vc->rfb_clip_s);

            /* Update the VNC server */
            make_stream(out_s);
            length = (unsigned int)(vc->rfb_clip_s->end -
                                    vc->rfb_clip_s->data);
            init_stream(out_s, 1 + 3 + 4 + length);
            out_uint8(out_s, RFB_C2S_CLIENT_CUT_TEXT);
            out_uint8s(out_s, 3);  /* padding */
            out_uint32_be(out_s, length);
            out_uint8p(out_s, vc->rfb_clip_s->data, length);
            s_mark_end(out_s);
            lib_send_copy(v, out_s);
            free_stream(out_s);
        }
    }

    return rv;
}

/******************************************************************************/
static int
handle_cb_caps(struct vnc *v, struct stream *s)
{
    int rv = 0;
    int i;
    int capset_count;
    int capset_type;
    int capset_length;
    int version;
    int flags;

    if (!s_check_rem_and_log(s, 4, "Reading clip capabilities"))
    {
        rv = 1;
    }
    else
    {
        in_uint16_le(s, capset_count);
        in_uint8s(s, 2); /* pad */

        for (i = 0; i < capset_count && rv == 0; ++i)
        {
            if (!s_check_rem_and_log(s, 4, "Reading capability set"))
            {
                rv = 1;
                break;
            }

            in_uint16_le(s, capset_type); /* Length includes these two fields */
            in_uint16_le(s, capset_length);
            capset_length -= 4; /* Account for last two fields */

            switch (capset_type)
            {
                case CB_CAPSTYPE_GENERAL:
                    if (!s_check_rem_and_log(s, 8, "Reading general cap set"))
                    {
                        rv = 1;
                    }
                    else
                    {
                        in_uint32_le(s, version); /* version */
                        in_uint32_le(s, flags); /* generalFlags */
                        capset_length -= 8;

                        /* Update our own capability fields */
                        if (version > 0 && version < v->vc->capability_version)
                        {
                            v->vc->capability_version = version;
                        }
                        v->vc->capability_flags &= flags;

                        LOG(LOG_LEVEL_DEBUG,
                            "Agreed MS-RDPECLIP capability"
                            "version=%d flags=%08x with RDP client",
                            v->vc->capability_version,
                            v->vc->capability_flags);
                    }
                    break;

                default:
                    LOG(LOG_LEVEL_WARNING, "clipboard_process_clip_caps: "
                        "unknown capabilitySetType %d", capset_type);
                    break;
            }

            /* Check for padding at the end of the set */
            if (capset_length > 0)
            {
                if (!s_check_rem_and_log(s, capset_length, "cap set padding"))
                {
                    rv = 1;
                }
                else
                {
                    in_uint8s(s, capset_length);
                }
            }
        }
    }

    return rv;
}

/***************************************************************************//**
 * Send a format list PDU to the RDP client
 *
 * Described in [MS-RDPECLIP] 2.2.3.1
 *
 * @param v VNC structure
 */
static void
send_format_list(struct vnc *v)
{
    struct vnc_clipboard_data *vc = v->vc;
    int use_long_names = vc->capability_flags & CB_USE_LONG_FORMAT_NAMES;
    struct stream *out_s;
    unsigned int i = 0;
    int format;

    make_stream(out_s);
    init_stream(out_s, 8192);
    out_cliprdr_header(out_s, CB_FORMAT_LIST, use_long_names);

    while ((format = g_supported_formats[i++]) != 0)
    {
        if (use_long_names)
        {
            /* Long format name [MS-RDPECLIP] 2.2.3.1.2.1 */
            out_uint32_le(out_s, format);
            out_uint8s(out_s, 2); /* wsz terminator */
        }
        else
        {
            /* Short format name [MS-RDPECLIP] 2.2.3.1.1.1 */
            out_uint32_le(out_s, format);
            out_uint8s(out_s, 32);
        }
    }
    s_mark_end(out_s);
    send_stream_to_clip_channel(v, out_s);
    free_stream(out_s);
}

/******************************************************************************/
void
vnc_clip_init(struct vnc *v)
{
    v->vc = (struct vnc_clipboard_data *)g_malloc(sizeof(*v->vc), 1);
    make_stream(v->vc->rfb_clip_s);
}

/******************************************************************************/
void
vnc_clip_exit(struct vnc *v)
{
    if (v != NULL && v->vc != NULL)
    {
        free_stream(v->vc->rfb_clip_s);
        free_stream(v->vc->dechunker_s);
        g_free(v->vc);
    }
}


/******************************************************************************/
int
vnc_clip_process_eclip_pdu(struct vnc *v, struct stream *s)
{
    int type;
    int msg_flags;
    int datalen;
    int rv = 0;

    /* Ignore PDUs with no complete header */
    if (s_check_rem_and_log(s, 8, "MS-RDPECLIP PDU Header"))
    {
        in_uint16_le(s, type);
        in_uint16_le(s, msg_flags);
        in_uint32_le(s, datalen);

        LOG(LOG_LEVEL_DEBUG, "got clip data type %s msg_flags %d length %d",
            CB_PDUTYPE_TO_STR(type), msg_flags, datalen);
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "clipboard data",
                          s->p, s->end - s->p);

        /* Check the PDU is contained in the stream */
        if (!s_check_rem_and_log(s, datalen, "MS-RDPECLIP PDU"))
        {
            datalen = s_rem(s);
        }
        else
        {
            /* Truncate the PDU to the data length so we can use the
             * normal functions to parse the PDU */
            s->end = s->p + datalen;
        }

        switch (type)
        {
            case CB_FORMAT_LIST:
                rv = handle_cb_format_list(v, msg_flags, s);
                break;

            case CB_FORMAT_LIST_RESPONSE:
                /* We don't need to do anything with this */
                break;

            case CB_FORMAT_DATA_REQUEST:
                rv = handle_cb_format_data_request(v, s);
                break;

            case CB_FORMAT_DATA_RESPONSE:
                if (msg_flags == CB_RESPONSE_OK)
                {
                    rv = handle_cb_format_data_response(v, s);
                }
                break;

            case CB_CLIP_CAPS:
                rv = handle_cb_caps(v, s);
                break;
        }
    }

    return rv;
}


/******************************************************************************/
/**
 * Process a [MS-RDPBCGR] 2.2.6.1 Virtual Channel PDU and re-assemble the
 * data chunks as needed - see 3.1.5.2.2.1
 */
int
vnc_clip_process_channel_data(struct vnc *v, char *data, int size,
                              int total_size, int flags)
{
    int rv = 1;
    struct vnc_clipboard_data *vc = v->vc;
    bool_t first = ((flags & XR_CHANNEL_FLAG_FIRST) != 0);
    bool_t last = ((flags & XR_CHANNEL_FLAG_LAST) != 0);

    if (size > total_size)
    {
        /* This should never happen */
        LOG(LOG_LEVEL_ERROR,
            "Ignoring bad PDU chunk data on clip channel");
    }
    else if (first && vc->dechunker_s != NULL)
    {
        /*
         * If this packet is marked as 'first', we should not be
         * dechunking data already */
        LOG(LOG_LEVEL_ERROR, "Packet chunking start state error");
        free_stream(vc->dechunker_s);
        vc->dechunker_s = NULL;
    }
    else if (!first && vc->dechunker_s == NULL)
    {
        /*
         * This is not the first packet, but the dechunker is not active */
        LOG(LOG_LEVEL_ERROR, "Packet chunking end state error");
    }
    else if (first && last)
    {
        /* this is a complete packet
         * Construct a temp stream for the complete packet, and pass it
         * to the application */
        struct stream packet_s = {0};

        packet_s.data = data;
        packet_s.size = size;
        packet_s.end = packet_s.data + size;
        packet_s.p = packet_s.data;
        rv = vnc_clip_process_eclip_pdu(v, &packet_s);
    }
    else if (first)
    {
        /* Start de-chunking the data */
        make_stream(vc->dechunker_s);
        init_stream(vc->dechunker_s, (int)total_size);

        /* MS-RDPBCGR 3.1.5.2.2.1 states:-
         *
         *     A reassembly buffer MUST be created by the virtual channel
         *     endpoint using the size specified by totalLength when
         *     the first chunk is received.
         *
         * The 'total_size' can be several GB in size, so we really need
         * to check for an allocation failure here */
        if (vc->dechunker_s->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR,
                "Memory exhausted dechunking a %u byte virtual channel PDU",
                total_size);
        }
        else
        {
            out_uint8a(vc->dechunker_s, data, size);
            rv = 0;
        }
    }
    else if (s_check_rem_out_and_log(vc->dechunker_s,
                                     size, "VNC dechunker:"))
    {
        out_uint8a(vc->dechunker_s, data, size);
        /* At the end? */
        if (last)
        {
            s_mark_end(vc->dechunker_s);
            vc->dechunker_s->p = vc->dechunker_s->data;

            /* Call the app and reset the dechunker */
            rv = vnc_clip_process_eclip_pdu(v, vc->dechunker_s);
            free_stream(vc->dechunker_s);
            vc->dechunker_s = NULL;
        }
        else
        {
            rv = 0;
        }
    }

    return rv;
}

/******************************************************************************/
/* clip data from the vnc server */
int
vnc_clip_process_rfb_data(struct vnc *v)
{
    struct vnc_clipboard_data *vc = v->vc;
    struct stream *s;
    int size;
    int rv;

    make_stream(s);
    init_stream(s, 8192);
    rv = trans_force_read_s(v->trans, s, 7);

    if (rv == 0)
    {
        in_uint8s(s, 3);
        in_uint32_be(s, size);

        if (v->clip_chanid < 0 || v->server_chansrv_in_use(v))
        {
            /* Skip this message */
            LOG(LOG_LEVEL_DEBUG, "Skipping %d clip bytes from RFB", size);
            rv = skip_trans_bytes(v->trans, size);
        }
        else
        {
            struct stream_characteristics old_chars;
            struct stream_characteristics new_chars;

            /* Compute the characteristics of the existing data */
            compute_stream_characteristics(vc->rfb_clip_s, &old_chars);

            /* Lose any existing RFB clip data */
            free_stream(vc->rfb_clip_s);
            vc->rfb_clip_s = 0;

            make_stream(vc->rfb_clip_s);
            if (size < 0)
            {
                /* This shouldn't happen - see Extended Clipboard
                 * Pseudo-Encoding */
                LOG(LOG_LEVEL_ERROR, "Unexpected size %d for RFB data", size);
                rv = 1;
            }
            else if (size == 0)
            {
                LOG(LOG_LEVEL_DEBUG, "RFB clip data cleared by VNC server");
            }
            else
            {
                init_stream(vc->rfb_clip_s, size);
                if (vc->rfb_clip_s->data == NULL)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Memory exhausted allocating %d bytes"
                        " for RFB clip data",
                        size);
                    rv = 1;
                }
                else
                {
                    LOG(LOG_LEVEL_DEBUG, "Reading %d clip bytes from RFB",
                        size);
                    rv = trans_force_read_s(v->trans, vc->rfb_clip_s, size);
                }
            }

            /* Consider telling the RDP client about the update only if we've
             * completed the startup handshake */
            if (rv == 0 && vc->startup_complete)
            {
                /* Has the data actually changed ? */
                compute_stream_characteristics(vc->rfb_clip_s, &new_chars);
                if (stream_characteristics_equal(&old_chars, &new_chars))
                {
                    LOG_DEVEL(LOG_LEVEL_INFO, "RFB Clip data is unchanged");
                }
                else
                {
                    LOG_DEVEL(LOG_LEVEL_INFO, "RFB Clip data is updated");
                    send_format_list(v);
                }
            }
        }
    }

    free_stream(s);
    return rv;
}

/******************************************************************************/
int
vnc_clip_open_clip_channel(struct vnc *v)
{
    v->clip_chanid = v->server_get_channel_id(v, CLIPRDR_SVC_CHANNEL_NAME);

    if (v->server_chansrv_in_use(v))
    {
        /*
         * The clipboard is provided by chansrv, if at all - it may of
         * course be disabled there.
         */
        LOG(LOG_LEVEL_INFO, "VNC: Clipboard (if available) is provided "
            "by chansrv facility");
    }
    else if (v->clip_chanid < 0)
    {
        LOG(LOG_LEVEL_INFO, "VNC: Clipboard is unavailable");
    }
    else
    {
        struct stream *s;

        LOG(LOG_LEVEL_INFO, "VNC: Clipboard supports ISO-8859-1 text only");

        make_stream(s);
        init_stream(s, 8192);

        v->vc->capability_version = CB_CAPS_VERSION_2;
        v->vc->capability_flags = CB_USE_LONG_FORMAT_NAMES;
        /**
         * Send two PDUs to initialise the channel. The client should
         * respond with a CB_CLIP_CAPS PDU of its own. See [MS-RDPECLIP]
         * 1.3.2.1 */
        out_cliprdr_header(s, CB_CLIP_CAPS, 0);
        out_uint16_le(s, 1); /* #cCapabilitiesSets */
        out_uint16_le(s, 0); /* pad1 */
        /* CLIPRDR_GENERAL_CAPABILITY */
        out_uint16_le(s, CB_CAPSTYPE_GENERAL); /* capabilitySetType */
        out_uint16_le(s, 12); /* lengthCapability */
        out_uint32_le(s, v->vc->capability_version);
        out_uint32_le(s, v->vc->capability_flags);
        s_mark_end(s);
        send_stream_to_clip_channel(v, s);

        /* Send the monitor ready PDU */
        init_stream(s, 0);
        out_cliprdr_header(s, CB_MONITOR_READY, 0);
        s_mark_end(s);
        send_stream_to_clip_channel(v, s);

        free_stream(s);
        /* Need to complete the startup handshake before we send formats */
        v->vc->startup_complete = 1;
    }

    return 0;
}
