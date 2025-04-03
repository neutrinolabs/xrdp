/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * xrdp / xserver info / caps
 */

#include "xrdp_client_info.h"
#include "xrdp_constants.h"

#if !defined(XRDP_CLIENT_INFO_ALL_H)
#define XRDP_CLIENT_INFO_ALL_H

/* xrdp keyboard overrides */
struct xrdp_keyboard_overrides
{
    int type;
    int subtype;
    int layout;
};

enum client_resize_mode
{
    CRMODE_NONE,
    CRMODE_SINGLE_SCREEN,
    CRMODE_MULTI_SCREEN
};

/**
 * Type describing Unicode input state
 */
enum unicode_input_state
{
    UIS_UNSUPPORTED = 0, ///< Client does not support Unicode
    UIS_SUPPORTED,       ///< Client supports Unicode, but it's not active
    UIS_ACTIVE           ///< Unicode input is active
};
/**
 * Information about the xrdp client
 *
 * @note This structure is shared with xorgxrdp. If you change anything
 *       above the 'private to xrdp below this line' comment, you MUST
 *       bump the CLIENT_INFO_CURRENT_VERSION number so that the mismatch
 *       can be detected.
 */
struct xrdp_client_info_all
{
    struct xrdp_client_info pub; // Information shared with xorgxrdp

    /* bitmap cache info */
    int cache1_entries;
    int cache1_size;
    int cache2_entries;
    int cache2_size;
    int cache3_entries;
    int cache3_size;
    int bitmap_cache_persist_enable; /* 0 or 2 */
    int bitmap_cache_version; /* ored 1 = original version, 2 = v2, 4 = v3 */
    /* pointer info */
    int pointer_cache_entries;
    /* other */
    int use_bitmap_comp;
    int use_bitmap_cache;
    int op1; /* use smaller bitmap header, non cache */
    int op2; /* use smaller bitmap header in bitmap cache */
    int desktop_cache;
    int use_compact_packets; /* rdp5 smaller packets */
    char hostname[32];
    int build;
    int keylayout;
    char username[INFO_CLIENT_MAX_CB_LEN];
    char password[INFO_CLIENT_MAX_CB_LEN];
    char domain[INFO_CLIENT_MAX_CB_LEN];
    char program[INFO_CLIENT_MAX_CB_LEN];
    char directory[INFO_CLIENT_MAX_CB_LEN];
    int rdp_compression;
    int rdp_autologin;
    int crypt_level; /* 1, 2, 3 = low, medium, high */
    int channels_allowed; /* 0 = no channels 1 = channels */
    int sound_code; /* 1 = leave sound at server */
    int is_mce;
    int rdp5_performanceflags;
    int brush_cache_code; /* 0 = no cache 1 = 8x8 standard cache
                           2 = arbitrary dimensions */

    int max_bpp;
    int rfx;

    /* CAPSETTYPE_RAIL */
    int rail_support_level;
    /* CAPSETTYPE_WINDOW */
    int wnd_support_level;
    int wnd_num_icon_caches;
    int wnd_num_icon_cache_entries;
    /* codecs */
    int rfx_codec_id;
    int rfx_prop_len;
    char rfx_prop[64];
    int ns_codec_id;
    int ns_prop_len;
    char ns_prop[64];
    int jpeg_codec_id;
    int jpeg_prop_len;
    char jpeg_prop[64];
    int v3_codec_id;
    int rfx_min_pixel;
    int use_bulk_comp;
    int use_fast_path;
    int require_credentials; /* when true, credentials *must* be passed on cmd line */

    int security_layer; /* 0 = rdp, 1 = tls , 2 = hybrid */
    int multimon; /* 0 = deny , 1 = allow */

    int keyboard_type;
    int keyboard_subtype;

    int png_codec_id;
    int png_prop_len;
    char png_prop[64];
    int vendor_flags[4];
    int mcs_connection_type;
    int mcs_early_capability_flags;

    int max_fastpath_frag_bytes;

    char certificate[1024];
    char key_file[1024];

    /* codec */
    int h264_codec_id;
    int h264_prop_len;
    char h264_prop[64];

    int use_frame_acks;
    int max_unacknowledged_frame_count;

    long ssl_protocols;
    char *tls_ciphers;

    char client_ip[MAX_PEER_ADDRSTRLEN];
    char client_description[MAX_PEER_DESCSTRLEN];

    int client_os_major;
    int client_os_minor;

    int no_orders_supported;
    int use_cache_glyph_v2;
    int rail_enable;
    // Mask of reasons why output may be suppressed
    // (see enum suppress_output_reason)
    unsigned int suppress_output_mask;

    int enable_token_login;
    char domain_user_separator[16];

    /* xrdp.override_* values */
    struct xrdp_keyboard_overrides xrdp_keyboard_overrides;

    /* These values are optionally send over as part of TS_UD_CS_CORE.
     * They can be used as a fallback for a single monitor session
     * if physical sizes are not available in the monitor-specific
     * data */
    unsigned int session_physical_width; /* in mm */
    unsigned int session_physical_height; /* in mm */

    int gfx;

    // Can we resize the desktop by using a Deactivation-Reactivation Sequence?
    enum client_resize_mode client_resize_mode;

    enum unicode_input_state unicode_input_support;
};

/*
 * Return true if output is suppressed for a particular reason
 */
#define OUTPUT_SUPPRESSED_FOR_REASON(ci,reason) \
    (((ci)->suppress_output_mask & (unsigned int)reason) != 0)

#endif
