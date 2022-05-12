#ifndef _XRDP_ENCODER_VIDEOTOOLBOX_H
#define _XRDP_ENCODER_VIDEOTOOLBOX_H

void *
xrdp_encoder_videotoolbox_create(int width, int height);

int
xrdp_encoder_videotoolbox_delete(void *handle);

int
xrdp_encoder_videotoolbox_encode(void *handle, int session,
                                 int frame_id,
                                 int width, int height,
                                 int format, const char *data,
                                 char *cdata, int *cdata_bytes);


#endif