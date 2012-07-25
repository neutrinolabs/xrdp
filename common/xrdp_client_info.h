/*
   Copyright (c) 2012 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   xrdp / xserver info / caps

*/

#if !defined(XRDP_CLIENT_INFO_H)
#define XRDP_CLIENT_INFO_H

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
  int channel_code; /* 0 = no channels 1 = channels */
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
};

#endif
