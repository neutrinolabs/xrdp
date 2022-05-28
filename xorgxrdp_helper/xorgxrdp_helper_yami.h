#ifndef _XORGXRDP_HELPER_YAMI_H
#define _XORGXRDP_HELPER_YAMI_H

struct enc_info;

int
xorgxrdp_helper_yami_init(void);
int
xorgxrdp_helper_yami_create_encoder(int width, int height, int tex,
                                    int tex_format, struct enc_info **ei);
int
xorgxrdp_helper_yami_delete_encoder(struct enc_info *ei);
int
xorgxrdp_helper_yami_encode(struct enc_info *ei, int tex,
                            void *cdata, int *cdata_bytes);

#endif