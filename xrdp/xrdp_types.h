/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Copyright (C) Jay Sorg 2004

   types

*/

/* tcp */
struct xrdp_tcp
{
  int sck;
  struct stream* in_s;
  struct stream* out_s;
  struct xrdp_iso* iso_layer;
};

/* iso */
struct xrdp_iso
{
  struct stream* in_s;
  struct stream* out_s;
  struct xrdp_mcs* mcs_layer;
  struct xrdp_tcp* tcp_layer;
};

/* mcs */
struct xrdp_mcs
{
  struct stream* in_s;
  struct stream* out_s;
  struct xrdp_sec* sec_layer;
  struct xrdp_iso* iso_layer;
  int userid;
  int chanid;
  struct stream* client_mcs_data;
  struct stream* server_mcs_data;
};

/* sec */
struct xrdp_sec
{
  struct stream* in_s;
  struct stream* out_s;
  struct xrdp_rdp* rdp_layer;
  struct xrdp_mcs* mcs_layer;
  char server_random[32];
  char client_random[64];
  char client_crypt_random[72];
  struct stream client_mcs_data;
  struct stream server_mcs_data;
  int decrypt_use_count;
  int encrypt_use_count;
  char decrypt_key[16];
  char encrypt_key[16];
  char decrypt_update_key[16];
  char encrypt_update_key[16];
  int rc4_key_size;
  int rc4_key_len;
  char sign_key[16];
  void* decrypt_rc4_info;
  void* encrypt_rc4_info;
};

/* rdp */
struct xrdp_rdp
{
  struct stream* in_s;
  struct stream* out_s;
  struct xrdp_process* pro_layer;
  struct xrdp_sec* sec_layer;
  char* next_packet;
  int share_id;
  int mcs_channel;
  int bpp;
  int width;
  int height;
};

/* rdp process */
struct xrdp_process
{
  int status;
  int sck;
  int term;
  struct stream in_s;
  struct stream out_s;
  struct xrdp_listen* lis_layer;
  struct xrdp_rdp* rdp_layer;
};

/* rdp listener */
struct xrdp_listen
{
  int status;
  int sck;
  int term;
  struct xrdp_process* process_list[100]; /* 100 processes possible */
  int process_list_count;
  int process_list_max;
};
