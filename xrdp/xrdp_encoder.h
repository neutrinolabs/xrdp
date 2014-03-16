#ifndef _XRDP_ENCODER_H
#define _XRDP_ENCODER_H

struct xrdp_mm *self;

int   init_xrdp_encoder(struct xrdp_mm *mm);
void  deinit_xrdp_encoder();
void *proc_enc_msg(void *arg);

#endif
