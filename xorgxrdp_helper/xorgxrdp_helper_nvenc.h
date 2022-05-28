
#ifndef _XORGXRDP_HELPER_NVENC_H
#define _XORGXRDP_HELPER_NVENC_H

struct enc_info;

int
xorgxrdp_helper_nvenc_init(void);
int
xorgxrdp_helper_nvenc_create_encoder(int width, int height, int tex,
                                     int tex_format, struct enc_info **ei);
int
xorgxrdp_helper_nvenc_delete_encoder(struct enc_info *ei);
int
xorgxrdp_helper_nvenc_encode(struct enc_info *ei, int tex,
                             void *cdata, int *cdata_bytes);

#endif
