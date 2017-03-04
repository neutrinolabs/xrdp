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

#if !defined(XRDP_CLIENT_INFO_H)
#define XRDP_CLIENT_INFO_H

struct monitor_info
{
  int left;
  int top;
  int right;
  int bottom;
  int is_primary;
};

struct xrdp_client_info
{
  int size; /* bytes for this structure */
  int bpp;
  int width;
  int height;
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
  char username[256];
  char password[256];
  char domain[256];
  char program[256];
  char directory[256];
  int rdp_compression;
  int rdp_autologin;
  int crypt_level; /* 1, 2, 3 = low, medium, high */
  int channels_allowed; /* 0 = no channels 1 = channels */
  int sound_code; /* 1 = leave sound at server */
  int is_mce;
  int rdp5_performanceflags;
  int brush_cache_code; /* 0 = no cache 1 = 8x8 standard cache
                           2 = arbitrary dimensions */
  char client_ip[256];
  int max_bpp;
  int jpeg; /* non standard bitmap cache v2 cap */
  int offscreen_support_level;
  int offscreen_cache_size;
  int offscreen_cache_entries;
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
  char orders[32];
  int order_flags_ex;
  int use_bulk_comp;
  int pointer_flags; /* 0 color, 1 new, 2 no new */
  int use_fast_path;
  int require_credentials; /* when true, credentials *must* be passed on cmd line */
  char client_addr[256];
  char client_port[256];

  int security_layer; /* 0 = rdp, 1 = tls , 2 = hybrid */
  int multimon; /* 0 = deny , 1 = allow */
  int monitorCount; /* number of monitors detected (max = 16) */
  struct monitor_info minfo[16]; /* client monitor data */
  struct monitor_info minfo_wm[16]; /* client monitor data, non-negative values */

  int keyboard_type;
  int keyboard_subtype;

  int png_codec_id;
  int png_prop_len;
  char png_prop[64];
  int vendor_flags[4];
  int mcs_connection_type;
  int mcs_early_capability_flags;

  int max_fastpath_frag_bytes;
  int capture_code;
  int capture_format;

  char certificate[1024];
  char key_file[1024];

  /* X11 keyboard layout - inferred from keyboard type/subtype */
  char model[16];
  char layout[16];
  char variant[16];
  char options[256];

  /* !!!!!!!!!!!!!!!!!!!!!!!!!! */
  /* NO CHANGES ABOVE THIS LINE */
  /* !!!!!!!!!!!!!!!!!!!!!!!!!! */

  /* codec */
  int h264_codec_id;
  int h264_prop_len;
  char h264_prop[64];

  int use_frame_acks;
  int max_unacknowledged_frame_count;

  long ssl_protocols;
  char *tls_ciphers;

  int client_os_major;
  int client_os_minor;

  int no_orders_supported;
};

#endif
