
#ifndef _XRDP_ENCODER_H
#define _XRDP_ENCODER_H

#include "arch.h"
#include "xrdp_client_info.h"
struct fifo;

struct xrdp_enc_data;

/* for codec mode operations */
struct xrdp_encoder
{
    struct xrdp_mm *mm;
    int in_codec_mode;
    int codec_id;
    int codec_quality;
    int max_compressed_bytes;
    tbus xrdp_encoder_event_to_proc;
    tbus xrdp_encoder_event_processed;
    tbus xrdp_encoder_term;
    struct fifo *fifo_to_proc;
    struct fifo *fifo_processed;
    tbus mutex;
    int (*process_enc)(struct xrdp_encoder *self, struct xrdp_enc_data *enc);
    void *codec_handle;
    int frame_id_client; /* last frame id received from client */
    int frame_id_server; /* last frame id received from Xorg */
    int frame_id_server_sent;
    int frames_in_flight;
    int gfx;
    int gfx_ack_off;
    const char *quants;
    int num_quants;
    int quant_idx_y;
    int quant_idx_u;
    int quant_idx_v;
};

/* used when scheduling tasks in xrdp_encoder.c */
struct xrdp_enc_data
{
    struct xrdp_mod *mod;
    int num_drects;
    short *drects;     /* 4 * num_drects */
    int num_crects;
    short *crects;     /* 4 * num_crects */
    char *data;
    int left;
    int top;
    int width;
    int height;
    int flags;
    int frame_id;
    void *shmem_ptr;
    int shmem_bytes;
};

typedef struct xrdp_enc_data XRDP_ENC_DATA;

/* used when scheduling tasks from xrdp_encoder.c */
struct xrdp_enc_data_done
{
    int comp_bytes;
    int pad_bytes;
    char *comp_pad_data;
    struct xrdp_enc_data *enc;
    int last; /* true is this is last message for enc */
    int continuation; /* true if this isn't the start of a frame */
    int x;
    int y;
    int cx;
    int cy;
    enum xrdp_encoder_flags flags;
};

typedef struct xrdp_enc_data_done XRDP_ENC_DATA_DONE;

struct xrdp_encoder *
xrdp_encoder_create(struct xrdp_mm *mm);
void
xrdp_encoder_delete(struct xrdp_encoder *self);
THREAD_RV THREAD_CC
proc_enc_msg(void *arg);

#endif
