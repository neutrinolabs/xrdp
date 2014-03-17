
#ifndef _XRDP_ENCODER_H
#define _XRDP_ENCODER_H

#include "arch.h"

struct xrdp_mm;

int APP_CC
init_xrdp_encoder(struct xrdp_mm *self);
void APP_CC
deinit_xrdp_encoder(struct xrdp_mm *self);
THREAD_RV THREAD_CC
proc_enc_msg(void *arg);

#endif
